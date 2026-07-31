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

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <imgui.h>

#include <Core/Core.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>
#include <Core/Types.hh>

#include <ImGui/ImGuiUtility.hh>

#include <Platform/Window.hh>

#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/Rhi.hh>

namespace mikoto::gui {

    using namespace mikoto::core;
    using namespace mikoto::platform;
    using namespace mikoto::renderer;

    struct ImGuiBackendCreateInfo {
        Window* mWindow{ nullptr };
        IGpuDevice* mDevice{ nullptr };
        GraphicsAPI mApi{ GraphicsAPI::eVulkan };
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
            : mWindow{ createInfo.mWindow }, mDevice{ createInfo.mDevice }, mApi{ createInfo.mApi }
        {}

        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto BeginFrame() -> void = 0;
        virtual auto EndFrame() -> void = 0;

        MKT_NODISCARD virtual auto GetFinalComposition() -> TextureHandle = 0;

        auto SetClearColor(const float4& color) -> void { mClearColor = color; }
        auto SetResolution( RenderResolution reso ) -> void { mResolution = reso; }

        MKT_NODISCARD virtual auto GetResolution() const -> RenderResolution { return mResolution; }

        MKT_NODISCARD virtual auto ConstructImGuiTextureID(const ITexture* texture) -> ImTextureID = 0;
        MKT_NODISCARD virtual auto ConstructImGuiTextureID(TextureHandle texture) -> ImTextureID = 0;

        virtual ~ImGuiBackend() = default;

        MKT_NODISCARD static auto Create(const ImGuiBackendCreateInfo& info) -> eastl::unique_ptr<ImGuiBackend>;

    protected:
        float4 mClearColor{ 0.9f, 0.6f, 0.85f, 1.0f };
        bool mIsInitialized{ false };

        Window* mWindow{};
        IGpuDevice* mDevice{};
        GraphicsAPI mApi{ GraphicsAPI::eVulkan };
        RenderResolution mResolution{ RenderResolution::e1080P };
    };

    struct ImGuiServiceDescription {
        // Will grab the device from the render service
        // which is required to be initialized before this one
        Window* mWindow{ nullptr };
        IGpuDevice* mDevice{ nullptr };
        GraphicsAPI mApi{ GraphicsAPI::eVulkan };
    };

    class ImGuiService final : public IService, public Singleton<ImGuiService> {
    public:

        explicit ImGuiService(const ImGuiServiceDescription& options);

        ~ImGuiService() override = default;

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto EndFrame() const -> void;
        auto PrepareFrame() const -> void;

        auto SetThemeDarkModeDefault() -> void;
        auto SetThemeDarkModeAlt() -> void;

        MKT_NODISCARD auto GetFinalComposition() const -> TextureHandle;

        auto SetImGuiBackGroundClearColor(const float4& color) -> void;

        MKT_NODISCARD auto GetTextureID(TextureHandle texture) -> ImTextureID;
        MKT_NODISCARD auto GetTextureID(const ITexture* texture) -> ImTextureID;

        MKT_NODISCARD auto GetBackend() -> ImGuiBackend*;
        MKT_NODISCARD auto GetBackend() const -> const ImGuiBackend*;

        auto PushFont( eastl::string_view str ) -> ImGuiScopedTextFont;

    private:

        auto InitImplementation() -> void;
        auto AddFont(float fontSize, const eastl::string &path, const ImFontConfig* config = nullptr, const ImWchar* glyphRanges = nullptr ) -> void;
        auto AddIconFont(float fontSize, const eastl::string &path, const eastl::array<ImWchar, 3> &iconRanges) -> void;

    private:
        static constexpr float kFontBaseSize{ 19.0f };

    private:
        Window* mWindow{ nullptr };
        IGpuDevice* mDevice{ nullptr };

        eastl::unique_ptr<ImGuiBackend> mImplementation{ nullptr };

        Path mFontsRootDir{};
        Path mImGuiFilesRootDir{};

        // index into ImGui internal font structures and
        // a path to keep track of where it is
        ankerl::unordered_dense::map<Path, i8> mImGuiFonts{};

        GraphicsAPI mBackendApi{ GraphicsAPI::eInvalid };
    };

}

#endif // MIKOTO_IMGUI_SERVICE_HH
