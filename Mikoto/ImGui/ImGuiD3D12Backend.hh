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

#ifndef MIKOTO_IMGUI_D3D12BACKEND_HH
#define MIKOTO_IMGUI_D3D12BACKEND_HH

#include <Core/Platform.hh>

#include <ImGui/ImGuiService.hh>

#include <Renderer/Core/Rhi.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <directx/d3d12.h>

namespace mikoto::gui {

    class ImGuiD3D12Backend final : public ImGuiBackend {
    public:
        explicit ImGuiD3D12Backend( const ImGuiBackendCreateInfo& createInfo );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto BeginFrame() -> void override;
        auto EndFrame() -> void override;

        MKT_NODISCARD auto GetFinalComposition() -> TextureHandle override;

        MKT_NODISCARD auto ConstructImGuiTextureID( const ITexture* texture ) -> ImTextureID override;
        MKT_NODISCARD auto ConstructImGuiTextureID( TextureHandle texture ) -> ImTextureID override;

    private:
        // [Internal usage]
        auto InitImages() -> void;
        auto InitImGuiForD3D12() -> void;

    private:
        D3D12_RESOURCE_DESC mDimensions{};

        CommandListHandle mCommandList{};

        TextureHandle mColorImage{};
        TextureHandle mDepthImage{};

    };
}

#endif

#endif //MIKOTO_IMGUI_D3D12BACKEND_HH