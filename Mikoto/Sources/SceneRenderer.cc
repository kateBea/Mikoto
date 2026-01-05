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
#include <Renderer/Passes/DebugPasses.hh>

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

        m_GraphicsContext->Shutdown();
        m_GraphicsContext.reset();
    }


    auto SceneRenderer::SetScene( Scene *scene ) -> void {
        m_Scene = scene;

        FinalCompositionPass* finalCompositionPass{ m_PassRegistry.Get<FinalCompositionPass>() };
        MKT_ASSERT( finalCompositionPass, "Trying to set scene for final composition pass while it is NULL" );

        finalCompositionPass->SetScene( m_Scene );

        LightCullingComp* lightCullingComp{ m_PassRegistry.Get<LightCullingComp>() };
        MKT_ASSERT( lightCullingComp, "Trying to set scene for light culling compute pass while it is NULL" );

        lightCullingComp->SetScene( m_Scene );
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

        FinalCompositionPass* finalCompositionPass{ m_PassRegistry.Get<FinalCompositionPass>() };
        MKT_ASSERT( finalCompositionPass, "Trying to set scene for final composition pass while it is NULL" );

        finalCompositionPass->SetCamera( m_Camera );

        AABBGenComp* aabbGenComp{ m_PassRegistry.Get<AABBGenComp>() };
        MKT_ASSERT( aabbGenComp, "Trying to set scene for aabb gen comp pass while it is NULL" );

        aabbGenComp->SetCamera( m_Camera );
    }

    auto SceneRenderer::GetGraph() -> FrameGraph & {
        return *m_FrameGraph;
    }

    auto SceneRenderer::Create( const SceneRendererCreateInfo &createInfo ) -> Unique<SceneRenderer> {
        return CreateScope<SceneRenderer>( createInfo );
    }

    auto SceneRenderer::SetClusterDebugVisualizer( bool enable ) -> void {
        AABBGenComp* aabbGenComp{ m_PassRegistry.Get<AABBGenComp>() };
        MKT_ASSERT( aabbGenComp, "Trying to set scene for aabb gen comp pass while it is NULL" );

        aabbGenComp->SetHeatMap( enable );
    }

    auto SceneRenderer::InitGraphicsContex() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        switch ( m_Device->GetApi() ) {
            case GraphicsAPI::VULKAN_API:
                m_GraphicsContext = GraphicsContext::Create( m_Device );
                break;
            default:
                MKT_CORE_LOGGER_CRITICAL( "SceneRenderer::InitGraphicsContex - Error Unsupported renderer API!" );
                break;
        }

        if (m_GraphicsContext) {
            m_GraphicsContext->Init();
        }
    }

    auto SceneRenderer::InitCoreFramePasses() -> void {
        if (m_GraphicsContext == nullptr) {
            return;
        }

        m_FrameGraph = FrameGraph::Create( m_GraphicsContext.get(), m_Device );

        FrameGraphBuilder builder{};

        // Create and configure Compute
        HelloTrianglePass* helloTrianglePass{ m_PassRegistry.Register<HelloTrianglePass>() };
        helloTrianglePass->Setup( builder );

        SimpleComputePass* simpleComputePass{ m_PassRegistry.Register<SimpleComputePass>() };
        simpleComputePass->Setup( builder );

        HelloTexture* helloTexture{ m_PassRegistry.Register<HelloTexture>() };
        helloTexture->Setup( builder );

        FinalCompositionPass* finalCompositionPass{ m_PassRegistry.Register<FinalCompositionPass>( m_Device ) };
        finalCompositionPass->Setup( builder );

        AABBGenComp* aabbGenComp { m_PassRegistry.Register<AABBGenComp>() };
        aabbGenComp->Setup( builder );

        LightCullingComp* lightCullingComp { m_PassRegistry.Register<LightCullingComp>() };
        lightCullingComp->Setup( builder );

        m_FrameGraph->RegisterPass( helloTrianglePass );
        m_FrameGraph->RegisterPass( simpleComputePass );
        m_FrameGraph->RegisterPass( helloTexture );
        m_FrameGraph->RegisterPass( finalCompositionPass );
        m_FrameGraph->RegisterPass( aabbGenComp );
        m_FrameGraph->RegisterPass( lightCullingComp );

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