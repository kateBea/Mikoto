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

#ifndef MIKOTO_IMGUI_IMGUI_OPENGL_BACKEND_HH
#define MIKOTO_IMGUI_IMGUI_OPENGL_BACKEND_HH

#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Texture.hh>

#include <ImGui/ImGuiService.hh>

namespace mikoto::imgui {

    class ImGuiOpenGLBackend final : public ImGuiBackend {
    public:
        explicit ImGuiOpenGLBackend( const ImGuiBackendCreateInfo& info );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto BeginFrame() -> void override;
        auto EndFrame() -> void override;

        MKT_NODISCARD auto GetFinalComposition() -> renderer::rhi::TextureHandle override;
        MKT_NODISCARD auto ConstructImGuiTextureID( const renderer::rhi::ITexture* texture ) -> ImTextureID override;
        MKT_NODISCARD auto ConstructImGuiTextureID( renderer::rhi::TextureHandle texture ) -> ImTextureID override;

    private:
        // [Internal usage]
        auto OnResize() -> void;

    private:
        core::u32 mFramebuffer{};
        renderer::rhi::TextureHandle mColor{};
        eastl::vector<renderer::rhi::TextureHandle> mFrameTextures{};
    };
}// namespace mikoto::imgui

#endif// MIKOTO_IMGUI_IMGUI_OPENGL_BACKEND_HH
