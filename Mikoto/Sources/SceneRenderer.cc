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

#include <ranges>

#include <Core/Profiler.hh>
#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/FramePass.hh>
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Core/RenderService.hh>
#include <Renderer/Core/SceneRenderer.hh>
#include <Renderer/Passes/DebugPasses.hh>
#include <Renderer/Passes/IBLPasses.hh>
#include <Renderer/Passes/PostEffectsPasses.hh>
#include <Renderer/Passes/ClusteredShading.hh>

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

        m_GraphicsContext->Shutdown();
        m_GraphicsContext.reset();
    }


    auto SceneRenderer::SetSceneParameters( Scene *scene ) -> void {
        ShadingPass* finalCompositionPass{ m_PassRegistry.Get<ShadingPass>() };
        MKT_ASSERT( finalCompositionPass, "Trying to set scene for final composition pass while it is NULL" );
        finalCompositionPass->SetScene( scene );

        LightCullingComp* lightCullingComp{ m_PassRegistry.Get<LightCullingComp>() };
        MKT_ASSERT( lightCullingComp, "Trying to set scene for light culling compute pass while it is NULL" );
        lightCullingComp->SetScene( scene );

        TextRenderPass* textRenderPass{ m_PassRegistry.Get<TextRenderPass>() };
        MKT_ASSERT( textRenderPass, "Trying to set scene for text render pass while it is NULL" );
        textRenderPass->SetScene( scene );

        WireFramePass* wireframePass{ m_PassRegistry.Get<WireFramePass>() };
        MKT_ASSERT( wireframePass, "Trying to set scene for wireframe render pass while it is NULL" );
        wireframePass->SetScene( scene );
    }

    auto SceneRenderer::Render( Scene* scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // If SetRenderResolution was called we will need to
        // Reconstruct render target which will require recompiling the frame graph
        if (m_WantResize) {

        }

        SetSceneParameters( scene );

        PassPreSetup();

        m_FrameGraph->Execute();
    }

    auto SceneRenderer::SetViewport( const UInt32 width, const UInt32 height ) -> void {
        m_ViewportWidth = width;
        m_ViewportHeight = height;
    }

    auto SceneRenderer::SetCamera( SceneCamera *camera ) -> void {
        m_Camera = camera;

        ShadingPass* finalCompositionPass{ m_PassRegistry.Get<ShadingPass>() };
        MKT_ASSERT( finalCompositionPass, "Trying to set scene for final composition pass while it is NULL" );

        finalCompositionPass->SetCamera( m_Camera );

        AABBGenComp* aabbGenComp{ m_PassRegistry.Get<AABBGenComp>() };
        MKT_ASSERT( aabbGenComp, "Trying to set scene for aabb gen comp pass while it is NULL" );

        aabbGenComp->SetCamera( m_Camera );

        SkyboxPass* skyboxPass{ m_PassRegistry.Get<SkyboxPass>() };
        MKT_ASSERT( skyboxPass, "Trying to set scene for Skybox pass while it is NULL" );

        skyboxPass->SetCamera( m_Camera );
    }

    auto SceneRenderer::GetGraph() -> FrameGraph & {
        return *m_FrameGraph;
    }

    auto SceneRenderer::SetClusterDebugVisualizer( bool enable ) -> void {
        AABBGenComp* aabbGenComp{ m_PassRegistry.Get<AABBGenComp>() };
        MKT_ASSERT( aabbGenComp, "Trying to set scene for aabb gen comp pass while it is NULL" );

        aabbGenComp->SetHeatMap( enable );
    }

    auto SceneRenderer::SetSkyBox( TextureHandle cubeMap ) -> void {
        m_SkyBoxTexture = cubeMap;
    }

    auto SceneRenderer::SetClearColor( const Vec4F& color ) -> void {
        m_ClearColor = color;
    }

    auto SceneRenderer::EnableSkybox( bool enable ) -> void {
        m_UseSkybox = enable;
    }

    auto SceneRenderer::GetRenderResolution() const -> RenderResolution {
        return m_RenderResolution;
    }

    auto SceneRenderer::IsRenderResolution( RenderResolution resolution ) const -> bool {
        return m_RenderResolution == resolution;
    }

    auto SceneRenderer::SetRenderResolution( RenderResolution resolution ) -> void {
        m_RenderResolution = resolution;
        m_WantResize = true;

        m_RenderTargetDimensions = InferDimensions(m_RenderResolution);
    }

    auto SceneRenderer::SetEnvironmentGamma( float value ) -> void {
        SkyboxPass* skyboxPass{ m_PassRegistry.Get<SkyboxPass>() };
        if (skyboxPass) {
            skyboxPass->SetGamma( value );
        }
    }

    auto SceneRenderer::SetEnvironmentExposure( float value ) -> void {
        SkyboxPass* skyboxPass{ m_PassRegistry.Get<SkyboxPass>() };
        if (skyboxPass) {
            skyboxPass->SetExposure( value );
        }
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

        CreateDebugPasses( );

        CreateMainPasses();

        // Add object outline and Wireframe
        for ( auto& passPtr: m_PassRegistry | std::views::values ) {
            m_FrameGraph->RegisterPass(  passPtr.get() );
        }

        m_FrameGraph->Compile();
    }

    auto SceneRenderer::PassPreSetup() -> void {
        AABBGenComp* aabbGenComp{ m_PassRegistry.Get<AABBGenComp>() };

        LightCullingComp* lightCullingComp{ m_PassRegistry.Get<LightCullingComp>() };
        lightCullingComp->SetClusterCount( aabbGenComp->GetClusterCount() );

        // Skybox
        SkyboxPass* skyboxPass{ m_PassRegistry.Get<SkyboxPass>() };
        ShadingPass* finalCompositionPass{ m_PassRegistry.Get<ShadingPass>() };

        skyboxPass->SetCubeMap( m_SkyBoxTexture );

        finalCompositionPass->EnableSkybox( m_UseSkybox );
        finalCompositionPass->SetClearColor(m_ClearColor);
    }

    auto SceneRenderer::CreateDebugPasses() -> void {
        // Debug Passes
        HelloTrianglePass* helloTrianglePass{ m_PassRegistry.Register<HelloTrianglePass>() };
        SimpleComputePass* simpleComputePass{ m_PassRegistry.Register<SimpleComputePass>() };
        HelloTexture* helloTexture{ m_PassRegistry.Register<HelloTexture>() };

        // Wireframe and Outlining
        ObjectOutlinePass* outlinePass{ m_PassRegistry.Register<ObjectOutlinePass>() };
        WireFramePass* wireFramePass{ m_PassRegistry.Register<WireFramePass>() };
    }

    auto SceneRenderer::CreateMainPasses() -> void {
        ShadingPass* finalCompositionPass{ m_PassRegistry.Register<ShadingPass>() };
        AABBGenComp* aabbGenComp { m_PassRegistry.Register<AABBGenComp>() };
        LightCullingComp* lightCullingComp { m_PassRegistry.Register<LightCullingComp>() };

        TextRenderPass* textRenderPass{ m_PassRegistry.Register<TextRenderPass>() };
        SkyboxPass* skyboxPass{ m_PassRegistry.Register<SkyboxPass>() };

        // Add IBL pre passes

        // IrradiancePass* irradiancePass{ m_PassRegistry.Register<IrradiancePass>() };
        // irradiancePass->SetExecutionPolicy( FramePassExecutionPolicy::ONCE );
        //
        // PrefilterPass* prefilterPass{ m_PassRegistry.Register<PrefilterPass>() };
        // prefilterPass->SetExecutionPolicy( FramePassExecutionPolicy::ONCE );
        //
        // BRDFLutPass* brdfLutPass{ m_PassRegistry.Register<BRDFLutPass>() };
        // brdfLutPass->SetExecutionPolicy( FramePassExecutionPolicy::ONCE );
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