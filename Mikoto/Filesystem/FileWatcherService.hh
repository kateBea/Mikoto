//
// Created by kate on 1/17/26.
//

#ifndef MIKOTO_FILE_WATCHER_HH
#define MIKOTO_FILE_WATCHER_HH

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Library/IO/File.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    enum class FileWatchEvent { MODIFIED, CREATED, DELETED, UNDEFINED };

    struct FileWatcherServiceCreateInfo {

    };

    class FileWatcherService final : public IService, public Singleton<FileWatcherService> {
    public:
        explicit FileWatcherService( const FileWatcherServiceCreateInfo& info);

        auto Watch(Path path) -> void;
        //auto Unwatch(Path path) -> void;

        MKT_NODISCARD auto CheckStatus(std::string_view path, FileWatchEvent watchEvent) -> bool;

        auto Init() -> void override;
        auto Update(float dt) -> void override;
        auto Shutdown() -> void override;

    private:
        struct FileMetaData {
            FileWatchEvent WatchEvent{ FileWatchEvent::UNDEFINED };
            std::filesystem::file_time_type LastModifiedTime{};
        };

    private:
        ankerl::unordered_dense::map<std::string, FileMetaData> m_WatchedPaths{};
    };
}// namespace Mikoto


#endif// MIKOTO_FILE_WATCHER_HH
