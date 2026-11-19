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

#ifndef MIKOTO_IMGUI_IMGUI_SERVICE_HH
#define MIKOTO_IMGUI_IMGUI_SERVICE_HH

#include <imgui.h>

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/optional.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Profiler.hh>
#include <Core/Singleton.hh>

#include <Platform/Window.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

#include <ImGui/Utility.hh>

struct SDL_Window;

namespace mikoto::imgui {

    struct ImGuiBackendCreateInfo {
        platform::IWindow* mWindow{ nullptr };
        renderer::rhi::IGpuDevice* mDevice{ nullptr };
        renderer::rhi::GraphicsAPI mApi{ renderer::rhi::GraphicsAPI::eVulkan };
    };

    /**
    * This class encapsulates backend implementation-specific details. ImGui is a graphics API
    * agnostic GUI library and provides several implementations, each for a specific graphics backend.
    * This class serves as a general abstraction over the currently active backend in use in the application
    * that will also be used with ImGui
    * */
    class ImGuiBackend {
    public:
        explicit ImGuiBackend( const ImGuiBackendCreateInfo& createInfo );

        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto BeginFrame() -> void = 0;
        virtual auto EndFrame() -> void = 0;

        auto SetClearColor( const core::float4& color ) -> void;
        auto SetResolution( renderer::rhi::RenderResolution reso ) -> void;

        MKT_NODISCARD virtual auto GetFinalComposition() -> renderer::rhi::TextureHandle = 0;

        MKT_NODISCARD virtual auto GetResolution() const -> renderer::rhi::RenderResolution;

        MKT_NODISCARD virtual auto ConstructImGuiTextureID(const renderer::rhi::ITexture* texture) -> ImTextureID = 0;
        MKT_NODISCARD virtual auto ConstructImGuiTextureID(renderer::rhi::TextureHandle texture) -> ImTextureID = 0;

        MKT_NODISCARD static auto Create(const ImGuiBackendCreateInfo& info) -> eastl::unique_ptr<ImGuiBackend>;

        virtual ~ImGuiBackend() = default;

    protected:
        MKT_NODISCARD auto GetSDLWindow() const -> SDL_Window*;

        core::float4 mClearColor{ 0.9f, 0.6f, 0.85f, 1.0f };

        platform::IWindow* mWindow{};
        renderer::rhi::IGpuDevice* mDevice{};
        renderer::rhi::RenderResolution mResolution{ renderer::rhi::RenderResolution::e1080P };

        bool mIsInitialized{ false };
    };

    struct ImGuiServiceDescription {
        // Will grab the device from the render service
        // which is required to be initialized before this one
        platform::IWindow* mWindow{ nullptr };
        renderer::rhi::IGpuDevice* mDevice{ nullptr };
        renderer::rhi::GraphicsAPI mApi{ renderer::rhi::GraphicsAPI::eVulkan };
    };

    class ImGuiService final : public core::IService, public core::Singleton<ImGuiService> {
    public:

        explicit ImGuiService(const ImGuiServiceDescription& options);

        ~ImGuiService() override = default;

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto EndFrame() const -> void;
        auto PrepareFrame() -> void;

        /**
         * @brief Queues saved ImGui window and docking settings for the next frame.
         * @param settings Owned INI document to apply before any windows begin rendering.
         */
        auto QueueLayoutRestore( eastl::string settings ) -> void;

        /**
         * @brief Changes the interface font at the next frame boundary.
         * @param path Local font file, or an empty path to restore the default.
         * @param faceIndex Index of the face within a font collection.
         */
        auto QueueInterfaceFont( const filesystem::Path& path, core::u32 faceIndex = 0 ) -> void;

        auto SetThemeDarkModeDefault() -> void;
        auto SetThemeDarkModeAlt() -> void;

        auto SetImGuiBackGroundClearColor(const core::float4& color) -> void;

        MKT_NODISCARD auto GetFinalComposition() const -> renderer::rhi::TextureHandle;

        MKT_NODISCARD auto GetTextureID(renderer::rhi::TextureHandle texture) -> ImTextureID;
        MKT_NODISCARD auto GetTextureID(const renderer::rhi::ITexture* texture) -> ImTextureID;

        MKT_NODISCARD auto GetBackend() -> ImGuiBackend*;
        MKT_NODISCARD auto GetBackend() const -> const ImGuiBackend*;

        auto PushFont( eastl::string_view str ) -> ImGuiScopedTextFont;

    private:
        struct InterfaceFontRequest {
            filesystem::Path mPath{};
            core::u32 mFaceIndex{};
        };

    private:
        auto InitImplementation() -> void;
        auto ApplyInterfaceFont() -> void;

        auto AddFont( const filesystem::Path& path ) -> void;
        auto AddInterfaceFont( const filesystem::Path& path, core::u32 faceIndex ) -> ImFont*;
        auto AddIconFont( const filesystem::Path& path, const eastl::array<ImWchar, 3>& iconRanges, ImFont* target ) -> void;

    private:
        static constexpr float kFontBaseSize{ 17.0f };

    private:
        platform::IWindow* mWindow{ nullptr };
        renderer::rhi::IGpuDevice* mDevice{ nullptr };

        eastl::unique_ptr<ImGuiBackend> mImplementation{ nullptr };

        filesystem::Path mFontsRootDir{};
        filesystem::Path mImGuiFilesRootDir{};
        eastl::string mPendingLayout{};

        eastl::optional<InterfaceFontRequest> mPendingInterfaceFont{};
        ImFont* mSelectedInterfaceFont{};
        ankerl::unordered_dense::map<filesystem::Path, ImFont*> mImGuiFonts{};

        renderer::rhi::GraphicsAPI mBackendApi{ renderer::rhi::GraphicsAPI::eInvalid };
    };
}

#endif// MIKOTO_IMGUI_IMGUI_SERVICE_HH
