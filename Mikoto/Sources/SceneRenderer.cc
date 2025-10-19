//
// Created by zanet on 4/5/2025.
//

#include <ranges>

#include <Renderer/RenderService.hh>
#include <Renderer/SceneRenderer.hh>
#include <nlohmann/json.hpp>

namespace Mikoto {

    SceneRenderer::SceneRenderer( const SceneRendererCreateInfo &createInfo )
        : m_Device{ createInfo.Device } {}

    auto SceneRenderer::Init() -> void {
        m_RendererBackend = RenderService::Get()->CreateRendererBackend( "SceneRenderer render backend" );
        if ( m_RendererBackend ) {
            m_RendererBackend->Init();
        }

        AddCoreRenderPasses();
    }

    auto SceneRenderer::Shutdown() -> void {
    }


    auto SceneRenderer::SetScene( Scene *scene ) -> void {
        m_Scene = scene;
    }

    auto SceneRenderer::Render( double ) -> void {

        CommandListHandle cmd{ m_Device->CreateCommandList( QueueType::GRAPHICS_QUEUE ) };
        cmd->Begin();

        for ( const Unique<IPass> &pass: m_Passes | std::ranges::views::values ) {
            pass->Execute( cmd );
        }

        m_RendererBackend->SetCamera( m_Camera );

        for ( auto &pass: m_Passes | std::ranges::views::values ) {
            if ( const auto renderPass{ dynamic_cast<IRenderPass *>( pass.get() ) } ) {
                renderPass->Render( m_RendererBackend, cmd );
            }
        }

        cmd->End();
        m_Device->SubmitCommands( cmd );
    }

    auto SceneRenderer::OnResize( UInt32 width, UInt32 height ) -> void {
    }

    auto SceneRenderer::AddCoreRenderPasses() -> void {
        // Final composition
        ShadingPass* shadingPass{ m_Passes.Register<ShadingPass>() };
        shadingPass->Init( m_Device );

        // Compute basic
        ComputeBasic* computeBasic{ m_Passes.Register<ComputeBasic>() };
        computeBasic->Init( m_Device );
    }

    auto SceneRenderer::SetCamera( Camera *camera ) -> void {
        m_Camera = camera;
    }

    auto SceneRenderer::GetFinalComposition() -> TextureHandle {
        TextureHandle handle{ TextureHandle::CreateEmpty() };
        ShadingPass* shadingPass{ m_Passes.Get<ShadingPass>() };
        if (shadingPass) {
            handle = shadingPass->GetFinalComposition();
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