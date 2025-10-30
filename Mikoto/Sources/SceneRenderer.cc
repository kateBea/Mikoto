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
        m_RendererBackend = RenderService::Get()->GetBackend();
    }

    auto SceneRenderer::Shutdown() -> void {
    }


    auto SceneRenderer::SetScene( Scene *scene ) -> void {
        m_Scene = scene;
    }

    auto SceneRenderer::Render( double ) -> void {

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

        m_Camera = dynamic_cast<SceneCamera*>(camera);
    }

    auto SceneRenderer::GetFinalComposition() const -> TextureHandle {
        TextureHandle handle{ TextureHandle::CreateEmpty() };
        if (m_RendererBackend) {
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