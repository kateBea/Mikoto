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
//
// #include <Core/Profiler.hh>
// #include <Memory/Allocator.hh>
// #include <Renderer/Core/CommandContext.hh>
// #include <Renderer/Passes/ShadowMappingPass.hh>
//
// namespace mikoto {
//
//     ShadowMappingPass::ShadowMappingPass( RenderResolution resolution )
//         : m_Resolution{ resolution } {}
//
//     auto ShadowMappingPass::SetScene( const Scene *scene ) -> void {
//         m_Scene = scene;
//     }
//
//     auto ShadowMappingPass::SetCamera( const Camera *camera ) -> void {
//         m_Camera = camera;
//     }
//
//     auto ShadowMappingPass::SetMeshCulling( MeshCulling &culling ) -> void {
//         m_MeshCullingPass = MKT_ADDRESSOF( culling );
//     }
//
//     auto ShadowMappingPass::RegisterPasses( FrameGraph &graph ) -> void {
//         // These would ideally render and update the shadow maps
//         // for dynamic shadow casters in a shadow texture atlas
//         // TODO: investigate. Start by first integrating shadows for one dir light and proceed with atlas integration
//         //https://www.adriancourreges.com/blog/2016/09/09/doom-2016-graphics-study/
//         RegisterDirShadowMap( graph );
//         RegisterSpotShadowMap( graph );
//         RegisterPointShadowMap( graph );
//
//         RegisterDebugViewsPass( graph );
//     }
//
//     auto ShadowMappingPass::RegisterDirShadowMap( FrameGraph &graph ) -> void {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         struct ShadowMapInfo {
//             RenderResolution Reso{ RenderResolution::UHD_3120P };
//         };
//
//         graph.RegisterPass<ShadowMapInfo>(
//             "DirectionalShadowMapPass",
//                 [this]( FramePassBuilder &b, ShadowMapInfo& data ) {
//                 MKT_BEGIN_PROFILER_NAMED();
//
//                 // Shadow pass only needs depth (color is used for debugging info for the directional light we want its map displayed)
//                 b.CreateTexture( "DirectionalShadowMapPass_ColorTarget", data.Reso, TextureFormat::RGBA8_UNORM, TextureUsage::COLOR );
//                 b.CreateTexture( "DirectionalShadowMapPass_DepthTarget", data.Reso, TextureFormat::D32_FLOAT, TextureUsage::DEPTH );
//                 b.CreateBuffer( "DirectionalShadowMapPass_CameraInfo", BufferUsage::eUniform, MKT_SIZEOF( LightCameraInfo ), 1 );
//
//                 b.UseShader( "Resources/Shaders/slang/DirLighShadows_Frag.slang", ShaderStage::eFragment );
//                 b.UseShader( "Resources/Shaders/slang/DirLighShadows_Vert.slang", ShaderStage::eVertex );
//
//                 GraphicsPipelineDescription graphicsDesc{
//                     .DepthTest{ true },
//                     .DepthWrite{ true },
//                     .AlphaBlending{ false },
//                     .PipelineCullMode{ CullMode::CULL_BACK }, // Front culling to avoid peter panning
//                     .VertexAttributesSpec{},
//                     .DepthAttachmentFormat{ TextureFormat::D32_FLOAT }
//                 };
//
//                 b.CreatePipeline( "DirectionalShadowMapPass_Pipeline", graphicsDesc );
//
//                 b.Read( "CameraInfoPass_CameraData", FrameResourceState::UniformBuffer );
//                 b.Write( "DirectionalShadowMapPass_ColorTarget", FrameResourceState::RenderTarget );
//                 b.Write( "DirectionalShadowMapPass_DepthTarget", FrameResourceState::DepthWrite );
//
//                 b.Read( "MeshCulling_GeometryInfo", FrameResourceState::UnorderedAccessView );
//                 b.Read( "MeshCulling_SkinningInfo", FrameResourceState::UnorderedAccessView );
//             },
//             [this]( CommandContext &ctx, FrameGraphBlackboard& blackboard ) -> void {
//                 MKT_BEGIN_PROFILER_NAMED();
//
//                 MKT_ASSERT( m_MeshCullingPass, "Mesh culling must be valid" );
//
//                 auto &registry{ m_Scene->GetRegistry() };
//                 auto lights{ registry.view<TransformComponent, LightComponent>() };
//
//                 if ( lights.size_hint() == 0 ) {
//                     return;
//                 }
//
//                 ctx.BindBuffer( ResourceGroup::BufferViews, "CameraInfoPass_CameraData", ResourceSlot::Slot_0 );
//                 ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "MeshCulling_GeometryInfo", ResourceSlot::Slot_0 );
//                 ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, "MeshCulling_SkinningInfo", ResourceSlot::Slot_1 );
//
//                 ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, m_MeshCullingPass->GetMeshVertices(), ResourceSlot::Slot_2 );
//                 ctx.BindBuffer( ResourceGroup::UnorderedAccessViews, m_MeshCullingPass->GetMeshIndices(), ResourceSlot::Slot_3 );
//
//                 for ( const auto &light: lights ) {
//                     auto &lightComp{ registry.get<LightComponent>( light ) };
//
//                     if ( lightComp.IsTypeActive( LightType::eDirectional ) ) {
//                         auto &transform{ registry.get<TransformComponent>( light ) };
//
//                         ctx.SetColorRenderTarget( "DirectionalShadowMapPass_ColorTarget" );
//                         ctx.SetDepthRenderTarget( "DirectionalShadowMapPass_DepthTarget" );
//
//                         ctx.SetClearColor( { 1.0f, 1.0f, 1.0f, 1.0f } );
//
//                         ctx.BeginRender();
//
//                         auto &shadowData{ blackboard.Get<ShadowMapInfo>() };
//                         const auto dimensions{ InferDimensions( shadowData.Reso ) };
//
//                         const float zNear{ m_Camera->GetNearPlane() };
//                         const float zFar{ m_Camera->GetFarPlane() };
//                         const float orthoSize{ 1000 /*dimensions.first*/ };
//
//                         m_DirectionalShadowMapCameraInfo.LightProjection =
//                                 glm::ortho(
//                                         -orthoSize,
//                                         orthoSize,
//                                         -orthoSize,
//                                         orthoSize,
//                                         zNear,
//                                         zFar );
//
//                         #if !defined(NDEBUG)
//                         #if defined( GLM_FORCE_DEPTH_ZERO_TO_ONE )
//                         //MKT_CORE_LOGGER_DEBUG( "Forced depth [0, 1]" );
//                         #endif
//                         #endif
//
//                         const glm::vec3 lightDir{ lightComp.Get<DirectionalLight>().GetDirection() };
//                         const glm::vec3 lightPos{ -lightDir * 500.0f };
//
//                         m_DirectionalShadowMapCameraInfo.LightView =
//                                 glm::lookAt(
//                                         lightPos,
//                                         glm::vec3{ 0.0f, 0.0f, 0.0f },
//                                         glm::vec3{ 0.0f, 1.0f, 0.0f } );
//
//                         ctx.PushConstants(
//                                 std::addressof( m_DirectionalShadowMapCameraInfo ),
//                                 sizeof( m_DirectionalShadowMapCameraInfo ) );
//
//                         // Reference light space info for final composition pass
//                         auto &data{ blackboard.Get<FinalShadingConstants>() };
//                         data.LightViewProjection = m_DirectionalShadowMapCameraInfo.LightProjection * m_DirectionalShadowMapCameraInfo.LightView;
//
//                         ctx.SetViewport( 0, 0, dimensions.first, dimensions.second );
//                         ctx.SetScissor( 0, 0, dimensions.first, dimensions.second );
//
//                         ctx.BindPipeline( "DirectionalShadowMapPass_Pipeline" );
//
//                         m_MeshCullingPass->DrawInstancesIndirect( ctx );
//
//                         ctx.EndRender();
//                     }
//                 }
//             } );
//     }
//
//     auto ShadowMappingPass::RegisterPointShadowMap( FrameGraph &graph ) -> void {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         graph.RegisterPass(
//             "PointLightShadowMapPass",
//             []( FramePassBuilder &b ) {
//                 MKT_BEGIN_PROFILER_NAMED();
//             },
//             []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
//                 MKT_BEGIN_PROFILER_NAMED();
//             } );
//     }
//
//     auto ShadowMappingPass::RegisterSpotShadowMap( FrameGraph &graph ) -> void {
//         MKT_BEGIN_PROFILER_NAMED();
//
//         graph.RegisterPass(
//             "SpotLightShadowMapPass",
//             []( FramePassBuilder &b ) {
//                 MKT_BEGIN_PROFILER_NAMED();
//             },
//             []( CommandContext &ctx, FrameGraphBlackboard & ) -> void {
//                 MKT_BEGIN_PROFILER_NAMED();
//             } );
//     }
//
//     auto ShadowMappingPass::RegisterDebugViewsPass( FrameGraph &graph ) -> void {
//         graph.RegisterPass(
//             "DebugViewsShadowMap",
//             []( FramePassBuilder &b ) -> void {
//                 MKT_BEGIN_PROFILER_NAMED();
//                 b.Read( "DirectionalShadowMapPass_ColorTarget", FrameResourceState::ShaderRead_GraphicsPipeline );
//             },
//             []( CommandContext &, FrameGraphBlackboard & ) -> void {
//                 MKT_BEGIN_PROFILER_NAMED();
//             } );
//     }
//
// }
