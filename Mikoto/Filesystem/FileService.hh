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

#ifndef MIKOTO_FILESYSTEM_FILE_SERVICE_HH
#define MIKOTO_FILESYSTEM_FILE_SERVICE_HH

#include <mutex>

#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Filesystem/File.hh>

namespace mikoto::filesystem {

    struct FileServiceCreateInfo {

    };

    class FileService final : public core::IService, public core::Singleton<FileService> {
    public:
        explicit FileService( const FileServiceCreateInfo& options );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        // Reuse the normalized cache entry and apply pending watcher changes.
        // Returns an empty handle on I/O failure; a valid empty file has a handle.
        MKT_NODISCARD auto LoadFile( const Path& path ) -> FileHandle;

        MKT_NODISCARD auto CreateNewFile( const Path& path ) -> FileHandle;
        MKT_NODISCARD auto CreateNewFile( eastl::string_view path ) -> FileHandle;

        // Cache lookup only (with pending reload), never creates a cache entry.
        MKT_NODISCARD auto GetFile( const Path& path ) -> FileHandle;
        MKT_NODISCARD auto GetFile( const Path& path ) const -> FileHandle;

        ~FileService() override = default;

    private:
        auto OpenFile( const Path& path, bool create ) -> FileHandle;
        auto WatchFile( FileHandle file ) -> void;

    private:
        mutable std::mutex mFileLoadMutex{};

        // Keys are absolute, lexically normalized UTF-8 paths.
        ankerl::unordered_dense::map<eastl::string, FileHandle> mFiles{};
    };
}// namespace mikoto::filesystem

#endif// MIKOTO_FILESYSTEM_FILE_SERVICE_HH