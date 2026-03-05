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
#include <Renderer/Passes/ShaderParameteres.hh>

namespace Mikoto {

    ClusteredShading::ClusteredShading( RenderResolution resolution )
        : m_Resolution{ resolution } {}

    auto ClusteredShading::SetScene( Scene *scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Scene = scene;
    }

    auto ClusteredShading::SetCameraPass( CameraPass &camPass ) -> void {
        m_CameraPass = std::addressof( camPass );
    }

    auto ClusteredShading::RegisterPasses( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        RegisterAABB( graph );
        RegisterLightCulling( graph );

        RegisterGBuffer( graph );
        RegisterDepthPrepass( graph );
    }

    auto ClusteredShading::SetMeshCulling( MeshCulling &cullingPass ) -> void {
        m_MeshCullingPass = std::addressof( cullingPass );
    }

    auto ClusteredShading::RegisterAABB( FrameGraph &graph ) -> void {
        graph.RegisterPass(
            "GenerateAABB",
            [this]( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();

                b.CreateBuffer( "AABBGenComp_Clusters", BufferUsage::SHADER_STORAGE,
                                MKT_SIZEOF( Cluster ), m_NumClusters, ResourceUsageType::RESOURCE_USAGE_DYNAMIC );
                
                b.UseShader( "Resources/Shaders/slang/AABBGen_Comp.slang", ShaderStage::COMPUTE );
                b.CreatePipeline( "AABBGenComp_Pipeline", ComputePipelineDescription{} );

                b.Write( "AABBGenComp_Clusters", FrameResourceState::UnorderedAccessView );
                b.Read( "CameraInfoPass_CameraData", FrameResourceState::UniformBuffer );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & blackboard ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                ctx.BindBuffer( ResourceGroup::BufferViews, "CameraInfoPass_CameraData", ResourceSlot::Slot_0 );
                ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "AABBGenComp_Clusters", ResourceSlot::Slot_0 );

                auto& data{ blackboard.Get<FinalShadingConstants>() };
                data.GridSize = glm::vec4{ m_GridSizeX, m_GridSizeY, m_GridSizeZ, 0.0f };

                m_ClusterShadingParams.GridSize = glm::vec4{ m_GridSizeX, m_GridSizeY, m_GridSizeZ, 0.0f };
                m_ClusterShadingParams.ShowHeatMap = MKT_SHADER_FALSE;

                ctx.PushConstants( std::addressof( m_ClusterShadingParams ), sizeof(m_ClusterShadingParams) );
                
                ctx.BindPipeline( "AABBGenComp_Pipeline" );
                ctx.Dispatch( m_GridSizeX, m_GridSizeY, m_GridSizeZ );
            } );
    }

    auto ClusteredShading::RegisterLightCulling( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Lights.resize( MAX_LIGHTS );

        graph.RegisterPass(
            "LightCulling",
            [this]( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();

                b.CreateBuffer( "LightCullingComp_LightsBuffer", BufferUsage::SHADER_STORAGE,
                                MKT_SIZEOF( ShaderLightTypeParams ), MAX_LIGHTS, ResourceUsageType::RESOURCE_USAGE_DYNAMIC );

                b.UseShader( "Resources/Shaders/slang/LightCulling_Comp.slang", ShaderStage::COMPUTE );
                b.CreatePipeline( "LightCullingComp_Pipeline", ComputePipelineDescription{} );

                b.Read( "AABBGenComp_Clusters", FrameResourceState::UnorderedAccessView );
                b.Read( "CameraInfoPass_CameraData", FrameResourceState::UniformBuffer );

                b.Write( "LightCullingComp_LightsBuffer", FrameResourceState::UnorderedAccessView );

                b.Use( ResourceGroup::Dynamic, "CameraInfoPass_CameraData", 0 );
                b.Use( ResourceGroup::Dynamic, "AABBGenComp_Clusters", 1 );
                b.Use( ResourceGroup::Dynamic, "LightCullingComp_LightsBuffer", 2 );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                SetupLightList( ctx );

                if (m_ClusterShadingParams.ActiveLightCount == 0) {
                    return;
                }

                ctx.BindBuffer( ResourceGroup::BufferViews, "CameraInfoPass_CameraData", ResourceSlot::Slot_0 );
                ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "AABBGenComp_Clusters", ResourceSlot::Slot_0 );
                ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "LightCullingComp_LightsBuffer", ResourceSlot::Slot_1 );

                ctx.PushConstants( std::addressof( m_ClusterShadingParams ), sizeof(m_ClusterShadingParams) );
                const auto numWorkGroupsX{ ( m_NumClusters + m_LocalSize - 1 ) / m_LocalSize };

                ctx.BindPipeline( "LightCullingComp_Pipeline" );
                ctx.Dispatch( numWorkGroupsX, 1, 1 );
            } );
    }

    auto ClusteredShading::RegisterGBuffer( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "GBuffer",
            [this]( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();
                b.CreateTexture( "GBuffer_Position", m_Resolution, TextureFormat::RGBA32_FLOAT, TextureUsage::COLOR );
                b.CreateTexture( "GBuffer_Normal", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                b.CreateTexture( "GBuffer_Color", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                b.CreateTexture( "GBuffer_Depth", m_Resolution, TextureFormat::D32_FLOAT_S8_UINT, TextureUsage::DEPTH );

                b.UseShader( "Resources/Shaders/slang/Gbuffer_Vert.slang", ShaderStage::VERTEX );
                b.UseShader( "Resources/Shaders/slang/Gbuffer_Frag.slang", ShaderStage::FRAGMENT );

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .AlphaBlending{ false },
                    .PipelineCullMode{ CullMode::NONE },
                    .ColorAttachmentFormats{
                        TextureFormat::RGBA32_FLOAT,
                        TextureFormat::RGBA8_UNORM,
                        TextureFormat::RGBA8_UNORM
                    },
                    .DepthAttachmentFormat{ TextureFormat::D32_FLOAT_S8_UINT }
                };
                b.CreatePipeline( "GBuffer_Pipeline", graphicsDesc );

                b.Write( "GBuffer_Position", FrameResourceState::RenderTarget );
                b.Write( "GBuffer_Normal", FrameResourceState::RenderTarget );
                b.Write( "GBuffer_Color", FrameResourceState::RenderTarget );
                b.Write( "GBuffer_Depth", FrameResourceState::DepthWrite );

                b.Read( "CameraInfoPass_CameraData", FrameResourceState::UniformBuffer );
                b.Read( "MeshCulling_GeometryInfo", FrameResourceState::UnorderedAccessView );
                b.Read( "MeshCulling_MaterialsInfo", FrameResourceState::UnorderedAccessView );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                ctx.BindBuffer( ResourceGroup::BufferViews, "CameraInfoPass_CameraData", ResourceSlot::Slot_0 );
                ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "MeshCulling_GeometryInfo", ResourceSlot::Slot_0 );
                ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "MeshCulling_MaterialsInfo", ResourceSlot::Slot_1 );

                ctx.BindGroup( ResourceGroup::UnboundedImageViews, "Texture2D_List" );

                ctx.SetColorRenderTarget( "GBuffer_Position" );
                ctx.SetColorRenderTarget( "GBuffer_Normal" );
                ctx.SetColorRenderTarget( "GBuffer_Color" );
                ctx.SetDepthRenderTarget( "GBuffer_Depth" );

                ctx.BeginRender();

                const auto dimensions{ InferDimensions( m_Resolution ) };

                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                ctx.BindPipeline( "GBuffer_Pipeline" );

                m_MeshCullingPass->DrawInstances( ctx );

                ctx.EndRender();
            } );
    }

    auto ClusteredShading::RegisterDepthPrepass( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "DepthPrePass",
            [this]( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();

                // Color attachment for debug
                b.CreateTexture("DepthPrePass_Color", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );

                b.CreateTexture( "DepthPrePass_Depth", m_Resolution, TextureFormat::D32_FLOAT_S8_UINT, TextureUsage::DEPTH );

                b.UseShader("Resources/Shaders/slang/ZPass_Vert.slang", ShaderStage::VERTEX );
                b.UseShader("Resources/Shaders/slang/ZPass_Frag.slang", ShaderStage::FRAGMENT );
                
                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .AlphaBlending{ false },
                    .PipelineCullMode{ CullMode::NONE },
                };

                b.CreatePipeline( "DepthPrePass_Pipeline", graphicsDesc );

                b.Write( "DepthPrePass_Color", FrameResourceState::RenderTarget );
                b.Write( "DepthPrePass_Depth", FrameResourceState::DepthWrite );

                b.Read( "CameraInfoPass_CameraData", FrameResourceState::UniformBuffer );
                b.Read( "MeshCulling_MaterialsInfo", FrameResourceState::UnorderedAccessView );
            },
            
            [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                ctx.BindPipeline( "DepthPrePass_Pipeline" );

                ctx.SetColorRenderTarget( "DepthPrePass_Color" );
                ctx.SetDepthRenderTarget( "DepthPrePass_Depth" );

                ctx.BindBuffer( ResourceGroup::BufferViews, "CameraInfoPass_CameraData", ResourceSlot::Slot_0 );
                ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "MeshCulling_GeometryInfo", ResourceSlot::Slot_0 );
                
                ctx.SetClearColor( { 1.0f, 1.0f, 1.0f, 1.0f } );

                ctx.BeginRender();

                const auto dimensions{ InferDimensions( m_Resolution ) };

                ctx.SetViewport( 0, 0, dimensions.first, dimensions.second );
                ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                m_MeshCullingPass->DrawInstances( ctx );

                ctx.EndRender();
            } );
    }

    auto ClusteredShading::SetupLightList( CommandContext &ctx ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        auto &registry{ m_Scene->GetRegistry() };
        const auto lightsView{ registry.view<TagComponent, TransformComponent, LightComponent>() };

        Int32 lightsCount{};

        for (auto &lightEntity: lightsView) {
            if (lightsCount >= MAX_LIGHTS) {
                break;
            }

            TagComponent &tag{ registry.get<TagComponent>( lightEntity ) };
            LightComponent &lightComp{ registry.get<LightComponent>( lightEntity ) };
            TransformComponent &transformCom{ registry.get<TransformComponent>( lightEntity ) };

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
                    uboLight.Diffuse = Vec4F( spot.GetColor(), 1.0f );

                    uboLight.CutOff = spot.GetCutOff();
                    uboLight.OuterCutOff = spot.GetOuterCutOff();

                    uboLight.Intensity = spot.GetIntensity();
                    uboLight.Radius = spot.GetRadius();

                    uboLight.ActiveLightType = static_cast<Int32>( ShaderActiveLightType::LIGHT_TYPE_SPOT );

                    break;
                }

                case LightType::DIRECTIONAL_LIGHT_TYPE: {
                    auto &dir{ lightComp.Get<DirectionalLight>() };

                    uboLight.Intensity = dir.GetIntensity();
                    uboLight.Direction = Vec4F( dir.GetDirection(), 0.0f );
                    uboLight.Position = Vec4F( transformCom.GetTranslation(), 1.0f );
                    uboLight.Diffuse = Vec4F( dir.GetColor() * dir.GetIntensity(), 1.0f );
                    uboLight.ActiveLightType = static_cast<Int32>( ShaderActiveLightType::LIGHT_TYPE_DIRECTIONAL );

                    break;
                }
            }

            ++lightsCount;
        }

        m_ClusterShadingParams.ActiveLightCount = lightsCount;

        ctx.CopyBuffer( "LightCullingComp_LightsBuffer", m_Lights.data(), lightsCount * MKT_SIZEOF( ShaderLightTypeParams ) );
    }
}
