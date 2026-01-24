//    Copyright 2025 ケイト
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

#include <Core/Profiler.hh>

#include <Scene/Scene.hh>
#include <Scene/Camera.hh>
#include <Scene/Component.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/ClusteredShading.hh>
#include <Renderer/Passes/ShaderRenderParams.hh>

namespace Mikoto {

    ClusteredShading::ClusteredShading( RenderResolution resolution )
        : m_Resolution{ resolution } {}

    auto ClusteredShading::SetScene( Scene *scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Scene = scene;
    }

    auto ClusteredShading::SetCamera( const Camera *camera ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Camera = camera;
    }

    auto ClusteredShading::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Lights.resize( MAX_LIGHTS );

        BuildAABB( graph );
        BuildLightCulling( graph );
        BuildShadowMapping( graph );
    }

    auto ClusteredShading::BuildAABB( FrameGraph &graph ) -> void {
        graph.RegisterPass(
                "GenerateAABB",
                [this]( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    b.Create<Buffer>( "AABBGenComp_CameraUBO", BufferUsage::UNIFORM, sizeof( CameraUBO ), 1 );
                    b.Create<Buffer>( "AABBGenComp_Clusters", BufferUsage::SSBO, m_NumClusters * sizeof( Cluster ) );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/AABBGen_Comp.sprv", ShaderStage::COMPUTE );
                    b.Create<Pipeline>( "AABBGenComp_Pipeline", ComputePipelineDescription{} );

                    b.Write( "AABBGenComp_Clusters", FrameResourceState::ShaderResource_Read );
                    b.Write( "AABBGenComp_CameraUBO", FrameResourceState::ShaderResource_Read );

                    b.UseSrg( SRGType::SRG_PerPass, "AABBGenComp_CameraUBO", 0 );
                    b.UseSrg( SRGType::SRG_PerPass, "AABBGenComp_Clusters", 1 );
                },
                [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();
                    ctx.BindPipeline( "AABBGenComp_Pipeline" );

                    m_CameraUBO = {
                        .ViewMatrix{ m_Camera->GetViewMatrix() },
                        .InverseProjection{ glm::inverse( m_Camera->GetProjection() ) },

                        .GridSize{ glm::vec4{ m_GridSizeX, m_GridSizeY, m_GridSizeZ, 0.0f } },
                        .ViewPosition{ glm::vec4{ m_Camera->GetPosition(), 0.0f } },

                        .Screen{ m_Camera->GetNearPlane(), m_Camera->GetFarPlane(), 1920.0f, 1080.0f },
                        .LightInfo{ m_CameraUBO.LightInfo.x }
                    };

                    ctx.UploadBuffer<CameraUBO>( "AABBGenComp_CameraUBO", m_CameraUBO );
                    ctx.Dispatch( m_GridSizeX, m_GridSizeY, m_GridSizeZ );
                } );
    }

    auto ClusteredShading::BuildLightCulling( FrameGraph &graph ) -> void {
        graph.RegisterPass(
                "LightCulling",
                [this]( FramePassBuilder &b ) {
                    MKT_BEGIN_PROFILER_NAMED();

                    b.Create<Buffer>( "LightCullingComp_LightsCullingInfo", BufferUsage::UNIFORM, sizeof( LightCullingUBO ), 1 );
                    b.Create<Buffer>( "LightCullingComp_LightsBuffer", BufferUsage::SSBO, sizeof( ShaderLightTypeParams ) * m_Lights.size() );

                    b.UseShader( "Resources/Shaders/vulkan-spirv/LightCulling_Comp.sprv", ShaderStage::COMPUTE );
                    b.Create<Pipeline>( "LightCullingComp_Pipeline", ComputePipelineDescription{} );

                    b.Read( "AABBGenComp_CameraUBO", FrameResourceState::ShaderResource_Read );
                    b.Read( "AABBGenComp_Clusters", FrameResourceState::ShaderResource_Read );

                    b.Write( "LightCullingComp_LightsBuffer", FrameResourceState::ShaderResource_Read );
                    b.Write( "LightCullingComp_LightsCullingInfo", FrameResourceState::ShaderResource_Read );

                    b.UseSrg( SRGType::SRG_PerPass, "AABBGenComp_CameraUBO", 0 );
                    b.UseSrg( SRGType::SRG_PerPass, "AABBGenComp_Clusters", 1 );
                    b.UseSrg( SRGType::SRG_PerPass, "LightCullingComp_LightsBuffer", 2 );
                    b.UseSrg( SRGType::SRG_PerPass, "LightCullingComp_LightsCullingInfo", 3 );
                },
                [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                    MKT_BEGIN_PROFILER_NAMED();

                    ctx.BindPipeline( "LightCullingComp_Pipeline" );

                    SetupLightList( ctx );

                    const auto numWorkGroupsX{ ( m_NumClusters + m_LocalSize - 1 ) / m_LocalSize };

                    ctx.Dispatch( numWorkGroupsX, 1, 1 );
                } );
    }

    auto ClusteredShading::SetupLightList( CommandContext &ctx ) -> void {
        auto &registry{ m_Scene->GetRegistry() };
        auto lightsView{ registry.view<TagComponent, TransformComponent, LightComponent>() };

        Int32 lightsCount{};

        for (auto &lightEntity: lightsView) {
            TagComponent &tag{ registry.get<TagComponent>( lightEntity ) };
            LightComponent &lightComp{ registry.get<LightComponent>( lightEntity ) };
            TransformComponent &transformCom{ registry.get<TransformComponent>( lightEntity ) };

            if (lightsCount >= MAX_LIGHTS) { break; }

            auto &uboLight{ m_Lights[lightsCount] };

            if (!tag.IsActive()) {
                uboLight.ActiveLightType = static_cast<Int32>( ShaderActiveLightType::LIGHT_TYPE_INACTIVE );
                continue;
            }

            switch (lightComp.GetActiveType()) {
                case LightType::POINT_LIGHT_TYPE: {
                    auto &point{ lightComp.Get<PointLight>() };

                    uboLight.Position = Vec4F( transformCom.GetTranslation(), 1.0f );
                    uboLight.Diffuse = Vec4F( point.GetColor(), 0.0f );

                    uboLight.Intensity = point.GetIntensity();
                    uboLight.Radius = point.GetRadius();

                    uboLight.ActiveLightType = static_cast<Int32>( ShaderActiveLightType::LIGHT_TYPE_POINT );

                    break;
                }

                case LightType::SPOT_LIGHT_TYPE: {
                    auto &spot{ lightComp.Get<SpotLight>() };

                    uboLight.Position = Vec4F( transformCom.GetTranslation(), 1.0f );
                    uboLight.Direction = Vec4F( spot.GetDirection(), 0.0f );
                    uboLight.Diffuse = Vec4F( spot.GetColor() * spot.GetIntensity(), 1.0f );

                    uboLight.CutOff = spot.GetCutOff();
                    uboLight.OuterCutOff = spot.GetOuterCutOff();

                    uboLight.Intensity = spot.GetIntensity();
                    uboLight.Radius = spot.GetRadius();

                    uboLight.ActiveLightType = static_cast<Int32>( ShaderActiveLightType::LIGHT_TYPE_SPOT );

                    break;
                }

                case LightType::DIRECTIONAL_LIGHT_TYPE: {
                    auto &dir{ lightComp.Get<DirectionalLight>() };

                    uboLight.Direction = Vec4F( dir.GetDirection(), 0.0f );
                    uboLight.Position = Vec4F( transformCom.GetTranslation(), 1.0f );// optional for shadows
                    uboLight.Diffuse = Vec4F( dir.GetColor() * dir.GetIntensity(), 1.0f );
                    uboLight.ActiveLightType = static_cast<Int32>( ShaderActiveLightType::LIGHT_TYPE_DIRECTIONAL );

                    break;
                }
            }

            ++lightsCount;
        }

        m_LightCullingUBO.LightCount = lightsCount;

        // Just copy the amount of active lights we visited
        ctx.UploadBuffer( "LightCullingComp_LightsBuffer", m_Lights.data(), lightsCount * sizeof( ShaderLightTypeParams ) );
        ctx.UploadBuffer( "LightCullingComp_LightsCullingInfo", std::addressof( m_LightCullingUBO ), sizeof( LightCullingUBO ) );
    }

    auto ClusteredShading::BuildShadowMapping( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();
    }
}
