//
// Created by zanet on 3/26/2025.
//

#include <Library/IO/File.hh>
#include <Logging/Logger.hh>

namespace Mikoto {

    auto File::Load( const Path &path, FileMode openMode ) -> Unique<File> {
        // Open the file once to import metadata and its contents
        if ( std::fstream stream{ path, std::ios::binary | std::ios::in | std::ios::out } ) {
            return Unique<File>( new File(path, std::move(stream), openMode) ) ;
        }

        return nullptr;
    }

    File::File( const Path& path, std::fstream&& stream, const FileMode openMode )
        : m_PathObject{ path }, m_Path{ path.string() }, m_FileStream{ std::move( stream ) }, m_OpenMode{ openMode } {

        LoadContents();
        InferFileSize();

        // Close the file after we have finished
        // fetching its contents, if any other external library attempts
        // to open the same file, it might fail
        if ( m_FileStream.is_open() ) {
            m_FileStream.close();

            m_FileStream = {};
        }

        m_Extension = InferExtensionFromFileSignature( m_Contents );
        m_Type = InferFileType( m_Extension );
    }

    auto File::FlushContents() -> void {
        if ( !IsModeSet( m_OpenMode, MKT_FILE_OPEN_MODE_WRITE ) ) {
            MKT_CORE_LOGGER_WARN( "File::FlushContents - File was not opened for writing." );
            return;
        }

        auto openMode{ std::ios_base::out };

        switch ( m_OpenMode ) {

            case MKT_FILE_OPEN_MODE_TRUNCATE:
                openMode |= std::ios_base::trunc;
                break;
            case MKT_FILE_OPEN_MODE_APPEND:
                openMode |= std::ios_base::app;
                break;
            default:
                break;
        }

        m_FileStream = std::move( std::fstream{ m_Path, openMode } );

        if ( m_FileStream.is_open() ) {
            m_FileStream << m_Contents;

            m_FileStream.close();
        }
    }

    auto File::SetContents( CStr contents ) -> void {
        m_Contents = contents;
    }

    auto File::LoadContents() -> void {
        if ( !m_FileStream.is_open() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to open file [ {} ]!", m_Path );
            return;
        }

        m_Contents = std::move( std::string{ std::istreambuf_iterator( m_FileStream ),
                                             std::istreambuf_iterator<std::vector<char>::value_type>() } );
    }

    auto File::InferFileSize() -> void {
        if ( m_FileStream.is_open() ) {
            // seek to the start if we had already read from this file
            m_FileStream.seekg( 0 );
            m_Size = std::distance( std::istreambuf_iterator<char>( m_FileStream ), std::istreambuf_iterator<char>() );
        }
    }

    auto File::InferFileType( const std::string &extension ) -> FileType {
        const static std::unordered_map<std::string, FileType> values{
            { "png", FileType::PNG_IMAGE_TYPE },
            { "jpeg", FileType::JPEG_IMAGE_TYPE },
            { "jpg", FileType::JPG_IMAGE_TYPE },
            { "bmp", FileType::BMP_IMAGE_TYPE },

            { "mp3", FileType::MP3_AUDIO_TYPE },
            { "wav", FileType::WAV_AUDIO_TYPE },

        };

        if ( const auto result{ values.find( extension ) }; result != values.end() ) {
            return result->second;
        }

        return FileType::UNKNOWN_FILE_TYPE;
    }

    auto File::CompareSignature( const std::string &fileContent, const std::vector<UChar> &signature ) -> bool {
        if ( fileContent.size() < signature.size() ) return false;
        for ( size_t i = 0; i < signature.size(); ++i ) {
            if ( static_cast<UChar>( fileContent[i] ) != signature[i] ) {
                return false;
            }
        }
        return true;
    }

    auto File::InferExtensionFromFileSignature( const std::string &fileContent ) -> std::string {
        // https://en.wikipedia.org/wiki/List_of_file_signatures
        static const std::unordered_map<std::string, std::vector<UChar>> signatureMap{
            { "png", { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A } },   // PNG
            { "jpeg", { 0xFF, 0xD8, 0xFF, 0xE0 } },                       // JPEG
            { "jpg", { 0xFF, 0xD8, 0xFF } },                              // JPG
            { "mp3", { 0x49, 0x44, 0x33 } },                              // MP3 with ID3 tag
            { "wav", { 'R', 'I', 'F', 'F' } },                            // WAV
            { "mp4", { 0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70 } },// MP4
        };

        const auto itResult{ std::ranges::find_if( signatureMap, [&fileContent]( const std::pair<const std::string, std::vector<UChar>> &pair ) {
            return CompareSignature( fileContent, pair.second );
        } ) };

        if ( itResult != signatureMap.end() ) {
            return itResult->first;
        }

        return "";
    }
}// namespace Mikoto