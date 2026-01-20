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

#include <ranges>
#include <iostream>
#include <filesystem>

#include <efsw/efsw.hpp>
#include <efsw/FileSystem.hpp>
#include <efsw/System.hpp>

#include <Logging/Logger.hh>
#include <Core/Exception.hh>
#include <Filesystem/FileWatcher.hh>
#include <Filesystem/FileSystem.hh>

namespace Mikoto {

    MKT_NODISCARD static auto ConverEventType(efsw::Action action) -> FileWatchEvent {
        switch ( action ) {
            case efsw::Actions::Add: return FileWatchEvent::CREATED;
            case efsw::Actions::Delete: return FileWatchEvent::DELETED;
            case efsw::Actions::Modified: return FileWatchEvent::MODIFIED;
            case efsw::Actions::Moved: return FileWatchEvent::MOVED;
            default:
                MKT_CORE_LOGGER_ERROR( "Error undefined backend file watch action!" );
        }

        // Make compiler happy
        return FileWatchEvent::CREATED;
    }

    FileWatcher::FileWatcher( const Path &path ) {
        m_WatchedPath = Filesystem::GetGetAbsolutePath( path.string() );
    }

    auto FileWatcher::RegisterWatchCallback(const Path& absolutePath, WatcherCallback &&callback ) -> void {
        const std::string filename{ Filesystem::GetGetAbsolutePath( absolutePath.string() ).filename().string() };
        m_Callbacks[filename].emplace_back( std::move(callback) );
    }

    auto FileWatcher::handleFileAction( efsw::WatchID watchID, const std::string &dir, const std::string &filename, efsw::Action action, std::string oldFilename ) -> void {
        const auto it{ m_Callbacks.find( filename ) };
        if (it != m_Callbacks.end()) {
            for (const auto& callback : it->second) {
                callback(m_WatchedPath, ConverEventType( action ));
            }
        }
    }
}