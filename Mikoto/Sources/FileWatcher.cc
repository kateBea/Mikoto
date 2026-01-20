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

    auto FileWatcher::UnRegisterWatchCallback( UInt64 watcherID ) -> bool {
        auto result{ m_Callbacks.erase( watcherID ) };
        auto resultPaths{ m_CallbacksByPath.erase( watcherID ) };

        return result != 0 && resultPaths != 0;
    }

    auto FileWatcher::RegisterWatchCallback(const Path& absolutePath, WatcherCallback &&callback ) -> UInt64 {
        UInt64 watcherID{ m_Callbacks.size() };

        const std::string absPath{ Filesystem::GetGetAbsolutePathString( absolutePath ) };

        const auto [it, success]{ m_Callbacks.try_emplace( watcherID, std::move(callback) ) };
        const auto [itPath, successPath]{ m_CallbacksByPath.try_emplace( watcherID, absPath ) };

        if (!success || !successPath) {
            MKT_THROW_RUNTIME_ERROR( "Error registering watch callback!" );
        }

        return watcherID;
    }

    auto FileWatcher::handleFileAction( efsw::WatchID watchID, const std::string &dir, const std::string &filename, efsw::Action action, std::string oldFilename ) -> void {
        for (const auto& [watchedFileID, callback] : m_Callbacks) {
            const std::string watchedFile{ Filesystem::GetGetAbsolutePath(  m_CallbacksByPath[watchedFileID] ).filename().string() };

            if (filename == watchedFile) {
                callback(m_WatchedPath, ConverEventType( action ));
            }
        }
    }
}