/**
 * Renderer.cc
 * Created by kate on 6/5/23.
 * */

// C++ Standard Library
#include <memory>
#include <utility>

// Project Headers
#include <Common/Common.hh>
#include <Renderer/RenderService.hh>
#include <Renderer/RenderCommand.hh>
#include <Renderer/RenderQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {

    RenderService::RenderService(const RenderServiceCreateInfo& options)
        : m_Options{ options }
    {}

    auto RenderService::Init() -> void {
        const RenderContextCreateInfo createInfo{
            .TargetWindow{ m_Options.TargetWindow },
        };

        m_BackendPool.Init( 10 );

        m_Context = RenderContext::Create(createInfo);
        MKT_ASSERT( m_Context, "RenderSystem::Init - Assertion failed. Could not create a valid Render context." );
        if (!m_Context->Init()) {
            MKT_THROW_RUNTIME_ERROR( "RenderSystem::Init - Could not initialize Render context." );
        }

        m_IsInitialized = true;
    }

    auto RenderService::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        m_Context->Shutdown();
        m_Context = nullptr;
    }

    auto RenderService::Update(float ts) -> void {

    }

    auto RenderService::PrepareFrame() const -> void {
        m_Context->PrepareFrame();
    }

    auto RenderService::EndFrame() -> void {
        Flush();
    }

    auto RenderService::CreateBackend() -> RendererBackend * {
        RendererDescription description{
                .Name{ "Editor Main renderer " },
                .GraphicsDevice{ m_Context->GetGraphicsDevice() },
                .RendererAPI{ m_Options.RendererAPI },
        };
        RendererBackend* result{ m_BackendPool.Allocate( description ) };

        if ( result ) {
            result->Init();
        } else {
            MKT_APP_LOGGER_ERROR( "RenderService::CreateBackend - Failed to create the editor renderer." );
        }

        return result;
    }

    auto RenderService::RegisterRenderCommand( RenderCommand &&command ) -> void {
        m_Commands.Submit( std::move( command ) );
    }

    auto RenderService::Flush() -> void {
        // Execute all the commands, render commands can include stuff not directly related to the API
        // For instance if we want to change the size of the viewport that is not a command
        // that is directly related to the API, but it is a command that is necessary for rendering
        // Even tho it is later needed to determine the size of the image we render to
        m_Commands.Flush();

        m_Context->SubmitFrame();
    }
}
