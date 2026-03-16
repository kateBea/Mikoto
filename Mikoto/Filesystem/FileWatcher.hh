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

#ifndef MIKOTO_FILE_WATCH_HH
#define MIKOTO_FILE_WATCH_HH

#include <vector>

#include <FileWatch.hh>
#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    struct FileEventInfo {
        std::chrono::steady_clock::time_point lastEvent{};
    };

    enum class FileWatchEvent { MODIFIED, CREATED, DELETED, MOVED };

    class FileWatcher final {
    public:
        using WatcherCallback = std::function< void( const Path& path, FileWatchEvent ) >;

        // eventTimeOut is used for debouncing (in ms).
        // Helps to prevent redundant, rapid-fire actions by waiting for an event to
        // stop triggering (e.g., after 100-500ms) before executing a handler
        explicit FileWatcher( const Path& path, UInt32 eventTimeOut = 500 );

        auto RegisterWatchCallback(WatcherCallback&& callback) -> void;

    private:
        Path m_WatchedPath{};
        std::chrono::milliseconds m_DebounceTime{};
        std::vector<WatcherCallback> m_Callbacks{};

        ankerl::unordered_dense::map<FileWatchEvent, FileEventInfo> m_EventTypeInfos{};

        Unique<filewatch::FileWatch<std::string>> m_Watcher{};
    };
}

#endif