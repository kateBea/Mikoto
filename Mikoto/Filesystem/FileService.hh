/**
 * Serializer.hh
 * Created by kate on 9/30/23.
 * */

#ifndef MIKOTO_FILE_MANAGER_HH
#define MIKOTO_FILE_MANAGER_HH

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Library/IO/File.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    struct FileServiceCreateInfo {

    };

    class FileService final : public IService, public Singleton<FileService> {
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
        auto LoadFile( const Path& path, FileMode mode = MKT_FILE_OPEN_MODE_NONE ) -> File*;
        auto LoadFileAsync( const Path& path, FileMode mode = MKT_FILE_OPEN_MODE_NONE ) -> void; // Return a future

        auto CreateFile( const Path& path ) -> File*;
        auto CreateFileAsync( const Path& path ) -> File*; // Return a future

        auto SaveFile( const File* file ) -> void;
        auto SaveFileAsync( const File* file ) -> void; // Return a future

        auto GetFile( const Path& path ) -> File*;
        auto GetFile( const Path& path ) const -> const File*;

        auto OpenDialog( const std::initializer_list<std::pair<std::string, std::string>>& filters ) -> Path;
        auto SaveDialog( const std::string& filename, const std::initializer_list<std::pair<std::string, std::string>>& filters ) -> Path;

        MKT_NODISCARD auto GetCurrentWorkingDirectory() const -> std::string;

        ~FileService() override = default;

    private:
        Path m_CurrentWorkingDir{};
        ankerl::unordered_dense::map<std::string, Unique<File>> m_Files{};
    };
}// namespace Mikoto


#endif// MIKOTO_FILE_MANAGER_HH
