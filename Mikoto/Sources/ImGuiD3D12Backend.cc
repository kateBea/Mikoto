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
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Assets/ImageProcessor.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/RenderSystem.hh>

#include <ImGui/ImGuiD3D12Backend.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#define GLFW_NATIVE_INCLUDE_NONE
#include <GLFW/glfw3native.h>

#include <imgui_impl_dx12.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_win32.h>

#include <Renderer/Rhi/D3D12/D3D12Device.hh>
#include <Renderer/Rhi/D3D12/D3D12Context.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>

namespace mikoto::imgui {

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    auto ExampleDescriptorHeapAllocator::Create( ID3D12Device* device, ID3D12DescriptorHeap* heap ) -> void {
        IM_ASSERT( mHeap == nullptr && mFreeIndices.empty() );
        mHeap = heap;
        D3D12_DESCRIPTOR_HEAP_DESC desc = heap->GetDesc();
        mHeapType = desc.Type;
        mHeapStartCpu = mHeap->GetCPUDescriptorHandleForHeapStart();
        mHeapStartGpu = mHeap->GetGPUDescriptorHandleForHeapStart();
        mHeapHandleIncrement = device->GetDescriptorHandleIncrementSize( mHeapType );
        mFreeIndices.reserve( ( int )desc.NumDescriptors );
        for ( int n = desc.NumDescriptors; n > 0; n-- )
            mFreeIndices.push_back( n - 1 );
    }

    auto ExampleDescriptorHeapAllocator::Destroy() -> void {
        mHeap = nullptr;
        mFreeIndices.clear();
    }

    auto ExampleDescriptorHeapAllocator::Alloc( D3D12_CPU_DESCRIPTOR_HANDLE* outCpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuDescHandle ) -> void {
        IM_ASSERT( mFreeIndices.Size > 0 );
        int idx = mFreeIndices.back();
        mFreeIndices.pop_back();
        outCpuDescHandle->ptr = mHeapStartCpu.ptr + ( idx * mHeapHandleIncrement );
        outGpuDescHandle->ptr = mHeapStartGpu.ptr + ( idx * mHeapHandleIncrement );
    }

    auto ExampleDescriptorHeapAllocator::Free( D3D12_CPU_DESCRIPTOR_HANDLE outCpuDescHandle, D3D12_GPU_DESCRIPTOR_HANDLE outGpuDescHandle ) -> void {
        int cpu_idx = ( int )( ( outCpuDescHandle.ptr - mHeapStartCpu.ptr ) / mHeapHandleIncrement );
        int gpu_idx = ( int )( ( outGpuDescHandle.ptr - mHeapStartGpu.ptr ) / mHeapHandleIncrement );
        IM_ASSERT( cpu_idx == gpu_idx );
        mFreeIndices.push_back( cpu_idx );
    }

    ImGuiD3D12Backend::ImGuiD3D12Backend( const ImGuiBackendCreateInfo &createInfo )
        : ImGuiBackend{ createInfo }
    {
        auto dimensions{ rhi::InferDimensions( mResolution ) };

        mDimensions.Width = dimensions.first;
        mDimensions.Height = dimensions.second;
        mDimensions.DepthOrArraySize = 1;
    }

    auto ImGuiD3D12Backend::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        InitImages();
        InitImGuiForD3D12();

        mCommandList = mDevice->CreateCommandList( QueueType::eGraphics );
        mCommandList->SetDebugName( "ImGui Command Buffer" );

        mDeviceResources = checked_cast<d3d12::Device*>( mDevice )->GetHeapResources();

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
        mColorImage.Release();
        mDepthImage.Release();

        for (const auto item : mTextureIdMap | std::views::values) {
            mSrvDescHeapAlloc.Free( item.mCpuHandle, item.mGpuHandle );
        }

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplGlfw_Shutdown();

        mCommandList.Release();

        mIsInitialized = false;
    }

    auto ImGuiD3D12Backend::BeginFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuizmo::BeginFrame();
    }

    auto ImGuiD3D12Backend::EndFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        d3d12::Context* context{ checked_cast<d3d12::Context*>( RenderSystem::Get()->GetContext() ) };

        // If the swap chain has been resized, we need to recreate the framebuffers and images
        d3d12::SwapChainHandle swapChain{ context->GetSwapChain() };
        if ( swapChain->GetWidth() != mDimensions.Width || swapChain->GetHeight() != mDimensions.Height ) {
            mDimensions.Width = swapChain->GetWidth();
            mDimensions.Height = swapChain->GetHeight();

            // Might not be optimal, but ensures the device is idle before recreating resources.
            // This blocks this thread until the GPU is idle, which can cause a performance,
            // But resizing should not be happening often
            mDevice->WaitIdle();

            InitImages();
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

        mCommandList->Begin( { .mScopeName = "ImGui Render" } );
        mCommandList->SetTransition( mColorImage.GetRaw(), ResourceStates::eRenderTarget );

        RecordCommands();

        mCommandList->End();

        auto submitInfo{ SubmitInfo{}
            .AddCommandList( mCommandList ) };
        RenderSystem::Get()->BatchSubmission( eastl::move( submitInfo ), QueueType::eGraphics );

#if false
        RenderSystem::Get()->SetPresentTarget( ImGuiService::Get()->GetFinalComposition() );
#endif

        if ( const ImGuiIO & io{ ImGui::GetIO() }; io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable ) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    auto ImGuiD3D12Backend::GetFinalComposition() -> TextureHandle {
        return mColorImage;
    }

    auto ImGuiD3D12Backend::ConstructImGuiTextureID( const ITexture *texture ) -> ImTextureID {
        if (!texture) {
            return 0;
        }

        std::lock_guard lock{ mImTextureAllocMutex };
        const d3d12::Texture* pTexture{ checked_cast<const d3d12::Texture*>( texture ) };
        if (!mTextureIdMap.contains( texture )) {

            auto& imTextureInfo{ mTextureIdMap[texture] = ImTextureInfo{}};

            mSrvDescHeapAlloc.Alloc( &imTextureInfo.mCpuHandle, &imTextureInfo.mGpuHandle );
            pTexture->CreateSRV(imTextureInfo.mCpuHandle.ptr, rhi::kAllSubResources, texture->GetFormat(), texture->GetDimension());

            //ID3D12Device2* device{ checked_cast<d3d12::Device*>( mDevice )->GetDevice() };
            //mDeviceResources->mShaderResourceViewHeap->CopyToShaderVisibleHeap(index);
        }

        return (ImTextureID)mTextureIdMap[texture].mGpuHandle.ptr;
    }

    auto ImGuiD3D12Backend::ConstructImGuiTextureID( TextureHandle texture ) -> ImTextureID {
        return ConstructImGuiTextureID( texture.GetRaw() );
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
        const auto window{ eastl::any_cast<GLFWwindow*>( mWindow->GetNativeWindow() ) };
        if (!ImGui_ImplGlfw_InitForOther(window, true)) {
            MKT_THROW_RUNTIME_ERROR( "ImGuiD3D12Backend - Failed to initialize Win32 for ImGui" );
        }

        d3d12::Device* device{ checked_cast<d3d12::Device*>( mDevice ) };
        d3d12::Context* context{ checked_cast<d3d12::Context*>( RenderSystem::Get()->GetContext() ) };

        d3d12::Queue* queue{ checked_cast<d3d12::Queue*>( device->GetQueue( QueueType::eGraphics ) ) };
        ID3D12CommandQueue* cmdQueue{ *queue };

        ImGui_ImplDX12_InitInfo initInfo{};
        initInfo.Device = device->GetDevice();
        initInfo.CommandQueue = cmdQueue;
        initInfo.NumFramesInFlight = context->GetBackBufferCount();
        initInfo.RTVFormat = d3d12::GetFormat( mColorImage->GetFormat() );
        initInfo.DSVFormat = d3d12::GetFormat( mDepthImage->GetFormat() );

        static constexpr u32 kSrvHeapSize{ 4096u };

        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = kSrvHeapSize;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        d3d12::ThrowIfFailed( device->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mSrvDescHeap)));
        mSrvDescHeapAlloc.Create(device->GetDevice(), mSrvDescHeap.Get());

        mSrvDescHeap->SetName( L"Imgui SRV Heap" );

        // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
        // (current version of the backend will only allocate one descriptor, future versions will need to allocate more)
        initInfo.SrvDescriptorHeap = mSrvDescHeap.Get();

        initInfo.SrvDescriptorAllocFn =
            [](ImGui_ImplDX12_InitInfo*,
                D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
                D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) {
            return mSrvDescHeapAlloc.Alloc(outCpuHandle, outGpuHandle);
        };
        initInfo.SrvDescriptorFreeFn =
            [](ImGui_ImplDX12_InitInfo*,
                D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
                D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle) {
                return mSrvDescHeapAlloc.Free(cpuHandle, gpuHandle);
        };
        if (!ImGui_ImplDX12_Init(&initInfo)) {
            MKT_THROW_RUNTIME_ERROR( "ImGuiD3D12Backend - Failed to initialize D3D12 for ImGui" );
        }
    }

    auto ImGuiD3D12Backend::RecordCommands() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        d3d12::CommandList* cmd{ checked_cast<d3d12::CommandList*>( mCommandList.GetRaw() ) };
        ID3D12GraphicsCommandList* d3d12CmdList{ *cmd };

        auto graphicsState{ GraphicsState{}
            .SetRenderArea( Rect{ 1920, 1080 } )
            .AddDepthTarget( mDepthImage )
            .AddRenderTarget( mColorImage, rhi::kColorMagenta ) };
        mCommandList->BeginRendering( graphicsState );
        mCommandList->SetClearColor( mColorImage, mClearColor );

        // ComPtr::operator&() is for output parameters and can release the existing pointer.
        // Use Get() when passing an existing COM object to an API.
        ID3D12DescriptorHeap* descriptorHeap{ mSrvDescHeap.Get() };
        d3d12CmdList->SetDescriptorHeaps(1, &descriptorHeap);

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), d3d12CmdList);

        mCommandList->EndRendering();
    }
}// namespace mikoto::gui

#endif