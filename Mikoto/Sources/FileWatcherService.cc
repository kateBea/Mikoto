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

#include <FileWatch.hh>

#include <Core/Profiler.hh>
#include <Filesystem/FileWatcher.hh>
#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileWatcherService.hh>

namespace Mikoto {

    FileWatcherService::FileWatcherService( const FileWatcherServiceCreateInfo& info)
        : m_FollowSymLinks{ info.FollowSymLinks }
    {
    }

    auto FileWatcherService::Watch( const Path &path, FileWatcher::WatcherCallback&& callback ) -> void {
        const std::string fileAbsolutePath{ path.string() };
        const auto it{ m_WatchedPaths.find( fileAbsolutePath ) };

        if (it == m_WatchedPaths.end()) {
            std::lock_guard lock{ m_WatcherInsertMutex };
            m_WatchedPaths[fileAbsolutePath] = CreateScope<FileWatcher>( Filesystem::GetGetAbsolutePath( path ) );
        }

        m_WatchedPaths.at(fileAbsolutePath)->RegisterWatchCallback( std::move( callback ) );
    }

    auto FileWatcherService::Init() -> void {
        MKT_CORE_LOGGER_INFO( "Initializing FileWatcherService..." );

        m_IsInitialized = true;
    }

    auto FileWatcherService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) {
            return;
        }

        m_WatchedPaths.clear();

        MKT_CORE_LOGGER_INFO( "Shutting down FileWatcherService..." );

        m_WatchedPaths.clear();
    }
}
