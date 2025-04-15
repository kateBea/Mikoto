//
// Created by zanet on 3/26/2025.
//

#include <Library/Filesystem/File.hh>

namespace Mikoto {

    File::File( const Path_T &path, FileMode openMode )
        : m_Path{ path.string() }, m_OpenMode{ openMode } {
        if ( std::filesystem::is_regular_file( m_Path ) ) {
            // Open the file once to import metadata and its contents
            m_FileStream = std::move( std::fstream{ path, std::ios::binary | std::ios::in | std::ios::out } );

            LoadContents();
            InferFileSize();

            // Close the file after we have finished
            // fetching its contents, if any other external library attempts
            // to open the same file it might fail
            if ( m_FileStream.is_open() ) {
                m_FileStream.close();

                m_FileStream = {};
            }

            m_Extension = InferExtensionFromFileSignature( m_Contents );
            m_Type = InferFileType( m_Extension );
        }
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

    auto File::CompareSignature( const std::string &fileContent, const std::vector<UChar_T> &signature ) -> bool {
        if ( fileContent.size() < signature.size() ) return false;
        for ( size_t i = 0; i < signature.size(); ++i ) {
            if ( static_cast<UChar_T>( fileContent[i] ) != signature[i] ) {
                return false;
            }
        }
        return true;
    }

    auto File::InferExtensionFromFileSignature( const std::string &fileContent ) -> std::string {
        // https://en.wikipedia.org/wiki/List_of_file_signatures
        static const std::unordered_map<std::string, std::vector<UChar_T>> signatureMap{
            { "png", { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A } },   // PNG
            { "jpeg", { 0xFF, 0xD8, 0xFF, 0xE0 } },                       // JPEG
            { "jpg", { 0xFF, 0xD8, 0xFF } },                              // JPG
            { "gif", { 'G', 'I', 'F', '8' } },                            // GIF87a or GIF89a
            { "pdf", { '%', 'P', 'D', 'F', '-' } },                       // PDF
            { "zip", { 0x50, 0x4B, 0x03, 0x04 } },                        // ZIP
            { "rar", { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07 } },            // RAR
            { "bmp", { 'B', 'M' } },                                      // BMP
            { "mp3", { 0x49, 0x44, 0x33 } },                              // MP3 with ID3 tag
            { "exe", { 0x4D, 0x5A } },                                    // MZ header for executables
            { "ogg", { 'O', 'g', 'g', 'S' } },                            // OGG
            { "wav", { 'R', 'I', 'F', 'F' } },                            // WAV
            { "mp4", { 0x00, 0x00, 0x00, 0x18, 0x66, 0x74, 0x79, 0x70 } },// MP4
            { "tar", { 0x75, 0x73, 0x74, 0x61, 0x72 } },                  // TAR
            { "7z", { 0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C } },             // 7-Zip
        };

        const auto itResult{ std::ranges::find_if( signatureMap, [&fileContent]( const std::pair<const std::string, std::vector<UChar_T>> &pair ) {
            return CompareSignature( fileContent, pair.second );
        } ) };

        if ( itResult != signatureMap.end() ) {
            return itResult->first;
        }

        return "";
    }
}// namespace Mikoto