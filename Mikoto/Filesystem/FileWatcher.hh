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

#include <efsw/efsw.hpp>
#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    enum class FileWatchEvent { MODIFIED, CREATED, DELETED, MOVED };

    // Watches a given directory for changes in its list of items
    class FileWatcher final : public efsw::FileWatchListener {
    public:
        using WatcherCallback = std::function< void( const Path& path, FileWatchEvent ) >;

        explicit FileWatcher( const Path& path );

        MKT_NODISCARD auto UnRegisterWatchCallback(UInt64 watcherID) -> bool;
        MKT_NODISCARD auto RegisterWatchCallback(const Path& absolutePath, WatcherCallback&& callback) -> UInt64;

    private:
        // From efsw::FileWatchListener
        auto handleFileAction( efsw::WatchID watchID,
            const std::string &dir,
            const std::string &filename,
            efsw::Action action,
            std::string oldFilename ) -> void override;

    private:
        Path m_WatchedPath{};
        ankerl::unordered_dense::map<UInt64, WatcherCallback> m_Callbacks{};
        ankerl::unordered_dense::map<UInt64, std::string> m_CallbacksByPath{};
    };
}

#endif