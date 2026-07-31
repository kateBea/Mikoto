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

#include <imgui.h>
#include <ImGuizmo.h>

#include <EASTL/array.h>
#include <EASTL/string.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Platform.hh>
#include <Core/Exception.hh>

#include <ImGui/ImGuiD3D11Backend.hh>

#include <Assets/ImageProcessor.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Core/Rhi.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/D3D11/D3D11Device.hh>
#include <Renderer/D3D11/D3D11Context.hh>
#include <Renderer/D3D11/D3D11Texture.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)
#include <imgui_impl_dx11.h>
#include <imgui_impl_glfw.h>

namespace mikoto::gui {

    auto ImGuiD3D11Backend::Init() -> void {
        using namespace mikoto::renderer;
        using namespace mikoto::renderer::d3d11;

        const auto window{ eastl::any_cast<GLFWwindow*>( mWindow->GetNativeWindow() ) };

        if (!ImGui_ImplGlfw_InitForOther(window, true)) {
            MKT_THROW_RUNTIME_ERROR( "Failed ImGui_ImplGlfw_InitForOther" );
        }

        // Grab device and device context
        Device* device{ as<Device*>( mDevice ) };

        if (!ImGui_ImplDX11_Init(device->GetDevice(), device->GetDeviceContext()) ) {
            MKT_THROW_RUNTIME_ERROR( "Failed ImGui_ImplDX11_Init" );
        }

        auto dimensions{ rhi::InferDimensions( mResolution ) };
        mExtentWidth = dimensions.first;
        mExtentHeight = dimensions.second;

        CreateImages();

        //TODO: InitFullScreenQuadRender()
        //This method will be used to render the final image on the swapchain

        mIsInitialized = true;
    }

    auto ImGuiD3D11Backend::CreateImages() -> void {
        using namespace mikoto::renderer;

        d3d11::Context* ctx{ as<d3d11::Context*>(RenderSystem::Get()->GetContext()) };

        // Swapchain images are very limited we want a format similar to
        // the one used to create the swap chain image
        Format swapChainFormat{ ctx->GetSwapChain()->GetFormat() };

        // Color Device attachment
        auto colorDesc{ TextureCreateDescription{}
            .SetWidth( as<i32>( mExtentWidth ) )
            .SetHeight( as<i32>( mExtentHeight ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kCopySrc ) // I will copy from this guy to swapchain image
            .SetFormat( swapChainFormat ) };

        mColorImage = mDevice->CreateTexture( colorDesc );
        mColorImage->SetDebugName( "ImGui Color image" );

        // Depth attachment
        auto depthDesc{ TextureCreateDescription{}
            .SetWidth( as<i32>( mExtentWidth ) )
            .SetHeight( as<i32>( mExtentHeight ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kDepthTarget )
            .SetFormat( Format::eD32 ) };

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

        // TODO:
        // On Shutdown there is a crash if I the windows
        // are not in window client area (are docked out of the main window)

        ImGui_ImplDX11_Shutdown();
        ImGui_ImplGlfw_Shutdown();
    }

    auto ImGuiD3D11Backend::BeginFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::BeginFrame();
    }

    auto ImGuiD3D11Backend::EndFrame() -> void {
        using namespace mikoto::renderer::d3d11;

        Context* ctx{ as<Context*>(RenderSystem::Get()->GetContext()) };

        // Handle resizing
        if (ctx->GetSwapChain()->GetWidth() != mColorImage->GetWidth() ||
            ctx->GetSwapChain()->GetHeight() != mColorImage->GetHeight()) {

            mExtentWidth = ctx->GetSwapChain()->GetWidth();
            mExtentHeight = ctx->GetSwapChain()->GetHeight();

            CreateImages();
        }

        ImGui::Render();

        // Grab device and device context
        Device* device{ as<Device*>( mDevice ) };

        const eastl::array clearColor{ mClearColor.r, mClearColor.g, mClearColor.b, mClearColor.a };

        ID3D11RenderTargetView* renderTargetView{ mColorImage->GetNativeHandle( ObjectType::D3D11_RTV ) };

        if (renderTargetView) {
            device->GetDeviceContext()->OMSetRenderTargets(1, MKT_ADDRESSOF( renderTargetView ), nullptr);
            device->GetDeviceContext()->ClearRenderTargetView(renderTargetView, clearColor.data());

            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }

        // Update and Render additional Platform Windows
        if (const ImGuiIO & io{ ImGui::GetIO() }; io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    auto ImGuiD3D11Backend::GetFinalComposition() -> TextureHandle {
        return mColorImage;
    }

    auto ImGuiD3D11Backend::ConstructImGuiTextureID( const ITexture *texture ) -> ImTextureID {
        if (!texture) {
            return 0;
        }

        ID3D11ShaderResourceView* directxTexture{ texture->GetNativeHandle( ObjectType::D3D11_SRV ) };
        return rc_cast<ImTextureID>( directxTexture );
    }

    auto ImGuiD3D11Backend::ConstructImGuiTextureID( TextureHandle texture ) -> ImTextureID {
        if (texture.IsEmpty()) {
            return 0;
        }

        ID3D11ShaderResourceView* directxTexture{ texture->GetNativeHandle( ObjectType::D3D11_SRV ) };
        return rc_cast<ImTextureID>( directxTexture );
    }
}

#endif
