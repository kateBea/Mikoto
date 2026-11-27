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

#include <bit>

#include <EASTL/algorithm.h>
#include <EASTL/bitset.h>
#include <EASTL/hash_map.h>
#include <EASTL/sort.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Profiler.hh>
#include <Core/String.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/ParticleSimulationModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/Particle/ParticleSystem.hh>

#include <Scene/Camera.hh>
#include <Scene/Component.hh>
#include <Scene/Scene.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;

    // Packed to match ParticleGpuState and ParticleGpuHeader in base/Particle.slang.
    struct ParticleState {
        core::float4 mPositionAge{};
        core::float4 mVelocityLifetime{};
        core::float4 mRotationFade{};// Rotation, fade-in fraction, fade-out fraction, bloom intensity.
        core::float4 mAccelerationDrag{};
        core::float4 mSizeRotationSoftness{};
        core::float4 mStartColor{};
        core::float4 mEndColor{};
        glm::uvec4 mTexture{};
    };

    struct EmitterState {
        core::u32 mRandom{};
        core::f32 mElapsed{};
        core::f32 mEmissionRemainder{};
        core::u32 mInitialized{};
        core::u32 mSeed{};
    };

    struct SortParameters {
        SPointer mOutput{};
        core::u32 mCount{};
        core::u32 mStage{};
        core::u32 mStride{};
        core::u32 mClear{};
    };

    using TextureDescriptors = eastl::hash_map<const rhi::ITexture *, core::u32>;

    MKT_NODISCARD static auto GetBillboardDescriptor( CommandContext &ctx, const rhi::TextureHandle &texture, TextureDescriptors &descriptors ) -> core::u32 {
        if ( !texture ) {
            return kInvalidParticleTextureIndex;
        }

        const auto [entry, added]{ descriptors.try_emplace( texture.GetPtr() ) };
        if ( added ) {
            entry->second = ctx.PushTexture_SRV( ctx.ImportTexture( texture ) );
        }

        return entry->second;
    }

    ParticleSimulationModule::ParticleSimulationModule( renderer::rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto ParticleSimulationModule::SetScene( const scene::Scene *scene ) -> void {
        if ( mScene != scene ) {
            mSlots.fill( {} );
            mScene = scene;
        }
    }

    auto ParticleSimulationModule::SetCamera( const scene::Camera *camera ) -> void {
        mCamera = camera;
    }

    auto ParticleSimulationModule::GetVisibleCount() const -> core::u32 {
        return static_cast<core::u32>( mRenderData.size() );
    }

    auto ParticleSimulationModule::GetSimulationMode() const -> ParticleSimulationMode {
        return mSimulationMode;
    }

    auto ParticleSimulationModule::GetDroppedCount() const -> core::u32 {
        return mDroppedCount;
    }

    auto ParticleSimulationModule::GatherCpuParticles( CommandContext &ctx ) -> void {
        MKT_BEGIN_PROFILER();
        mRenderData.clear();
        mDroppedCount = 0;

        if ( !mScene || !mCamera ) {
            return;
        }

        const auto &view{ mCamera->GetViewMatrix() };
        TextureDescriptors descriptors{};

        for ( const auto [entity, simulator, tag]:
            mScene->GetRegistry().view<scene::ParticleSimulatorComponent, scene::TagComponent>().each() ) {
            if ( !tag.IsActive() ) {
                continue;
            }

            for ( const auto &instance: simulator.GetInstances() ) {
                for ( const auto &p: instance.GetParticles() ) {
                    const auto viewPosition{ view * core::float4{ p.mPosition, 1 } };
                    const auto age{ glm::clamp( p.mAge / p.mLifetime, 0.0f, 1.0f ) };
                    const auto size{ glm::mix( p.mStartSize, p.mEndSize, age ) };
                    const auto color{ p.GetColor() };

                    if ( viewPosition.z >= size || size <= 0 || color.a <= 0 ) {
                        continue;
                    }

                    if ( mRenderData.size() == kMaxRenderedParticles ) {
                        ++mDroppedCount;
                        continue;
                    }

                    mRenderData.push_back( { .mPositionSize = { p.mPosition, size },
                        .mColor = color,
                        .mRotationSoftness = { p.mRotation, p.mSoftness, -viewPosition.z, core::as<core::f32>( mRenderData.size() ) },
                        .mTexture = { GetBillboardDescriptor( ctx, instance.GetParticleTexture( p.mTextureIndex ), descriptors ), std::bit_cast<core::u32>( p.mBloomIntensity ), 0, 0 } } );
                }
            }
        }

        // Straight-alpha quads require back-to-front ordering across all emitters.
        // Equal depths retain gather order without an allocating stable_sort.
        eastl::sort( mRenderData.begin(), mRenderData.end(),
            []( const auto &a, const auto &b ) {
                return a.mRotationSoftness.z != b.mRotationSoftness.z
                           ? a.mRotationSoftness.z > b.mRotationSoftness.z
                           : a.mRotationSoftness.w < b.mRotationSoftness.w;
            } );
    }

    auto ParticleSimulationModule::SetSimulationMode( ParticleSimulationMode mode ) -> void {
        mSimulationMode = mode;
    }

    auto ParticleSimulationModule::RegisterSimulationPasses( FrameGraph &graph ) -> void {
        if ( mSimulationMode == ParticleSimulationMode::eGpu ) {
            RegisterParameterUploadPass( graph );
            RegisterSnapshotClearPass( graph );
            RegisterSimulationPass( graph );
            RegisterSortPass( graph );
        }
        else {
            RegisterUploadPass( graph );
        }
    }

    auto ParticleSimulationModule::RegisterUploadPass( FrameGraph &graph ) -> void {
        auto &info{ graph.GetOrCreate<ParticleSimulationInfo>() };

        info.mParticlesBuffer = graph.Create( FGBufferDescription{}
            .SetName( "ParticleSimulation_Instances" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                           renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetElementsSize( kMaxRenderedParticles, sizeof( ParticleRenderData ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );

        graph.RegisterPass(
            "ParticleSimulation_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, core::Blackboard &b ) {
                builder.UseResource( b.Get<ParticleSimulationInfo>().mParticlesBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, core::Blackboard &b ) {
                GatherCpuParticles( ctx );

                if ( !mRenderData.empty() ) {
                    ctx.CopyBuffer( b.Get<ParticleSimulationInfo>().mParticlesBuffer, 0, mRenderData.data(), mRenderData.size() * MKT_SIZEOF( ParticleRenderData ) );
                }
            } );
    }

    auto ParticleSimulationModule::RegisterRenderPass( FrameGraph &graph ) -> void {
        auto &info{ graph.GetOrCreate<ParticleSimulationInfo>() };

        info.mSampler = graph.Create( FGSamplerDescription{}
            .SetFilter( rhi::SamplerFilter::eLinear )
            .SetWrap( rhi::SamplerWrapMode::eClampToEdge ) );

        info.mGraphicsPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "ParticleSimulation_Billboards" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( false )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .SetBlendEnable( true )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .AddColorFormat( renderer::rhi::Format::eRGBA16_FLOAT )
            .PushShader( "Particle_Vert.slang", FGStageType::eVertex )
            .PushShader( "Particle_Frag.slang", FGStageType::ePixel ) );

        graph.RegisterPass(
            "ParticleSimulation_Graphics",
            FGPassType::eGraphics,
            []( FGNodeBuilder &builder, core::Blackboard &b ) {
                builder.UseResource( b.Get<ParticleSimulationInfo>().mParticlesBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eRead );
                builder.UseResource( b.Get<GeomShadingModuleInfo>().mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
                builder.UseResource( b.Get<PrepassModuleInfo>().mGBufferEmissiveTarget, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, core::Blackboard &b ) {
                const auto drawCount{ mSimulationMode == ParticleSimulationMode::eGpu ? mGpuDrawCount : core::as<core::u32>( mRenderData.size() ) };

                if ( !drawCount ) {
                    return;
                }

                const auto &info{ b.Get<ParticleSimulationInfo>() };
                struct DrawParams {
                    SPointer mParticles{};
                    SPointer mCamera{};
                    core::u32 mSampler{};
                } params{
                    .mParticles = ctx.GetDeviceBufferAddress( info.mParticlesBuffer ),
                    .mCamera = ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData ),
                    .mSampler = ctx.PushSampler( info.mSampler )
                };

                ctx.PushConstants( params );
                const auto [width, height]{ InferDimensions( mResolution ) };

                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ static_cast<core::i32>( width ), static_cast<core::i32>( height ) } )
                    .AddDepthTarget( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, renderer::rhi::LoadOp::eLoad )
                    .AddRenderTarget( b.Get<GeomShadingModuleInfo>().mColorImage, renderer::rhi::Color{}, renderer::rhi::LoadOp::eLoad )
                    .AddRenderTarget( b.Get<PrepassModuleInfo>().mGBufferEmissiveTarget, renderer::rhi::Color{}, renderer::rhi::LoadOp::eLoad ) );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                        .AddViewportAndScissorRect( renderer::rhi::Viewport( width, height ) ) );

                ctx.BindPipeline( info.mGraphicsPipeline );
                ctx.Draw( 6, drawCount );

                ctx.EndRender();
            } );
    }

    auto ParticleSimulationModule::GatherGpuEmitters( CommandContext &ctx ) -> void {
        MKT_BEGIN_PROFILER();

        mGpuDrawCount = 0;
        mParticleIndices.clear();
        mTextureDescriptors.clear();
        mParameters.fill( {} );

        if ( !mScene || !mCamera ) {
            return;
        }

        const auto view{ mScene->GetRegistry().view<scene::ParticleSimulatorComponent, scene::TransformComponent, scene::TagComponent>() };

        for ( auto &slot: mSlots ) {
            slot.mSeen = false;
        }

        for ( const auto [entity, simulator, transform, tag]: view.each() ) {
            for ( const auto &instance: simulator.GetInstances() ) {
                if ( instance.GetSimulationMode() != ParticleSimulationMode::eGpu || instance.GetPlayback() == ParticlePlayback::eStopped ) {
                    continue;
                }

                const auto slot{ eastl::find_if( mSlots.begin(), mSlots.end(), [&]( const EmitterSlot &value ) {
                    return value.mInstanceID == instance.GetID();
                } ) };

                if ( slot != mSlots.end() ) {
                    slot->mSeen = true;
                }
            }
        }

        eastl::bitset<kMaxRenderedParticles> used{};
        core::usize textureBindingCount{};
        for ( auto &slot: mSlots ) {
            if ( !slot.mSeen ) {
                slot = {};
                continue;
            }

            textureBindingCount += slot.mTextures.size();
            for ( const auto index: slot.mIndices ) {
                used.set( index );
            }
        }

        core::u32 nextFree{};
        TextureDescriptors descriptors{};
        for ( const auto [entity, simulator, transform, tag]: view.each() ) {
            core::u64 seed{ tag.GetGuid() };
            for ( const auto &instance: simulator.GetInstances() ) {
                const auto emitterSeed{ seed++ };

                if ( instance.GetSimulationMode() != ParticleSimulationMode::eGpu || instance.GetPlayback() == ParticlePlayback::eStopped ) {
                    continue;
                }

                auto slot{ eastl::find_if( mSlots.begin(), mSlots.end(), [&]( const EmitterSlot &value ) {
                    return value.mInstanceID == instance.GetID();
                } ) };

                const bool newSlot{ slot == mSlots.end() };
                if ( newSlot ) {
                    slot = eastl::find_if( mSlots.begin(), mSlots.end(), []( const EmitterSlot &value ) {
                        return value.mInstanceID == 0;
                    } );

                    if ( slot == mSlots.end() ) {
                        continue;
                    }
                }

                const auto &recipe{ instance.GetEmitter() };
                const auto &s{ recipe->GetSettings() };
                const auto previousCapacity{ core::as<core::u32>( slot->mIndices.size() ) };

                // Grow into free slots without moving live GPU particles. A lower
                // spawn capacity lets previously allocated slots drain naturally.
                while ( slot->mIndices.size() < s.mMaxParticles ) {
                    while ( nextFree < kMaxRenderedParticles && used.test( nextFree ) ) {
                        ++nextFree;
                    }

                    if ( nextFree == kMaxRenderedParticles ) {
                        break;
                    }

                    slot->mIndices.push_back( nextFree );
                    used.set( nextFree++ );
                }

                if ( slot->mIndices.empty() ) {
                    continue;
                }

                const auto tick{ instance.GetSimulationTick() };
                const auto revision{ instance.GetRestartRevision() };
                const bool reset{ newSlot || slot->mRevision != revision || tick < slot->mTick };
                const auto steps{ core::as<core::u32>( eastl::min<core::u64>( reset ? tick : tick - slot->mTick, 8 ) ) };

                if ( reset ) {
                    textureBindingCount -= slot->mTextures.size();
                    slot->mTextures.clear();
                    slot->mSimulatedTicks = 0;
                }

                // GPU particles store an emitter-local texture index. Remap it to
                // this frame's descriptors, and retain old textures until expiry.
                const auto texture{ recipe->GetTexture() };
                core::u32 textureIndex{ kInvalidParticleTextureIndex };
                core::u32 freeBinding{ kInvalidParticleTextureIndex };

                for ( core::u32 index{}; index < slot->mTextures.size(); ++index ) {
                    auto &binding{ slot->mTextures[index] };
                    if ( texture && binding.mTexture == texture ) {
                        textureIndex = index;
                    }
                    else if ( binding.mExpiresAtTick <= slot->mSimulatedTicks ) {
                        binding = {};
                        freeBinding = index;
                    }
                }

                if ( texture && textureIndex == kInvalidParticleTextureIndex ) {
                    if ( freeBinding == kInvalidParticleTextureIndex && textureBindingCount < kMaxGpuTextureBindings ) {
                        freeBinding = core::as<core::u32>( slot->mTextures.size() );
                        slot->mTextures.emplace_back();
                        ++textureBindingCount;
                    }

                    if ( freeBinding != kInvalidParticleTextureIndex ) {
                        slot->mTextures[freeBinding].mTexture = texture;
                        textureIndex = freeBinding;
                    }
                }

                slot->mSimulatedTicks += steps;
                if ( textureIndex != kInvalidParticleTextureIndex && steps != 0 ) {
                    auto &binding{ slot->mTextures[textureIndex] };
                    const auto lifetimeTicks{ core::as<core::u64>( glm::ceil( s.mLifetime * ( 1.0f + s.mLifetimeVariation ) * 60.0f ) ) };
                    binding.mExpiresAtTick = eastl::max( binding.mExpiresAtTick, slot->mSimulatedTicks + lifetimeTicks + 1 );
                }

                const auto textureOffset{ core::as<core::u32>( mTextureDescriptors.size() ) };
                for ( const auto &binding: slot->mTextures ) {
                    mTextureDescriptors.push_back( GetBillboardDescriptor( ctx, binding.mTexture, descriptors ) );
                }

                const auto tint{ recipe->GetTint() };
                auto &parameters{ mParameters[slot - mSlots.begin()] };
                parameters = {
                    .mWorld = transform.GetWorldTransform(),
                    .mDirectionCone = { s.mDirection, glm::radians( s.mConeAngle ) },
                    .mAccelerationDrag = { s.mAcceleration, s.mDrag },
                    .mLifetimeSpeed = { s.mLifetime, s.mLifetimeVariation, s.mSpeed, s.mSpeedVariation },
                    .mEmission = { s.mEmissionRate, s.mRadius, s.mDuration,
                        recipe->GetMaterial() && recipe->GetMaterial()->IsBloomEnabled() ? recipe->GetMaterial()->GetBloomIntensity() : 0.0f },
                    .mSizeRotationSoftness = { s.mStartSize, s.mEndSize, s.mAngularSpeed, s.mSoftness },
                    .mStartColor = s.mStartColor * tint,
                    .mEndColor = s.mEndColor * tint,
                    .mOffset = core::as<core::u32>( mParticleIndices.size() ),
                    .mCapacity = core::as<core::u32>( slot->mIndices.size() ),
                    .mSteps = steps,
                    .mReset = reset,
                    .mSeed = s.mSeed ^ core::as<core::u32>( emitterSeed ) ^ core::as<core::u32>( emitterSeed >> 32 ),
                    .mBurst = s.mBurstCount,
                    .mEmitting = instance.IsEmitting(),
                    .mVisible = tag.IsActive(),
                    .mLooping = s.mLooping,
                    .mInitialize = tick != 0,
                    .mMaxParticles = eastl::min( s.mMaxParticles, core::as<core::u32>( slot->mIndices.size() ) ),
                    .mPreviousCapacity = previousCapacity,
                    .mTextureIndex = textureIndex,
                    .mTextureOffset = textureOffset,
                    .mFadeIn = s.mFadeEnabled ? s.mFadeIn : 0.0f,
                    .mFadeOut = s.mFadeEnabled ? s.mFadeOut : 0.0f
                };

                mParticleIndices.insert( mParticleIndices.end(), slot->mIndices.begin(), slot->mIndices.end() );
                slot->mInstanceID = instance.GetID();
                slot->mTick = tick;
                slot->mRevision = revision;
            }
        }

        if ( !mParticleIndices.empty() ) {
            const auto required{ *eastl::max_element( mParticleIndices.begin(), mParticleIndices.end() ) + 1 };
            mGpuDrawCount = 1;

            while ( mGpuDrawCount < required ) {
                mGpuDrawCount <<= 1;
            }
        }
    }

    auto ParticleSimulationModule::RegisterParameterUploadPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER();

        auto &simModuleInfo{ graph.GetOrCreate<ParticleSimulationInfo>() };
        simModuleInfo.mParticlesBuffer = graph.Create( FGBufferDescription{}
            .SetName( "ParticleSimulation_Instances" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                           renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetElementsSize( kMaxRenderedParticles, sizeof( ParticleRenderData ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );

        const auto createBuffer{ [&]( core::cstr name, core::u32 count, core::u32 stride ) {
            return graph.Create( FGBufferDescription{}
                .SetName( name )
                .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest )
                .SetElementsSize( count, stride )
                .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );
        } };

        simModuleInfo.mEmitterParameters = createBuffer( "ParticleGpu_Parameters", kMaxGpuEmitters, sizeof( EmitterParameters ) );

        simModuleInfo.mParticleIndices = createBuffer( "ParticleGpu_Indices", kMaxRenderedParticles, sizeof( core::u32 ) );
        simModuleInfo.mTextureDescriptors = createBuffer( "ParticleGpu_TextureDescriptors", kMaxGpuTextureBindings, sizeof( core::u32 ) );

        // Upload the current emitter inputs and the addresses of their persistent slots.
        graph.RegisterPass<ParticleSimulationInfo>(
            "ParticleGpu_Parameters",
            FGPassType::eTransfer,
            []( FGNodeBuilder &builder, ParticleSimulationInfo &info ) {
                builder.UseResource( info.mEmitterParameters, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( info.mParticleIndices, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
                builder.UseResource( info.mTextureDescriptors, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this, &graph]( CommandContext &ctx, core::Blackboard &blackboard ) {
                const auto previousDrawCount{ mGpuDrawCount };
                GatherGpuEmitters( ctx );

                if ( previousDrawCount != mGpuDrawCount ) {
                    for ( const auto &stage: mSortStages ) {
                        if ( stage.mRequiredCount <= mGpuDrawCount ) {
                            graph.EnablePass( stage.mName );
                        }
                        else {
                            graph.DisablePass( stage.mName );
                        }
                    }
                }

                if ( mGpuDrawCount ) {
                    const auto &info{ blackboard.Get<ParticleSimulationInfo>() };
                    ctx.CopyBuffer( info.mEmitterParameters, 0, mParameters.data(), sizeof( mParameters ) );
                    ctx.CopyBuffer( info.mParticleIndices, 0, mParticleIndices.data(), MKT_VECTOR_SIZE_BYTES( mParticleIndices ) );
                    if ( !mTextureDescriptors.empty() ) {
                        ctx.CopyBuffer( info.mTextureDescriptors, 0, mTextureDescriptors.data(), MKT_VECTOR_SIZE_BYTES( mTextureDescriptors ) );
                    }
                }
            } );
    }

    auto ParticleSimulationModule::RegisterSnapshotClearPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER();

        auto &info{ graph.GetOrCreate<ParticleSimulationInfo>() };
        info.mSortPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "ParticleGpu_Sort" )
            .SetPipelineType( renderer::rhi::PipelineType::eCompute )
            .PushShader( "ParticleSort_Comp.slang", FGStageType::eCompute ) );

        // Clear holes and sort padding without destroying persistent simulation state.
        graph.RegisterPass<ParticleSimulationInfo>(
            "ParticleGpu_ClearSnapshot",
            FGPassType::eCompute,
            []( FGNodeBuilder &builder, ParticleSimulationInfo &info ) {
                builder.UseResource( info.mEmitterParameters, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( info.mParticlesBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, core::Blackboard &blackboard ) {
                if ( !mGpuDrawCount ) {
                    return;
                }

                const auto &info{ blackboard.Get<ParticleSimulationInfo>() };
                ctx.BindPipeline( info.mSortPipeline );
                ctx.PushConstants( SortParameters{
                    .mOutput = ctx.GetDeviceBufferAddress( info.mParticlesBuffer ),
                    .mCount = mGpuDrawCount,
                    .mStage = 0,
                    .mStride = 0,
                    .mClear = 1,
                } );
                ctx.Dispatch( ( mGpuDrawCount + 127 ) / 128, 1, 1 );
            } );
    }

    auto ParticleSimulationModule::RegisterSimulationPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER();

        auto &info{ graph.GetOrCreate<ParticleSimulationInfo>() };

        const auto createBuffer{ [&]( core::cstr name, core::u32 count, core::u32 stride ) {
            return graph.Create( FGBufferDescription{}
                .SetName( name )
                .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured |
                               renderer::rhi::BufferUsageFlagsBits::CopyDest )
                .SetElementsSize( count, stride )
                .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );
        } };

        info.mParticleStates = createBuffer( "ParticleGpu_State", kMaxRenderedParticles, MKT_SIZEOF( ParticleState ) );
        info.mEmitterStates = createBuffer( "ParticleGpu_Emitters", kMaxGpuEmitters, MKT_SIZEOF( EmitterState ) );

        info.mSimulationPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "ParticleGpu_Simulate" )
            .SetPipelineType( renderer::rhi::PipelineType::eCompute )
            .PushShader( "ParticleSimulation_Comp.slang", FGStageType::eCompute ) );

        // Each emitter consumes scene ticks once, then publishes this camera's snapshot.
        graph.RegisterPass<ParticleSimulationInfo>(
            "ParticleGpu_Simulate",
            FGPassType::eCompute,
            []( FGNodeBuilder &builder, ParticleSimulationInfo &info ) {
                builder.UseResource( info.mEmitterParameters, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( info.mParticleIndices, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( info.mTextureDescriptors, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( info.mParticleStates, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                builder.UseResource( info.mEmitterStates, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                builder.UseResource( info.mParticlesBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, core::Blackboard &blackboard ) {
                if ( !mGpuDrawCount ) {
                    return;
                }

                const auto &info{ blackboard.Get<ParticleSimulationInfo>() };

                struct Parameters {
                    SPointer mStates{};
                    SPointer mHeaders{};
                    SPointer mEmitters{};
                    SPointer mOutput{};
                    SPointer mIndices{};
                    SPointer mTextureDescriptors{};
                    core::float4x4 mView{};
                } params{
                    .mStates = ctx.GetDeviceBufferAddress( info.mParticleStates ),
                    .mHeaders = ctx.GetDeviceBufferAddress( info.mEmitterStates ),
                    .mEmitters = ctx.GetDeviceBufferAddress( info.mEmitterParameters ),
                    .mOutput = ctx.GetDeviceBufferAddress( info.mParticlesBuffer ),
                    .mIndices = ctx.GetDeviceBufferAddress( info.mParticleIndices ),
                    .mTextureDescriptors = ctx.GetDeviceBufferAddress( info.mTextureDescriptors ),
                    .mView = mCamera->GetViewMatrix()
                };

                ctx.PushConstants( params );
                ctx.BindPipeline( info.mSimulationPipeline );

                ctx.Dispatch( kMaxGpuEmitters, 1, 1 );
            } );
    }

    auto ParticleSimulationModule::RegisterSortPass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER();

        // Each compare step needs a barrier before the next one can read its
        // results. Declaring separate passes lets the graph synchronize them.
        for ( core::u32 stage{ 2 }; stage <= kMaxRenderedParticles; stage <<= 1 ) {
            for ( core::u32 stride{ stage >> 1 }; stride; stride >>= 1 ) {
                const auto &sort{ mSortStages.emplace_back( SortStage{
                    .mName = string::Format( "ParticleGpu_Sort_{}_{}", stage, stride ),
                    .mRequiredCount = stage } ) };
                graph.RegisterPass<ParticleSimulationInfo>(
                    sort.mName,
                    FGPassType::eCompute,
                    []( FGNodeBuilder &builder, ParticleSimulationInfo &info ) {
                        builder.UseResource( info.mParticlesBuffer, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                    },
                    [this, stage, stride]( CommandContext &ctx, core::Blackboard &blackboard ) {
                        const auto &info{ blackboard.Get<ParticleSimulationInfo>() };
                        ctx.BindPipeline( info.mSortPipeline );
                        ctx.PushConstants( SortParameters{
                            .mOutput = ctx.GetDeviceBufferAddress( info.mParticlesBuffer ),
                            .mCount = mGpuDrawCount,
                            .mStage = stage,
                            .mStride = stride } );
                        ctx.Dispatch( ( mGpuDrawCount + 127 ) / 128, 1, 1 );
                    } );
                graph.DisablePass( sort.mName );
            }
        }
    }
}// namespace mikoto::renderer
