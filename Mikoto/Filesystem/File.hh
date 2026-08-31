//    Copyright 2025 ケイト
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

#ifndef MIKOTO_FILE_HH
#define MIKOTO_FILE_HH

#include <filesystem>
#include <fstream>
#include <mutex>
#include <ranges>
#include <unordered_map>
#include <utility>
#include <vector>


#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/ReferenceCounted.hh>

#include <Filesystem/Path.hh>
#include <Filesystem/FileSystem.hh>

namespace mikoto::filesystem {

    using namespace mikoto::core;

    // This object is exsposed to the application which does not care about internal
    // it facilitates operations on files such as loading the current contents of a file and exposing it to the engine
    // or persisting engine side current contents to disk
    class File final : public ReferenceCounted {
    public:
        // If create == true, the file is created if it does not exists
        explicit File( const Path& path, bool create = false );
        explicit File( const eastl::string& path, bool create = false );
        explicit File( eastl::string_view path, bool create = false );

        // Name of file with extension
        MKT_NODISCARD auto GetName() const -> eastl::string_view;

        MKT_NODISCARD auto GetPath() const -> const Path&;
        MKT_NODISCARD auto GetDirectory() const -> Path;

        MKT_NODISCARD auto HasContents() const -> bool;
        MKT_NODISCARD auto GetContentsBytes() -> void*;
        MKT_NODISCARD auto GetContentsBytes() const -> const void*;
        MKT_NODISCARD auto GetContentsString() const -> const eastl::string&;

        // Size in bytes
        MKT_NODISCARD auto GetSize() const -> size_t;
        MKT_NODISCARD auto GetType() const -> FileType;

        MKT_NODISCARD auto GetExtension() const ->  eastl::string_view;

        auto FlushContents() -> void;
        auto SetContents( eastl::string&& contents ) -> void;

        // Set contents and flush
        auto Write( const void* ptr, size_t sizeBytes) -> void;

        auto UpdateContentsFromDisk() -> void;

        DISABLE_COPY_AND_MOVE_FOR( File );

    private:
        auto LoadContents() -> void;

        MKT_NODISCARD static auto InferFileType( eastl::string_view extension ) -> FileType;
        MKT_NODISCARD static auto InferExtensionFromFileSignature( const eastl::string &fileContent ) -> eastl::string;
        MKT_NODISCARD static auto CompareSignature( const eastl::string& fileContent, const eastl::vector<uchar>& signature ) -> bool;

    private:
        Path mPath{};
        std::fstream mFileStream{};

        // This will be used
        // to infer the size
        eastl::string mContents{};

        FileType mType{ FileType::eInvalid };

        mutable std::mutex mFileUpdateMutex{};
    };

    using FileHandle = core::Ref<File>;
}// namespace mikoto::filesystem


#endif// MIKOTO_FILE_HH
