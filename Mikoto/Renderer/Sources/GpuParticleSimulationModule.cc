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

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/ParticleSimulationModule.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>

namespace mikoto::renderer {

    using namespace core;
    using namespace rhi;

    struct ParticleState {
        core::float4 mPositionAge{};
        core::float4 mVelocityLifetime{};
        core::float4 mRotation{};
    };

    struct EmitterState {
        core::u32 mRandom{};
        core::f32 mElapsed{};
        core::f32 mEmissionRemainder{};
        core::u32 mInitialized{};
    };

    struct SortParameters {
        SPointer mOutput{};
        core::u32 mCount{};
        core::u32 mStage{};
        core::u32 mStride{};
        core::u32 mClear{};
    };

    auto GpuParticleSimulationModule::SetScene( const scene::Scene* scene ) -> void {
        if ( mScene == scene ) {
            return;
        }

        mSlots.fill( {} );
        mScene = scene;
    }

    auto GpuParticleSimulationModule::SetCamera( const scene::Camera* camera ) -> void {
        mCamera = camera;
    }

    auto GpuParticleSimulationModule::GetDrawCount() const -> core::u32 {
        return mDrawCount;
    }

    auto GpuParticleSimulationModule::Gather() -> void {
        MKT_BEGIN_PROFILER();

        mDrawCount = 0;

        for ( auto& slot: mSlots ) {
            slot.mSeen = false;
        }

        mParameters.fill( {} );

        if ( !mScene || !mCamera ) {
            return;
        }

        const auto view{ mScene->GetRegistry().view<scene::ParticleEmitterComponent, scene::TransformComponent, scene::TagComponent>() };
        // Reclaim missing recipes/entities before allocating ranges. Existing emitters
        // keep their offsets: removing a neighbour must not restart their simulation.
        for ( const auto [entity, component, transform, tag]: view.each() ) {
            for ( auto& slot: mSlots ) {
                if ( slot.mRecipe && slot.mGuid == tag.GetGuid() && slot.mRecipe == component.GetInstance().GetEmitter() ) {
                    slot.mSeen = true;
                }
            }
        }

        for ( auto& slot: mSlots ) {
            if ( !slot.mSeen ) {
                slot = {};
            }
        }

        for ( const auto [entity, component, transform, tag]: view.each() ) {
            const auto& instance{ component.GetInstance() };
            const auto& recipe{ instance.GetEmitter() };

            if ( !recipe || instance.GetSimulationMode() != ParticleSimulationMode::eGpu ) {
                continue;
            }

            const auto& world{ transform.GetWorldTransform() };

            auto* slot{ eastl::find_if( mSlots.begin(), mSlots.end(), [&]( const auto& item ) {
                return item.mRecipe && item.mGuid == tag.GetGuid();
            } ) };

            bool reset{};
            if ( slot == mSlots.end() ) {
                slot = eastl::find_if( mSlots.begin(), mSlots.end(), []( const auto& item ) {
                    return !item.mRecipe;
                } );

                if ( slot == mSlots.end() ) {
                    continue;
                }

                const core::u32 capacity{ recipe->GetSettings().mMaxParticles };
                core::u32 offset{};
                // First-fit over a small emitter table, never a per-particle CPU loop.
                for ( bool overlap{ true }; overlap; ) {
                    overlap = false;

                    for ( const auto& used: mSlots ) {
                        if ( used.mRecipe && offset < used.mOffset + used.mCapacity && offset + capacity > used.mOffset ) {
                            offset = used.mOffset + used.mCapacity;
                            overlap = true;
                        }
                    }
                }

                if ( offset + capacity > kMaxRenderedParticles ) {
                    continue;
                }

                *slot = { tag.GetGuid(), instance.GetRestartRevision(), instance.GetSimulationTick(), recipe, offset, capacity, true };
                reset = true;
            }

            reset |= slot->mRevision != instance.GetRestartRevision() || instance.GetSimulationTick() < slot->mTick;

            // Upload one coherent recipe snapshot for this emitter's simulation steps.
            const auto& s{ recipe->GetSettings() };
            auto& parameters{ mParameters[slot - mSlots.begin()] };
            parameters.mWorld = world;
            parameters.mDirectionCone = core::float4{ s.mDirection, glm::radians( s.mConeAngle ) };
            parameters.mAccelerationDrag = core::float4{ s.mAcceleration, s.mDrag };
            parameters.mLifetimeSpeed = { s.mLifetime, s.mLifetimeVariation, s.mSpeed, s.mSpeedVariation };
            parameters.mEmission = { s.mEmissionRate, s.mRadius, s.mDuration, 0 };
            parameters.mSizeRotationSoftness = { s.mStartSize, s.mEndSize, s.mAngularSpeed, s.mSoftness };
            parameters.mStartColor = s.mStartColor;
            parameters.mEndColor = s.mEndColor;

            // Keep the emitter's arena bounds and restart state with its bounded catch-up work.
            parameters.mOffset = slot->mOffset;
            parameters.mCapacity = slot->mCapacity;
            parameters.mSteps = static_cast<core::u32>( eastl::min<core::u64>( reset ? instance.GetSimulationTick() : 
                instance.GetSimulationTick() - slot->mTick, 8 ) );
            parameters.mReset = reset;
            parameters.mSeed = s.mSeed ^ tag.GetGuid();
            parameters.mBurst = s.mBurstCount;
            parameters.mEmitting = instance.IsEmitting() && instance.GetPlayback() != ParticlePlayback::eStopped;
            parameters.mVisible = tag.IsActive();
            parameters.mLooping = s.mLooping;
            parameters.mInitialize = instance.GetSimulationTick() != 0;

            slot->mTick = instance.GetSimulationTick();
            slot->mRevision = instance.GetRestartRevision();
            mDrawCount = eastl::max( mDrawCount, slot->mOffset + slot->mCapacity );
        }

        // Bitonic sorting requires a power-of-two extent. The clear pass fills
        // holes and padding with invisible records before any emitter writes.
        if ( mDrawCount ) {
            core::u32 padded{ 1 };

            while ( padded < mDrawCount ) {
                padded <<= 1;
            }

            mDrawCount = padded;
        }
    }

    auto GpuParticleSimulationModule::RegisterParameterUploadPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();

        auto& simModuleInfo{ graph.GetOrCreate<ParticleSimulationInfo>() };
        simModuleInfo.mParticlesBuffer = graph.Create( FGBufferDescription{}
            .SetName( "ParticleSimulation_Instances" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | 
                renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetElementsSize( kMaxRenderedParticles, sizeof( ParticleRenderData ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );

        auto& gpuParticleSimInfo{ graph.GetOrCreate<GpuParticleSimulationModuleInfo>() };
        gpuParticleSimInfo.mOutput = simModuleInfo.mParticlesBuffer;

        const auto createBuffer{ [&]( core::cstr name, core::u32 count, core::u32 stride ) {
            return graph.Create( FGBufferDescription{}
                .SetName( name )
                .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest )
                .SetElementsSize( count, stride )
                .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );
        } };

        gpuParticleSimInfo.mEmitterParameters = createBuffer( "ParticleGpu_Parameters", kMaxGpuEmitters, sizeof( EmitterParameters ) );

        // Publish only emitter inputs; live particle state stays GPU-owned between frames.
        graph.RegisterPass<GpuParticleSimulationModuleInfo>(
            "ParticleGpu_Parameters",
            FGPassType::eTransfer,
            []( FGNodeBuilder& builder, GpuParticleSimulationModuleInfo& info ) {
                builder.UseResource( info.mEmitterParameters, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& blackboard ) {
                Gather();

                if ( mDrawCount ) {
                    const auto& info{ blackboard.Get<GpuParticleSimulationModuleInfo>() };
                    ctx.CopyBuffer( info.mEmitterParameters, 0, mParameters.data(), sizeof( mParameters ) );
                }
            } );
    }

    auto GpuParticleSimulationModule::RegisterSnapshotClearPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();

        auto& info{ graph.GetOrCreate<GpuParticleSimulationModuleInfo>() };
        info.mSortPipeline = graph.Create( FGPipelineDescription{}
            .SetName( "ParticleGpu_Sort" )
            .SetPipelineType( renderer::rhi::PipelineType::eCompute )
            .PushShader( "ParticleSort_Comp.slang", FGStageType::eCompute ) );

        // Clear holes and sort padding without destroying persistent simulation state.
        graph.RegisterPass<GpuParticleSimulationModuleInfo>(
            "ParticleGpu_ClearSnapshot",
            FGPassType::eCompute,
            []( FGNodeBuilder& builder, GpuParticleSimulationModuleInfo& info ) {
                builder.UseResource( info.mEmitterParameters, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( info.mOutput, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& blackboard ) {
                if ( !mDrawCount ) {
                    return;
                }

                const auto& info{ blackboard.Get<GpuParticleSimulationModuleInfo>() };
                ctx.BindPipeline( info.mSortPipeline );
                ctx.PushConstants( SortParameters{ ctx.GetDeviceBufferAddress( info.mOutput ), mDrawCount, 0, 0, 1 } );
                ctx.Dispatch( ( mDrawCount + 127 ) / 128, 1, 1 );
            } );
    }

    auto GpuParticleSimulationModule::RegisterSimulationPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();

        auto& info{ graph.GetOrCreate<GpuParticleSimulationModuleInfo>() };

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
        graph.RegisterPass<GpuParticleSimulationModuleInfo>(
            "ParticleGpu_Simulate",
            FGPassType::eCompute,
            []( FGNodeBuilder& builder, GpuParticleSimulationModuleInfo& info ) {
                builder.UseResource( info.mEmitterParameters, FGPipelineStage::eComputeShader, FGResourceAccess::eRead );
                builder.UseResource( info.mParticleStates, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                builder.UseResource( info.mEmitterStates, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
                builder.UseResource( info.mOutput, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& blackboard ) {
                if ( !mDrawCount ) {
                    return;
                }

                const auto& info{ blackboard.Get<GpuParticleSimulationModuleInfo>() };

                struct Parameters {
                    SPointer mStates{};
                    SPointer mHeaders{};
                    SPointer mEmitters{};
                    SPointer mOutput{};
                    core::float4x4 mView{};
                } params {
                    .mStates = ctx.GetDeviceBufferAddress( info.mParticleStates ),
                    .mHeaders = ctx.GetDeviceBufferAddress( info.mEmitterStates ),
                    .mEmitters = ctx.GetDeviceBufferAddress( info.mEmitterParameters ),
                    .mOutput = ctx.GetDeviceBufferAddress( info.mOutput ),
                    .mView = mCamera->GetViewMatrix(),
                };

                ctx.PushConstants( params );
                ctx.BindPipeline( info.mSimulationPipeline );

                ctx.Dispatch( kMaxGpuEmitters, 1, 1 );
            } );
    }

    auto GpuParticleSimulationModule::RegisterSortPass( FrameGraph& graph ) -> void {
        MKT_BEGIN_PROFILER();

        // Keep alpha blending ordered across emitters 
        // without reading particles back to the CPU.
        graph.RegisterPass<GpuParticleSimulationModuleInfo>(
            "ParticleGpu_Sort",
            FGPassType::eCompute,
            []( FGNodeBuilder& builder, GpuParticleSimulationModuleInfo& info ) {
                builder.UseResource( info.mOutput, FGPipelineStage::eComputeShader, FGResourceAccess::eWrite );
            },
            [this, &graph]( CommandContext& ctx, core::Blackboard& blackboard ) {
                if ( !mDrawCount ) {
                    return;
                }

                const auto& info{ blackboard.Get<GpuParticleSimulationModuleInfo>() };
                ctx.BindPipeline( info.mSortPipeline );

                const auto output{ ctx.GetDeviceBufferAddress( info.mOutput ) };
                auto buffer{ graph.GetBuffer( info.mOutput ) };

                // A dispatch cannot synchronize workgroups internally. Each compare
                // stage publishes its writes before the next stage reads the same slots.
                for ( core::u32 stage{ 2 }; stage <= mDrawCount; stage <<= 1 ) {
                    for ( core::u32 stride{ stage >> 1 }; stride; stride >>= 1 ) {
                        ctx.PushConstants( SortParameters{ output, mDrawCount, stage, stride, 0 } );
                        ctx.Dispatch( ( mDrawCount + 127 ) / 128, 1, 1 );
                    }
                }
            } );
    }
}// namespace mikoto::renderer
