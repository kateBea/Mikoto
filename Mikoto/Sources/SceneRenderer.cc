// //
// // Created by zanet on 4/5/2025.
// //
//
// #include <FileSystem/FileService.hh>
// #include <Renderer/SceneRenderer.hh>
// #include <nlohmann/json.hpp>
//
// namespace Mikoto {
//
//
//     SceneRenderer::SceneRenderer( const SceneRendererCreateInfo &createInfo ) {}
//
//     auto SceneRenderer::Init() -> void {}
//
//     auto SceneRenderer::Shutdown() -> void {}
//
//     auto SceneRenderer::SetState( SceneState state ) -> void {}
//
//     auto SceneRenderer::SetScene( Scene *scene ) -> void {}
//
//     auto SceneRenderer::Render( double timeStep ) const -> void {}
//
//     auto SceneRenderer::OnResize( UInt32_T width, UInt32_T height ) -> void {}
//
//     auto SceneRenderer::SetCamera( Camera *camera ) -> void {}
//
//     auto SceneRenderer::SetRenderBackend( RendererBackend *backend ) -> void {}
//
//     auto SceneRenderer::SetRenderResolution( RenderResolution resolution ) -> void {}
//
//     auto SceneRenderer::Create( const SceneRendererCreateInfo &createInfo ) -> Scope_T<SceneRenderer> {}
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
//     }
// }// namespace Mikoto