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

#include <EASTL/sort.h>
#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Profiler.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Particle/ParticleSystem.hh>

#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/ParticleSimulationModule.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::rhi;

    ParticleSimulationModule::ParticleSimulationModule( renderer::rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto ParticleSimulationModule::SetScene( const scene::Scene* scene ) -> void {
        mScene = scene;

        if ( mGpuSimulation ) {
            mGpuSimulation->SetScene( scene );
        }
    }

    auto ParticleSimulationModule::SetCamera( const scene::Camera* camera ) -> void {
        mCamera = camera;

        if ( mGpuSimulation ) {
            mGpuSimulation->SetCamera( camera );
        }
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

    auto ParticleSimulationModule::Gather() -> void {
        MKT_BEGIN_PROFILER();
        mRenderData.clear();
        mDroppedCount = 0;

        if ( !mScene || !mCamera ) {
            return;
        }

        const auto& view{ mCamera->GetViewMatrix() };

        for ( const auto [entity, component, tag]:
            mScene->GetRegistry().view<scene::ParticleEmitterComponent, scene::TagComponent>().each() ) {
            const auto& instance{ component.GetInstance() };
            const auto& emitter{ instance.GetEmitter() };

            if ( !tag.IsActive() || !emitter ) {
                continue;
            }

            const auto& s{ emitter->GetSettings() };

            for ( const auto& p: instance.GetParticles() ) {
                const auto viewPosition{ view * core::float4{ p.mPosition, 1 } };
                const core::f32 age{ glm::clamp( p.mAge / p.mLifetime, 0.0f, 1.0f ) };
                const core::f32 size{ glm::mix( s.mStartSize, s.mEndSize, age ) };
                const auto color{ glm::mix( s.mStartColor, s.mEndColor, age ) };

                if ( viewPosition.z >= size || size <= 0 || color.a <= 0 ) {
                    continue;
                }

                if ( mRenderData.size() == kMaxRenderedParticles ) {
                    ++mDroppedCount;
                    continue;
                }

                // Store view depth once for sorting rather than multiplying a matrix
                // in each comparison. The final component is a deterministic tie key.
                mRenderData.push_back( { core::float4{ p.mPosition, size }, color,
                    core::float4{ p.mRotation, s.mSoftness, -viewPosition.z, static_cast<core::f32>( mRenderData.size() ) } } );
            }
        }

        // Straight-alpha quads require back-to-front ordering across all emitters.
        // Equal depths retain gather order without an allocating stable_sort.
        eastl::sort( mRenderData.begin(), mRenderData.end(),
            []( const auto& a, const auto& b ) {
            return a.mRotationSoftness.z != b.mRotationSoftness.z
                       ? a.mRotationSoftness.z > b.mRotationSoftness.z
                       : a.mRotationSoftness.w < b.mRotationSoftness.w;
        } );
    }

    auto ParticleSimulationModule::SetSimulationMode( ParticleSimulationMode mode ) -> void {
        mSimulationMode = mode;

        if ( mode == ParticleSimulationMode::eGpu ) {
            mGpuSimulation = eastl::make_unique<GpuParticleSimulationModule>();
            mGpuSimulation->SetScene( mScene );
            mGpuSimulation->SetCamera( mCamera );
        }
        else {
            mGpuSimulation.reset();
        }
    }

    auto ParticleSimulationModule::GetGpuSimulation() -> GpuParticleSimulationModule* {
        return mGpuSimulation.get();
    }

    auto ParticleSimulationModule::RegisterUploadPass( FrameGraph& graph ) -> void {
        auto& info{ graph.GetOrCreate<ParticleSimulationInfo>() };

        info.mParticlesBuffer = graph.Create( FGBufferDescription{}
            .SetName( "ParticleSimulation_Instances" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetElementsSize( kMaxRenderedParticles, sizeof( ParticleRenderData ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );

        graph.RegisterPass(
            "ParticleSimulation_Upload",
            FGPassType::eTransfer,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                builder.UseResource( b.Get<ParticleSimulationInfo>().mParticlesBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& b ) {
                Gather();

                if ( !mRenderData.empty() ) {
                    ctx.CopyBuffer( b.Get<ParticleSimulationInfo>().mParticlesBuffer, 0, mRenderData.data(), mRenderData.size() * MKT_SIZEOF( ParticleRenderData ) );
                }
            } );
    }

    auto ParticleSimulationModule::RegisterRenderPass( FrameGraph& graph ) -> void {
        auto& info{ graph.GetOrCreate<ParticleSimulationInfo>() };

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
            .PushShader( "Particle_Vert.slang", FGStageType::eVertex )
            .PushShader( "Particle_Frag.slang", FGStageType::ePixel ) );

        graph.RegisterPass(
            "ParticleSimulation_Graphics",
            FGPassType::eGraphics,
            []( FGNodeBuilder& builder, core::Blackboard& b ) {
                builder.UseResource( b.Get<ParticleSimulationInfo>().mParticlesBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eRead );
                builder.UseResource( b.Get<GeomShadingModuleInfo>().mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this]( CommandContext& ctx, core::Blackboard& b ) {
                const auto drawCount{ mGpuSimulation ? mGpuSimulation->GetDrawCount() : static_cast<core::u32>( mRenderData.size() ) };

                if ( !drawCount ) {
                    return;
                }

                const auto& info{ b.Get<ParticleSimulationInfo>() };
                struct DrawParams {
                    SPointer mParticles{};
                    SPointer mCamera{};
                } params{
                    .mParticles = ctx.GetDeviceBufferAddress( info.mParticlesBuffer ),
                    .mCamera = ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData )
                };

                ctx.PushConstants( params );
                const auto [width, height]{ InferDimensions( mResolution ) };

                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ static_cast<core::i32>( width ), static_cast<core::i32>( height ) } )
                    .AddDepthTarget( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, renderer::rhi::LoadOp::eLoad )
                    .AddRenderTarget( b.Get<GeomShadingModuleInfo>().mColorImage, renderer::rhi::Color{}, renderer::rhi::LoadOp::eLoad ) );

                ctx.SetViewportState( renderer::rhi::ViewportState{}
                    .AddViewportAndScissorRect( renderer::rhi::Viewport( width, height ) ) );

                ctx.BindPipeline( info.mGraphicsPipeline );
                ctx.Draw( 6, drawCount );

                ctx.EndRender();
            } );
    }
}// namespace mikoto::renderer
