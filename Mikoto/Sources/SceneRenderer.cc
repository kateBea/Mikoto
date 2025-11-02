//
// Created by zanet on 4/5/2025.
//

#include <Core/Profiler.hh>
#include <Renderer/RenderService.hh>
#include <Renderer/SceneRenderer.hh>

namespace Mikoto {

    static auto TestCode() -> void {
        // for reference usage framegraph later
        // FrameGraph frameGraph;
        //
        // // Scene data
        // std::vector<MeshHandle> shadowCasters = loadShadowCasters();
        // std::vector<MeshHandle> opaqueObjects = loadOpaqueObjects();
        // LightHandle sunLight = createDirectionalLight();
        //
        // // Text to render
        // std::vector<TextPass::TextRenderData> uiTexts = {
        //     { "FPS: 60", 10, 10, 1.0f, { 1, 1, 1, 1 }, defaultFont },
        //     { "Score: 1337", 10, 40, 1.0f, { 1, 1, 0, 1 }, defaultFont }
        // };
        //
        // // Create passes
        // auto shadowPass = std::make_unique<ShadowPass>( sunLight, shadowCasters );
        // auto pbrPass = std::make_unique<PBRPass>( opaqueObjects, shadowPass->getShadowMap() );
        // auto textPass = std::make_unique<TextPass>( pbrPass->getColorOutput(), uiTexts );
        //
        // // Add to frame graph
        // frameGraph.addPass( std::move( shadowPass ) );
        // frameGraph.addPass( std::move( pbrPass ) );
        // frameGraph.addPass( std::move( textPass ) );
        //
        // // Execute
        // auto renderer = std::make_unique<VulkanBackend>();
        // frameGraph.compile( *renderer );
        // frameGraph.execute( *renderer );
    }

    SceneRenderer::SceneRenderer( const SceneRendererCreateInfo &createInfo )
        : m_Device{ createInfo.Device } {}

    auto SceneRenderer::Init() -> void {
        m_RendererBackend = RenderService::Get()->GetBackend();
    }

    auto SceneRenderer::Shutdown() -> void {
    }


    auto SceneRenderer::SetScene( Scene *scene ) -> void {
        m_Scene = scene;
    }

    auto SceneRenderer::Render( double ) -> void {
        MKT_BEGIN_PROFILER_NAMED();


        CommandListHandle cmd{ m_Device->CreateCommandList( QueueType::GRAPHICS_QUEUE ) };

        m_RendererBackend->SetCamera( m_Camera );

        // Renderer backend will know where it opens the command list
        // here we will just submit it
        m_RendererBackend->BeginRender( cmd );

        m_RendererBackend->SetViewport( 0, 0, m_Camera->GetViewPort().first, m_Camera->GetViewPort().second );
        m_RendererBackend->DrawScene( m_Scene );

        m_RendererBackend->EndRender();

        m_Device->SubmitCommands( cmd );
    }

    auto SceneRenderer::OnResize( UInt32 width, UInt32 height ) -> void {
    }

    auto SceneRenderer::SetCamera( SceneCamera *camera ) -> void {

        m_Camera = dynamic_cast<SceneCamera *>( camera );
    }

    auto SceneRenderer::GetFinalComposition() const -> TextureHandle {
        TextureHandle handle{ TextureHandle::CreateEmpty() };
        if ( m_RendererBackend ) {
            handle = m_RendererBackend->GetFinalComposition();
        }

        return handle;
    }

    auto SceneRenderer::Create( const SceneRendererCreateInfo &createInfo ) -> Unique<SceneRenderer> {
        return CreateScope<SceneRenderer>( createInfo );
    }

    auto SceneRendererCreateInfo::WithName( std::string_view name ) -> SceneRendererCreateInfo & {
        this->Name = name;
        return *this;
    }

    auto SceneRendererCreateInfo::WithDevice( GpuDevice *device ) -> SceneRendererCreateInfo & {
        this->Device = device;
        return *this;
    }

}// namespace Mikoto