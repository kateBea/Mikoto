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

#include <chrono>
#include <filesystem>

#include <EASTL/utility.h>
#include <EASTL/unique_ptr.h>

#include <FileWatch.hh>

#include <Core/Profiler.hh>
#include <Logging/Logger.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileWatcher.hh>
#include <Filesystem/FileWatcherService.hh>

namespace mikoto::filesystem {

    using namespace mikoto::core;

    FileWatcherService::FileWatcherService( const FileWatcherServiceCreateInfo& )
    {}

    auto FileWatcherService::Watch( const Path &path, WatcherCallback&& callback ) -> void {
        std::lock_guard lock{ mWatcherInsertMutex };

        const Path fileAbsolutePath{ path.ToAbsolute() };
        auto it{ mWatchedPaths.find( fileAbsolutePath ) };

        if (it == mWatchedPaths.end()) {
            it = mWatchedPaths.try_emplace( it, fileAbsolutePath, eastl::make_unique<FileWatcher>( fileAbsolutePath ) );
        }

        it->second->RegisterWatchCallback( eastl::move( callback ) );
    }

    auto FileWatcherService::Initialize() -> void {
        MKT_CORE_LOGGER_INFO( "Initializing FileWatcherService..." );

        mIsInitialized = true;
    }

    auto FileWatcherService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!mIsInitialized) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down FileWatcherService..." );

        mWatchedPaths.clear();
    }
}
