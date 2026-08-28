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

#ifndef MIKOTO_FILE_SERVICE_HH
#define MIKOTO_FILE_SERVICE_HH

#include <mutex>

#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>
#include <Filesystem/File.hh>

namespace mikoto::filesystem {

    struct FileServiceCreateInfo {};

    class FileService final : public core::IService, public core::Singleton<FileService> {
    public:
        explicit FileService( const FileServiceCreateInfo& options );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto LoadFile( const Path& path ) -> FileHandle;

        MKT_NODISCARD auto CreateNewFile( const Path& path ) -> FileHandle;
        MKT_NODISCARD auto CreateNewFile( eastl::string_view path ) -> FileHandle;

        MKT_NODISCARD auto GetFile( const Path& path ) -> FileHandle;
        MKT_NODISCARD auto GetFile( const Path& path ) const -> const FileHandle;

        ~FileService() override = default;

    private:
        std::mutex mFileLoadMutex{};

        // Change from Unique to Ref<File>
        ankerl::unordered_dense::map<eastl::string, FileHandle> mFiles{};
    };
}

#endif // MIKOTO_FILE_SERVICE_HH
