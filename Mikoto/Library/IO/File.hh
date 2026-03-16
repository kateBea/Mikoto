//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef FILE_HH
#define FILE_HH

#include <filesystem>
#include <fstream>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>
#include <mutex>

// TODO: move this file to filesystem
#include <Common/Common.hh>
#include <Common/ReferenceCounted.hh>
#include <Library/Utility/Types.hh>
#include <Logging/Logger.hh>

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
        UNKNOWN_FILE_TYPE = -1,

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

    class File final {
    public:
        File(File&&) = delete;
        auto operator=(File&&) noexcept -> File& = delete;

        // Name of file with extension
        MKT_NODISCARD auto GetName() const -> std::string;

        MKT_NODISCARD auto GetPath() const -> const std::string&;
        MKT_NODISCARD auto GetPathView() const -> std::string_view;

        MKT_NODISCARD auto GetContentsBytes() const -> const void*;
        MKT_NODISCARD auto GetContentsString() const -> const std::string&;

        // Size in bytes
        MKT_NODISCARD auto GetSize() const -> Size;
        MKT_NODISCARD auto GetType() const -> FileType;

        MKT_NODISCARD auto GetExtension() const -> const std::string&;

        auto FlushContents() -> void;
        auto SetContents( std::string&& contents ) -> void;

        auto UpdateContentsFromDisk() -> void;

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
        Path m_Path{};
        std::string m_PathUtf8{};
        std::string m_Extension{};

        Size m_Size{};
        std::fstream m_FileStream{};

        std::string m_Contents{};
        FileType m_Type{ FileType::UNKNOWN_FILE_TYPE };
        FileMode m_OpenMode{ MKT_FILE_OPEN_MODE_NONE };

        mutable std::mutex m_FileUpdateMutex{};
    };

    using FileHandle = Ref<File>;
}

#endif // MIKOTO_FILE_HH
