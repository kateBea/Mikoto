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
        m_GraphicsContext = nullptr;
    }


    auto SceneRenderer::SetScene( Scene *scene ) -> void {
        m_Scene = scene;
    }

    auto SceneRenderer::Render( double ) const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_FrameGraph->Execute();
    }

    auto SceneRenderer::SetViewport( const UInt32 width, const UInt32 height ) -> void {
        m_ViewportWidth = width;
        m_ViewportHeight = height;
    }

    auto SceneRenderer::SetCamera( SceneCamera *camera ) -> void {

        m_Camera = camera;
    }

    auto SceneRenderer::GetGraph() -> FrameGraph & {
        return *m_FrameGraph;
    }

    auto SceneRenderer::Create( const SceneRendererCreateInfo &createInfo ) -> Unique<SceneRenderer> {
        return CreateScope<SceneRenderer>( createInfo );
    }

    auto SceneRenderer::InitGraphicsContex() -> void {
        m_GraphicsContext = RenderService::Get()->GetGraphicsContext();
    }

    auto SceneRenderer::InitCoreFramePasses() -> void {
        if (m_GraphicsContext == nullptr) {
            return;
        }

        m_FrameGraph = FrameGraph::Create( m_GraphicsContext, m_Device );

        FrameGraphBuilder builder{};

#if false
        // Create and configure shadow pass
        ShadowPass* shadowPass{ m_PassRegistry.Register<ShadowPass>() };
        shadowPass->Setup( builder );

        // Create and configure final composition
        FinalCompositionPass* finalCompositionPass{ m_PassRegistry.Register<FinalCompositionPass>() };
        finalCompositionPass->Setup( builder );

        // Create and configure Text pass
        TextPass* textPass{ m_PassRegistry.Register<TextPass>() };
        textPass->Setup( builder );

        // Create and configure Compute
        SimpleComputePass* simpleComputePass{ m_PassRegistry.Register<SimpleComputePass>() };
        simpleComputePass->Setup( builder );

#endif

        // Create and configure Compute
        HelloTrianglePass* helloTrianglePass{ m_PassRegistry.Register<HelloTrianglePass>() };
        helloTrianglePass->Setup( builder );

        SimpleComputePass* simpleComputePass{ m_PassRegistry.Register<SimpleComputePass>() };
        simpleComputePass->Setup( builder );

        HelloTexture* helloTexture{ m_PassRegistry.Register<HelloTexture>() };
        helloTexture->Setup( builder );

        m_FrameGraph->RegisterPass( helloTrianglePass );
        m_FrameGraph->RegisterPass( simpleComputePass );
        m_FrameGraph->RegisterPass( helloTexture );

        m_FrameGraph->Compile( builder );
    }

    auto SceneRendererCreateInfo::WithName( std::string_view name ) -> SceneRendererCreateInfo & {
        this->Name = name;
        return *this;
    }

    auto SceneRendererCreateInfo::WithDevice( GpuDevice *device ) -> SceneRendererCreateInfo & {
        this->Device = device;
        return *this;
    }

    MaterialViewer::MaterialViewer( RendererBackend *backend )
        : m_RendererBackend{ backend }
    {
    }
    auto MaterialViewer::SetMaterial( MaterialHandle material ) -> void {
        m_Material = material;
    }
    auto MaterialViewer::SetViewPort( float width, float height ) -> void {
        m_ViewportWidth = width;
        m_ViewportHeight = height;
    }

}// namespace Mikoto