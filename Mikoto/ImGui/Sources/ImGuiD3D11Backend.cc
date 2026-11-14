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

#include <EASTL/array.h>
#include <EASTL/string.h>

// ImGuizmo requires ImGui declarations before its header.
#include <imgui.h>
#include <ImGuizmo.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Core/RenderSystem.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <imgui_impl_dx11.h>
#include <imgui_impl_sdl3.h>

#include <Renderer/Rhi/D3D11/D3D11Device.hh>
#include <Renderer/Rhi/D3D11/D3D11Context.hh>

#include <ImGui/ImGuiD3D11Backend.hh>

namespace mikoto::imgui {

    using namespace mikoto::renderer;
    using namespace mikoto::renderer::d3d11;

    auto ImGuiD3D11Backend::Init() -> void {
        MKT_BEGIN_PROFILER();

        // Grab device and device context
        renderer::d3d11::Device* device{ core::checked_cast<renderer::d3d11::Device*>( mDevice ) };

        if ( !ImGui_ImplSDL3_InitForD3D( GetSDLWindow() ) ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to initialize ImGui SDL for Direct3D." );
        }

        if ( !ImGui_ImplDX11_Init( device->GetDevice(), device->GetDeviceContext() ) ) {
            ImGui_ImplSDL3_Shutdown();
            MKT_THROW_RUNTIME_ERROR( "Failed ImGui_ImplDX11_Init" );
        }

        auto dimensions{ renderer::rhi::InferDimensions( mResolution ) };
        mExtentWidth = dimensions.first;
        mExtentHeight = dimensions.second;

        try {
            CreateImages();
        } catch ( ... ) {
            ImGui_ImplDX11_Shutdown();
            ImGui_ImplSDL3_Shutdown();
            throw;
        }

        //TODO: InitFullScreenQuadRender()
        //This method will be used to render the final image on the swapchain

        mIsInitialized = true;
    }

    auto ImGuiD3D11Backend::CreateImages() -> void {
        MKT_BEGIN_PROFILER();
        using namespace mikoto::renderer;

        renderer::d3d11::Context* ctx{ core::as<renderer::d3d11::Context*>( renderer::RenderSystem::Get()->GetContext() ) };

        // Swapchain images are very limited we want a format similar to
        // the one used to create the swap chain image
        renderer::rhi::Format swapChainFormat{ ctx->GetSwapChain()->GetFormat() };

        // Color Device attachment
        auto colorDesc{ renderer::rhi::TextureCreateDescription{}
            .SetWidth( core::as<core::i32>( mExtentWidth ) )
            .SetHeight( core::as<core::i32>( mExtentHeight ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::RenderTarget 
                | renderer::rhi::TextureUsageFlagsBits::CopySource )// I will copy from this guy to swapchain image
            .SetFormat( swapChainFormat ) };

        mColorImage = mDevice->CreateTexture( colorDesc );
        mColorImage->SetDebugName( "ImGui Color image" );

        // Depth attachment
        auto depthDesc{ renderer::rhi::TextureCreateDescription{}
            .SetWidth( core::as<core::i32>( mExtentWidth ) )
            .SetHeight( core::as<core::i32>( mExtentHeight ) )
            .SetDimensions( renderer::rhi::TextureDimension::eTexture2D )
            .SetMultisampling( renderer::rhi::Multisampling::eMsaaX1 )
            .SetUsage( renderer::rhi::TextureUsageFlagsBits::DepthTarget )
            .SetFormat( renderer::rhi::Format::eD32 ) };

        mDepthImage = mDevice->CreateTexture( depthDesc );
        mDepthImage->SetDebugName( "ImGui Depth image" );
    }

    auto ImGuiD3D11Backend::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        mColorImage.Reset();
        mDepthImage.Reset();

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        mIsInitialized = false;
    }

    auto ImGuiD3D11Backend::BeginFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::BeginFrame();
    }

    auto ImGuiD3D11Backend::EndFrame() -> void {
        MKT_BEGIN_PROFILER();
        using namespace mikoto::renderer::d3d11;

        renderer::d3d11::Context* ctx{ core::as<renderer::d3d11::Context*>( renderer::RenderSystem::Get()->GetContext() ) };

        // Handle resizing
        if ( ctx->GetSwapChain()->GetWidth() != mColorImage->GetWidth() ||
             ctx->GetSwapChain()->GetHeight() != mColorImage->GetHeight() ) {

            mExtentWidth = ctx->GetSwapChain()->GetWidth();
            mExtentHeight = ctx->GetSwapChain()->GetHeight();

            CreateImages();
        }

#if false
        ImGui::Begin("Performance");

        const float fps = ImGui::GetIO().Framerate;
        const float frameTime = 1000.0f / fps;

        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Frame time: %.2f ms", frameTime);

        ImGui::End();
#endif

        ImGui::Render();

        // Grab device and device context
        renderer::d3d11::Device* device{ core::checked_cast<renderer::d3d11::Device*>( mDevice ) };

        const eastl::array clearColor{ mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a };

        ID3D11RenderTargetView* renderTargetView{ mColorImage->GetNativeHandle( renderer::rhi::ObjectType::D3D11_RTV ) };

        if ( renderTargetView ) {
            device->GetDeviceContext()->OMSetRenderTargets( 1, MKT_ADDRESSOF( renderTargetView ), nullptr );
            device->GetDeviceContext()->ClearRenderTargetView( renderTargetView, clearColor.data() );

            ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData() );
        }

#if false
        renderer::RenderSystem::Get()->SetPresentTarget( ImGuiService::Get()->GetFinalComposition() );
#endif

        // Update and Render additional Platform Windows
        if ( const ImGuiIO & io{ ImGui::GetIO() }; io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    auto ImGuiD3D11Backend::GetFinalComposition() -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        return mColorImage;
    }

    auto ImGuiD3D11Backend::ConstructImGuiTextureID( const renderer::rhi::ITexture* texture ) -> ImTextureID {
        MKT_BEGIN_PROFILER();

        if ( !texture ) {
            return 0;
        }

        ID3D11ShaderResourceView* directxTexture{ texture->GetNativeHandle( renderer::rhi::ObjectType::D3D11_SRV ) };
        return core::rc_cast<ImTextureID>( directxTexture );
    }

    auto ImGuiD3D11Backend::ConstructImGuiTextureID( renderer::rhi::TextureHandle texture ) -> ImTextureID {
        MKT_BEGIN_PROFILER();

        if ( texture.IsEmpty() ) {
            return 0;
        }

        ID3D11ShaderResourceView* directxTexture{ texture->GetNativeHandle( renderer::rhi::ObjectType::D3D11_SRV ) };
        return core::rc_cast<ImTextureID>( directxTexture );
    }

    ImGuiD3D11Backend::ImGuiD3D11Backend( const ImGuiBackendCreateInfo& createInfo )
        : ImGuiBackend{ createInfo } {
    }
}
#endif
