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

#include <volk.h>
#include <GLFW/glfw3.h>

#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/utility.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Timer.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Platform/Window.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>

namespace mikoto::renderer::vulkan {

    auto Context::Init() -> bool {
        // We need to first get volk up and running
        const VkResult ret{ volkInitialize() };
        MKT_ASSERT( ret == VK_SUCCESS, "Volk initialization failed, no loader found." );

        mInstance = InstanceBuilder{}
            .SetAppName( "Mikoto Application" )
            .SetEngineName( "Mikoto Vulkan Engine" )
            .SetAppVersion( 1, 0, 0 )
            .SetEngineVersion( 1, 0, 0 )
            .SetApiVersion( kVersionMajor, kVersionMinor, kVersionPatch )
#if !defined( NDEBUG )
            .EnableValidationLayers( true )
#else
            .EnableValidationLayers( false )
#endif
            .QueryGLFWExtensions( true )
            .QuerySurfaceSupport( mWindow )
            .SetValidationLevel( InstanceBuilder::ValidationLevel::eCore )
            .Build();

        mMaxFramesInFlight = kMaxFramesInFlight;

        // Initialize the device
        // Init the device when the context is ready
        mDevice = GpuDevice::Create({
            .mApi = GraphicsAPI::eVulkan,
            .mFeaturesSupport{
                // If  the context was created with a window
                // we request for a device with support for presentation
                .mEnablePresentation = mWindow != nullptr,
            },
        });

        if (!mDevice) {
            MKT_THROW_RUNTIME_ERROR( "VulkanContext - Could not initialize GPU Device." );
        }
        mDevice->Init();

        CreateSwapchain();
        InitSynchronization();

        return mInstance->mIsReady && mDevice && mDevice->IsInitialized();
    }

    auto Context::Shutdown() -> void {
        Device* device{ checked_cast<Device*>(mDevice.get()) };
        device->WaitIdle();

        mSwapChainRenderCmds.Reset();

        mPresentTarget.Reset();
        mSwapchain.Reset();

        mFrames.clear();

        mDevice->Shutdown();
        mDevice.reset();

        // Instance cleanup
        mInstance.reset();
    }

    auto Context::SetPresentTarget( TextureHandle texture ) -> void {
        mPresentTarget = texture;
    }

    auto Context::SubmitFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& frame{ mFrames[mCurrentFrameIndex] };
        auto device{ checked_cast<Device*>( mDevice.get() ) };

        // https://community.khronos.org/t/is-it-recommended-to-use-vkcmdcopyimage-to-copy-to-the-swapchain-image-instead-of-a-shader/112122
        // Most vendors recommend you draw the image onto the final output using a fullscreen triangle.
        // TODO: instead render a full quad sampling the final image
        // otherwise it is annoying as you ned ensure the texture you rendered to before is compatible with swap chain image to copy
        if (!mPresentTarget.IsEmpty()) {
            TextureHandle currentSwapchainImage{ mSwapchain->GetImage( mCurrentImageIndex ) };
            mSwapChainRenderCmds->Begin( { .mScopeName = "Blit Swapchain" } );

            // TODO: Here I will do my full quad render I can use my RHI the pass is pretty simple here i basically just build a command buffer and submit it then submit a transition too
            // when I call flush it will start execution all those commands that i just queued but I never submitted actually to the GPU to be executed
            // I do it like this because this way i do one big submit instead of multiple submits in a single frame

            // See SwapChainRender.hh for the pass to render directly to swapchain if requested

            TextureSlice srcSlice{
                .mWidth = (u32)mPresentTarget->GetWidth(),
                .mHeight = (u32)mPresentTarget->GetHeight(),
            };

            TextureSlice dstSlice{
                .mWidth = (u32)currentSwapchainImage->GetWidth(),
                .mHeight = (u32)currentSwapchainImage->GetHeight(),
            };

            mSwapChainRenderCmds->Copy(
                    mPresentTarget.GetRaw(), srcSlice,
                    currentSwapchainImage.GetRaw(), dstSlice );

            mSwapChainRenderCmds->SetResourceState(
                    currentSwapchainImage.GetRaw(),
                    ResourceStates::ePresent );

            mSwapChainRenderCmds->End();

            // enqueue instead of submit
            frame.mSubmissionID = device->SubmitCommands( mSwapChainRenderCmds );
        }

        // External sync
        device->AddQueueWaitSemaphore(
            QueueType::eGraphics,
            checked_cast<BinarySemaphore*>(frame.mImageAvailableSemaphore.GetRaw()),
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        );

        auto& semaphore{ frame.mRenderFinishedSemaphore };
        device->AddQueueSignalSemaphore(
            QueueType::eGraphics,
            checked_cast<BinarySemaphore*>(semaphore.GetRaw()),
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
        );

        // SINGLE submission point
        device->Flush();
    }

    auto Context::PrepareFrame() -> void {
        //MKT_BEGIN_PROFILER_NAMED();
        //MKT_PROFILE_SCOPE_MARKED( "Context::PrepareFrame" );

        const auto& frame{ mFrames[mCurrentFrameIndex] };
        auto device{ checked_cast<Device*>( mDevice.get() ) };

        device->WaitForSubmission( QueueType::eGraphics, frame.mSubmissionID);
        mDevice->RunGarbageCollection();

        const auto ret{ mSwapchain->GetNextImage(mCurrentImageIndex, *checked_cast<const BinarySemaphore*>( frame.mImageAvailableSemaphore.GetRaw() ) ) };

        if (ret == VK_ERROR_OUT_OF_DATE_KHR) {
            mSwapchain->OnResize( mWindow->GetWidth(), mWindow->GetHeight() );
        } else if (ret != VK_SUCCESS) {
            MKT_ASSERT( false, "VulkanContext Failed to acquire swap chain image!");
        }
    }

    auto Context::Update() -> void {

    }

    auto Context::SetRefreshRate( RefreshRate rate ) -> void {
        mRefreshRate = rate;
        mSwapchain->SetRefreshType( mRefreshRate );
    }

    auto Context::Present() -> void {
        if (mPresentTarget.IsEmpty()) {
            return;
        }

        auto& frame{ mFrames[mCurrentFrameIndex] };
        const auto result{ mSwapchain->Present( mCurrentImageIndex, *checked_cast<BinarySemaphore*>( frame.mRenderFinishedSemaphore.GetRaw() ) ) };

        if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) {
            mSwapchain->OnResize( mWindow->GetWidth(), mWindow->GetHeight() );
        } else if ( result != VK_SUCCESS ) {
            MKT_ASSERT( false, "VulkanDevice Error failed present images to swapchain." );
        }

        // Frame is advanced if we work with the swap chain
        mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mMaxFramesInFlight;
    }

    auto Context::GetMaxFramesInFlight() const -> u32 {
        return mMaxFramesInFlight;
    }

    auto Context::GetCurrentImageIndex() const -> u32 {
        return mCurrentImageIndex;
    }

    auto Context::GetCurrentFrameIndex() const -> u32 {
        return mCurrentFrameIndex;
    }

    auto Context::GetInstance() -> Instance& {
        return const_cast<Instance&>( eastl::as_const( *this ).GetInstance() );
    }

    auto Context::GetInstance() const -> const Instance& {
        return *mInstance;
    }

    auto Context::GetSwapchain() -> SwapChainHandle {
        return mSwapchain;
    }

    auto Context::GetApiVersion() const -> u32 {
        return mInstance->mApiVer;
    }

    auto Context::CreateSwapchain() -> void {
        if (!mWindow) {
            return;
        }

        const SwapChainCreateInfo createInfo{
            .mPhysicalDevice = as<Device*>(mDevice.get())->GetPhysicalDevice(),
            .mWidth = as<u32>( mWindow->GetWidth() ),
            .mHeight = as<u32>( mWindow->GetHeight() ),
            .mSurface = mInstance->mSurface,
            .mRefreshRate = mRefreshRate,
            .mFormat = Format::eBGRA8_UNORM,
        };

        mSwapchain = as<Device*>(mDevice.get())->CreateSwapChain(createInfo);

        // Prepare for swapchain render
        // This command buffer is created here because it is only
        // used to submit swapchain work
        mSwapChainRenderCmds = mDevice->CreateCommandList( QueueType::eGraphics );
        mSwapChainRenderCmds->SetDebugName( "Context Swapchain CommandBuffer" );
    }

    auto Context::InitSynchronization() -> void {
        Device* device{ as<Device*>(mDevice.get()) };

        mFrames.resize(mMaxFramesInFlight);

        for (u32 frameIndex{ 0 }; auto& frame : mFrames) {
            frame.mImageAvailableSemaphore = device->CreateBinarySemaphore();
            frame.mImageAvailableSemaphore->SetDebugName( string::Format( "SwapChain Img Avail. BinSemaphore frame {}", frameIndex ) );

            frame.mRenderFinishedSemaphore = device->CreateBinarySemaphore();
            frame.mRenderFinishedSemaphore->SetDebugName( string::Format( "SwapChain Render Done BinSemaphore frame {}", frameIndex ) );

            ++frameIndex;
        }
    }
}// namespace mikoto::renderer::vulkan