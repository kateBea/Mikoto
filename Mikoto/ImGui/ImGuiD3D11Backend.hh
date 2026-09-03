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

#ifndef MIKOTO_IMGUI_D3D11BACKEND_HH
#define MIKOTO_IMGUI_D3D11BACKEND_HH

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Renderer/Text/Font.hh>
#include <Renderer/Core/FontFactory.hh>

#include <ImGui/ImGuiService.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

namespace mikoto::imgui {

    using namespace mikoto::core;
    using namespace mikoto::platform;

    class ImGuiD3D11Backend final : public ImGuiBackend {
    public:
        explicit ImGuiD3D11Backend( const ImGuiBackendCreateInfo& createInfo )
            : ImGuiBackend{ createInfo } {}

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto BeginFrame() -> void override;
        auto EndFrame() -> void override;

        MKT_NODISCARD auto GetFinalComposition() -> TextureHandle override;

        MKT_NODISCARD auto ConstructImGuiTextureID( const ITexture* texture ) -> ImTextureID override;
        MKT_NODISCARD auto ConstructImGuiTextureID( TextureHandle texture ) -> ImTextureID override;

    private:
        // [ Internal ]
        auto CreateImages() -> void;

    private:
        u32 mExtentWidth{};
        u32 mExtentHeight{};

        TextureHandle mColorImage{};
        TextureHandle mDepthImage{};

        float4 mClearColor{ 0.2f, 0.4f, 0.5f, 1.0f };
    };
}

#endif

#endif //MIKOTO_IMGUI_D3D11BACKEND_HH