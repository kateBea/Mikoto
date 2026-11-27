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

#include <algorithm>
#include <cmath>
#include <cstring>

#include <Core/Exception.hh>
#include <Core/String.hh>

#include <Math/Math.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/DenoiserModule.hh>
#include <Renderer/ForwardRenderer/Passes/OidnModule.hh>

#if MIKOTO_ENABLE_NRD
#include <NRD.h>
#endif

namespace mikoto::renderer {
    using namespace rhi;

    auto DenoiserSettings::Sanitize() -> void {
        if ( mBackend != DenoiserBackend::eNrd && mBackend != DenoiserBackend::eOidn ) {
            mBackend = DenoiserBackend::eOidn;
        }
        if ( static_cast<core::u32>( mOidnQuality ) > static_cast<core::u32>( OidnQuality::eHigh ) ) {
            mOidnQuality = OidnQuality::eHigh;
        }
        if ( mType != DenoiserType::eReblur && mType != DenoiserType::eRelax ) {
            mType = DenoiserType::eReblur;
        }
        mHistoryFrames = glm::clamp( mHistoryFrames, 1u, 63u );
        mFastHistoryFrames = glm::clamp( mFastHistoryFrames, 1u, mHistoryFrames );
        mAtrousIterations = glm::clamp( mAtrousIterations, 2u, 8u );
        mDisocclusionThreshold = glm::clamp( mDisocclusionThreshold, .001f, .1f );
        mSplitScreen = glm::clamp( mSplitScreen, 0.0f, 1.0f );
    }

    struct DenoiserModule::State {
#if MIKOTO_ENABLE_NRD
        struct Pipeline {
            PipelineHandle mPipeline{};
            BindingLayoutHandle mResources{};
        };

        struct Dispatch {
            core::u32 mPipeline{};
            core::u32 mX{};
            core::u32 mY{};
            eastl::vector<ExternalComputeTexture> mTextures{};
            eastl::vector<core::ubyte> mConstants{};
        };

        // The SDK describes dispatches; Mikoto owns their GPU resources.
        nrd::Instance *mInstance{};
        IGpuDevice *mDevice{};
        DenoiserInputs mInputs{};
        eastl::vector<FGTextureHandle> mPermanent{};
        eastl::vector<FGTextureHandle> mTransient{};
        eastl::vector<Pipeline> mPipelines{};
        eastl::vector<FGBufferHandle> mConstants{};
        eastl::vector<Dispatch> mDispatches{};
        BindingLayoutHandle mConstantLayout{};
        eastl::array<SamplerHandle, 2> mSamplers{};
        FGTextureHandle mDiffuse{};
        FGTextureHandle mSpecular{};
        FGTextureHandle mValidation{};
        FGTextureHandle mConfidence{};

        // Reprojection history belongs to this view and resets on camera cuts.
        core::float4x4 mPreviousView{ 1 };
        core::float4x4 mPreviousProjection{ 1 };
        core::float3 mPreviousPosition{};
        core::u64 mTopologyRevision{};
        core::u64 mResetRevision{};
        core::u64 mLightingRevision{};
        core::u32 mReactiveFrames{};
        core::f32 mHistoryConfidence{ 1.0f };
        const scene::Scene *mScene{};
        bool mRestart{ true };
        bool mReady{};
        core::u32 mConstantSlot{};
        core::u32 mSamplerSlot{};
        core::u32 mWidth{};
        core::u32 mHeight{};

        ~State() {
            if ( mInstance ) {
                nrd::DestroyInstance( *mInstance );
            }
        }
#endif
    };

    DenoiserModule::DenoiserModule( RenderResolution resolution ) : mState{ eastl::make_unique<State>() }, mOidn{ eastl::make_unique<OidnModule>( resolution ) }, mResolution{ resolution } {
    }

    DenoiserModule::~DenoiserModule() = default;

    auto DenoiserModule::SetSettings( DenoiserSettings settings ) -> void {
        settings.Sanitize();
        mSettings = settings;
        mOidn->SetSettings( settings );
    }

    auto DenoiserModule::GetError() const -> const eastl::string & {
        static const eastl::string empty{};
        return mSettings.mBackend == DenoiserBackend::eOidn ? mOidn->GetError() : empty;
    }

    auto DenoiserModule::IsActive() const -> bool {
        if ( mSettings.mBackend == DenoiserBackend::eOidn ) {
            return mOidn->IsActive();
        }
#if MIKOTO_ENABLE_NRD
        return mSettings.mEnabled && mState->mReady;
#else
        return false;
#endif
    }

#if MIKOTO_ENABLE_NRD
    static auto NrdFormat( nrd::Format format ) -> Format {
        switch ( format ) {
            case nrd::Format::R8_UNORM:
                return Format::eR8_UNORM;
            case nrd::Format::R8_SNORM:
                return Format::eR8_SNORM;
            case nrd::Format::R8_UINT:
                return Format::eR8_UINT;
            case nrd::Format::RG8_UNORM:
                return Format::eRG8_UNORM;
            case nrd::Format::RGBA8_UNORM:
                return Format::eRGBA8_UNORM;
            case nrd::Format::R16_SFLOAT:
                return Format::eR16_FLOAT;
            case nrd::Format::R16_UNORM:
                return Format::eR16_UNORM;
            case nrd::Format::R16_UINT:
                return Format::eR16_UINT;
            case nrd::Format::RG16_SFLOAT:
                return Format::eRG16_FLOAT;
            case nrd::Format::RG16_SNORM:
                return Format::eRG16_SNORM;
            case nrd::Format::RG16_UNORM:
                return Format::eRG16_UNORM;
            case nrd::Format::RGBA16_SFLOAT:
                return Format::eRGBA16_FLOAT;
            case nrd::Format::RGBA16_SNORM:
                return Format::eRGBA16_SNORM;
            case nrd::Format::R32_SFLOAT:
                return Format::eR32_FLOAT;
            case nrd::Format::R32_UINT:
                return Format::eR32_UINT;
            case nrd::Format::RG32_SFLOAT:
                return Format::eRG32_FLOAT;
            case nrd::Format::RGBA32_SFLOAT:
                return Format::eRGBA32_FLOAT;
            case nrd::Format::R10_G10_B10_A2_UNORM:
                return Format::eR10G10B10A2_UNORM;
            case nrd::Format::R11_G11_B10_UFLOAT:
                return Format::eR11G11B10_FLOAT;
            default:
                MKT_THROW_RUNTIME_ERROR( "NRD requested an unsupported texture format." );
        }
    }

    static auto NrdCheck( nrd::Result result ) -> void {
        if ( result != nrd::Result::SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "NRD rejected its configuration or frame inputs." );
        }
    }
#endif

    auto DenoiserModule::RegisterPasses( FrameGraph &graph, IGpuDevice *device, bool pathTracing ) -> void {
        if ( mSettings.mBackend == DenoiserBackend::eOidn ) {
            mOidn->RegisterPasses( graph, device, pathTracing );
            return;
        }
#if MIKOTO_ENABLE_NRD
        if ( !mSettings.mEnabled ) {
            return;
        }
        auto &state{ *mState };
        state.mDevice = device;
        state.mInputs = graph.GetData<RayTracingInfo>().mDenoiser;
        if ( !state.mInputs.mDiffuse.mHandle ) {
            return;
        }

        // Match the selected NRD method to its native pipeline descriptions.
        const nrd::DenoiserDesc denoiser{ 0, mSettings.mType == DenoiserType::eReblur ? nrd::Denoiser::REBLUR_DIFFUSE_SPECULAR : nrd::Denoiser::RELAX_DIFFUSE_SPECULAR };
        const nrd::InstanceCreationDesc create{ {}, &denoiser, 1 };
        NrdCheck( nrd::CreateInstance( create, state.mInstance ) );
        const auto &instance{ *nrd::GetInstanceDesc( *state.mInstance ) };
        const auto &offsets{ nrd::GetLibraryDesc()->spirvBindingOffsets };
        const bool vulkan{ device->GetGraphicsApi() == GraphicsAPI::eVulkan };
        state.mConstantSlot = vulkan ? offsets.constantBufferOffset : 0;
        state.mSamplerSlot = vulkan ? offsets.samplerOffset : 0;

        // Outputs and both NRD texture pools use the current view resolution.
        const auto [width, height]{ InferDimensions( mResolution ) };
        state.mWidth = static_cast<core::u32>( width );
        state.mHeight = static_cast<core::u32>( height );
        const auto texture{ [&]( const char *name, Format format, core::u32 downsample = 1 ) {
            return graph.Create( FGTextureDescription{}
                .SetName( name )
                .SetWidth( ( width + downsample - 1 ) / downsample )
                .SetHeight( ( height + downsample - 1 ) / downsample )
                .SetFormat( format )
                .SetUsage( TextureUsageFlagsBits::ShaderResource | TextureUsageFlagsBits::UnorderedAccess ) );
        } };

        state.mDiffuse = texture( "Denoiser_Diffuse", Format::eRGBA16_FLOAT );
        state.mSpecular = texture( "Denoiser_Specular", Format::eRGBA16_FLOAT );
        state.mValidation = texture( "Denoiser_Validation", Format::eRGBA8_UNORM );
        graph.GetOrCreate<DenoiserModuleInfo>().mValidation = state.mValidation;
        state.mConfidence = texture( "Denoiser_Confidence", Format::eR8_UNORM );

        for ( core::u32 i{}; i < instance.permanentPoolSize; ++i ) {
            const auto &t{ instance.permanentPool[i] };
            state.mPermanent.push_back( texture( string::Format( "Denoiser_History_{}", i ).c_str(), NrdFormat( t.format ), t.downsampleFactor ) );
        }

        for ( core::u32 i{}; i < instance.transientPoolSize; ++i ) {
            const auto &t{ instance.transientPool[i] };
            state.mTransient.push_back( texture( string::Format( "Denoiser_Scratch_{}", i ).c_str(), NrdFormat( t.format ), t.downsampleFactor ) );
        }

        // Constants and samplers occupy a separate descriptor space from images.
        BindingLayoutDescription constants{};
        constants.SetRegisterSpace( instance.constantBufferAndSamplersSpaceIndex )
            .SetShaderVisibility( ShaderFlagsBits::Compute );
        constants.AddItem( BindingLayoutItem::ConstantBuffer( state.mConstantSlot ) );
        for ( core::u32 i{}; i < 2; ++i ) {
            constants.AddItem( BindingLayoutItem::Sampler( state.mSamplerSlot + i ) );
        }

        state.mConstantLayout = device->CreateBindingLayout( constants );
        for ( core::u32 i{}; i < 2; ++i ) {
            state.mSamplers[i] = device->CreateSampler( SamplerCreateDescription{}
                .SetFilter( i == 0 ? SamplerFilter::eNearest : SamplerFilter::eLinear )
                .SetWrap( SamplerWrapMode::eClampToEdge ) );
        }

        // Use the original SDK bytecode with the active RHI binding offsets.
        for ( core::u32 i{}; i < instance.pipelinesNum; ++i ) {
            const auto &source{ instance.pipelines[i] };
            BindingLayoutDescription bindings{};
            bindings.SetRegisterSpace( instance.resourcesSpaceIndex )
                .SetShaderVisibility( ShaderFlagsBits::Compute );
            for ( core::u32 range{}; range < source.resourceRangesNum; ++range ) {
                const auto &r{ source.resourceRanges[range] };
                for ( core::u32 j{}; j < r.descriptorsNum; ++j ) {
                    BindingLayoutItem item{};
                    item.mSlot = j + ( r.descriptorType == nrd::DescriptorType::TEXTURE ? offsets.textureOffset : offsets.storageTextureAndBufferOffset );
                    item.mType = r.descriptorType == nrd::DescriptorType::TEXTURE ? ResourceType::eTexture_SRV : ResourceType::eTexture_UAV;
                    bindings.AddItem( item );
                }
            }
            State::Pipeline pipeline{};
            pipeline.mResources = device->CreateBindingLayout( bindings );
            auto layout{ device->CreatePipelineLayout( PipelineLayoutCreateDescription{}
                .SetPushConstantSize( 0 )
                .AddBindingLayout( pipeline.mResources )
                .AddBindingLayout( state.mConstantLayout ) ) };
            const auto &binary{ vulkan ? source.computeShaderSPIRV : source.computeShaderDXIL };
            if ( !binary.bytecode || !binary.size ) {
                MKT_THROW_RUNTIME_ERROR( "NRD shader binaries are missing for the active graphics API." );
            }
            auto shader{ device->CreateShader( ShaderModuleCreateDescription{}
                .SetStage( ShaderType::eCompute )
                .SetLanguage( vulkan ? ShaderLanguage::eSPIRV : ShaderLanguage::eDXIL )
                .SetEntryPoint( instance.shaderEntryPoint )
                .SetContents( const_cast<void *>( binary.bytecode ), binary.size ) ) };
            pipeline.mPipeline = device->CreatePipeline( ComputePipelineDescription{}
                .SetPipelineLayout( layout )
                .SetComputeStage( shader ) );
            state.mPipelines.push_back( eastl::move( pipeline ) );
        }

        // Each dispatch gets its own graph-uploaded constants, so neither dispatches nor
        // frames in flight overwrite a uniform buffer still being consumed by the GPU.
        const auto capacity{ glm::max( 128u, instance.descriptorPoolDesc.setsMaxNum ) };
        const auto constantBytes{ ( instance.constantBufferMaxDataSize + 255u ) & ~255u };
        for ( core::u32 i{}; i < capacity; ++i ) {
            state.mConstants.push_back( graph.Create( FGBufferDescription{}
                .SetName( string::Format( "Denoiser_Constants_{}", i ) )
                .SetSizeBytes( constantBytes )
                .SetHeapType( HeapType::eDeviceLocal )
                .SetUsage( BufferUsageFlagsBits::Constant | BufferUsageFlagsBits::CopyDest ) ) );
        }

        // Upload only the constants requested by this frame's dispatch recipe.
        graph.RegisterPass(
            "Denoiser_Upload",
            FGPassType::eTransfer,
            [this]( FGNodeBuilder &b, core::Blackboard & ) {
                for ( auto c: mState->mConstants ) {
                    b.UseResource( c, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                }
            },
            [this]( CommandContext &ctx, core::Blackboard & ) {
                for ( core::u32 i{}; i < mState->mDispatches.size(); ++i ) {
                    const auto &bytes{ mState->mDispatches[i].mConstants };
                    if ( !bytes.empty() ) {
                        ctx.CopyBuffer( mState->mConstants[i], 0, bytes.data(), bytes.size() );
                    }
                }
            } );

        // Native confidence inputs make animated emissive lighting responsive.
        const auto confidencePipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Denoiser_ConfidencePipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "Denoiser_Confidence.slang", FGStageType::eCompute ) ) };
        graph.RegisterPass(
            "Denoiser_Confidence",
            FGPassType::eCompute,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( mState->mConfidence, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this, confidencePipeline]( CommandContext &context, core::Blackboard &blackboard ) {
                if ( !mState->mReady || !blackboard.Get<RayTracingInfo>().mTraceOutputsReady ) {
                    return;
                }
                struct Parameters {
                    core::u32 mOutput{};
                    core::u32 mWidth{};
                    core::u32 mHeight{};
                    core::f32 mConfidence{};
                } parameters{};
                parameters.mOutput = context.PushTexture_UAV( mState->mConfidence );
                parameters.mWidth = mState->mWidth;
                parameters.mHeight = mState->mHeight;
                parameters.mConfidence = mState->mHistoryConfidence;
                context.BindPipeline( confidencePipeline );
                context.PushConstants( parameters );
                context.Dispatch( ( parameters.mWidth + 7 ) / 8, ( parameters.mHeight + 7 ) / 8, 1 );
            } );

        // Filtering owns the internal read/write transitions between SDK dispatches.
        graph.RegisterPass(
            "Denoiser_Filter",
            FGPassType::eCompute,
            [this]( FGNodeBuilder &b, core::Blackboard & ) {
                for ( auto c: mState->mConstants ) {
                    b.UseResource( c, FGPipelineStage::eConstantBuffer, FGResourceAccess::eRead );
                }
                for ( auto t: mState->mInputs.Textures() ) {
                    if ( t.mHandle != mState->mInputs.mMotion.mHandle ) {
                        b.UseResource( t, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                    }
                }
                b.UseResource( mState->mConfidence, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                // REBLUR patches IN_MV in place. Keep a dedicated motion image in
                // UAV state, with scoped SRV transitions for dispatches that read it.
                b.UseResource( mState->mInputs.mMotion, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                for ( auto t: mState->mPermanent ) {
                    b.UseResource( t, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                }
                for ( auto t: mState->mTransient ) {
                    b.UseResource( t, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                }
                for ( auto t: { mState->mDiffuse, mState->mSpecular, mState->mValidation } ) {
                    b.UseResource( t, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                }
            },
            [this]( CommandContext &ctx, core::Blackboard &b ) {
                auto &outputs{ b.Get<DenoiserModuleInfo>() };
                outputs.mValidationReady = false;
                if ( !mState->mReady || !b.Get<RayTracingInfo>().mTraceOutputsReady ) {
                    mState->mRestart = true;
                    mState->mReady = false;
                    return;
                }
                for ( core::u32 i{}; i < mState->mDispatches.size(); ++i ) {
                    const auto &d{ mState->mDispatches[i] };
                    const auto &p{ mState->mPipelines[d.mPipeline] };
                    ctx.DispatchExternalCompute( { mState->mDevice, p.mPipeline, p.mResources, mState->mConstantLayout,
                        mState->mConstants[i], mState->mConstantSlot, d.mTextures, mState->mSamplers,
                        mState->mSamplerSlot, { d.mX, d.mY, 1 } } );
                }
                outputs.mValidationReady = mSettings.mValidation;
            } );

        // Restore material factors and combine filtered lobes with the base signal.
        auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "Denoiser_CompositePipeline" )
            .SetPipelineType( PipelineType::eCompute )
            .PushShader( "Denoiser_Composite.slang", FGStageType::eCompute ) ) };
        const auto target{ pathTracing ? graph.GetData<RayTracingInfo>().mDirect : graph.GetData<RayTracingInfo>().mIndirect };
        graph.RegisterPass(
            "Denoiser_Composite",
            FGPassType::eCompute,
            [this, target]( FGNodeBuilder &b, core::Blackboard & ) {
                for ( auto t: { mState->mDiffuse, mState->mSpecular, mState->mInputs.mBase,
                          mState->mInputs.mDiffuseFactor, mState->mInputs.mSpecularFactor } ) {
                    b.UseResource( t, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                }
                b.UseResource( target, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this, target, pipeline]( CommandContext &ctx, core::Blackboard &b ) {
                if ( !mState->mReady || !b.Get<RayTracingInfo>().mTraceOutputsReady ) {
                    mState->mRestart = true;
                    mState->mReady = false;
                    return;
                }
                struct Parameters {
                    core::uint4 mInputs{};
                    core::u32 mBase{};
                    core::u32 mOutput{};
                    core::u32 mWidth{};
                    core::u32 mHeight{};
                    core::u32 mMethod{};
                    core::uint3 mPadding{};
                } p{};
                p.mInputs = { ctx.PushTexture_SRV( mState->mDiffuse ), ctx.PushTexture_SRV( mState->mSpecular ),
                    ctx.PushTexture_SRV( mState->mInputs.mDiffuseFactor ), ctx.PushTexture_SRV( mState->mInputs.mSpecularFactor ) };
                p.mBase = ctx.PushTexture_SRV( mState->mInputs.mBase );
                p.mOutput = ctx.PushTexture_UAV( target );
                p.mMethod = static_cast<core::u32>( mSettings.mType );
                p.mWidth = mState->mWidth;
                p.mHeight = mState->mHeight;
                ctx.BindPipeline( pipeline );
                ctx.PushConstants( p );
                ctx.Dispatch( ( p.mWidth + 7 ) / 8, ( p.mHeight + 7 ) / 8, 1 );
            } );
#endif
    }

    auto DenoiserModule::Prepare( const scene::Scene *scene, const scene::Camera *camera, const RayTracingModule &rays ) -> void {
        if ( mSettings.mBackend == DenoiserBackend::eOidn ) {
            mOidn->Prepare( rays );
            return;
        }
#if MIKOTO_ENABLE_NRD
        auto &state{ *mState };
        state.mReady = false;
        state.mDispatches.clear();
        if ( !state.mInstance || !mSettings.mEnabled || !camera || !rays.IsReady() ) {
            state.mRestart = true;
            return;
        }

        // Current and previous camera transforms define world-space reprojection.
        nrd::CommonSettings common{};
        const auto view{ camera->GetViewMatrix() };
        const auto projection{ camera->GetProjection() };
        const bool cut{ state.mRestart || state.mScene != scene || state.mTopologyRevision != rays.GetTopologyRevision() ||
                        state.mResetRevision != mSettings.mResetRevision || glm::distance( state.mPreviousPosition, camera->GetPosition() ) > 10.0f ||
                        glm::length( core::float4{ projection[0] - state.mPreviousProjection[0] } ) > .1f ||
                        glm::dot( core::float3{ glm::inverse( view )[2] }, core::float3{ glm::inverse( state.mPreviousView )[2] } ) < .5f };

        std::memcpy( common.worldToViewMatrix, &view, sizeof( view ) );
        std::memcpy( common.viewToClipMatrix, &projection, sizeof( projection ) );
        std::memcpy( common.worldToViewMatrixPrev, cut ? &view : &state.mPreviousView, sizeof( view ) );
        std::memcpy( common.viewToClipMatrixPrev, cut ? &projection : &state.mPreviousProjection, sizeof( projection ) );
        common.resourceSize[0] = common.resourceSizePrev[0] = common.rectSize[0] = common.rectSizePrev[0] = static_cast<uint16_t>( state.mWidth );
        common.resourceSize[1] = common.resourceSizePrev[1] = common.rectSize[1] = common.rectSizePrev[1] = static_cast<uint16_t>( state.mHeight );
        common.motionVectorScale[0] = common.motionVectorScale[1] = common.motionVectorScale[2] = 1;
        common.isMotionVectorInWorldSpace = true;
        common.frameIndex = rays.GetTraceFrameIndex();
        common.denoisingRange = 500000;
        common.accumulationMode = cut ? nrd::AccumulationMode::CLEAR_AND_RESTART : nrd::AccumulationMode::CONTINUE;
        common.disocclusionThreshold = mSettings.mDisocclusionThreshold;
        common.splitScreen = mSettings.mSplitScreen;
        common.enableValidation = mSettings.mValidation;
        common.isHistoryConfidenceAvailable = true;

        if ( mSettings.mResponsiveHistory && !cut && state.mLightingRevision != rays.GetLightingRevision() ) {
            state.mReactiveFrames = 3;
        }
        else if ( state.mReactiveFrames ) {
            --state.mReactiveFrames;
        }
        if ( !mSettings.mResponsiveHistory || cut ) {
            state.mReactiveFrames = 0;
        }
        state.mHistoryConfidence = state.mReactiveFrames ? 0.15f : 1.0f;
        const auto historyFrames{ state.mReactiveFrames ? glm::min( mSettings.mHistoryFrames, 8u ) : mSettings.mHistoryFrames };
        const auto fastHistoryFrames{ glm::min( mSettings.mFastHistoryFrames, historyFrames ) };
        NrdCheck( nrd::SetCommonSettings( *state.mInstance, common ) );

        // History and spatial filtering controls differ between REBLUR and RELAX.
        if ( mSettings.mType == DenoiserType::eReblur ) {
            nrd::ReblurSettings settings{};
            settings.diffusePrepassBlurRadius = 12.0f;
            settings.specularPrepassBlurRadius = 16.0f;
            settings.maxBlurRadius = 16.0f;
            settings.convergenceSettings.b = state.mReactiveFrames ? 1.0f : 0.2f;
            settings.maxAccumulatedFrameNum = historyFrames;
            settings.maxFastAccumulatedFrameNum = fastHistoryFrames;
            settings.historyFixFrameNum = glm::min( 3u, fastHistoryFrames - 1 );
            settings.hitDistanceReconstructionMode = nrd::HitDistanceReconstructionMode::AREA_3X3;
            settings.enableAntiFirefly = mSettings.mAntiFirefly;
            NrdCheck( nrd::SetDenoiserSettings( *state.mInstance, 0, &settings ) );
        }
        else {
            nrd::RelaxSettings settings{};
            settings.diffuseMaxAccumulatedFrameNum = settings.specularMaxAccumulatedFrameNum = historyFrames;
            settings.diffuseMaxFastAccumulatedFrameNum = settings.specularMaxFastAccumulatedFrameNum = fastHistoryFrames;
            settings.historyFixFrameNum = glm::min( 3u, fastHistoryFrames - 1 );
            settings.atrousIterationNum = mSettings.mAtrousIterations;
            settings.hitDistanceReconstructionMode = nrd::HitDistanceReconstructionMode::AREA_3X3;
            settings.enableAntiFirefly = mSettings.mAntiFirefly;
            NrdCheck( nrd::SetDenoiserSettings( *state.mInstance, 0, &settings ) );
        }

        // Resolve SDK semantic resources into frame-graph handles for this frame.
        const nrd::DispatchDesc *dispatches{};
        uint32_t count{};
        const nrd::Identifier identifier{ 0 };
        NrdCheck( nrd::GetComputeDispatches( *state.mInstance, &identifier, 1, dispatches, count ) );
        if ( count > state.mConstants.size() ) {
            MKT_THROW_RUNTIME_ERROR( "NRD exceeded its advertised dispatch budget." );
        }
        const auto &offsets{ nrd::GetLibraryDesc()->spirvBindingOffsets };
        for ( core::u32 i{}; i < count; ++i ) {
            const auto &source{ dispatches[i] };
            State::Dispatch d{};
            d.mPipeline = source.pipelineIndex;
            d.mX = source.gridWidth;
            d.mY = source.gridHeight;
            if ( source.constantBufferDataSize ) {
                d.mConstants.assign( source.constantBufferData, source.constantBufferData + source.constantBufferDataSize );
            }
            core::u32 srv{};
            core::u32 uav{};
            for ( core::u32 j{}; j < source.resourcesNum; ++j ) {
                const auto &r{ source.resources[j] };
                FGTextureHandle texture{};
                bool internal{};
                switch ( r.type ) {
                    case nrd::ResourceType::IN_DIFF_CONFIDENCE:
                    case nrd::ResourceType::IN_SPEC_CONFIDENCE:
                        texture = state.mConfidence;
                        break;
                    case nrd::ResourceType::IN_MV:
                        texture = state.mInputs.mMotion;
                        internal = true;
                        break;
                    case nrd::ResourceType::IN_NORMAL_ROUGHNESS:
                        texture = state.mInputs.mNormalRoughness;
                        break;
                    case nrd::ResourceType::IN_VIEWZ:
                        texture = state.mInputs.mViewZ;
                        break;
                    case nrd::ResourceType::IN_DIFF_RADIANCE_HITDIST:
                        texture = state.mInputs.mDiffuse;
                        break;
                    case nrd::ResourceType::IN_SPEC_RADIANCE_HITDIST:
                        texture = state.mInputs.mSpecular;
                        break;
                    case nrd::ResourceType::OUT_DIFF_RADIANCE_HITDIST:
                        texture = state.mDiffuse;
                        internal = true;
                        break;
                    case nrd::ResourceType::OUT_SPEC_RADIANCE_HITDIST:
                        texture = state.mSpecular;
                        internal = true;
                        break;
                    case nrd::ResourceType::OUT_VALIDATION:
                        texture = state.mValidation;
                        internal = true;
                        break;
                    case nrd::ResourceType::PERMANENT_POOL:
                        texture = state.mPermanent.at( r.indexInPool );
                        internal = true;
                        break;
                    case nrd::ResourceType::TRANSIENT_POOL:
                        texture = state.mTransient.at( r.indexInPool );
                        internal = true;
                        break;
                    default:
                        MKT_THROW_RUNTIME_ERROR( "NRD requested an unbound input signal." );
                }
                const bool storage{ r.descriptorType == nrd::DescriptorType::STORAGE_TEXTURE };
                d.mTextures.push_back( { texture, storage ? offsets.storageTextureAndBufferOffset + uav++ : offsets.textureOffset + srv++, storage, internal } );
            }
            state.mDispatches.push_back( eastl::move( d ) );
        }

        // Commit the camera history only after every dispatch has been prepared.
        state.mPreviousView = view;
        state.mPreviousProjection = projection;
        state.mPreviousPosition = camera->GetPosition();
        state.mScene = scene;
        state.mTopologyRevision = rays.GetTopologyRevision();
        state.mResetRevision = mSettings.mResetRevision;
        state.mLightingRevision = rays.GetLightingRevision();
        state.mRestart = false;
        state.mReady = true;
#endif
    }
}// namespace mikoto::renderer
