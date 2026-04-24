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

#ifndef MIKOTO_FILE_WATCHER_HH
#define MIKOTO_FILE_WATCHER_HH

#include <EASTL/utility.h>
#include <EASTL/unique_ptr.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Filesystem/Path.hh>
#include <Filesystem/FileWatcher.hh>

namespace mikoto::filesystem {

    using namespace mikoto::core;

    struct FileWatcherServiceCreateInfo {};

    class FileWatcherService final : public IService, public Singleton<FileWatcherService> {
    public:

        explicit FileWatcherService( const FileWatcherServiceCreateInfo& info);

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto Watch(const Path& path, FileWatcher::WatcherCallback&& callback) -> void;

    private:
        std::mutex mWatcherInsertMutex{};
        ankerl::unordered_dense::map<Path, eastl::unique_ptr<FileWatcher>> mWatchedPaths{};
    };
}

#endif// MIKOTO_FILE_WATCHER_HH
