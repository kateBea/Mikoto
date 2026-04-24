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

#include <EASTL/algorithm.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/utility.h>
#include <EASTL/vector.h>
#include <GLFW/glfw3.h>
#include <volk.h>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/String.hh>
#include <Core/Timer.hh>
#include <Core/Types.hh>
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>
#include <Memory/Allocator.hh>
#include <Platform/Window.hh>
#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Core/Rhi.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <any>
#include <memory>
#include <numeric>
#include <ranges>
#include <vector>

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
            .EnableValidationLayers( true )
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

                // Rest are default values
            },
        });

        if (!mDevice) {
            MKT_THROW_RUNTIME_ERROR( "VulkanContext - Could not initialize GPU Device." );
        }
        mDevice->Init();

        CreateSwapchain();
        PrepareSynchronization();

        return mInstance->mIsReady && mDevice && mDevice->IsInitialized();
    }

    auto Context::Shutdown() -> void {
        mPresentTarget.Reset();
        mSwapchain.Reset();

        Device* device{ as<Device*>(mDevice.get()) };
        device->WaitIdle();

        for (auto& object : mFrames ) {
            object.mImageAvailableSemaphore.Destroy( device->GetDevice() );
            object.mRenderFinishedSemaphore.Destroy( device->GetDevice() );
        }

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

        // https://community.khronos.org/t/is-it-recommended-to-use-vkcmdcopyimage-to-copy-to-the-swapchain-image-instead-of-a-shader/112122
        // Most vendors recommend you draw the image onto the final output using a fullscreen triangle.
        // TODO: instead render a full quad sampling the final image
        // otherwise it is annoying as you ned ensure the texture you rendered to before is compatible with swap chain image to copy
        if (!mPresentTarget.IsEmpty()) {
            TextureHandle currentSwapchainImage{ mSwapchain->GetImage( mCurrentImageIndex ) };
            CommandListHandle cmd{ mDevice->CreateCommandList( QueueType::eGraphics ) };
            cmd->Begin();


            // TODO: Here I will do my full quad render I can use my RHI the pass is pretty simple here i basically just build a command buffer and submit it then submit a transition too
            // when I call flush it will start execution all those commands that i just queued but I never submitted actually to the GPU to be executed
            // I do it like this because this way i do one big submit instead of multiple submits in a single frame

            TextureSlice srcSlice{
                .mWidth = (u32)mPresentTarget->GetWidth(),
                .mHeight = (u32)mPresentTarget->GetHeight(),
            };

            TextureSlice destSlice{
                .mWidth = (u32)currentSwapchainImage->GetWidth(),
                .mHeight = (u32)currentSwapchainImage->GetHeight(),
            };


            cmd->CopyTexture( mPresentTarget.GetRaw(), srcSlice, currentSwapchainImage.GetRaw(), destSlice );

            cmd->SetResourceState( currentSwapchainImage.GetRaw(), ResourceStates::ePresent );

            cmd->End();
            mDevice->SubmitCommands( cmd );
        }

        // Submit swapchain work
        const auto& frame{ mFrames[mCurrentFrameIndex] };
        checked_cast<Device*>( mDevice.get() )->Flush(
            QueueSubmitSync{
                    .mWaitBinarySemaphore = frame.mImageAvailableSemaphore.mSemaphore,
                    .mWaitBinaryStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,

                    .mSignalBinarySemaphore = frame.mRenderFinishedSemaphore.mSemaphore,
                    .mSignalBinaryStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT
            }, QueueType::eGraphics );

        // Flush rest of work
        mDevice->Flush();

        mCurrentFrameIndex = (mCurrentFrameIndex + 1) % mMaxFramesInFlight;
    }

    auto Context::PrepareFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        mDevice->RunGarbageCollection();

        const auto& frame{ mFrames[mCurrentFrameIndex] };
        const auto ret{ mSwapchain->GetNextImage(mCurrentImageIndex, frame.mImageAvailableSemaphore ) };

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

        // Because the frame index is advanced in the call to SubmitFrame(..)
        auto& frame{ mFrames[mCurrentImageIndex] };
        const auto result{ mSwapchain->Present( mCurrentImageIndex, frame.mRenderFinishedSemaphore ) };

        if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) {
            mSwapchain->OnResize( mWindow->GetWidth(), mWindow->GetHeight() );
        } else if ( result != VK_SUCCESS ) {
            MKT_ASSERT( false, "VulkanDevice Error failed present images to swapchain." );
        }
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
    }

    auto Context::PrepareSynchronization() -> void {
        Device* device{ as<Device*>(mDevice.get()) };

        // --- Per-frame sync ---
        mFrames.resize(mMaxFramesInFlight);
        VkSemaphoreCreateInfo binarySemaphoreCreateInfo{ initializers::SemaphoreCreateInfo() };
        for (auto& frame : mFrames) {
            frame.mImageAvailableSemaphore.Create(binarySemaphoreCreateInfo, device->GetDevice());
            frame.mRenderFinishedSemaphore.Create(binarySemaphoreCreateInfo, device->GetDevice());
        }
    }
}// namespace mikoto::renderer::vulkan