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

#include <Core/Profiler.hh>
#include <Memory/Allocator.hh>

#include <Renderer/Core/CommandContext.hh>
#include <Renderer/Passes/ShadowMappingPass.hh>

namespace Mikoto {

    ShadowMappingPass::ShadowMappingPass( RenderResolution resolution )
        : m_Resolution{ resolution } {}

    auto ShadowMappingPass::SetScene( const Scene *scene ) -> void {
        m_Scene = scene;
    }

    auto ShadowMappingPass::SetCamera( const Camera *camera ) -> void {
        m_Camera = camera;
    }

    auto ShadowMappingPass::SetMeshCulling( MeshCulling &culling ) -> void {
        m_MeshCullingPass = MKT_ADDRESSOF( culling );
    }

    auto ShadowMappingPass::RegisterPasses( FrameGraph &graph ) -> void {
        // These would ideally render and update the shadow maps
        // for dynamic shadow casters in a shadow texture atlas
        // TODO: investigate. Start by first integrating shadows for one dir light and proceed with atlas integration
        //https://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/
        RegisterDirShadowMap( graph );
        RegisterSpotShadowMap( graph );
        RegisterPointShadowMap( graph );
    }

    auto ShadowMappingPass::RegisterDirShadowMap( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "DirectionalShadowMapPass",
            [this]( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();

                b.CreateTexture( "DirectionalShadowMapPass_ColorTarget", m_Resolution, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
                b.CreateTexture( "DirectionalShadowMapPass_DepthTarget", m_Resolution, TextureFormat::D32_FLOAT, TextureUsage::DEPTH );

                b.CreateBuffer( "DirectionalShadowMapPass_CameraInfo", BufferUsage::UNIFORM, MKT_SIZEOF( LightCameraInfo ), 1 );

                b.UseShader( "Resources/Shaders/slang/DirLighShadows_Frag.slang", ShaderStage::FRAGMENT );
                b.UseShader( "Resources/Shaders/slang/DirLighShadows_Vert.slang", ShaderStage::VERTEX );

                GraphicsPipelineDescription graphicsDesc{
                    .DepthTest{ true },
                    .DepthWrite{ true },
                    .DepthAttachmentFormat{ TextureFormat::D32_FLOAT }
                };

                b.CreatePipeline( "DirectionalShadowMapPass_Pipeline", graphicsDesc );

                b.Write( "DirectionalShadowMapPass_ColorTarget", FrameResourceState::RenderTarget );
                b.Write( "DirectionalShadowMapPass_DepthTarget", FrameResourceState::DepthWrite );

                b.Read( "MeshCulling_GeometryInfo", FrameResourceState::UnorderedAccessView );
            },
            [this]( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();

                MKT_ASSERT( m_MeshCullingPass, "Mesh culling must be valid" );

                auto &registry{ m_Scene->GetRegistry() };
                auto lights{ registry.view<TransformComponent, LightComponent>() };

                if (lights.size_hint() == 0) {
                    return;
                }

                ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "MeshCulling_GeometryInfo", ResourceSlot::Slot_0 );

                for (const auto &light : lights) {
                    auto &lightComp{ registry.get<LightComponent>( light ) };

                    if (lightComp.IsTypeActive( LightType::DIRECTIONAL_LIGHT_TYPE )) {
                        auto &transform{ registry.get<TransformComponent>( light ) };

                        ctx.SetColorRenderTarget( "DirectionalShadowMapPass_ColorTarget" );
                        ctx.SetDepthRenderTarget( "DirectionalShadowMapPass_DepthTarget" );

                        ctx.BeginRender();

                        constexpr float zNear{ 0.01f };
                        constexpr float zFar{ 2000.0f };
                        constexpr float degreesFOV{ 45.0f };

                        m_DirectionalShadowMapCameraInfo.LightView = glm::lookAt(transform.GetTranslation(), glm::vec3(0.0f), glm::vec3(0, 1, 0));
                        m_DirectionalShadowMapCameraInfo.LightProjection = glm::perspective(glm::radians(degreesFOV), 1.0f, zNear, zFar);

                        ctx.PushConstants( std::addressof( m_DirectionalShadowMapCameraInfo ), sizeof(m_DirectionalShadowMapCameraInfo ) );

                        const auto dimensions{ InferDimensions( m_Resolution ) };
                        ctx.SetViewport( 0, 0, dimensions.first, dimensions.second );
                        ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );

                        ctx.BindPipeline( "DirectionalShadowMapPass_Pipeline" );
                        m_MeshCullingPass->DrawInstances( ctx );

                        // We then copy this data to an depth image that is part of an unbounded image view set (just a bindless texture list of shadow maps, same for
                        // the other shadow maps ligt types

                        ctx.EndRender();
                    }
                }
            } );
    }

    auto ShadowMappingPass::RegisterPointShadowMap( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "PointLightShadowMapPass",
            []( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();
            },
            []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
            } );
    }

    auto ShadowMappingPass::RegisterSpotShadowMap( FrameGraph &graph ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        graph.RegisterPass(
            "SpotLightShadowMapPass",
            []( FramePassBuilder &b ) {
                MKT_BEGIN_PROFILER_NAMED();
            },
            []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
                MKT_BEGIN_PROFILER_NAMED();
            } );
    }

}
