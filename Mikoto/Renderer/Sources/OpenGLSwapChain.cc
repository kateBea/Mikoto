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

#include <Core/Exception.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Rhi/OpenGL/OpenGLSwapChain.hh>

namespace mikoto::renderer::opengl {

    SwapChain::SwapChain( const renderer::rhi::SwapChainDescription &desc )
        : mDesc{ desc } {
    }

    auto SwapChain::GetImageFormat() const -> renderer::rhi::Format {
        return mDesc.mFormat;
    }

    auto SwapChain::GetNextBackBuffer() const -> renderer::rhi::TextureHandle {
        return mImage;
    }

    auto SwapChain::GetBackBufferCount() const -> core::u32 {
        return 1;
    }

    auto SwapChain::Initialize() -> void {
        OnResize( mDesc.mDimension );
        mIsAllocated = true;
    }

    auto SwapChain::Destroy() -> void {
        mImage.Reset();
        mIsAllocated = false;
    }

    auto SwapChain::OnResize( const renderer::rhi::Extent2D &dimensions ) -> void {
        if ( !dimensions.mWidth || !dimensions.mHeight ) {
            return;
        }

        mDesc.mDimension = dimensions;
        mImage = mDevice->CreateTexture( renderer::rhi::TextureCreateDescription{}
            .SetName( mDesc.mName )
            .SetWidth( dimensions.mWidth )
            .SetHeight( dimensions.mHeight )
            .SetFormat( mDesc.mFormat )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget | renderer::rhi::TextureUsageFlagsBits::CopySource |
                           renderer::rhi::TextureUsageFlagsBits::ShaderResource ) );
    }

    auto SwapChain::GetBackBuffer( core::usize index ) const -> renderer::rhi::TextureHandle {
        if ( index ) {
            MKT_THROW_OUT_OF_RANGE( "OpenGL exposes one offscreen presentation image." );
        }

        return mImage;
    }

    auto SwapChain::SetPresentMode( renderer::rhi::PresentMode mode ) -> void {
        auto *renderer{ RenderSystem::GetPtr() };

        if ( !renderer || renderer->GetGpuDevice() != mDevice ) {
            MKT_THROW_LOGIC_ERROR( "Set presentation mode through the owning OpenGL RenderContext." );
        }

        renderer->GetContext()->SetRefreshRate(
            mode == renderer::rhi::PresentMode::eVSync || mode == renderer::rhi::PresentMode::eVSyncAdaptive ? renderer::rhi::RefreshRate::eSync : renderer::rhi::RefreshRate::eUnlimited );
    }
}// namespace mikoto::renderer::opengl
