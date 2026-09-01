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
#include <memory>

#include <EASTL/chrono.h>
#include <EASTL/vector.h>
#include <EASTL/functional.h>
#include <EASTL/unique_ptr.h>

#include <FileWatch.hh>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Types.hh>

#include <Filesystem/Path.hh>

namespace mikoto::filesystem {

    enum class FileWatchEvent {
        eInvalid,
        eModified,
        eCreated,
        eDeleted,
        eMoved
    };

    using WatcherCallback = eastl::function<void( const Path& path, FileWatchEvent )>;

    class FileWatcher final {
    public:

        // eventTimeOut is used for debouncing (in ms).
        // Helps to prevent redundant, rapid-fire actions by waiting for an event to
        // stop triggering (e.g., after 100-500ms) before executing a handler
        explicit FileWatcher( const Path& path, core::u32 timeOut = 500 );

        auto RegisterWatchCallback(WatcherCallback&& callback) -> void;

    private:
        Path mWatchedPath{};
        eastl::chrono::milliseconds mDebounceTime{};
        eastl::vector<WatcherCallback> mCallbacks{};

        // filewatch::FileWatch expects std::string
        eastl::unique_ptr<filewatch::FileWatch<std::string>> mWatcher{};
    };
}

#endif