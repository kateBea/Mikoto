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

#include <Platform/Windows/Windows.hh>

#include <FileWatch.hh>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileWatcher.hh>
#include <Filesystem/FileWatcherService.hh>

namespace mikoto::filesystem {

    using namespace mikoto::core;

    FileWatcherService::FileWatcherService( const FileWatcherServiceCreateInfo& ) {
    }

    auto FileWatcherService::Watch( const Path& path, WatcherCallback&& callback ) -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mWatcherInsertMutex };

        if ( !mAcceptingWatches ) {
            return;
        }

        const Path fileAbsolutePath{ path.ToAbsolute() };
        auto it{ mWatchedPaths.find( fileAbsolutePath ) };

        if ( it == mWatchedPaths.end() ) {
            it = mWatchedPaths.try_emplace( it, fileAbsolutePath, eastl::make_unique<FileWatcher>( fileAbsolutePath ) );
        }

        it->second->RegisterWatchCallback( eastl::move( callback ) );
    }

    auto FileWatcherService::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_INFO( "Initializing FileWatcherService..." );

        std::lock_guard lock{ mWatcherInsertMutex };

        mAcceptingWatches = true;
        mIsInitialized = true;
    }

    auto FileWatcherService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down FileWatcherService..." );

        decltype( mWatchedPaths ) retired{};
        {
            std::lock_guard lock{ mWatcherInsertMutex };
            mAcceptingWatches = false;

            // ankerl's member swap uses ADL, which finds both std::swap and
            // eastl::swap for this map. Move ownership without that lookup.
            retired = eastl::move( mWatchedPaths );
        }

        // Destruction joins watcher threads. Do it outside the registration
        // mutex so a finishing callback can attempt Watch() and return safely.
        retired.clear();

        mIsInitialized = false;
    }
}// namespace mikoto::filesystem
