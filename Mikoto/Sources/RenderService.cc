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
#include <Renderer/Vulkan/VulkanContext.hh>

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

        // Init the device when the context is ready
        m_Device = GpuDevice::Create({ .Api = GraphicsAPI::VULKAN_API });
        if (!m_Device) {
            MKT_THROW_RUNTIME_ERROR( "RenderContext::Create - Could not initialize GPU Device." );
        }
        m_Device->Init();

        // When we have the context and device ready,
        // we can prepare to prepare stuff to screen presentation if presentation is supported and was requested
        m_Context->PrepareForPresentation();

        m_IsInitialized = true;
    }

    auto RenderService::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down RenderService..." );

        // Device needs a valid context
        m_Device->Shutdown();
        m_Device = nullptr;

        m_Context->Shutdown();
        m_Context = nullptr;

        m_IsInitialized = false;
    }

    auto RenderService::Update(float ts) -> void {
        m_Device->RunGarbageCollection();

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
}
