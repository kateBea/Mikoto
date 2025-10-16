//
// Created by zanet on 4/5/2025.
//

#include <nlohmann/json.hpp>

#include <Renderer/SceneRenderer.hh>

namespace Mikoto {

     SceneRenderer::SceneRenderer( const SceneRendererCreateInfo &createInfo ) {}

     auto SceneRenderer::Init() -> void {
         // Final composition
         Unique<ShadingPass> shadingPass{ new ShadingPass() };
         shadingPass->Init( m_Device );

         m_Passes.emplace_back( std::move(shadingPass) );

         // Compute basic
         Unique<ComputeBasic> computeBasic{ new ComputeBasic() };
         computeBasic->Init( m_Device );

         m_Passes.emplace_back( std::move(computeBasic) );
     }

     auto SceneRenderer::Shutdown() -> void {

     }


     auto SceneRenderer::SetScene( Scene *scene ) -> void {
         m_Scene = scene;
     }

     auto SceneRenderer::Render( double timeStep ) -> void {

         for ( const Unique<IPass> &pass: m_Passes ) {
             pass->Execute();
         }

         CommandListHandle cmd{ m_Device->CreateCommandList( QueueType::GRAPHICS_QUEUE ) };
         cmd->Begin();

         const FrameContext context{
            .Cmd{ cmd },
            .FrameIndex{ 0 },
            .DeltaTime{ static_cast<float>( timeStep ) }
         };
         for ( auto& pass : m_Passes ) {
             if ( const auto renderPass{ dynamic_cast<IRenderPass*>(pass.get()) }) {
                 renderPass->Render( context, m_RendererBackend );
             }
         }

         cmd->End();
     }

     auto SceneRenderer::SetCamera( Camera *camera ) -> void {
         m_Camera = camera;
     }

     auto SceneRenderer::Create( const SceneRendererCreateInfo &createInfo ) -> Unique<SceneRenderer> {
         return CreateScope<SceneRenderer>( createInfo );
     }

}// namespace Mikoto