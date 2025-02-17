//
// Created by zanet on 1/27/2025.
//

#ifndef FILE_HH
#define FILE_HH

#include <Core/Logging/Logger.hh>
#include <Library/Utility/Types.hh>
#include <vector>

namespace Mikoto {
    class File {
    public:
        explicit File( const Path_T& path )
            : m_Path{ path } {

            m_FileStream = std::move( std::fstream{ path , std::ios::binary | std::ios::in | std::ios::out } );

            LoadContents();
            SetFileSize();

            m_Type = SetType( path.extension().string() );

            // Close the file after we have finished
            // fetching its contents, if any other external library attempts
            // to open the same file it might fail
            if (m_FileStream.is_open()) {
                m_FileStream.close();
            }
        }

        MKT_NODISCARD auto GetPath() const -> const Path_T& { return m_Path; }
        MKT_NODISCARD auto GetFileContents() const -> const std::string& { return m_Contents; }
        MKT_NODISCARD auto GetSize() const -> Size_T { return m_Size / 1'000; }
        MKT_NODISCARD auto IsDirectory() const -> bool { return std::filesystem::is_directory(m_Path); }
        MKT_NODISCARD auto IsFile() const -> bool { return std::filesystem::is_directory(m_Path); }

    private:
        /**
         * Returns a string containing the data from a file
         * @returns contents of the file
         * */
        auto LoadContents() -> void {
            if ( !m_FileStream.is_open() ) {
                MKT_CORE_LOGGER_ERROR( "Failed to open file [ {} ]!", m_Path.string() );
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
                m_FileStream.seekg(0);
                m_Size = std::distance(std::istreambuf_iterator<char>( m_FileStream ), std::istreambuf_iterator<char>() );
            }
        }

        MKT_NODISCARD static auto SetType(const std::string& extension) -> FileType {
            const static std::unordered_map<std::string, FileType> values{
                    { ".png" , FileType::PNG_IMAGE_TYPE },
                    { ".jpeg", FileType::JPEG_IMAGE_TYPE},
                    { ".jpg" , FileType::JPG_IMAGE_TYPE },
                };

            if ( const auto result{ values.find( extension ) }; result != values.end()) {
                return result->second;
            }

            return FileType::UNKNOWN_IMAGE_TYPE;
        }

    private:
        Path_T m_Path{};
        Size_T m_Size{};
        std::fstream m_FileStream{};

        FileType m_Type{ FileType::UNKNOWN_IMAGE_TYPE };
        std::string m_Contents{};
    };

}// namespace Mikoto
#endif//FILE_HH
