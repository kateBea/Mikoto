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

#include <Filesystem/FileService.hh>

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
                .mDeviceType = GpuDeviceType::eDiscrete,
            },
        });

        if (!mDevice) {
            MKT_THROW_RUNTIME_ERROR( "VulkanContext - Could not initialize GPU Device." );
        }
        mDevice->Init();

        CreateSwapchain();
        InitSynchronization();

        InitSwapchainRender();

        return mDevice && mDevice->IsInitialized();
    }

    auto Context::Shutdown() -> void {
        Device* device{ checked_cast<Device*>(mDevice.get()) };
        device->WaitIdle();

        mPipeline.Reset();
        mPipelineLayoutHandle.Reset();
        mBindingLayoutHandle.Reset();

        mBindlessLayout.Reset();
        mDescriptorTable.Reset();

        mVertexShader.Reset();
        mPixelShader.Reset();

        mSamplerState.Reset();

        mBindingSetHandle.Reset();

        mCommandList.Reset();

        mPresentTarget.Reset();
        mSwapchain.Reset();

        mFrames.clear();

        mDevice->Shutdown();
        mDevice.reset();

        // Instance cleanup
        mInstance.reset();
    }

    auto Context::SetPresentTarget( TextureHandle texture ) -> void {
        if (texture.GetRaw() != mPresentTarget.GetRaw()) {
            mPresentTarget = texture;
            mTableUpdateRequired = true;
        }
    }

    auto Context::SubmitFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& frame{ mFrames[mCurrentFrameIndex] };
        auto device{ checked_cast<Device*>( mDevice.get() ) };

        // https://community.khronos.org/t/is-it-recommended-to-use-vkcmdcopyimage-to-copy-to-the-swapchain-image-instead-of-a-shader/112122
        if (!mPresentTarget.IsEmpty() && !mSwapchain.IsEmpty()) {
#if true
            // Blit via full quad render
            TextureHandle colorImage{ mSwapchain->GetImage( mCurrentImageIndex ) };
            mCommandList->Begin( { .mScopeName = "Blit Swapchain" } );

            //const ResourceStates previousState{ mPresentTarget->GetResourceState() };
            mCommandList->SetResourceState( mPresentTarget.GetRaw(), ResourceStates::eShaderResource );

            if (mTableUpdateRequired) {
                (void)mDevice->WriteDescriptorTable( mDescriptorTable, BindingSetItem::Texture_SRV( 0, mPresentTarget.GetRaw() ) );
                mTableUpdateRequired = false;
            }

            auto graphicsState{ GraphicsState{}
                .SetRenderArea( Rect{ as<i32>(mSwapchain->GetWidth()), as<i32>(mSwapchain->GetHeight()) } )
                .AddRenderTarget( colorImage, Color{ .0f } )
            };

            mCommandList->BeginRendering( graphicsState );

            mCommandList->BindPipeline( mPipeline.GetRaw() );
            mCommandList->BindPipelineResources( BindResourcesDescription{}
                .AddResourceSet( 0, mBindingSetHandle.GetRaw() )
                .AddResourceSet( 1, mDescriptorTable.GetRaw() )
                .SetPipelineLayout( mPipelineLayoutHandle.GetRaw() )
                .SetBindPoint( PipelineType::eGraphics ));

            mCommandList->SetViewportState( ViewportState{}
                .AddViewportAndScissorRect( Viewport( mSwapchain->GetWidth(), mSwapchain->GetHeight() ) ) );

            // Issue draw call
            constexpr auto drawArguments{ DrawArguments{}
                .SetVertexCount( 3 ) };
            mCommandList->Draw( drawArguments );

            mCommandList->EndRendering();

            mCommandList->SetResourceState( colorImage.GetRaw(), ResourceStates::ePresent );

            mCommandList->End();

            // enqueue instead of submit
            device->SubmitCommands( mCommandList );
#else
            // Blit via copy command
            TextureHandle currentSwapchainImage{ mSwapchain->GetImage( mCurrentImageIndex ) };
            mCommandList->Begin( { .mScopeName = "Blit Swapchain" } );

            TextureSlice srcSlice{
                .mWidth = (u32)mPresentTarget->GetWidth(),
                .mHeight = (u32)mPresentTarget->GetHeight(),
            };

            TextureSlice dstSlice{
                .mWidth = (u32)currentSwapchainImage->GetWidth(),
                .mHeight = (u32)currentSwapchainImage->GetHeight(),
            };

            mCommandList->Copy(
                    mPresentTarget.GetRaw(), srcSlice,
                    currentSwapchainImage.GetRaw(), dstSlice );

            mCommandList->SetResourceState(
                    currentSwapchainImage.GetRaw(),
                    ResourceStates::ePresent );

            mCommandList->End();

            device->SubmitCommands( mCommandList );
#endif
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

        frame.mFenceValue++;
        mDevice->Signal( QueueType::eGraphics, frame.mFence, frame.mFenceValue );

        // SINGLE submission point
        device->ExecutePendingCommands();
    }

    auto Context::PrepareFrame() -> void {
        //MKT_BEGIN_PROFILER_NAMED();
        //MKT_PROFILE_SCOPE_MARKED( "Context::PrepareFrame" );

        const auto& frame{ mFrames[mCurrentFrameIndex] };
        auto device{ checked_cast<Device*>( mDevice.get() ) };

        device->Wait( QueueType::eGraphics, frame.mFence, frame.mFenceValue );
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
        mCommandList = mDevice->CreateCommandList( QueueType::eGraphics );
        mCommandList->SetDebugName( "Context Swapchain CommandBuffer" );
    }

    auto Context::InitSwapchainRender() -> void {
        // Create shaders
        auto vertexShaderDescription{ ShaderModuleCreateDescription{}
            .SetFile( FileService::Get()->LoadFile( "Resources/Shaders/slang/SwapChainBlit_Vert.slang" ) )
            .SetStage( ShaderType::eVertex )
        };
        mVertexShader = mDevice->CreateShader( vertexShaderDescription );

        auto fragmentShaderDescription{ ShaderModuleCreateDescription{}
            .SetFile( FileService::Get()->LoadFile( "Resources/Shaders/slang/SwapChainBlit_Frag.slang" ) )
            .SetStage( ShaderType::ePixel )
        };
        mPixelShader = mDevice->CreateShader( fragmentShaderDescription );

        // We will upload a texture and a buffer to do some effects, see Triangle_Frag
        // Ideally we want to automate this process by allowing each backend to be able to use shader reflection
        auto layoutDesc{ BindingLayoutDescription{}
            .SetRegisterSpace( 0 )
            .SetShaderVisibility(ShaderFlagsBits::kAll)
            .AddItem(BindingLayoutItem::Sampler(0))};
        mBindingLayoutHandle = mDevice->CreateBindingLayout(layoutDesc);

        auto bindlessLayout{ BindlessLayoutDescription{}
            .SetVisibility(ShaderFlagsBits::kAll)
            .SetRegisterSpace( 1 )
            .AddBindlessItem(BindlessLayoutItem::Texture_SRV(0, 1)) }; // I just need one image slot I can update
        mBindlessLayout = mDevice->CreateBindlessLayout( bindlessLayout );

        mPipelineLayoutHandle = mDevice->CreatePipelineLayout( PipelineLayoutCreateDescription{}
            .AddBindingLayout( mBindingLayoutHandle )
            .AddBindingLayout( mBindlessLayout ));

        auto graphicsPipelineDescription{ GraphicsPipelineDescription{}
            .AddShader( mPixelShader )
            .AddShader( mVertexShader )

            .AddColorFormat( Format::eBGRA8_UNORM )

            .SetPolygonMode( PolygonMode::eFill )
            .SetCullMode( CullMode::eCullBack )
            .SetWindingOrder( WindingOrder::eCounterClockwise )
            .SetTopology( PrimitiveTopology::eTriangleList )
            .SetPipelineLayout( mPipelineLayoutHandle )
        };

        mPipeline = mDevice->CreatePipeline( graphicsPipelineDescription );

        // Sampler
        auto samplerDes{ SamplerCreateDescription{}
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( SamplerWrapMode::eClampToBorder )
            .SetBorderColor( kColorWhite ),
        };
        mSamplerState = mDevice->CreateSampler( samplerDes );

        // Bindless set
        mDescriptorTable = mDevice->CreateDescriptorTable( mBindlessLayout );

        // Non bindless set
        auto bindingSetDesc{ BindingSetDescription{}
            .AddItem( BindingSetItem::Sampler( 0, mSamplerState.GetRaw() ) ) };
        mBindingSetHandle = mDevice->CreateBindingSet( bindingSetDesc, mBindingLayoutHandle );
    }

    auto Context::InitSynchronization() -> void {
        Device* device{ as<Device*>(mDevice.get()) };

        mFrames.resize(mMaxFramesInFlight);

        for (u32 frameIndex{ 0 }; auto& frame : mFrames) {
            frame.mFence = mDevice->CreateFence( frame.mFenceValue );

            frame.mImageAvailableSemaphore = device->CreateBinarySemaphore();
            frame.mImageAvailableSemaphore->SetDebugName( string::Format( "SwapChain Img Avail. BinSemaphore frame {}", frameIndex ) );

            frame.mRenderFinishedSemaphore = device->CreateBinarySemaphore();
            frame.mRenderFinishedSemaphore->SetDebugName( string::Format( "SwapChain Render Done BinSemaphore frame {}", frameIndex ) );

            ++frameIndex;
        }
    }
}// namespace mikoto::renderer::vulkan