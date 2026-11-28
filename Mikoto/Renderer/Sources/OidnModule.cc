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

#include <memory>
#include <vector>

#include <Core/String.hh>

#include <Logging/Logger.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/OidnModule.hh>
#include <Renderer/ForwardRenderer/Passes/RayTracingModule.hh>

#if MIKOTO_ENABLE_OIDN
#include <Renderer/Rhi/Vulkan/VulkanContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>

#include <vk_oidn/vk_oidn.h>
#endif

namespace mikoto::renderer {

    // https://github.com/nvpro-samples/vk_oidn/blob/main/docs/architecture.md
    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    // Retaining this device object in the native command list keeps SDK buffers,
    // pipelines and the upload command pool alive through the GPU submission.
    struct OidnModule::State final : DeviceObject {
        eastl::string mError{};
        bool mReady{};
        bool mExecuted{};
        bool mHasResult{};
        bool mUploadPending{};

#if MIKOTO_ENABLE_OIDN
        std::unique_ptr<vkoidn::Context> mContext{};
        std::unique_ptr<vkoidn::Denoiser> mDenoiser{};
        VkDevice mNativeDevice{};
        VkCommandPool mUploadPool{};
        VkCommandBuffer mUploadCommands{};
        FGTextureHandle mInput{};
        FGTextureHandle mOutput{};
        core::u32 mWidth{};
        core::u32 mHeight{};
        OidnQuality mQuality{};
#endif

        ~State() override;

        auto Initialize() -> void override;

        auto Destroy() -> void override;
    };

    OidnModule::State::~State() {
        Destroy();
    }

    auto OidnModule::State::Initialize() -> void {
#if MIKOTO_ENABLE_OIDN
        auto *device{ core::checked_cast<vulkan::Device *>( GetOwningDevice() ) };
        mNativeDevice = device->GetDevice();
        std::vector<const char *> extensions{};
        for ( const auto &extension: device->GetEnabledExtensions() ) {
            extensions.push_back( extension.c_str() );
        }

        vkoidn::ContextCreateInfo contextInfo{};
        contextInfo.instance = vulkan::Context::Get()->GetInstance().mInstance;
        contextInfo.physicalDevice = device->GetPhysicalDevice()->mPhysicalDevice;
        contextInfo.device = mNativeDevice;
        contextInfo.getInstanceProcAddr = vkGetInstanceProcAddr;
        contextInfo.getDeviceProcAddr = vkGetDeviceProcAddr;
        contextInfo.enabledExtensions = extensions.data();
        contextInfo.enabledExtensionCount = static_cast<uint32_t>( extensions.size() );
        auto result{ vkoidn::Context::create( contextInfo, mContext ) };
        if ( result != vkoidn::Result::eSuccess ) {
            mError = string::Format( "OIDN initialization failed: {}", vkoidn::getResultString( result ) );
            MKT_CORE_LOGGER_ERROR( "{}", mError );
            return;
        }

        // Record the weights on this worker's own pool. The first filter pass
        // executes this secondary buffer on Mikoto's graphics queue, before
        // any network dispatch; initialization itself submits no GPU work.
        const auto *queue{ core::checked_cast<vulkan::Queue *>( device->GetQueue( QueueType::eGraphics ) ) };

        VkCommandPoolCreateInfo pool{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
        pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pool.queueFamilyIndex = queue->GetFamilyIndex();
        MKT_VK_CHECK( vkCreateCommandPool( mNativeDevice, &pool, nullptr, &mUploadPool ) );

        VkCommandBufferAllocateInfo allocation{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        allocation.commandPool = mUploadPool;
        allocation.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        allocation.commandBufferCount = 1;
        MKT_VK_CHECK( vkAllocateCommandBuffers( mNativeDevice, &allocation, &mUploadCommands ) );

        VkCommandBufferInheritanceInfo inheritance{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO };
        VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        begin.pInheritanceInfo = &inheritance;
        MKT_VK_CHECK( vkBeginCommandBuffer( mUploadCommands, &begin ) );

        // Denoiser::create compiles the network kernels and allocates scratch
        // buffers. Keep the entire operation on the graph compilation worker,
        // rather than deferring this expensive part to the first render pass.
        vkoidn::DenoiserCreateInfo network{};
        network.width = mWidth;
        network.height = mHeight;
        network.hdr = true;
        network.quality = static_cast<vkoidn::Quality>( mQuality );
        result = vkoidn::Denoiser::create( *mContext, mUploadCommands, network, mDenoiser );
        MKT_VK_CHECK( vkEndCommandBuffer( mUploadCommands ) );
        
        if ( result != vkoidn::Result::eSuccess ) {
            mError = string::Format( "OIDN network creation failed: {}", vkoidn::getResultString( result ) );
            MKT_CORE_LOGGER_ERROR( "{}", mError );
            return;
        }

        mUploadPending = true;
        mIsAllocated = true;
#endif
    }

    auto OidnModule::State::Destroy() -> void {
#if MIKOTO_ENABLE_OIDN
        mDenoiser.reset();
        mContext.reset();

        if ( mUploadPool ) {
            vkDestroyCommandPool( mNativeDevice, mUploadPool, nullptr );
            mUploadPool = VK_NULL_HANDLE;
        }

        mIsAllocated = false;
#endif
    }

    OidnModule::OidnModule( RenderResolution resolution )
        : mState{ core::Ref<State>::New() }, mResolution{ resolution } {
    }

    OidnModule::~OidnModule() = default;

    auto OidnModule::SetSettings( DenoiserSettings settings ) -> void {
        mSettings = settings;
    }

    auto OidnModule::IsActive() const -> bool {
        return mSettings.mEnabled && mState->mReady && mState->mHasResult;
    }

    auto OidnModule::GetError() const -> const eastl::string & {
        return mState->mError;
    }

    auto OidnModule::Initialize( IGpuDevice *device ) -> void {
#if MIKOTO_ENABLE_OIDN
        if ( mSettings.mEnabled && mState->mError.empty() && mState->mOutput.mHandle ) {
            mState->DeviceObject::Initialize( device );
        }
#endif
    }

    auto OidnModule::Prepare( const RayTracingModule &rays ) -> void {
        mState->mReady = false;

#if MIKOTO_ENABLE_OIDN
        mState->mReady = mSettings.mEnabled && mState->mError.empty() && mState->mDenoiser && rays.IsReady();
#endif
    }

    auto OidnModule::RegisterPasses( FrameGraph &graph, IGpuDevice *device, bool pathTracing ) -> void {
        if ( !mSettings.mEnabled ) {
            return;
        }

#if MIKOTO_ENABLE_OIDN
        if ( !device->IsGraphicsApi( GraphicsAPI::eVulkan ) ) {
            mState->mError = "OIDN currently requires the Vulkan renderer.";
            return;
        }

        auto *vulkanDevice{ core::checked_cast<vulkan::Device *>( device ) };
        if ( !vulkanDevice->SupportsOidn() ) {
            mState->mError = "OIDN requires FP16 storage/compute, Vulkan memory-model features and push descriptors.";
            return;
        }

        auto &state{ *mState };
        const auto [width, height]{ InferDimensions( mResolution ) };
        state.mWidth = static_cast<core::u32>( width );
        state.mHeight = static_cast<core::u32>( height );
        state.mQuality = mSettings.mOidnQuality;
        state.mInput = pathTracing ? graph.GetData<RayTracingInfo>().mDirect : graph.GetData<RayTracingInfo>().mIndirect;

        state.mOutput = graph.Create( FGTextureDescription{}
            .SetName( "OIDN_FilteredHDR" )
            .SetWidth( width )
            .SetHeight( height )
            .SetFormat( Format::eRGBA32_FLOAT )
            .SetUsage( TextureUsageFlagsBits::ShaderResource | 
                TextureUsageFlagsBits::UnorderedAccess ) );

        // Storage-image input keeps this SDK pass entirely on the GPU. Declaring
        // UAV access places both images in GENERAL and orders it after transport
        // history has been saved. The SDK owns its internal scratch barriers.
        graph.RegisterPass(
            "OIDN_Filter",
            FGPassType::eCompute,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( mState->mInput, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                builder.UseResource( mState->mOutput, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &context, core::Blackboard &blackboard ) {
                auto &state{ *mState };
                state.mExecuted = false;
                if ( !state.mReady || !blackboard.Get<RayTracingInfo>().mTraceOutputsReady ) {
                    return;
                }
                auto *native{ core::checked_cast<vulkan::CommandList *>( context.GetCommandList() ) };
                native->RetainResource( mState.GetPtr() );
                VkCommandBuffer commands{ native->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
                if ( state.mUploadPending ) {
                    vkCmdExecuteCommands( commands, 1, &state.mUploadCommands );
                    state.mUploadPending = false;
                }

                vkoidn::DenoiseInputs inputs{};
                inputs.colorView.view = context.GetTexture( state.mInput )->GetNativeHandle( ObjectType::Vk_ImageView );
                inputs.outputView.view = context.GetTexture( state.mOutput )->GetNativeHandle( ObjectType::Vk_ImageView );
                inputs.outputView.format = VK_FORMAT_R32G32B32A32_SFLOAT;
                const auto result{ state.mDenoiser->cmdDenoise( commands, inputs ) };
                state.mExecuted = result == vkoidn::Result::eSuccess;
                if ( !state.mExecuted ) {
                    state.mError = string::Format( "OIDN filtering failed: {}", vkoidn::getResultString( result ) );
                    state.mReady = false;
                }
            } );

        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "OIDN_CompositePipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "Oidn_Composite.slang", FGStageType::eCompute ) ) };

        graph.RegisterPass(
            "OIDN_Composite",
            FGPassType::eCompute,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( mState->mOutput, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( mState->mInput, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext &context, core::Blackboard & ) {
                if ( !mState->mExecuted ) {
                    return;
                }

                struct Parameters {
                    core::u32 mFiltered{};
                    core::u32 mOutput{};
                    core::u32 mWidth{};
                    core::u32 mHeight{};
                    core::f32 mSplit{};
                } parameters{
                    .mFiltered = context.PushTexture_SRV( mState->mOutput ),
                    .mOutput = context.PushTexture_UAV( mState->mInput ),
                    .mWidth = mState->mWidth,
                    .mHeight = mState->mHeight,
                    .mSplit = mSettings.mSplitScreen,
                };

                // Rebind Mikoto's pipeline, descriptor table and push constants
                // after the SDK's native command recording changed those bindings.
                context.BindPipeline( pipeline );
                context.PushConstants( parameters );

                constexpr core::u32 groupSide{ 8 };

                const u32 x{ ( parameters.mWidth + groupSide - 1 ) / groupSide };
                const u32 y{ ( parameters.mHeight + groupSide - 1 ) / groupSide };
                const u32 z{ 1u };

                context.Dispatch( x, y, z );

                mState->mHasResult = true;
            } );
#else
        mState->mError = "OIDN is disabled in this build.";
#endif
    }
}
