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

#ifndef MIKOTO_FILE_WATCHER_HH
#define MIKOTO_FILE_WATCHER_HH

#include <efsw/efsw.hpp>
#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Library/IO/File.hh>
#include <Library/Utility/Types.hh>
#include <Filesystem/FileWatcher.hh>

namespace Mikoto {

    struct FileWatcherServiceCreateInfo {
        bool FollowSymLinks{ false };
    };

    class FileWatcherService final : public IService, public Singleton<FileWatcherService> {
    public:

        explicit FileWatcherService( const FileWatcherServiceCreateInfo& info);

        MKT_NODISCARD auto UnWatch(const Path &path, UInt64 watchID) -> bool;
        MKT_NODISCARD auto Watch(const Path& path, FileWatcher::WatcherCallback&& callback) -> UInt64;

        auto Init() -> void override;
        auto Shutdown() -> void override;

    private:
        // efsw
        Unique<efsw::FileWatcher> m_FileWatcher{};

    private:
        ankerl::unordered_dense::map<std::string, Unique<FileWatcher>> m_WatchedPaths{};

        bool m_FollowSymLinks{ false };
    };
}// namespace Mikoto

#endif// MIKOTO_FILE_WATCHER_HH
