//
// Created by zanet on 1/27/2025.
//

#ifndef FILE_HH
#define FILE_HH
#include <Core/Logging/Logger.hh>
#include <Library/Utility/Types.hh>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Mikoto {

    enum FileMode {
        // Open for read and write operations
        MKT_FILE_OPEN_MODE_NONE = 0,

        MKT_FILE_OPEN_MODE_READ = BIT_SET( 0 ),
        MKT_FILE_OPEN_MODE_WRITE = BIT_SET( 1 ),

        MKT_FILE_OPEN_MODE_TRUNCATE = BIT_SET( 2 ),
        MKT_FILE_OPEN_MODE_APPEND = BIT_SET( 3 ),
    };

    MKT_NODISCARD static auto IsModeSet( const FileMode modes, const FileMode searchMode ) -> bool {
        return modes & searchMode;
    }

    MKT_NODISCARD constexpr auto GetFileExtensionName( const FileType type ) -> std::string_view {
        switch ( type ) {
            case FileType::PNG_IMAGE_TYPE:
                return "png";
            case FileType::JPEG_IMAGE_TYPE:
                return "jpeg";
            case FileType::JPG_IMAGE_TYPE:
                return "jpg";
            case FileType::BMP_IMAGE_TYPE:
                return "bmp";
            case FileType::GIF_IMAGE_TYPE:
                return "gif";
            case FileType::TIFF_IMAGE_TYPE:
                return "tiff";
            case FileType::WEBP_IMAGE_TYPE:
                return "webp";
            case FileType::ICO_IMAGE_TYPE:
                return "ico";
            case FileType::SVG_VECTOR_IMAGE_TYPE:
                return "svg";

            case FileType::PDF_DOCUMENT_TYPE:
                return "pdf";
            case FileType::TEXT_DOCUMENT_TYPE:
                return "txt";

            case FileType::MP3_AUDIO_TYPE:
                return "mp3";
            case FileType::WAV_AUDIO_TYPE:
                return "wav";
            case FileType::OGG_AUDIO_TYPE:
                return "ogg";
            case FileType::FLAC_AUDIO_TYPE:
                return "flac";
            case FileType::AAC_AUDIO_TYPE:
                return "aac";
            case FileType::WMA_AUDIO_TYPE:
                return "wma";

            case FileType::UNKNOWN_IMAGE_TYPE:
            default:
                return "unknown";
        }
    }

    class File final {
    public:
        explicit File( const Path_T& path, const FileMode openMode = MKT_FILE_OPEN_MODE_NONE )
            : m_Path{ path.string().c_str() }, m_OpenMode{ openMode } {

            if ( std::filesystem::is_regular_file( m_Path ) ) {
                // Open the file once to import metadata and its contents
                m_FileStream = std::move( std::fstream{ path, std::ios::binary | std::ios::in | std::ios::out } );

                LoadContents();
                SetFileSize();

                // Close the file after we have finished
                // fetching its contents, if any other external library attempts
                // to open the same file it might fail
                if ( m_FileStream.is_open() ) {
                    m_FileStream.close();

                    m_FileStream = {};
                }

                m_Extension = GetExtensionFromFileSignature( m_Contents );
                m_Type = SetType( m_Extension );
            }
        }

        MKT_NODISCARD auto GetPath() const -> const std::string& { return m_Path; }
        MKT_NODISCARD auto GetExtension() const -> const std::string& { return m_Extension; }
        MKT_NODISCARD auto GetPathCStr() const -> CStr_T { return m_Path.c_str(); }
        MKT_NODISCARD auto GetFileContents() const -> const std::string& { return m_Contents; }
        MKT_NODISCARD auto GetSize() const -> double { return static_cast<double>( m_Size ) / 1'000'000.0; }
        MKT_NODISCARD auto GetType() const -> FileType { return m_Type; }
        MKT_NODISCARD auto GetSizBytes() const -> Size_T { return m_Size; }
        MKT_NODISCARD auto IsDirectory() const -> bool { return std::filesystem::is_directory( m_Path ); }
        MKT_NODISCARD auto IsFile() const -> bool { return std::filesystem::is_directory( m_Path ); }

        auto FlushContents( const CStr_T contents, const FileMode mode = MKT_FILE_OPEN_MODE_TRUNCATE ) -> void {
            if ( !IsModeSet( m_OpenMode, MKT_FILE_OPEN_MODE_WRITE ) ) {
                MKT_CORE_LOGGER_WARN( "File::FlushContents - File was not opened for writing." );
                return;
            }

            m_Contents = contents;

            auto openMode{ std::ios_base::out };

            switch ( mode ) {

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

    private:
        /**
         * Returns a string containing the data from a file
         * @returns contents of the file
         * */
        auto LoadContents() -> void {
            if ( !m_FileStream.is_open() ) {
                MKT_CORE_LOGGER_ERROR( "Failed to open file [ {} ]!", m_Path );
                return;
            }

            m_Contents = std::move( std::string{ std::istreambuf_iterator( m_FileStream ),
                                                 std::istreambuf_iterator<std::vector<char>::value_type>() } );
        }

        /**
         * @brief Determines the size of a file.
         * @returns Size in KB of the given file, -1 if the file is not valid (not a directory or does not exist).
         * */
        MKT_NODISCARD auto SetFileSize() -> void {
            if ( m_FileStream.is_open() ) {
                // seek to the start if we had already read from this file
                m_FileStream.seekg( 0 );
                m_Size = std::distance( std::istreambuf_iterator<char>( m_FileStream ), std::istreambuf_iterator<char>() );
            }
        }

        MKT_NODISCARD static auto SetType( const std::string& extension ) -> FileType {
            const static std::unordered_map<std::string, FileType> values{
                { "png", FileType::PNG_IMAGE_TYPE },
                { "jpeg", FileType::JPEG_IMAGE_TYPE },
                { "jpg", FileType::JPG_IMAGE_TYPE },
                { "bmp", FileType::BMP_IMAGE_TYPE },
                { "gif", FileType::GIF_IMAGE_TYPE },
                { "tiff", FileType::TIFF_IMAGE_TYPE },
                { "webp", FileType::WEBP_IMAGE_TYPE },
                { "ico", FileType::ICO_IMAGE_TYPE },

                { "pdf", FileType::PDF_DOCUMENT_TYPE },
                { "txt", FileType::TEXT_DOCUMENT_TYPE },

                { "mp3", FileType::MP3_AUDIO_TYPE },
                { "wav", FileType::WAV_AUDIO_TYPE },
                { "ogg", FileType::OGG_AUDIO_TYPE },
                { "flac", FileType::FLAC_AUDIO_TYPE },
                { "aac", FileType::AAC_AUDIO_TYPE },
                { "wma", FileType::WMA_AUDIO_TYPE },


                { "svg", FileType::SVG_VECTOR_IMAGE_TYPE },

            };

            if ( const auto result{ values.find( extension ) }; result != values.end() ) {
                return result->second;
            }

            return FileType::UNKNOWN_IMAGE_TYPE;
        }

        MKT_NODISCARD static auto CompareSignature( const std::string& fileContent, const std::vector<UChar_T>& signature ) -> bool {
            if ( fileContent.size() < signature.size() ) return false;
            for ( size_t i = 0; i < signature.size(); ++i ) {
                if ( static_cast<UChar_T>( fileContent[i] ) != signature[i] ) {
                    return false;
                }
            }
            return true;
        }

        MKT_NODISCARD static auto GetExtensionFromFileSignature( const std::string& fileContent ) -> std::string {
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

            const auto itResult{ std::ranges::find_if( signatureMap, [&fileContent]( const std::pair<const std::string, std::vector<UChar_T>>& pair ) {
                return CompareSignature( fileContent, pair.second );
            } ) };

            if ( itResult != signatureMap.end() ) {
                return itResult->first;
            }

            return "";
        }

    private:
        // Make it byte string for cross-platform compatibility
        std::string m_Path{};
        std::string m_Extension{};

        Size_T m_Size{};
        std::fstream m_FileStream{};

        FileType m_Type{ FileType::UNKNOWN_IMAGE_TYPE };
        std::string m_Contents{};

        FileMode m_OpenMode{ MKT_FILE_OPEN_MODE_NONE };
    };

}// namespace Mikoto
#endif//FILE_HH
