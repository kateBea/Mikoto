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

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Library/IO/File.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    enum class FileWatchEvent { MODIFIED, CREATED, DELETED, UNDEFINED };

    struct FileWatcherServiceCreateInfo {};

    class FileWatcherService final : public IService, public Singleton<FileWatcherService> {
    public:
        using FileWatchCallback = std::function< void( const Path& path, FileWatchEvent ) >;

        explicit FileWatcherService( const FileWatcherServiceCreateInfo& info);

        auto Watch(const Path& path, FileWatchCallback&& callback) -> void;

        auto Init() -> void override;
        auto Shutdown() -> void override;

    private:
        ankerl::unordered_dense::map<std::string, std::vector<FileWatchCallback>> m_WatchedPaths{};
    };
}// namespace Mikoto

#endif// MIKOTO_FILE_WATCHER_HH
