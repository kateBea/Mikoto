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

#include <GLFW/glfw3.h>

#include <imgui.h>
#include <ImGuizmo.h>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Profiler.hh>
#include <Core/Platform.hh>

#include <Logging/Logger.hh>

#include <Assets/ImageProcessor.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/RenderSystem.hh>

#include <ImGui/ImGuiD3D12Backend.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Renderer/D3D12/D3D12Device.hh>
#include <Renderer/D3D12/D3D12Context.hh>

#include <imgui_impl_dx12.h>
#include <imgui_impl_glfw.h>

namespace mikoto::gui {

    ImGuiD3D12Backend::ImGuiD3D12Backend( const ImGuiBackendCreateInfo &createInfo )
        : ImGuiBackend{ createInfo }
    {
        auto dimensions{ rhi::InferDimensions( mResolution ) };

        mDimensions.Width = dimensions.first;
        mDimensions.Height = dimensions.second;
        mDimensions.DepthOrArraySize = 1;
    }

    auto ImGuiD3D12Backend::Init() -> void {
        InitImages();
        InitImGuiForD3D12();

        mCommandList = mDevice->CreateCommandList( QueueType::eGraphics );
        mCommandList->SetDebugName( "ImGui Command Buffer" );

        mIsInitialized = true;
    }

    auto ImGuiD3D12Backend::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        mDevice->WaitIdle();

        // Handles need to be disabled as the destruction of the graphics context
        // is deferred to the ImGuiService destruction where we might not have a context ready
        // Services in Mikoto do not do their cleanup in the destructor they do it on the Shutdown method
        mColorImage.Reset();
        mDepthImage.Reset();

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        mCommandList.Reset();

        mIsInitialized = false;
    }

    auto ImGuiD3D12Backend::BeginFrame() -> void {

    }

    auto ImGuiD3D12Backend::EndFrame() -> void {

    }

    auto ImGuiD3D12Backend::GetFinalComposition() -> TextureHandle {
        return TextureHandle::CreateEmpty();
    }

    auto ImGuiD3D12Backend::ConstructImGuiTextureID( const ITexture *texture ) -> ImTextureID {
        return 0;
    }

    auto ImGuiD3D12Backend::ConstructImGuiTextureID( TextureHandle texture ) -> ImTextureID {
        return 0;
    }

    auto ImGuiD3D12Backend::InitImages() -> void {
        // Color Device attachment. Since we will manage these textures here we can set
        // the tracking state to manual so we will handle the transitions and barriers ourselves
        auto colorDesc{ TextureCreateDescription{}
            .SetWidth( as<i32>( mDimensions.Width ) )
            .SetHeight( as<i32>( mDimensions.Height ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kRenderTarget | TextureUsageFlagsBits::kCopySrc | TextureUsageFlagsBits::kShaderResource ) // I will copy from this guy to swapchain image
            .SetFormat( Format::eBGRA8_UNORM ) };

        mColorImage = mDevice->CreateTexture( colorDesc );
        mColorImage->SetDebugName( "ImGui Color image" );

        // Depth attachment
        auto depthDesc{ TextureCreateDescription{}
            .SetWidth( as<i32>( mDimensions.Width ) )
            .SetHeight( as<i32>( mDimensions.Height ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetMultisampling( Multisampling::eMsaaX1 )
            .SetUsage( TextureUsageFlagsBits::kDepthTarget )
            .SetFormat( Format::eD32 ) };

        mDepthImage = mDevice->CreateTexture( depthDesc );
        mDepthImage->SetDebugName( "ImGui Depth image" );
    }

    auto ImGuiD3D12Backend::InitImGuiForD3D12() -> void {
        const auto window{ eastl::any_cast<GLFWwindow*>( m_Window->GetNativeWindow() ) };

        d3d12::Device* device{ checked_cast<d3d12::Device*>( mDevice ) };
        d3d12::Context* context{ checked_cast<d3d12::Context*>( RenderSystem::Get()->GetContext() ) };
    }
}// namespace mikoto::gui

#endif