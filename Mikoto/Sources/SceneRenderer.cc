//
// Created by zanet on 4/5/2025.
//

#include <nlohmann/json.hpp>

#include <Renderer/SceneRenderer.hh>

namespace Mikoto {


     SceneRenderer::SceneRenderer( const SceneRendererCreateInfo &createInfo ) {}

     auto SceneRenderer::Init() -> void {
         // Ideally, construct render passes that can be tweaked from options specified in JSONS
     }

     auto SceneRenderer::Shutdown() -> void {

     }

     auto SceneRenderer::SetState( const SceneState state ) -> void {
         m_SceneState = state;
     }

     auto SceneRenderer::SetScene( Scene *scene ) -> void {
         m_Scene = scene;
     }
     auto SceneRenderer::Render( double ) const -> void {

     }

     auto SceneRenderer::SetCamera( Camera *camera ) -> void {
         m_Camera = camera;
     }

     auto SceneRenderer::Create( const SceneRendererCreateInfo &createInfo ) -> Unique<SceneRenderer> {
         return CreateScope<SceneRenderer>( createInfo );
     }

//
//     auto SceneRenderer::OnResize( UInt32_T width, UInt32_T height ) -> void {}
//
//     auto SceneRenderer::SetCamera( Camera *camera ) -> void {}
//
//     auto SceneRenderer::SetRenderResolution( RenderResolution resolution ) -> void {}
//
//
//     auto SceneRenderer::AddCoreRenderPasses() -> void {
//         // Gbuffer
//         GBufferPass::GBufferPassDescription gbufferDescription{
//             .ViewportWidth = m_ViewportWidth,
//             .ViewportHeight = m_ViewportHeight,
//             .m_ShaderPaths = {}
//         };
//
//         m_GBufferPass = Ref<GBufferPass>::Create( new GBufferPass( gbufferDescription ) );
//
//         m_GBufferPass->Init( m_Device );

}// namespace Mikoto