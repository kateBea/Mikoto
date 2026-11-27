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

#include <chrono>
#include <future>
#include <memory>
#include <utility>
#include <vector>

#include <Core/String.hh>

#include <Logging/Logger.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/OidnModule.hh>
#include <Renderer/ForwardRenderer/Passes/RayTracingModule.hh>

#if MIKOTO_ENABLE_OIDN
#include <Renderer/Rhi/Vulkan/VulkanContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>

#include <vk_oidn/vk_oidn.h>
#endif

namespace mikoto::renderer {

    // https://github.com/nvpro-samples/vk_oidn/blob/main/docs/architecture.md
    
    using namespace rhi;

    struct OidnModule::State {
        eastl::string mError{};
        bool mReady{};
        bool mExecuted{};
        bool mHasResult{};
#if MIKOTO_ENABLE_OIDN
        struct Initialization {
            vkoidn::Result mResult{};
            std::unique_ptr<vkoidn::Context> mContext{};
        };

        // The future is declared last, so it joins before the SDK objects are
        // released. Initialization creates pipelines only; it never submits GPU work.
        std::unique_ptr<vkoidn::Context> mContext{};
        std::unique_ptr<vkoidn::Denoiser> mDenoiser{};
        FGTextureHandle mInput{};
        FGTextureHandle mOutput{};
        core::u32 mWidth{};
        core::u32 mHeight{};
        std::future<Initialization> mInitialization{};
#endif
    };

    OidnModule::OidnModule( RenderResolution resolution )
        : mState{ eastl::make_unique<State>() }, mResolution{ resolution } {
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

    auto OidnModule::Prepare( const RayTracingModule &rays ) -> void {
        auto &state{ *mState };
        state.mReady = false;
#if MIKOTO_ENABLE_OIDN
        if ( state.mInitialization.valid() &&
             state.mInitialization.wait_for( std::chrono::seconds{ 0 } ) == std::future_status::ready ) {
            auto initialization{ state.mInitialization.get() };
            if ( initialization.mResult != vkoidn::Result::eSuccess ) {
                state.mError = string::Format( "OIDN initialization failed: {}", vkoidn::getResultString( initialization.mResult ) );
                MKT_CORE_LOGGER_ERROR( "{}", state.mError );
            }
            state.mContext = std::move( initialization.mContext );
        }
        state.mReady = mSettings.mEnabled && state.mError.empty() && state.mContext && rays.IsReady();
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
        state.mInput = pathTracing ? graph.GetData<RayTracingInfo>().mDirect : graph.GetData<RayTracingInfo>().mIndirect;
        state.mOutput = graph.Create( FGTextureDescription{}
            .SetName( "OIDN_FilteredHDR" )
            .SetWidth( width )
            .SetHeight( height )
            .SetFormat( Format::eRGBA32_FLOAT )
            .SetUsage( TextureUsageFlagsBits::ShaderResource | TextureUsageFlagsBits::UnorderedAccess ) );
        graph.GetOrCreate<DenoiserModuleInfo>();

        std::vector<const char *> extensions{};
        for ( const auto &extension: vulkanDevice->GetEnabledExtensions() ) {
            extensions.push_back( extension.c_str() );
        }
        const VkInstance instance{ vulkan::Context::Get()->GetInstance().mInstance };
        const VkPhysicalDevice physical{ vulkanDevice->GetPhysicalDevice()->mPhysicalDevice };
        const VkDevice nativeDevice{ vulkanDevice->GetDevice() };
        state.mInitialization = std::async( std::launch::async,
            [instance, physical, nativeDevice, extensions = std::move( extensions )]() {
                State::Initialization result{};
                vkoidn::ContextCreateInfo info{};
                info.instance = instance;
                info.physicalDevice = physical;
                info.device = nativeDevice;
                info.getInstanceProcAddr = vkGetInstanceProcAddr;
                info.getDeviceProcAddr = vkGetDeviceProcAddr;
                info.enabledExtensions = extensions.data();
                info.enabledExtensionCount = static_cast<uint32_t>( extensions.size() );
                result.mResult = vkoidn::Context::create( info, result.mContext );
                return result;
            } );

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
                VkCommandBuffer commands{ context.GetCommandList()->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
                if ( !state.mDenoiser ) {
                    vkoidn::DenoiserCreateInfo info{};
                    info.width = state.mWidth;
                    info.height = state.mHeight;
                    info.hdr = true;
                    info.quality = static_cast<vkoidn::Quality>( mSettings.mOidnQuality );
                    // Filter the whole beauty signal. First-hit surface guides
                    // cannot describe refracted content, participating media or
                    // a sampled lens, so do not feed NRD's packed guides to OIDN.
                    const auto result{ vkoidn::Denoiser::create( *state.mContext, commands, info, state.mDenoiser ) };
                    if ( result != vkoidn::Result::eSuccess ) {
                        state.mError = string::Format( "OIDN network creation failed: {}", vkoidn::getResultString( result ) );
                        state.mReady = false;
                        return;
                    }
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
                context.Dispatch( ( parameters.mWidth + groupSide - 1 ) / groupSide,
                    ( parameters.mHeight + groupSide - 1 ) / groupSide, 1 );
                mState->mHasResult = true;
            } );
#else
        mState->mError = "OIDN is disabled in this build.";
#endif
    }
}
