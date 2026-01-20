
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


#include <chrono>
#include <filesystem>
#include <iostream>

#include <efsw/efsw.hpp>

#include <Core/Profiler.hh>
#include <Filesystem/FileWatcher.hh>
#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileWatcherService.hh>

namespace Mikoto {

// For better readability
#define IS_WATCH_ID_ERROR(EFSW_ID) !(EFSW_ID > 0)

    static auto TestCode() -> void {

        // Create the instance of your efsw::FileWatcherListener implementation
        //UpdateListener* listener = new UpdateListener();

        // Add a folder to watch, and get the efsw::WatchID
        // It will watch the /tmp folder recursively ( the third parameter indicates that is recursive )
        // Reporting the files and directories changes to the instance of the listener
        //efsw::WatchID watchID = fileWatcher->addWatch( ".", listener, true );

        // Adds another directory to watch. This time as non-recursive.
        //efsw::WatchID watchID2 = fileWatcher->addWatch( "/usr", listener, false );
    }

    FileWatcherService::FileWatcherService( const FileWatcherServiceCreateInfo& info)
        : m_FollowSymLinks{ info.FollowSymLinks }
    {

    }

    auto FileWatcherService::Watch( const Path &path, FileWatcher::WatcherCallback&& callback ) -> void {
        const std::string fileAbsolutePath{ Filesystem::GetGetAbsolutePathString( path ) };

        // EFSW watches whole directory, not individual files
        Path absolutePath{ fileAbsolutePath };
        absolutePath.remove_filename();

        const std::string directoryString{ absolutePath.string() };
        const auto it{ m_WatchedPaths.find( directoryString ) };

        if (it == m_WatchedPaths.end()) {
            // W want a single watcher per directory
            m_WatchedPaths[directoryString] = CreateScope<FileWatcher>( directoryString );

            // For now, we make it non-recursive so each watcher monitors a specific directory and avoid multiple watchers per directory tree
            efsw::WatchID watchID{ m_FileWatcher->addWatch( directoryString, m_WatchedPaths[directoryString].get(), false ) };
            if (IS_WATCH_ID_ERROR(watchID)) {
                MKT_CORE_LOGGER_ERROR( "Error registering callback for watched path [{}]. Error {}", directoryString, efsw::Errors::Log::getLastErrorLog().c_str() );
            }
        }

        m_WatchedPaths[directoryString]->RegisterWatchCallback( fileAbsolutePath, std::move( callback ) );
    }

    auto FileWatcherService::Init() -> void {
        MKT_CORE_LOGGER_INFO( "Initializing FileWatcherService..." );

        // Create the file system watcher instance
        // efsw::FileWatcher allow a first boolean parameter that indicates if it should start with the
        // generic file watcher instead of the platform specific backend
        m_FileWatcher = CreateScope<efsw::FileWatcher>();

        // Some configurations
        //m_FileWatcher->followSymlinks( m_FollowSymLinks );
        //m_FileWatcher->allowOutOfScopeLinks( false );

        // Start watching asynchronously the directories
        m_FileWatcher->watch();

        m_IsInitialized = true;
    }

    auto FileWatcherService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) {
            return;
        }

        m_FileWatcher = nullptr;

        m_WatchedPaths.clear();

        MKT_CORE_LOGGER_INFO( "Shutting down FileWatcherService..." );

        m_WatchedPaths.clear();
    }
}
