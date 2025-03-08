/**
 * Serializer.hh
 * Created by kate on 9/30/23.
 * */

#ifndef MIKOTO_FILE_MANAGER_HH
#define MIKOTO_FILE_MANAGER_HH

#include <Core/Engine.hh>
#include <Library/Filesystem/File.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    class FileSystem final : public IEngineSystem {
    public:
        explicit FileSystem() = default;
        explicit FileSystem( const EngineConfig& options )
            : m_ShadersRootPath{ options.Options.ShadersPath },
        m_FontsRootPath( options.Options.FontsPath ),
        m_IconsRootPath( options.Options.IconsPath ),
        m_LogFilePath( options.Options.LogFilePath ),
        m_AssetsRootPath{ options.Options.AssetsRootPath},
        m_ImGuiDir{ options.Options.ImGuiConfigDir}

        {
            if (m_ShadersRootPath.empty()) {
                MKT_CORE_LOGGER_WARN("FileSystem::FileSystem - Root folder for shaders is empty.");
            }

            if (m_FontsRootPath.empty()) {
                MKT_CORE_LOGGER_WARN("FileSystem::FileSystem - Root folder for fonts is empty.");
            }

            if (m_IconsRootPath.empty()) {
                MKT_CORE_LOGGER_WARN("FileSystem::FileSystem - Root folder for icons is empty.");
            }

            if (m_LogFilePath.empty()) {
                MKT_CORE_LOGGER_WARN("FileSystem::FileSystem - Root folder for logs is empty.");
            }

            if (m_AssetsRootPath.empty()) {
                MKT_CORE_LOGGER_WARN("FileSystem::FileSystem - Root folder for assets is empty.");
            }

            if (m_ImGuiDir.empty()) {
                MKT_CORE_LOGGER_WARN("FileSystem::FileSystem - Root folder for ImGui files is empty.");
            }
        }

        ~FileSystem() override = default;

        /**
         * Initializes the serializer utilities and some libraries it requires
         * like NFD. The later is very important to be initialized after any
         * platform windowing abstraction framework such as SDL or GLFW.
         * */
        auto Init() -> void override;

        MKT_NODISCARD auto GetShadersRootPath() const -> const Path_T& { return m_ShadersRootPath; }
        MKT_NODISCARD auto GetFontsRootPath() const -> const Path_T& { return m_FontsRootPath; }
        MKT_NODISCARD auto GetIconsRootPath() const -> const Path_T& { return m_IconsRootPath; }
        MKT_NODISCARD auto GetLogFilePath() const -> const Path_T& { return m_LogFilePath; }
        MKT_NODISCARD auto GetAssetsRootPath() const -> const Path_T& { return m_AssetsRootPath; }
        MKT_NODISCARD auto GetImGuiDir() const -> const Path_T& { return m_ImGuiDir; }

        /**
         * Releases resources from the Serializer namespace and shuts down
         * associated libraries.
         * */
        auto Shutdown() -> void override;
        auto Update() -> void override;

        auto LoadFile( const Path_T& path, FileMode mode = MKT_FILE_OPEN_MODE_NONE ) -> File*;

        /**
         * Opens a save file dialog with the given filters. Every filter has a name
         * followed by the extension for that filter (coma separated). An
         * example of filter would be: { "Source File", "cc,c,cpp,cxx" }.
         * It offers the possibility to specify a default name when saving the file
         * @param filename default save file name
         * @param filters dialog filters
         * */
        auto SaveDialog( const std::string& filename, const std::initializer_list<std::pair<std::string, std::string>>& filters ) -> Path_T;

        /**
         * Opens a file dialog with the given filters. Every filter has a name
         * followed by the extension for that filter (coma separated). An
         * example of filter would be: { "Source File", "cc,c,cpp,cxx" }
         * @param filters dialog filters
         * */
        auto OpenDialog( const std::initializer_list<std::pair<std::string, std::string>>& filters ) -> Path_T;

    private:
        Path_T m_ShadersRootPath{};
        Path_T m_FontsRootPath{};
        Path_T m_IconsRootPath{};
        Path_T m_LogFilePath{};
        Path_T m_AssetsRootPath{};
        Path_T m_ImGuiDir{};

        std::unordered_map<std::string, Scope_T<File>> m_Files{};
    };
}// namespace Mikoto


#endif// MIKOTO_FILE_MANAGER_HH
