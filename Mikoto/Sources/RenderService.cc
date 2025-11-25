/**
 * Renderer.cc
 * Created by kate on 6/5/23.
 * */

// C++ Standard Library
#include <memory>
#include <utility>

// Project Headers
#include <Common/Common.hh>
#include <Core/Profiler.hh>
#include <Logging/Logger.hh>

#include <Renderer/Core/RenderService.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>

namespace Mikoto {

    RenderService::RenderService(const RenderServiceCreateInfo& options)
        : m_Options{ options }
    {}

    auto RenderService::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO("Initializing RenderService...");

        InitContext();

        InitShaderLibrary();

        InitRendererBackend();

        m_IsInitialized = true;
    }

    auto RenderService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down RenderService..." );

        m_ShaderLibrary->Shutdown();
        m_ShaderLibrary.reset();

        m_RenderBackend->Shutdown();
        m_RenderBackend.reset();

        m_GraphicsContext->Shutdown();
        m_GraphicsContext.reset();

        m_Context->Shutdown();
        m_Context = nullptr;

        m_IsInitialized = false;
    }

    auto RenderService::Update(float) -> void {
        MKT_BEGIN_PROFILER_NAMED();

    }

    auto RenderService::PrepareFrame() const -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Context->PrepareFrame();
    }

    auto RenderService::EndFrame() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        m_Context->SubmitFrame();
    }

    auto RenderService::IsGraphicsActive( const GraphicsAPI api ) const -> bool {
        return m_ActiveAPI == api;
    }

    auto RenderService::InitContext() -> void {
        const RenderContextCreateInfo createInfo{
            .Api{ m_Options.RendererAPI },
            .TargetWindow{ m_Options.TargetWindow },
        };

        m_Context = RenderContext::Create(createInfo);
        if (!m_Context->Init()) {
            MKT_THROW_RUNTIME_ERROR( "RenderSystem::Init - Could not initialize Render context." );
        }
    }

    auto RenderService::InitRendererBackend() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        switch ( m_ActiveAPI ) {
            case GraphicsAPI::VULKAN_API:
                m_RenderBackend = CreateScope<VulkanRenderer>( GetGpuDevice(), "Vulkan Renderer" );
                m_GraphicsContext = GraphicsContext::Create( m_ActiveAPI );
                break;
            default:
                MKT_CORE_LOGGER_CRITICAL( "RenderService::CreateRendererBackend - Error Unsupported renderer API!" );
                break;
        }

        if ( m_RenderBackend ) {
            m_RenderBackend->Init();
        }

        if (m_GraphicsContext) {
            m_GraphicsContext->Init();
        }

    }

    auto RenderService::InitShaderLibrary() -> void {
        MKT_CORE_LOGGER_INFO( "Initializing ShaderLibrary..." );

        const ShaderLibraryDescription description{
            .Device{ GetGpuDevice() }
        };

        m_ShaderLibrary = CreateScope<ShaderLibrary>( description );
        if (m_ShaderLibrary) {
            m_ShaderLibrary->Init();
        }

    }
}// namespace Mikoto
