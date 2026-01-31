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
#include <Renderer/Core/GraphicsContext.hh>
#include <Renderer/Core/RenderService.hh>
#include <Renderer/Core/SceneRenderer.hh>
#include <Renderer/Passes/DebugPasses.hh>
#include <Renderer/Passes/IBLPasses.hh>
#include <Renderer/Passes/MeshCulling.hh>
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

    auto SceneRenderer::Render( Scene* scene ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_MeshCulling.SetScene( scene );
        m_IBLPasses.SetScene( scene );
        m_DebugPasses.SetScene( scene );
        m_PostEffectsPasses.SetScene( scene );
        m_ClusteredShadingPasses.SetScene( scene );

        OnPreRender();

        m_FrameGraph->Execute();

        OnPostRender();
    }

    auto SceneRenderer::SetCamera( SceneCamera *camera ) -> void {
        m_IBLPasses.SetCamera( camera );
        m_ClusteredShadingPasses.SetCamera( camera );
    }

    auto SceneRenderer::SetSkyBox( TextureHandle cubeMap ) -> void {
        m_IBLPasses.SetCubeMap( cubeMap );
    }

    auto SceneRenderer::SetClearColor( const Vec4F& color ) -> void {
        m_IBLPasses.SetClearColor( color );
    }

    auto SceneRenderer::EnableSkybox( bool enable ) -> void {
        m_IBLPasses.EnableSkybox( enable );
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
        m_IBLPasses.SetGamma( value );
    }

    auto SceneRenderer::SetEnvironmentExposure( float value ) -> void {
        m_IBLPasses.SetExposure( value );
    }

    auto SceneRenderer::GetTexture( std::string_view name ) const -> TextureHandle {
        return m_FrameGraph->GetTexture(name);
    }

    auto SceneRenderer::GetBuffer( std::string_view name ) const -> BufferHandle {
        return m_FrameGraph->GetBuffer(name);
    }

    auto SceneRenderer::Create( const SceneRendererCreateInfo &spec ) -> Unique<SceneRenderer> {
        return CreateScope<SceneRenderer>( spec );
    }

    auto SceneRenderer::InitGraphicsContex() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_GraphicsContext = GraphicsContext::Create( m_Device );

        if (m_GraphicsContext) {
            m_GraphicsContext->Init();
        }
    }

    auto SceneRenderer::InitCoreFramePasses() -> void {
        if (m_GraphicsContext == nullptr) {
            return;
        }

        m_FrameGraph = FrameGraph::Create( m_GraphicsContext.get(), m_Device );

        m_MeshCulling.RegisterPasses( *m_FrameGraph );
        m_DebugPasses.RegisterPasses( *m_FrameGraph );
        m_ClusteredShadingPasses.RegisterPasses( *m_FrameGraph );
        m_IBLPasses.RegisterPasses( *m_FrameGraph );
        m_PostEffectsPasses.RegisterPasses( *m_FrameGraph, m_Device );

        m_FrameGraph->Compile();
    }

    auto SceneRenderer::OnPreRender() -> void {
        // If SetRenderResolution was called we will need to
        // Reconstruct render target which will require recompiling the frame graph
        if (m_WantResize) {

        }

        m_IBLPasses.SetMeshCulling( m_MeshCulling );
    }

    auto SceneRenderer::OnPostRender() -> void {

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