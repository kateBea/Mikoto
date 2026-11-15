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

#include <EASTL/utility.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>
#include <EASTL/numeric_limits.h>

#include <volk.h>

#include <Core/CVar.hh>
#include <Core/Core.hh>
#include <Core/Timer.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Filesystem/FileService.hh>

#include <Platform/Window.hh>
#include <Platform/Utility.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer::rhi;

    Context::Context( const RenderContextCreateInfo& createInfo )
        : RenderContext{ createInfo } {
    }

    auto Context::Init() -> bool {
        MKT_BEGIN_PROFILER();

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
            .QueryWindowExtensions( true )
            .QuerySurfaceSupport( mWindow )
            .SetValidationLevel( InstanceBuilder::ValidationLevel::eCore )
            .Build();

        // https://erfan-ahmadi.github.io/blog/Nabla/fif
        // https://docs.vulkan.org/samples/latest/samples/performance/swapchain_images/README.html
        mMaxFramesInFlight = kMaxFramesInFlight;

        const eastl::string deviceTypeString{ core::CVarRegistry::Get()->GetValue<eastl::string>( "device_type" ) };
        const renderer::rhi::GpuDeviceType deviceType{ rhi::InferGpuDeviceType( deviceTypeString ) };

        MKT_ASSERT( deviceType != renderer::rhi::GpuDeviceType::eInvalid, "Invalid device type." );

        // Initialize the device
        // Init the device when the context is ready
        mDevice = renderer::rhi::IGpuDevice::Create( { 
            .mApi = renderer::rhi::GraphicsAPI::eVulkan,
            .mDeviceType = deviceType,
            .mFeatureSupportFlags = GetRequestedDeviceFeatures()
        } );

        if ( !mDevice ) {
            MKT_THROW_RUNTIME_ERROR( "Could not create GPU Device." );
        }

        mDevice->Initialize();

        if ( !mDevice->IsInitialized() ) {
            MKT_THROW_RUNTIME_ERROR( "Could not initialize GPU Device." );
        }

        InitPresentation();
        InitSynchronization();

        InitRenderingObjects();

        return mDevice != nullptr;
    }

    auto Context::Shutdown() -> void {
        MKT_BEGIN_PROFILER();
        mDevice->WaitIdle();

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

        mFrameContexts.clear();

        mDevice->Shutdown();
        mDevice.reset();

        // Instance cleanup
        mInstance.reset();
    }

    auto Context::SetPresentTarget( renderer::rhi::TextureHandle texture ) -> void {
        MKT_BEGIN_PROFILER();
        mPresentTarget = texture;
    }

    auto Context::BatchSubmission( rhi::SubmitInfo&& submitInfo, rhi::QueueType queue ) -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mBatchedSubmissionMutex };

        // Preserve scheduler boundaries: graphics can release images to video, then wait for their decode.
        // Combining those graphics batches would make their wait depend on their own later signal.
        // https://docs.vulkan.org/spec/latest/chapters/synchronization.html#synchronization-semaphores
        mBatchedSubmissions.emplace_back( queue, eastl::move( submitInfo ) );
    }

    auto Context::SubmitFrame() -> void {
        MKT_BEGIN_PROFILER();

        auto& frame{ mFrameContexts[mCurrentFrameIndex] };

        // Submit batched commands
        decltype( mBatchedSubmissions ) submissions{};
        {
            std::lock_guard lock{ mBatchedSubmissionMutex };
            submissions.swap( mBatchedSubmissions );
        }

        for ( const auto& [queueType, submitInfo]: submissions ) {
            switch (queueType) {
                case QueueType::eVideoDecode:
                    mVideoDecodeQueue->ExecuteCommandLists( submitInfo );
                    break;
                default:
                    mGraphicsQueue->ExecuteCommandLists( submitInfo );
                    break;
            }
        }

        // https://community.khronos.org/t/is-it-recommended-to-use-vkcmdcopyimage-to-copy-to-the-swapchain-image-instead-of-a-shader/112122
        if ( !mPresentTarget.IsEmpty() ) {
#if true
            // Blit via full quad render
            renderer::rhi::TextureHandle colorImage{ mSwapchain->GetImage( mCurrentImageIndex ) };
            mCommandList->Open( { .mScopeName = "Blit SwapChain" } );
            mCommandList->SetTransition( renderer::rhi::TransitionDescription{}.AddTexture( mPresentTarget.GetPtr(), renderer::rhi::ResourceStates::eShaderResource ) );

            if ( frame.mBlitTarget != mPresentTarget ) {
                // PrepareFrame waited for this frame's fence, so its old descriptor is no longer in use.
                if ( frame.mTextureDescriptorIndex != rhi::kInvalidBindingItemIndex ) {
                    mDevice->ReleaseDescriptorTableIndex( mDescriptorTable, 0, frame.mTextureDescriptorIndex );
                    frame.mTextureDescriptorIndex = rhi::kInvalidBindingItemIndex;
                    frame.mBlitTarget.Reset();
                }

                frame.mTextureDescriptorIndex = mDevice->WriteDescriptorTable( mDescriptorTable,
                    renderer::rhi::BindingTableItem::TextureSRV( 0, mPresentTarget.GetPtr() ) );
                frame.mBlitTarget = mPresentTarget;
            }

            auto graphicsState{ renderer::rhi::RenderPassDescription{}
                .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( mSwapchain->GetWidth() ),
                        core::as<core::i32>( mSwapchain->GetHeight() ) } )
                .AddRenderTarget( colorImage, renderer::rhi::Color{ .0f } ) };

            mCommandList->BeginRenderPass( graphicsState );

            struct DrawParams {
                core::u32 mSamplerIndex{};
                core::u32 mTextureIndex{};
            } params{
                .mTextureIndex = core::as<core::u32>( frame.mTextureDescriptorIndex )
            };

            eastl::array<core::ubyte, kMaxPushConstantSize> ps{};
            std::memcpy( ps.data(), &params, sizeof( params ) );
            mCommandList->SetPushConstants( mPipelineLayoutHandle.GetPtr(), ps.data(),
                MKT_VECTOR_SIZE_BYTES( ps ), renderer::rhi::ShaderFlagsBits::All );

            mCommandList->BindPipeline( mPipeline.GetPtr() );
            mCommandList->BindPipelineResources( renderer::rhi::BindResourcesDescription{}
                .AddResourceSet( 0, mBindingSetHandle.GetPtr() )
                .AddResourceSet( 1, mDescriptorTable.GetPtr() )
                .SetPipelineLayout( mPipelineLayoutHandle.GetPtr() )
                .SetBindPoint( renderer::rhi::PipelineType::eGraphics ) );

            mCommandList->SetViewportState( renderer::rhi::ViewportState{}
                .AddViewportAndScissorRect( Viewport( core::as<core::f32>( mSwapchain->GetWidth() ), core::as<core::f32>( mSwapchain->GetHeight() ) ) ) );

            constexpr auto drawArguments{ renderer::rhi::DrawArguments{}
                .SetVertexCount( 3 ) };
            mCommandList->Draw( drawArguments );

            mCommandList->EndRenderPass();

            mCommandList->SetTransition( { colorImage.GetPtr(), renderer::rhi::ResourceStates::ePresent } );

            mCommandList->Close();
#else
            // Blit via copy command
            renderer::rhi::TextureHandle currentSwapChainImage{ mSwapchain->GetImage( mCurrentImageIndex ) };
            mCommandList->Open( { .mScopeName = "Blit SwapChain" } );

            const renderer::rhi::TextureSlice srcSlice{
                .mWidth = ( core::u32 )mPresentTarget->GetWidth(),
                    .mHeight = ( core::u32 )mPresentTarget->GetHeight(),
                        .mDepth = 1
            };

            const renderer::rhi::TextureSlice dstSlice{
                .mWidth = ( core::u32 )currentSwapChainImage->GetWidth(),
                    .mHeight = ( core::u32 )currentSwapChainImage->GetHeight(),
                        .mDepth = 1
            };

            mCommandList->Copy(
                mPresentTarget.GetPtr(), srcSlice,
                currentSwapChainImage.GetPtr(), dstSlice );

            mCommandList->SetTransition( { currentSwapChainImage.GetPtr(), renderer::rhi::ResourceStates::ePresent } );

            mCommandList->Close();
#endif
        }

        ++frame.mFenceValue;

        auto submitInfo{ SubmitSemaphoresInfo{}
            .AddCommandList( mCommandList )
            .AddSignalFence( frame.mFence, frame.mFenceValue, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT )
            .AddSignalSemaphore( mSwapchain->GetRenderFinishedSemaphore( mCurrentImageIndex ), VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT )
            .AddWaitSemaphore( frame.mImageAvailableSemaphore, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT )
        };

        mGraphicsQueue->ExecuteCommandLists( eastl::move( submitInfo ) );
    }

    auto Context::PrepareFrame() -> void {
        MKT_BEGIN_PROFILER();

        auto& frame{ mFrameContexts[mCurrentFrameIndex] };

        ( void )frame.mFence->Wait( frame.mFenceValue, eastl::numeric_limits<core::u64>::max() );
        mDevice->RunGarbageCollection();

        // No swapchain or minizmized window
        if ( mSwapchain.IsEmpty() || mWindow->GetWidth() <= 0 || mWindow->GetHeight() <= 0 ) {
            return;
        }

        auto ret{ mSwapchain->GetNextImageIndex( mCurrentImageIndex, frame.mImageAvailableSemaphore.GetPtr() ) };

        if ( ret == VK_ERROR_OUT_OF_DATE_KHR ) {
            if ( !mSwapchain->OnResize( mWindow->GetWidth(), mWindow->GetHeight() ) ) {
                return;
            }

            // After resizing current image might different one
            ret = mSwapchain->GetNextImageIndex( mCurrentImageIndex, frame.mImageAvailableSemaphore.GetPtr() );
        }

        // A second resize can race the retry. No image or acquire signal exists
        // on OUT_OF_DATE, so this frame must not submit/present swapchain work.
        if ( ret == VK_ERROR_OUT_OF_DATE_KHR ) {
            return;
        }

        if ( ret != VK_SUCCESS && ret != VK_SUBOPTIMAL_KHR ) {
            MKT_THROW_RUNTIME_ERROR( "Vulkan Context: Failed to acquire swapchain image." );
        }
    }

    auto Context::Update() -> void {
        // Nothing
        // [DEBUG] PrintStatsString shows allocations with friendly names amongts other other stuff
        // auto* allocator{ core::checked_cast<Device*>( mDevice.get() )->GetAllocator() };
        // allocator->PrintStatsString();
    }

    auto Context::SetRefreshRate( renderer::rhi::RefreshRate rate ) -> void {
        MKT_BEGIN_PROFILER();
        mRefreshRate = rate;
        mSwapchain->SetRefreshType( mRefreshRate );
    }

    auto Context::Present() -> void {
        MKT_BEGIN_PROFILER();

        const auto result{ mSwapchain->Present( mCurrentImageIndex ) };
        if ( result != VK_SUCCESS && result != VK_ERROR_OUT_OF_DATE_KHR && result != VK_SUBOPTIMAL_KHR ) {
            MKT_THROW_RUNTIME_ERROR( "Vulkan Context: Error failed present images to swapchain." );
        }

        if ( ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) &&
             mWindow->GetWidth() > 0 && mWindow->GetHeight() > 0 ) {
            ( void )mSwapchain->OnResize( mWindow->GetWidth(), mWindow->GetHeight() );
        }

        // Frame is advanced if we work with a swap chain
        mCurrentFrameIndex = ( mCurrentFrameIndex + 1 ) % mMaxFramesInFlight;
    }

    auto Context::GetMaxFramesInFlight() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mMaxFramesInFlight;
    }

    auto Context::GetCurrentImageIndex() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mCurrentImageIndex;
    }

    auto Context::GetCurrentFrameIndex() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mCurrentFrameIndex;
    }

    auto Context::GetInstance() -> Instance& {
        MKT_BEGIN_PROFILER();
        return const_cast<Instance&>( eastl::as_const( *this ).GetInstance() );
    }

    auto Context::GetInstance() const -> const Instance& {
        MKT_BEGIN_PROFILER();
        return *mInstance;
    }

    auto Context::GetSwapChain() -> VulkanSwapChainHandle {
        MKT_BEGIN_PROFILER();
        return mSwapchain;
    }

    auto Context::GetApiVersion() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mInstance->mApiVer;
    }

    auto Context::InitPresentation() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mWindow ) {
            return;
        }

        const SwapChainCreateInfo createInfo{
            .mPhysicalDevice = core::checked_cast<Device*>( mDevice.get() )->GetPhysicalDevice(),
            .mWidth = core::as<core::u32>( mWindow->GetWidth() ),
            .mHeight = core::as<core::u32>( mWindow->GetHeight() ),
            .mSurface = mInstance->mSurface,
            .mRefreshRate = mRefreshRate,
            .mFormat = renderer::rhi::Format::eBGRA8_UNORM
        };

        mSwapchain = core::checked_cast<Device*>( mDevice.get() )->CreateSwapChain( createInfo );
    }

    auto Context::InitRenderingObjects() -> void {
        MKT_BEGIN_PROFILER();

        // Create shaders
        filesystem::FileHandle vsShader{ filesystem::FileService::Get()->LoadFile( "Resources/Shaders/slang/SwapChainBlit_Vert.slang" ) };
        auto vertexShaderDescription{ renderer::rhi::ShaderModuleCreateDescription{}
            .SetContents( vsShader )
            .SetModuleName( vsShader->GetName() )
            .SetModulePath( vsShader->GetPath() )
            .SetLanguage( renderer::rhi::ShaderLanguage::eSlang )
            .SetStage( renderer::rhi::ShaderType::eVertex ) };
        mVertexShader = mDevice->CreateShader( vertexShaderDescription );

        filesystem::FileHandle pxShader{ filesystem::FileService::Get()->LoadFile( "Resources/Shaders/slang/SwapChainBlit_Frag.slang" ) };
        auto fragmentShaderDescription{ renderer::rhi::ShaderModuleCreateDescription{}
            .SetContents( pxShader )
            .SetModuleName( pxShader->GetName() )
            .SetModulePath( pxShader->GetPath() )
            .SetLanguage( renderer::rhi::ShaderLanguage::eSlang )
            .SetStage( renderer::rhi::ShaderType::ePixel ) };
        mPixelShader = mDevice->CreateShader( fragmentShaderDescription );

        auto layoutDesc{ renderer::rhi::BindingLayoutDescription{}
            .SetRegisterSpace( 0 )
            .SetShaderVisibility( renderer::rhi::ShaderFlagsBits::All )
            .AddItem( renderer::rhi::BindingLayoutItem::Sampler( 0 ) ) };
        mBindingLayoutHandle = mDevice->CreateBindingLayout( layoutDesc );

        auto bindlessLayout{ renderer::rhi::DescriptorTableLayoutDescription{}
            .SetVisibility( renderer::rhi::ShaderFlagsBits::All )
            .SetRegisterSpace( 1 )
            .AddBindlessItem( renderer::rhi::BindlessLayoutItem::TextureSRV( 0, mMaxFramesInFlight ) ) };
        mBindlessLayout = mDevice->CreateDescriptorTableLayout( bindlessLayout );

        mPipelineLayoutHandle = mDevice->CreatePipelineLayout( renderer::rhi::PipelineLayoutCreateDescription{}
            .AddBindingLayout( mBindingLayoutHandle )
            .AddBindingLayout( mBindlessLayout ) );

        auto graphicsPipelineDescription{ renderer::rhi::GraphicsPipelineDescription{}
            .AddShader( mPixelShader )
            .AddShader( mVertexShader )

            .AddColorFormat( renderer::rhi::Format::eBGRA8_UNORM )

            .SetPolygonMode( renderer::rhi::PolygonMode::eFill )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetWindingOrder( renderer::rhi::WindingOrder::eCounterClockwise )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetPipelineLayout( mPipelineLayoutHandle ) };

        mPipeline = mDevice->CreatePipeline( graphicsPipelineDescription );

        // Sampler
        auto samplerDes{ renderer::rhi::SamplerCreateDescription{}
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( renderer::rhi::SamplerWrapMode::eClampToBorder )
            .SetBorderColor( renderer::rhi::kColorWhite ) };
        mSamplerState = mDevice->CreateSampler( samplerDes );

        // Bindless set
        mDescriptorTable = mDevice->CreateDescriptorTable( mBindlessLayout );

        // Non-bindless set
        auto bindingSetDesc{ renderer::rhi::BindingTableDescription{}
            .AddItem( renderer::rhi::BindingTableItem::Sampler( 0, mSamplerState.GetPtr() ) ) };
        mBindingSetHandle = mDevice->CreateBindingTable( bindingSetDesc, mBindingLayoutHandle );

        mCommandList = mDevice->CreateCommandList( renderer::rhi::QueueType::eGraphics );
        mCommandList->SetDebugName( "Context Swapchain CommandBuffer" );
    }

    auto Context::InitSynchronization() -> void {
        MKT_BEGIN_PROFILER();
        Device* device{ core::checked_cast<Device*>( mDevice.get() ) };

        mFrameContexts.resize( mMaxFramesInFlight );

        for ( core::u32 frameIndex{ 0 }; auto& frame: mFrameContexts ) {
            // We do not increment the value here because in the first call to PrepareFrame() we will
            // wait for this value which will return immediately because there is nothing to wait for
            // which is essentially the same as a signaled vulkan fence; on the first call to SubmitFrame()
            // we first increment the value or the next batch of work.
            frame.mFence = mDevice->CreateFence( frame.mFenceValue );

            frame.mImageAvailableSemaphore = device->CreateBinarySemaphore();
            frame.mImageAvailableSemaphore->SetDebugName( string::Format( "SwapChain Img Avail. BinSemaphore frame {}", frameIndex ) );

            ++frameIndex;
        }

        mGraphicsQueue = core::checked_cast<Queue*>( device->GetQueue( renderer::rhi::QueueType::eGraphics ) );
        mVideoDecodeQueue = core::checked_cast<Queue*>( device->GetQueue( renderer::rhi::QueueType::eVideoDecode ) );
        mVideoEnCodeQueue = core::checked_cast<Queue*>( device->GetQueue( renderer::rhi::QueueType::eVideoEncode ) );
    }
}// namespace mikoto::renderer::vulkan
