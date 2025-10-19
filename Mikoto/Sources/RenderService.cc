/**
 * Renderer.cc
 * Created by kate on 6/5/23.
 * */

// C++ Standard Library
#include <memory>
#include <utility>

// Project Headers
#include <Common/Common.hh>
#include <Logging/Logger.hh>
#include <Renderer/RenderService.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>

namespace Mikoto {

    RenderService::RenderService(const RenderServiceCreateInfo& options)
        : m_Options{ options }
    {}

    auto RenderService::Init() -> void {
        MKT_CORE_LOGGER_INFO("Initializing RenderService...");

        const RenderContextCreateInfo createInfo{
            .Api{ m_Options.RendererAPI },
            .TargetWindow{ m_Options.TargetWindow },
        };

        m_Context = RenderContext::Create(createInfo);
        if (!m_Context->Init()) {
            MKT_THROW_RUNTIME_ERROR( "RenderSystem::Init - Could not initialize Render context." );
        }

        m_IsInitialized = true;
    }

    auto RenderService::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down RenderService..." );

        m_RenderBackends.clear();

        m_Context->Shutdown();
        m_Context = nullptr;

        m_IsInitialized = false;
    }

    auto RenderService::Update(float ts) -> void {


    }

    auto RenderService::PrepareFrame() const -> void {
        m_Context->PrepareFrame();
    }

    auto RenderService::EndFrame() -> void {
        Flush();
    }

    auto RenderService::IsGraphicsActive( const GraphicsAPI api ) const -> bool {
        return m_ActiveAPI == api;
    }

    auto RenderService::Flush() -> void {

        m_Context->SubmitFrame();
    }

    auto RenderService::CreateRendererBackend( const std::string_view name) -> RendererBackend * {
        RendererBackend* renderer{ nullptr };

        switch ( m_ActiveAPI ) {
            case GraphicsAPI::VULKAN_API:

                renderer = m_RenderBackends.emplace_back(CreateScope<VulkanRenderer>( GetGpuDevice(), name )).get();
            break;
            default:
                MKT_CORE_LOGGER_CRITICAL( "RenderService::CreateRendererBackend - Error Unsupported renderer API!" );
            break;
        }

        return renderer;
    }
}
