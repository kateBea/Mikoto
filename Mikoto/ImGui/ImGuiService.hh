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

#ifndef MIKOTO_IMGUI_SERVICE_HH
#define MIKOTO_IMGUI_SERVICE_HH

#include <imgui.h>

#include <Common/Service.hh>
#include <Assets/Texture.hh>
#include <ImGui/ImGuiUtility.hh>

#include <Platform/Window.hh>

#include <Library/Utility/Types.hh>

#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/RenderUtility.hh>

namespace Mikoto {

    struct ImGuiBackendCreateInfo {
        const Window* Handle{};
        GraphicsAPI API{ GraphicsAPI::VULKAN_API };
        GpuDevice* Device{ nullptr };
    };

    /**
    * This class encapsulates backend implementation-specific details. ImGui is a graphics API
    * agnostic GUI library and provides several implementations, each for a specific graphics backend.
    * This class serves as a general abstraction over the currently active backend in use in the application
    * that will also be used with ImGui
    * */
    class ImGuiBackend {
    public:
        explicit ImGuiBackend( const ImGuiBackendCreateInfo& createInfo )
            : m_Window{ createInfo.Handle }, m_GpuDevice{ createInfo.Device }, m_Api{ createInfo.API }
        {}

        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto BeginFrame() -> void = 0;
        virtual auto EndFrame() -> void = 0;

        MKT_NODISCARD virtual auto GetFinalComposition() -> TextureHandle = 0;

        auto SetClearColor(const Vec4F& color) -> void { m_ClearColor = color; }

        MKT_NODISCARD virtual auto ConstructImGuiTextureID(const Texture* texture) -> ImTextureID = 0;
        MKT_NODISCARD virtual auto ConstructImGuiTextureID(TextureHandle texture) -> ImTextureID = 0;

        virtual ~ImGuiBackend() = default;

        MKT_NODISCARD static auto Create(const ImGuiBackendCreateInfo& info) -> Unique<ImGuiBackend>;

    protected:
        Vec4F m_ClearColor{ 0.9f, 0.6f, 0.85f, 1.0f };
        bool m_IsInitialized{ false };

        const Window* m_Window{};
        GpuDevice* m_GpuDevice{};
        GraphicsAPI m_Api{ GraphicsAPI::VULKAN_API };
    };

    struct ImGuiServiceDescription {
        GpuDevice* Device{ nullptr };
        GraphicsAPI BackendApi{ GraphicsAPI::VULKAN_API };
        Window* TargetWindow{ nullptr };
    };

    class ImGuiService final : public IService, public Singleton<ImGuiService> {
    public:

        explicit ImGuiService(const ImGuiServiceDescription& options);

        ~ImGuiService() override = default;

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto EndFrame() const -> void;
        auto PrepareFrame() const -> void;

        MKT_NODISCARD auto GetFinalComposition() const -> TextureHandle;

        auto SetImGuiBackGroundClearColor(const Vec4F& color) -> void;

        MKT_NODISCARD auto GetTextureID(TextureHandle texture) -> ImTextureID;
        MKT_NODISCARD auto GetTextureID(const Texture* texture) -> ImTextureID;

        MKT_NODISCARD auto GetBackend() -> ImGuiBackend*;
        MKT_NODISCARD auto GetBackend() const -> const ImGuiBackend*;

        auto PushFont( std::string_view str ) -> ImGuiUtils::ImGuiScopedTextFont;

    private:

        auto InitImplementation() -> void;
        auto AddFont(float fontSize, const std::string &path, const ImFontConfig* config = nullptr, const std::array<ImWchar, 3>* iconRanges = nullptr ) -> void;
        auto AddIconFont(float fontSize, const std::string &path, const std::array<ImWchar, 3> &iconRanges) -> void;

    private:
        static constexpr float FONT_BASE_SIZE{ 16.0f };

    private:
        GpuDevice* m_Device{ nullptr };
        std::string m_ImGuiFilesRootDir{};
        GraphicsAPI m_BackendApi{ GraphicsAPI::INVALID_API };

        Window* m_Window{ nullptr };

        Unique<ImGuiBackend> m_Implementation{ nullptr };

        // index into ImGui internal font structures and
        // a path to keep track of where it is
        ankerl::unordered_dense::map<std::string, Int8> m_ImGuiFonts{};
    };

}

#endif // MIKOTO_IMGUI_SERVICE_HH
