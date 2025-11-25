//
// Created by zanet on 4/5/2025.
//

#include <ranges>

#include <Core/Profiler.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Core/RenderService.hh>
#include <Renderer/Core/SceneRenderer.hh>

namespace Mikoto {

    SceneRenderer::SceneRenderer( const SceneRendererCreateInfo &createInfo )
        : m_Device{ createInfo.Device } {}

    auto SceneRenderer::Init() -> void {
        InitGraphicsContex();

        InitCoreFramePasses();
    }

    auto SceneRenderer::Shutdown() -> void {
        m_Camera = nullptr;
        m_Device = nullptr;
        m_Scene = nullptr;
        m_RendererBackend = nullptr;
    }


    auto SceneRenderer::SetScene( Scene *scene ) -> void {
        m_Scene = scene;
    }

    auto SceneRenderer::Render( double ) const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        CommandListHandle cmd{ m_Device->CreateCommandList( QueueType::GRAPHICS_QUEUE ) };

        m_RendererBackend->SetCamera( m_Camera );

        m_RendererBackend->BeginRender( cmd );

        m_RendererBackend->SetViewport( 0, 0, m_ViewportWidth, m_ViewportHeight );
        m_RendererBackend->DrawScene( m_Scene );

        m_RendererBackend->EndRender();

        m_Device->SubmitCommands( cmd );
    }

    auto SceneRenderer::SetViewport( const UInt32 width, const UInt32 height ) -> void {
        m_ViewportWidth = width;
        m_ViewportHeight = height;
    }

    auto SceneRenderer::SetCamera( SceneCamera *camera ) -> void {

        m_Camera = camera;
    }

    auto SceneRenderer::GetFinalComposition() const -> TextureHandle {
        TextureHandle handle{ TextureHandle::CreateEmpty() };
        if ( m_RendererBackend ) {
            handle = m_RendererBackend->GetFinalComposition();
        }

        return handle;
    }

    auto SceneRenderer::SetRenderResolution( RenderResolution resolution ) -> void {
        m_RenderResolution = resolution;
    }

    auto SceneRenderer::GetRenderResolution() const -> RenderResolution {
        return m_RenderResolution;
    }

    auto SceneRenderer::SetClearColor( float r, float g, float b, float a ) -> void {
        m_RendererBackend->SetClearColor( r, g, b, a );
    }

    auto SceneRenderer::Create( const SceneRendererCreateInfo &createInfo ) -> Unique<SceneRenderer> {
        return CreateScope<SceneRenderer>( createInfo );
    }

    auto SceneRenderer::InitGraphicsContex() -> void {
        m_RendererBackend = RenderService::Get()->GetBackend();
        m_GraphicsContext = RenderService::Get()->GetGraphicsContext();
    }

    auto SceneRenderer::InitCoreFramePasses() -> void {
        //TODO: temporary early return
        return;

        if (m_GraphicsContext == nullptr) {
            return;
        }

        // These passes will be configurable

        // Create and configure shadow pass
        ShadowPass* shadowPass{ m_Registry.Register<ShadowPass>() };
        shadowPass->Setup( m_GraphicsContext );

        // Create and configure final composition
        FinalCompositionPass* finalCompositionPass{ m_Registry.Register<FinalCompositionPass>() };
        finalCompositionPass->Setup( m_GraphicsContext );

        // Create and configure Text pass
        TextPass* textPass{ m_Registry.Register<TextPass>() };
        textPass->Setup( m_GraphicsContext );

        // Create and configure Compute
        SimpleComputePass* simpleComputePass{ m_Registry.Register<SimpleComputePass>() };
        simpleComputePass->Setup( m_GraphicsContext );

        // Register passes
        for (auto& pass : m_Registry | std::ranges::views::values) {
            m_FrameGraph.RegisterPass( pass.get() );
        }

        m_FrameGraph.Compile( *m_GraphicsContext );
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