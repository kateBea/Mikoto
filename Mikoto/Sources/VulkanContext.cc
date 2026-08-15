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

#include <Renderer/Rhi/Types.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <Renderer/Rhi/Vulkan/VulkanContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    Context::Context( const RenderContextCreateInfo& createInfo )
        : RenderContext{ createInfo }
    {}

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
        mDevice = IGpuDevice::Create({
            .mApi = GraphicsAPI::eVulkan,
            .mFeaturesSupport{
                // If  the context was created with a window
                // we request for a device with support for presentation
                .mEnablePresentation = mWindow != nullptr,
                .mDeviceType = GpuDeviceType::eDiscrete,
            },
        });

        if (!mDevice) {
            MKT_THROW_RUNTIME_ERROR( "VulkanContext - Could not create GPU Device." );
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

    auto Context::BatchSubmission( rhi::SubmitInfo&& submitInfo, rhi::QueueType queue ) -> void {
        std::lock_guard lock{ mBatchedSubmissionEmplaceMutex };

        Device* device{ checked_cast<Device*>( mDevice.get() ) };
        Queue* pQueue{ checked_cast<Queue*>( device->GetQueue( queue ) ) };

        auto& submissionBatchMap{ mBatchedSubmissions[pQueue] };

        // Batch all commands and fences into same submit info
        // they go to the same submission batch anyway there is no need to split them.
        submissionBatchMap.AddCommandLists( submitInfo.mCommands );
        submissionBatchMap.AddSignals( submitInfo.mSignals );
    }

    auto Context::SubmitFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& frame{ mFrames[mCurrentFrameIndex] };
        auto device{ checked_cast<Device*>( mDevice.get() ) };

        // Submit batched commands
        SubmitInfoMap swapMap{};
        {
            std::lock_guard lock{ mBatchedSubmissionProcessMutex };
            swapMap = eastl::move(mBatchedSubmissions);
        }

        for (const auto& [queue, submitInfo] : swapMap) {
            queue->ExecuteCommandLists( submitInfo );
        }

        // https://community.khronos.org/t/is-it-recommended-to-use-vkcmdcopyimage-to-copy-to-the-swapchain-image-instead-of-a-shader/112122
        if (!mPresentTarget.IsEmpty() && !mSwapchain.IsEmpty()) {
#if true
            // Blit via full quad render
            TextureHandle colorImage{ mSwapchain->GetImage( mCurrentImageIndex ) };
            mCommandList->Begin( { .mScopeName = "Blit Swapchain" } );
            mCommandList->SetTransition( mPresentTarget.GetRaw(), ResourceStates::eShaderResource );

            if (mTableUpdateRequired) {
                (void)mDevice->WriteDescriptorTable( mDescriptorTable, BindingSetItem::Texture_SRV( 0, mPresentTarget.GetRaw() ) );
                mTableUpdateRequired = false;
            }

            auto graphicsState{ GraphicsState{}
                .SetRenderArea( Rect{ as<i32>(mSwapchain->GetWidth()), as<i32>(mSwapchain->GetHeight()) } )
                .AddRenderTarget( colorImage, Color{ .0f } ) };

            mCommandList->BeginRendering( graphicsState );

            mCommandList->BindPipeline( mPipeline.GetRaw() );
            mCommandList->BindPipelineResources( BindResourcesDescription{}
                .AddResourceSet( 0, mBindingSetHandle.GetRaw() )
                .AddResourceSet( 1, mDescriptorTable.GetRaw() )
                .SetPipelineLayout( mPipelineLayoutHandle.GetRaw() )
                .SetBindPoint( PipelineType::eGraphics ));

            mCommandList->SetViewportState( ViewportState{}
                .AddViewportAndScissorRect( Viewport( as<f32>( mSwapchain->GetWidth() ), as<f32>( mSwapchain->GetHeight() ) ) ) );

            constexpr auto drawArguments{ DrawArguments{}
                .SetVertexCount( 3 ) };
            mCommandList->Draw( drawArguments );

            mCommandList->EndRendering();

            mCommandList->SetTransition( colorImage.GetRaw(), ResourceStates::ePresent );

            mCommandList->End();
#else
            // Blit via copy command
            TextureHandle currentSwapchainImage{ mSwapchain->GetImage( mCurrentImageIndex ) };
            mCommandList->Begin( { .mScopeName = "Blit Swapchain" } );

            const TextureSlice srcSlice{
                .mWidth = (u32)mPresentTarget->GetWidth(),
                .mHeight = (u32)mPresentTarget->GetHeight() };

            const TextureSlice dstSlice{
                .mWidth = (u32)currentSwapchainImage->GetWidth(),
                .mHeight = (u32)currentSwapchainImage->GetHeight() };

            mCommandList->Copy(
                mPresentTarget.GetRaw(), srcSlice,
                currentSwapchainImage.GetRaw(), dstSlice );

            mCommandList->SetTransition(
                currentSwapchainImage.GetRaw(),
                ResourceStates::ePresent );

            mCommandList->End();
#endif
        }

        Queue* queue{ checked_cast<Queue*>( device->GetQueue( QueueType::eGraphics ) ) };

        queue->PushWaitSemaphore(
                frame.mImageAvailableSemaphore.GetRaw(),
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );

        queue->PushSignalSemaphore(
                frame.mRenderFinishedSemaphore.GetRaw(),
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT );

        frame.mFenceValue++;

        eastl::array commandList{ mCommandList };
        queue->ExecuteCommandsWithSemaphores( commandList, frame.mFence.GetRaw(), frame.mFenceValue );
    }

    auto Context::PrepareFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto& frame{ mFrames[mCurrentFrameIndex] };
        auto device{ checked_cast<Device*>( mDevice.get() ) };

        Queue* graphicsQueue{ checked_cast<Queue*>( device->GetQueue( QueueType::eGraphics ) ) };

        graphicsQueue->Wait( frame.mFence.GetRaw(), frame.mFenceValue );
        mDevice->RunGarbageCollection();

        const auto ret{ mSwapchain->GetNextImageIndex( mCurrentImageIndex, *frame.mImageAvailableSemaphore.GetRaw() ) };

        if ( ret == VK_ERROR_OUT_OF_DATE_KHR ) {
            mSwapchain->OnResize( mWindow->GetWidth(), mWindow->GetHeight() );
        } else if ( ret != VK_SUCCESS ) {
            MKT_ASSERT( false, "VulkanContext Failed to acquire swap chain image!" );
        }
    }

    auto Context::Update() -> void {
        // Nothing
    }

    auto Context::SetRefreshRate( RefreshRate rate ) -> void {
        mRefreshRate = rate;
        mSwapchain->SetRefreshType( mRefreshRate );
    }

    auto Context::Present() -> void {
        if ( mPresentTarget.IsEmpty() ) {
            return;
        }

        auto& frame{ mFrames[mCurrentFrameIndex] };
        const auto result{ mSwapchain->Present( mCurrentImageIndex, *frame.mRenderFinishedSemaphore ) };

        if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) {
            mSwapchain->OnResize( mWindow->GetWidth(), mWindow->GetHeight() );
        } else if ( result != VK_SUCCESS ) {
            MKT_ASSERT( false, "VulkanDevice Error failed present images to swapchain." );
        }

        // Frame is advanced if we work with a swap chain
        mCurrentFrameIndex = ( mCurrentFrameIndex + 1 ) % mMaxFramesInFlight;
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

    auto Context::GetSwapChain() -> SwapChainHandle {
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
            .mPhysicalDevice = checked_cast<Device*>(mDevice.get())->GetPhysicalDevice(),
            .mWidth = as<u32>( mWindow->GetWidth() ),
            .mHeight = as<u32>( mWindow->GetHeight() ),
            .mSurface = mInstance->mSurface,
            .mRefreshRate = mRefreshRate,
            .mFormat = Format::eBGRA8_UNORM };

        mSwapchain = checked_cast<Device*>(mDevice.get())->CreateSwapChain(createInfo);
    }

    auto Context::InitSwapchainRender() -> void {
        // Create shaders
        FileHandle vsShader{ FileService::Get()->LoadFile( "Resources/Shaders/slang/SwapChainBlit_Vert.slang" ) };
        auto vertexShaderDescription{ ShaderModuleCreateDescription{}
            .SetContents( vsShader )
            .SetModuleName( vsShader->GetName() )
            .SetModulePath( vsShader->GetPath() )
            .SetLanguage( ShaderLanguage::eSlang )
            .SetStage( ShaderType::eVertex ) };
        mVertexShader = mDevice->CreateShader( vertexShaderDescription );

        FileHandle pxShader{ FileService::Get()->LoadFile( "Resources/Shaders/slang/SwapChainBlit_Frag.slang" ) };
        auto fragmentShaderDescription{ ShaderModuleCreateDescription{}
            .SetContents( pxShader )
            .SetModuleName( pxShader->GetName() )
            .SetModulePath( pxShader->GetPath() )
            .SetLanguage( ShaderLanguage::eSlang )
            .SetStage( ShaderType::ePixel ) };
        mPixelShader = mDevice->CreateShader( fragmentShaderDescription );

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
            .SetPipelineLayout( mPipelineLayoutHandle ) };

        mPipeline = mDevice->CreatePipeline( graphicsPipelineDescription );

        // Sampler
        auto samplerDes{ SamplerCreateDescription{}
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( SamplerWrapMode::eClampToBorder )
            .SetBorderColor( kColorWhite ) };
        mSamplerState = mDevice->CreateSampler( samplerDes );

        // Bindless set
        mDescriptorTable = mDevice->CreateDescriptorTable( mBindlessLayout );

        // Non-bindless set
        auto bindingSetDesc{ BindingSetDescription{}
            .AddItem( BindingSetItem::Sampler( 0, mSamplerState.GetRaw() ) ) };
        mBindingSetHandle = mDevice->CreateBindingSet( bindingSetDesc, mBindingLayoutHandle );

        mCommandList = mDevice->CreateCommandList( QueueType::eGraphics );
        mCommandList->SetDebugName( "Context Swapchain CommandBuffer" );
    }

    auto Context::InitSynchronization() -> void {
        Device* device{ checked_cast<Device*>(mDevice.get()) };

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