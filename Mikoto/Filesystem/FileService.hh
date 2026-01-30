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

#ifndef MIKOTO_FILE_SERVICE_HH
#define MIKOTO_FILE_SERVICE_HH

#include <mutex>
#include <future>

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Filesystem/File.hh>

#include <Library/IO/File.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    struct FileServiceCreateInfo {};

    class FileService final : public IService, public Singleton<FileService> {
    public:
        explicit FileService( const FileServiceCreateInfo& options );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto SaveFile( const File* file ) -> bool;

        MKT_NODISCARD auto LoadFile( const Path& path, FileMode mode = MKT_FILE_OPEN_MODE_BINARY ) -> File*;

        MKT_NODISCARD auto CreateNewFile( const Path& path ) -> File*;

        MKT_NODISCARD auto GetFile( const Path& path ) -> File*;
        MKT_NODISCARD auto GetFile( const Path& path ) const -> const File*;

        // TODO: move to Filesystem helpers
        MKT_NODISCARD auto OpenDialog( const std::initializer_list<std::pair<std::string, std::string>>& filters ) -> Path;
        MKT_NODISCARD auto SaveDialog( const std::string& filename, const std::initializer_list<std::pair<std::string, std::string>>& filters ) -> Path;

        ~FileService() override = default;

    private:
        std::mutex m_FileLoadMutex{};
        ankerl::unordered_dense::map<std::string, Unique<File>> m_Files{};
    };
}

#endif // MIKOTO_FILE_SERVICE_HH
