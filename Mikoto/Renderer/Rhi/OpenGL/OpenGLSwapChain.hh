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

#ifndef MIKOTO_RENDERER_RHI_OPENGL_OPENGL_SWAP_CHAIN_HH
#define MIKOTO_RENDERER_RHI_OPENGL_OPENGL_SWAP_CHAIN_HH

#include <Renderer/Rhi/SwapChain.hh>

namespace mikoto::renderer::opengl {

    // GL presents through the window system. This RHI swap chain supplies one
    // offscreen color image for Context::SetPresentTarget; GL orders its reuse.
    class SwapChain final : public renderer::rhi::ISwapChain {
    public:
        explicit SwapChain( const renderer::rhi::SwapChainDescription &desc );

        auto OnResize( const renderer::rhi::Extent2D &dimensions ) -> void override;
        auto SetPresentMode( renderer::rhi::PresentMode mode ) -> void override;

        MKT_NODISCARD auto GetImageFormat() const -> renderer::rhi::Format override;
        MKT_NODISCARD auto GetNextBackBuffer() const -> renderer::rhi::TextureHandle override;
        MKT_NODISCARD auto GetBackBuffer( core::usize index ) const -> renderer::rhi::TextureHandle override;
        MKT_NODISCARD auto GetBackBufferCount() const -> core::u32 override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        renderer::rhi::TextureHandle mImage{};
        renderer::rhi::SwapChainDescription mDesc{};
    };
}// namespace mikoto::renderer::opengl

#endif// MIKOTO_RENDERER_RHI_OPENGL_OPENGL_SWAP_CHAIN_HH
