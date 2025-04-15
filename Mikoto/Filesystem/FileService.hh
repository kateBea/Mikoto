/**
 * Serializer.hh
 * Created by kate on 9/30/23.
 * */

#ifndef MIKOTO_FILE_MANAGER_HH
#define MIKOTO_FILE_MANAGER_HH

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Library/Filesystem/File.hh>
#include <Library/Utility/Types.hh>
#include <Threading/Task.hh>

namespace Mikoto {

    struct FileServiceCreateInfo {

    };

    class FileService final : public IService<FileService> {
    public:
        explicit FileService( const FileServiceCreateInfo& options );

        /**
         * Initializes the serializer utilities and some libraries it requires
         * like NFD. The later is very important to be initialized after any
         * platform windowing abstraction framework such as SDL or GLFW.
         * */
        auto Init() -> void override;

        /**
         * Releases resources from the Serializer namespace and shuts down
         * associated libraries.
         * */
        auto Shutdown() -> void override;

        // load from disc
        auto LoadFile( const Path_T& path, FileMode mode = MKT_FILE_OPEN_MODE_NONE ) -> File*;
        auto LoadFileAsync( const Path_T& path, FileMode mode = MKT_FILE_OPEN_MODE_NONE ) -> Task<File>*;

        auto GetFile( const Path_T& path ) -> File*;
        auto GetFile( const Path_T& path ) const -> const File*;

        auto SaveDialog( const std::string& filename, const std::initializer_list<std::pair<std::string, std::string>>& filters ) -> Path_T;

        auto OpenDialog( const std::initializer_list<std::pair<std::string, std::string>>& filters ) -> Path_T;

        MKT_NODISCARD static auto GetCurrentWorkingDirectory() -> std::string;

        ~FileService() override = default;

    private:

        std::vector<Scope_T<ITask>> m_FileLoadTasks{};
        ankerl::unordered_dense::map<std::string, Scope_T<File>> m_Files{};
    };
}// namespace Mikoto


#endif// MIKOTO_FILE_MANAGER_HH
