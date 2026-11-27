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

#include <cmath>

#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Animation/AnimationSystem.hh>
#include <Animation/SkinningPalette.hh>
#include <Animation/Utility.hh>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/Types.hh>

#include <Math/Math.hh>
#include <Math/Random.hh>

#include <Physics/PhysicSystem.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/ForwardRenderer/Passes/CameraModule.hh>
#include <Renderer/ForwardRenderer/Passes/DebugOverlayModule.hh>
#include <Renderer/ForwardRenderer/Passes/GeometryShadingModule.hh>
#include <Renderer/ForwardRenderer/Passes/PostProcessModule.hh>
#include <Renderer/ForwardRenderer/Passes/PrepassModule.hh>
#include <Renderer/ForwardRenderer/Passes/PresentationModule.hh>

#include <Scene/Component.hh>
#include <Scene/Scene.hh>
#include <Scene/Wind.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::renderer::rhi;

    DebugOverlayModule::DebugOverlayModule( rhi::RenderResolution resolution )
        : mResolution{ resolution } {
    }

    auto DebugOverlayModule::SetMultisampling( rhi::Multisampling multisampling ) -> void {
    }

    auto DebugOverlayModule::SetScene( const scene::Scene *scene ) -> void {
        MKT_BEGIN_PROFILER();
        mScene = scene;
    }

    auto DebugOverlayModule::SetCamera( const scene::Camera *camera ) -> void {
        MKT_BEGIN_PROFILER();
        mCamera = camera;
    }

    auto DebugOverlayModule::SetGeometryManager( GeometryCullModule &geom ) -> void {
        MKT_BEGIN_PROFILER();
        mGeometryCullModule = MKT_ADDRESSOF( geom );
    }

    auto DebugOverlayModule::GetSceneVolumeSettings() -> SceneVolumeSettings & {
        MKT_BEGIN_PROFILER();
        return mSceneVolumeSettings;
    }

    auto DebugOverlayModule::GetPhysicsSettings() -> physics::PhysicsDebugDrawSettings & {
        MKT_BEGIN_PROFILER();
        return mPhysicsSettings;
    }

    auto DebugOverlayModule::GetPhysicsLineCount() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mPhysicsLineCount;
    }

    auto DebugOverlayModule::GetDroppedPhysicsLineCount() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mDroppedPhysicsLines;
    }

    auto DebugOverlayModule::RegisterSceneVolumes( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER();
        mVolumeLinesBuffer = graph.Create( FGBufferDescription{}
            .SetName( "SceneVolumes_Lines" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( kMaxVolumeLines * sizeof( DebugLine ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );

        // Share the collider line shader: each world-space segment becomes
        // two triangles, giving both backends the same adjustable line width.
        const auto pipeline{ graph.Create( FGPipelineDescription{}
            .SetName( "SceneVolumes_DepthTested" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .AddColorFormat( renderer::rhi::Format::eBGRA8_UNORM )
            .PushShader( "PhysicsDebugOverlay_Vert.slang", FGStageType::eVertex )
            .PushShader( "PhysicsDebugOverlay_Frag.slang", FGStageType::ePixel ) ) };

        graph.RegisterPass(
            "SceneVolumes_Upload",
            FGPassType::eTransfer,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( mVolumeLinesBuffer, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, core::Blackboard & ) {
                GatherSceneVolumes();

                if ( !mVolumeLines.empty() ) {
                    ctx.CopyBuffer( mVolumeLinesBuffer, 0, mVolumeLines.data(), mVolumeLines.size() * sizeof( DebugLine ) );
                }
            } );

        graph.RegisterPass(
            "DebugOverlaySceneVolumes_Render",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder &builder, core::Blackboard &b ) {
                builder.UseResource( mVolumeLinesBuffer, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PresentationPassData>().mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, pipeline]( CommandContext &ctx, core::Blackboard &b ) {
                if ( mVolumeLines.empty() ) {
                    return;
                }

                const auto [width, height]{ InferDimensions( mResolution ) };
                struct Parameters {
                    SPointer mLines;
                    SPointer mCamera;
                    core::float2 mViewport;
                    float mLineWidth;
                    float mPadding;
                } params{
                    .mLines = ctx.GetDeviceBufferAddress( mVolumeLinesBuffer ),
                    .mCamera = ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData ),
                    .mViewport = core::float2{ width, height },
                    .mLineWidth = mSceneVolumeSettings.mLineWidth,
                    .mPadding = 0,
                };
                ctx.PushConstants( params );
                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( width ), core::as<core::i32>( height ) } )
                    .AddRenderTarget( b.Get<PresentationPassData>().mColorImage, renderer::rhi::Color{}, renderer::rhi::LoadOp::eLoad )
                    .AddDepthTarget( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, renderer::rhi::LoadOp::eLoad ) );
                ctx.SetViewportState( renderer::rhi::ViewportState{}
                        .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<float>( width ), core::as<float>( height ) ) ) );
                ctx.BindPipeline( pipeline );
                ctx.Draw( 6, core::as<core::u32>( mVolumeLines.size() ) );
                ctx.EndRender();
            } );
    }

    auto DebugOverlayModule::GatherWindAreas() -> void {
        const auto view{ mSceneVolumeSettings.mWindDebugView };

        if ( !mScene || view == WindDebugView::eDisabled ) {
            return;
        }

        // Use the simulation snapshot so disabled/singular areas and the area
        // budget match physics and grass. Sampling the combined field also
        // makes overlapping zones visible without inventing a second model.
        const auto field{ mScene->GetWindField() };
        constexpr core::float4 boundaryColor{ .35f, .8f, 1.0f, 1.0f };
        constexpr core::float4 falloffColor{ .2f, .45f, .55f, 1.0f };
        constexpr core::float4 velocityColor{ 1.0f, .8f, .25f, 1.0f };

        const auto line = [this]( const core::float3 &a, const core::float3 &b, const core::float4 &color ) {
            if ( mVolumeLines.size() < kMaxVolumeLines ) {
                mVolumeLines.push_back( DebugLine{ { core::float4{ a, 1.0f }, color }, { core::float4{ b, 1.0f }, color } } );
            }
        };

        const auto arrow = [&]( const core::float3 &origin, core::f32 length ) {
            const auto velocity{ scene::SampleWind( field, origin ) };
            const float speed{ glm::length( velocity ) };

            if ( speed < .001f ) {
                return;
            }

            const auto direction{ velocity / speed };
            const auto side{ glm::normalize( glm::cross( direction, glm::abs( direction.y ) > .9f ? core::float3{ 0, 0, 1 } : core::float3{ 0, 1, 0 } ) ) };
            const float size{ length * glm::clamp( speed / 7.0f, .1f, 1.0f ) };
            const auto tip{ origin + direction * size };
            line( origin, tip, velocityColor );
            line( tip, tip - direction * size * .25f + side * size * .15f, velocityColor );
            line( tip, tip - direction * size * .25f - side * size * .15f, velocityColor );
        };

        for ( const auto &area: field.mAreas ) {
            const auto world{ glm::inverse( area.mWorldToLocal ) };
            const auto shape{ static_cast<scene::WindAreaShape>( area.mControl.x ) };
            const auto point = [&]( const core::float3 &local ) {
                return core::float3{ world * core::float4{ local, 1.0f } };
            };

            if ( shape == scene::WindAreaShape::eDirectional ) {
                // A global field has no finite boundary; keep its direction
                // visible at the authored origin even in the boundaries view.
                arrow( point( {} ), 3.0f );
                continue;
            }

            const auto boundary = [&]( core::f32 radius, const core::float4 &color ) {
                if ( shape == scene::WindAreaShape::eBox ) {
                    for ( core::u32 corner{}; corner < 8; ++corner ) {
                        const core::float3 a{ corner & 1 ? radius : -radius,
                            corner & 2 ? radius : -radius, corner & 4 ? radius : -radius };

                        for ( core::u32 axis{ 1 }; axis <= 4; axis <<= 1 ) {
                            if ( !( corner & axis ) ) {
                                auto b{ a };
                                b[axis == 1 ? 0 : axis == 2 ? 1
                                                            : 2] = radius;
                                line( point( a ), point( b ), color );
                            }
                        }
                    }
                }
                else {
                    for ( core::u32 axis{}; axis < 3; ++axis ) {
                        for ( core::u32 segment{}; segment < 48; ++segment ) {
                            const float a{ glm::two_pi<float>() * segment / 48.0f };
                            const float b{ glm::two_pi<float>() * ( segment + 1 ) / 48.0f };
                            core::float3 p{};
                            core::float3 q{};
                            p[( axis + 1 ) % 3] = glm::cos( a ) * radius;
                            p[( axis + 2 ) % 3] = glm::sin( a ) * radius;
                            q[( axis + 1 ) % 3] = glm::cos( b ) * radius;
                            q[( axis + 2 ) % 3] = glm::sin( b ) * radius;
                            line( point( p ), point( q ), color );
                        }
                    }
                }
            };

            boundary( .5f, boundaryColor );
            const float falloff{ area.mVelocityFalloff.w };

            if ( falloff > .001f && falloff < .999f ) {
                boundary( .5f * ( 1.0f - falloff ), falloffColor );
            }

            if ( view != WindDebugView::eBoundsAndVelocity ) {
                continue;
            }

            const float extent{ glm::min( glm::length( core::float3{ world[0] } ),
                glm::min( glm::length( core::float3{ world[1] } ), glm::length( core::float3{ world[2] } ) ) ) };
            const float length{ glm::clamp( extent * .16f, .25f, 8.0f ) };

            for ( core::i32 z{ -1 }; z <= 1; ++z ) {
                for ( core::i32 y{ -1 }; y <= 1; ++y ) {
                    for ( core::i32 x{ -1 }; x <= 1; ++x ) {
                        const core::float3 local{ core::float3{ x, y, z } * .3f };

                        if ( shape != scene::WindAreaShape::eSphere || glm::length2( local ) < .25f ) {
                            arrow( point( local ), length );
                        }
                    }
                }
            }
        }
    }


    auto DebugOverlayModule::GatherSceneVolumes() -> void {
        MKT_BEGIN_PROFILER();
        mVolumeLines.clear();
        const auto &settings{ mSceneVolumeSettings };

        if ( !mScene || !mCamera ||
             ( !settings.mShowBounds && settings.mWindDebugView == WindDebugView::eDisabled &&
                 !settings.mShowCameraFrustums && !settings.mShowPointLightRanges && !settings.mShowSpotLightRanges ) ) {
            return;
        }

        constexpr core::float4 boundsAndLightColor{ 70.0f / 255, 220.0f / 255, 90.0f / 255, 1.0f };
        const auto line = [this]( core::float3 a, core::float3 b, core::float4 color ) {
            if ( mVolumeLines.size() >= kMaxVolumeLines ) {
                return;
            }

            mVolumeLines.push_back( DebugLine{ { core::float4{ a, 1 }, color }, { core::float4{ b, 1 }, color } } );
        };
        const auto circle = [&]( core::float3 center, core::float3 right, core::float3 up, float radius, core::float4 color ) {
            constexpr core::u32 segments{ 48 };

            for ( core::u32 index{}; index < segments; ++index ) {
                const float a{ glm::two_pi<float>() * index / segments };
                const float b{ glm::two_pi<float>() * ( index + 1 ) / segments };
                line( center + radius * ( right * glm::cos( a ) + up * glm::sin( a ) ),
                    center + radius * ( right * glm::cos( b ) + up * glm::sin( b ) ), color );
            }
        };
        GatherWindAreas();

        // Reuse the palette storage; only read it after a successful build.
        eastl::array<core::float4x4, asset::kMaxBonesPerMesh> palette;

        for ( const auto &[id, owned]: mScene->GetEntities() ) {
            const auto &entity{ *owned };

            if ( !entity.GetComponent<scene::TagComponent>().IsActive() ) {
                continue;
            }

            const auto world{ entity.GetComponent<scene::TransformComponent>().GetWorldTransform() };

            if ( settings.mShowBounds && entity.HasComponent<scene::MeshComponent>() &&
                 entity.GetComponent<scene::MeshComponent>().HasMesh() ) {
                const auto *mesh{ entity.GetComponent<scene::MeshComponent>().GetMesh() };
                bool skinned{};

                if ( entity.HasComponent<scene::SkinnedMeshRenderer>() ) {
                    auto *animations{ animation::AnimationSystem::GetPtr() };
                    const auto *animator{ animations ? animations->GetAnimator( entity.GetComponent<scene::SkinnedMeshRenderer>().GetAnimatorID() ) : nullptr };

                    if ( animator ) {
                        const auto &pose{ animator->GetJointModelMatrices( mesh->GetSkinBinding().mRigIndex ) };
                        skinned = animation::BuildSkinningPalette( mesh->GetSkinBinding(), { pose.data(), pose.size() }, palette );
                    }
                }

                const auto bounds{ skinned ? mesh->GetSkinnedBounds( palette, world ) : mesh->GetTransformedBounds( world ) };

                if ( bounds.HasExtent() ) {
                    core::float3 corners[8];

                    for ( core::u32 index{}; index < 8; ++index ) {
                        corners[index] = { index & 1 ? bounds.mMaximum.x : bounds.mMinimum.x,
                            index & 2 ? bounds.mMaximum.y : bounds.mMinimum.y,
                            index & 4 ? bounds.mMaximum.z : bounds.mMinimum.z };
                    }

                    for ( core::u32 index{}; index < 8; ++index ) {
                        for ( core::u32 axis{ 1 }; axis <= 4; axis <<= 1 ) {
                            if ( !( index & axis ) ) {
                                line( corners[index], corners[index | axis], boundsAndLightColor );
                            }
                        }
                    }
                }
            }

            // The selection filter applies to camera/light volumes, not mesh bounds.
            if ( settings.mSelectedOnly && ( settings.mSelectedEntity == 0 || id != settings.mSelectedEntity ) ) {
                continue;
            }

            if ( settings.mShowCameraFrustums && entity.HasComponent<scene::CameraComponent>() ) {
                const auto &component{ entity.GetComponent<scene::CameraComponent>() };

                if ( component.HasCamera() ) {
                    scene::Camera camera{ static_cast<const scene::Camera &>( component.GetCamera() ) };

                    if ( !component.IsAspectRatioFixed() && settings.mGameViewport.x > 0 && settings.mGameViewport.y > 0 ) {
                        camera.SetViewportSize( settings.mGameViewport.x, settings.mGameViewport.y );
                    }

                    // Match the render camera's pose: entity scale changes neither
                    // the lens nor the near/far distances.
                    const auto pose{ scene::Camera::ExtractRenderPose( world ) };
                    const auto inverse{ pose * glm::inverse( camera.GetProjection() ) };
                    core::float3 corners[8]{};
                    bool valid{ true };

                    for ( core::u32 index{}; index < 8; ++index ) {
                        const auto corner{ inverse * core::float4{ index & 1 ? 1.0f : -1.0f, index & 2 ? 1.0f : -1.0f, index & 4 ? 1.0f : 0.0f, 1.0f } };
                        valid &= glm::abs( corner.w ) > 1e-7f;

                        if ( valid ) {
                            corners[index] = core::float3{ corner } / corner.w;
                        }
                    }

                    if ( valid ) {
                        for ( core::u32 index{}; index < 8; ++index ) {
                            for ( core::u32 axis{ 1 }; axis <= 4; axis <<= 1 ) {
                                if ( !( index & axis ) ) {
                                    line( corners[index], corners[index | axis], core::float4{ 80.0f / 255, 215.0f / 255, 1.0f, 1.0f } );
                                }
                            }
                        }
                    }
                }
            }

            if ( entity.HasComponent<scene::LightComponent>() ) {
                const auto &light{ entity.GetComponent<scene::LightComponent>() };
                const core::float3 origin{ world[3] };
                const auto &color{ boundsAndLightColor };

                if ( settings.mShowPointLightRanges && light.IsTypeActive( renderer::LightType::ePoint ) ) {
                    const auto radius{ light.Get<renderer::PointLight>().GetRadius() };
                    circle( origin, { 1, 0, 0 }, { 0, 1, 0 }, radius, color );
                    circle( origin, { 1, 0, 0 }, { 0, 0, 1 }, radius, color );
                    circle( origin, { 0, 1, 0 }, { 0, 0, 1 }, radius, color );

                    // Latitude rings describe the same spherical range from oblique viewpoints.
                    constexpr core::u32 latitudeSteps{ 6 };

                    for ( core::u32 latitude{ 1 }; latitude < latitudeSteps; ++latitude ) {
                        if ( latitude == latitudeSteps / 2 ) {
                            continue;// The equator is already drawn above.
                        }

                        const float angle{ glm::pi<float>() * ( core::as<float>( latitude ) / latitudeSteps - 0.5f ) };
                        const core::float3 center{ origin + core::float3{ 0, radius * glm::sin( angle ), 0 } };
                        circle( center, { 1, 0, 0 }, { 0, 0, 1 }, radius * glm::cos( angle ), color );
                    }
                }
                else if ( settings.mShowSpotLightRanges && light.IsTypeActive( renderer::LightType::eSpot ) ) {
                    const auto &spot{ light.Get<renderer::SpotLight>() };
                    const auto forward{ spot.GetWorldDirection( world ) };
                    const core::float3 axis{ glm::abs( forward.y ) > 0.99f ? core::float3{ 0, 0, 1 } : core::float3{ 0, 1, 0 } };
                    const auto right{ glm::normalize( glm::cross( forward, axis ) ) };
                    const auto up{ glm::cross( right, forward ) };
                    const float angle{ glm::radians( spot.GetAngle() ) };
                    const float range{ spot.GetRadius() };
                    const auto center{ origin + forward * ( range * glm::cos( angle ) ) };
                    const float radius{ range * glm::sin( angle ) };
                    circle( center, right, up, radius, color );

                    for ( const auto direction: { right, -right, up, -up } ) {
                        line( origin, center + direction * radius, color );
                        // The light's radial attenuation bounds the cone with a spherical cap.
                        for ( core::u32 segment{}; segment < 12; ++segment ) {
                            const float a{ angle * segment / 12.0f };
                            const float b{ angle * ( segment + 1 ) / 12.0f };
                            line( origin + range * ( forward * glm::cos( a ) + direction * glm::sin( a ) ),
                                origin + range * ( forward * glm::cos( b ) + direction * glm::sin( b ) ), color );
                        }
                    }
                }
            }
        }
    }

    auto DebugOverlayModule::RegisterPhysicsDebugger( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
#if defined( JPH_DEBUG_RENDERER )
        mPhysicsLines = graph.Create( FGBufferDescription{}
            .SetName( "PhysicsDebug_Lines" )
            .SetUsage( renderer::rhi::BufferUsageFlagsBits::Structured | renderer::rhi::BufferUsageFlagsBits::CopyDest )
            .SetSizeBytes( PhysicsDebugRendererSimple::kMaxLines * sizeof( DebugLine ) )
            .SetHeapType( renderer::rhi::HeapType::eDeviceLocal ) );

        auto pipelineDesc{ FGPipelineDescription{}
            .SetName( "PhysicsDebug_DepthTested" )
            .SetPipelineType( renderer::rhi::PipelineType::eGraphics )
            .SetTopology( renderer::rhi::PrimitiveTopology::eTriangleList )
            .SetCullMode( renderer::rhi::CullMode::eNone )
            .SetDepthTest( true )
            .SetDepthWrite( false )
            .SetBlendEnable( false )
            .SetDepthFormat( renderer::rhi::Format::eD32 )
            .AddColorFormat( renderer::rhi::Format::eBGRA8_UNORM )
            .PushShader( "PhysicsDebugOverlay_Vert.slang", FGStageType::eVertex )
            .PushShader( "PhysicsDebugOverlay_Frag.slang", FGStageType::ePixel ) };

        const auto depthPipeline{ graph.Create( pipelineDesc ) };
        const auto overlayPipeline{ graph.Create( pipelineDesc
                .SetName( "PhysicsDebug_ThroughGeometry" )
                .SetDepthTest( false ) ) };

        graph.RegisterPass(
            "PhysicsDebug_Upload",
            FGPassType::eTransfer,
            [this]( FGNodeBuilder &builder, core::Blackboard & ) {
                builder.UseResource( mPhysicsLines, FGPipelineStage::eCopy, FGResourceAccess::eWrite );
            },
            [this]( CommandContext &ctx, core::Blackboard & ) {
                mPhysicsLineCount = 0;
                mDroppedPhysicsLines = 0;
                auto *renderer{ physics::PhysicSystem::Get()->GetDebugRendererSimple() };

                if ( !renderer ) {
                    return;
                }
                // Gather from the scene being displayed, even before Simulate has ever run.
                // Starting a fresh capture also discards deleted bodies and previous scene data.
                renderer->BeginFrame();

                if ( !mPhysicsSettings.mEnabled || !mScene || !mCamera ) {
                    return;
                }

                auto *world{ mScene->GetPhysicsWorld() };

                if ( !world ) {
                    return;
                }

                const auto &cameraPosition{ mCamera->GetPosition() };
                renderer->SetCameraPos( JPH::RVec3{ cameraPosition.x, cameraPosition.y, cameraPosition.z } );
                world->DrawPhysics( renderer, mPhysicsSettings );
                const auto &lines{ renderer->GetLines() };
                mPhysicsLineCount = core::as<core::u32>( lines.size() );
                mDroppedPhysicsLines = renderer->GetDroppedLineCount();

                if ( !lines.empty() ) {
                    ctx.CopyBuffer( mPhysicsLines, 0, lines.data(), lines.size() * sizeof( DebugLine ) );
                }
            } );

        graph.RegisterPass(
            "DebugOverlayColliders_Render",
            FGPassType::eGraphics,
            [this]( FGNodeBuilder &builder, core::Blackboard &b ) {
                builder.UseResource( mPhysicsLines, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<CameraModuleInfo>().mCameraData, FGPipelineStage::eVertexShader, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, FGPipelineStage::eDepthTarget, FGResourceAccess::eRead );
                builder.UseResource( b.Get<PresentationPassData>().mColorImage, FGPipelineStage::eRenderTarget, FGResourceAccess::eWrite );
            },
            [this, depthPipeline, overlayPipeline]( CommandContext &ctx, core::Blackboard &b ) {
                if ( !mPhysicsSettings.mEnabled || mPhysicsLineCount == 0 ) {
                    return;
                }

                struct Parameters {
                    SPointer mLines;
                    SPointer mCamera;
                    core::float2 mViewport;
                    float mLineWidth;
                    float mPadding;
                } params{
                    .mLines = ctx.GetDeviceBufferAddress( mPhysicsLines ),
                    .mCamera = ctx.GetDeviceBufferAddress( b.Get<CameraModuleInfo>().mCameraData ),
                    .mViewport = core::float2{ InferDimensions( mResolution ).first, InferDimensions( mResolution ).second },
                    .mLineWidth = mPhysicsSettings.mLineWidth,
                    .mPadding = 0,
                };
                ctx.PushConstants( params );
                const auto [width, height]{ InferDimensions( mResolution ) };
                ctx.BeginRender( RenderBeginDescription{}
                    .SetRenderArea( renderer::rhi::Rect{ core::as<core::i32>( width ), core::as<core::i32>( height ) } )
                    .AddRenderTarget( b.Get<PresentationPassData>().mColorImage, renderer::rhi::Color{}, renderer::rhi::LoadOp::eLoad )
                    .AddDepthTarget( b.Get<PrepassModuleInfo>().mPrepassDepthTarget, renderer::rhi::LoadOp::eLoad ) );
                ctx
                    .SetViewportState( renderer::rhi::ViewportState{}
                            .AddViewportAndScissorRect( renderer::rhi::Viewport( core::as<float>( width ), core::as<float>( height ) ) ) );
                ctx.BindPipeline( mPhysicsSettings.mShowThroughGeometry ? overlayPipeline : depthPipeline );
                ctx.Draw( 6, mPhysicsLineCount );
                ctx.EndRender();
            } );
#endif
    }
}// namespace mikoto::renderer
