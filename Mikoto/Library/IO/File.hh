//
// Created by zanet on 1/27/2025.
//

#ifndef FILE_HH
#define FILE_HH

// TODO: move this file to filesystem
#include <Common/Common.hh>
#include <Common/ReferenceCounted.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Logger.hh>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>
#include <mutex>

#define MKT_CSTRING_PATH(path) path.string().c_str()

namespace Mikoto {

    enum FileMode {
        // Open for read and write operations
        MKT_FILE_OPEN_MODE_NONE = 0,

        MKT_FILE_OPEN_MODE_READ = BIT_SET( 0 ),
        MKT_FILE_OPEN_MODE_WRITE = BIT_SET( 1 ),
        MKT_FILE_OPEN_MODE_READ_WRITE = BIT_SET( 1 ),

        MKT_FILE_OPEN_MODE_TRUNCATE = BIT_SET( 2 ),
        MKT_FILE_OPEN_MODE_APPEND = BIT_SET( 3 ),

        MKT_FILE_OPEN_MODE_BINARY = BIT_SET( 3 ),
    };

    enum class FileType {
        UNKNOWN_FILE_TYPE,

        PNG_IMAGE_TYPE,
        JPEG_IMAGE_TYPE,
        JPG_IMAGE_TYPE,
        BMP_IMAGE_TYPE,

        MP3_AUDIO_TYPE,
        WAV_AUDIO_TYPE,
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
            case FileType::MP3_AUDIO_TYPE:
                return "mp3";
            case FileType::WAV_AUDIO_TYPE:
                return "wav";

            case FileType::UNKNOWN_FILE_TYPE:
            default:
                return "unknown";
        }
    }

    class File final /*IResource so it is ref counted*/ {
    public:
        File(File&&) = default;
        auto operator=(File&&) noexcept -> File& = default;

        MKT_NODISCARD auto GetPath() const -> const std::string& { return m_Path; }
        MKT_NODISCARD auto GetName() const -> std::string { return m_PathObject.filename().string(); }
        MKT_NODISCARD auto GetExtension() const -> const std::string& { return m_Extension; }
        MKT_NODISCARD auto GetPathCStr() const -> CStr { return m_Path.c_str(); }
        MKT_NODISCARD auto GetFileBytes() const -> const void* { return m_Contents.c_str(); }
        MKT_NODISCARD auto GetFileContents() const -> const std::string&;
        MKT_NODISCARD auto GetSize() const -> double { return static_cast<double>( m_Size ) / 1'000'000.0; }
        MKT_NODISCARD auto GetType() const -> FileType { return m_Type; }
        MKT_NODISCARD auto GetSizeBytes() const -> Size { return m_Size; }
        MKT_NODISCARD auto IsDirectory() const -> bool { return std::filesystem::is_directory( m_Path ); }
        MKT_NODISCARD auto IsFile() const -> bool { return std::filesystem::is_regular_file( m_Path ); }

        auto FlushContents() -> void;
        auto SetContents( CStr contents ) -> void;

        auto UpdateContents() -> void;

        static auto Load( const Path& path, FileMode openMode = MKT_FILE_OPEN_MODE_NONE ) -> Unique<File>;
        static auto Create( const Path& path, FileMode openMode = MKT_FILE_OPEN_MODE_NONE ) -> Unique<File>;

    private:
        File( const Path& path, std::fstream&& stream, FileMode openMode = MKT_FILE_OPEN_MODE_NONE );

        /**
         * Returns a string containing the data from a file
         * @returns contents of the file
         * */
        auto LoadContents() -> void;

        /**
         * @brief Determines the size of a file.
         * @returns Size in KB of the given file, -1 if the file is not valid (not a directory or does not exist).
         * */
        auto InferFileSize() -> void;

        MKT_NODISCARD static auto InferFileType( const std::string& extension ) -> FileType;
        MKT_NODISCARD static auto CompareSignature( const std::string& fileContent, const std::vector<UChar>& signature ) -> bool;
        MKT_NODISCARD static auto InferExtensionFromFileSignature( const std::string& fileContent ) -> std::string;

    private:
        Path m_PathObject{};
        std::string m_Path{};
        std::string m_Extension{};

        Size m_Size{};
        std::fstream m_FileStream{};

        std::string m_Contents{};
        FileType m_Type{ FileType::UNKNOWN_FILE_TYPE };
        FileMode m_OpenMode{ MKT_FILE_OPEN_MODE_NONE };

        mutable std::mutex m_FileUpdateMutex{};
    };

    using FileHandle = Ref<File>;

}// namespace Mikoto
#endif//FILE_HH
