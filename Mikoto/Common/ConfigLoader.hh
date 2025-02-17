//
// Created by kate on 1/4/25.
//

#ifndef CONFIGLOADER_HH
#define CONFIGLOADER_HH

/**
 * Can disable exceptions in
 * compiler flags and/or explicitly disable the library's use of them by setting the option
 * #TOML_EXCEPTIONS to 0. In either case, the parsing functions return a
 * toml::parse_result instead of a toml::table:
 *
 *  only necessary if you've left them enabled in your compiler #include <toml++/toml.hpp>
 * */
#define TOML_EXCEPTIONS 0

#include <Common/Common.hh>
#include <Library/Filesystem/PathBuilder.hh>
#include <Library/Utility/Types.hh>
#include <Models/Enums.hh>
#include <Platform/Window/Window.hh>
#include <toml++/toml.hpp>


namespace Mikoto {
    struct ConfigOptions {
        // Engine
        std::string EngineName{};
        UInt32_T EngineVersionMajor{};
        UInt32_T EngineVersionMinor{};
        UInt32_T EngineVersionPatch{};

        // Paths
        Path_T AssetsRootPath{};
        Path_T ShadersPath{};
        Path_T LogFilePath{};
        Path_T TexturesPath{};
        Path_T ImGuiConfigFile{};
        Path_T IconsPath{};
        Path_T FontsPath{};

        // Renderer
        bool EnableVSync{ };
        GraphicsAPI RendererAPI{ GraphicsAPI::VULKAN_API };

        // Application
        std::string WindowTitle{};
        Int32_T WindowWidth{};
        Int32_T WindowHeight{};
        bool AllowWindowResizing{ true };

        // Setup by the loader
        Path_T WorkingDirectory{};

        // Debug
        bool ShowFPS{ false };
        bool DrawDebugOverlay{ false };

    };

    class ConfigLoader final {
    public:
        static auto LoadFromFile(const Path_T& path) -> Scope_T<ConfigOptions> {
            toml::parse_result result{ toml::parse_file(path.c_str()) };
            if (!result) {
                MKT_THROW_RUNTIME_ERROR(fmt::format("ConfigLoader::LoadFromFile - Error failed to parse config file: {}", path.string()));
            }

            const toml::table& config{ result.table() };

            const auto& engine{ config["engine"].as_table() };
            const auto& paths{ config["paths"].as_table() };
            const auto& renderer{ config["renderer"].as_table() };
            const auto& application{ config["application"].as_table() };
            const auto& logging{ config["logging"].as_table() };
            const auto& debug{ config["debug"].as_table() };

            Path_T assetsRoot{ PathBuilder()
                .WithPath(std::filesystem::current_path().string())
                .WithPath(paths->at("root").value_or(""))
                .Build()
            };

            ConfigOptions options{
                .EngineName{ engine->at("name").value_or("") },
                .EngineVersionMajor{ engine->at("version_major").value_or<UInt32_T>(0) },
                .EngineVersionMinor{ engine->at("version_minor").value_or<UInt32_T>(0) },
                .EngineVersionPatch{ engine->at("version_patch").value_or<UInt32_T>(0) },

                .AssetsRootPath{ PathBuilder().WithPath(assetsRoot.string()).WithPath(paths->at("assets").value_or("")).Build() },
                .ShadersPath{ PathBuilder().WithPath(assetsRoot.string()).WithPath(paths->at("shaders").value_or("")).Build() },
                .LogFilePath{ PathBuilder().WithPath(std::filesystem::current_path().string()).WithPath(paths->at("logs").value_or("")).Build() },
                .TexturesPath{ PathBuilder().WithPath(assetsRoot.string()).WithPath(paths->at("textures").value_or("")).Build() },
                .ImGuiConfigFile{ PathBuilder().WithPath(assetsRoot.string()).WithPath(paths->at("imgui_config").value_or("")).Build() },
                .IconsPath{ PathBuilder().WithPath(assetsRoot.string()).WithPath(paths->at("icons").value_or("")).Build() },
                .FontsPath{ PathBuilder().WithPath(assetsRoot.string()).WithPath(paths->at("fonts").value_or("")).Build() },

                .EnableVSync{ renderer->at("vsync").value_or(false) },
                .RendererAPI{ GraphicsAPI::VULKAN_API },
                .WindowTitle{ application->at("title").value_or("") },
                .WindowWidth{ application->at("width").value_or(0) },
                .WindowHeight{ application->at("height").value_or(0) },
                .AllowWindowResizing{ application->at("resizable").value_or(true) },
                .WorkingDirectory{ std::filesystem::current_path()  },
                .ShowFPS{ debug->at("show_fps").value_or(false) },
                .DrawDebugOverlay{ debug->at("draw_debug_overlay").value_or(false) }
            };

            return CreateScope<ConfigOptions>(options);
        }

    private:
        inline static ConfigOptions s_Options{};
    };
}
#endif //CONFIGLOADER_HH
