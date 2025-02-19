/**
 * Renderer.cc
 * Created by kate on 6/5/23.
 * */

// C++ Standard Library
#include <memory>
#include <utility>

// Project Headers
#include <Common/Common.hh>
#include <Core/System/RenderSystem.hh>
#include <Renderer/Core/RenderCommand.hh>
#include <Renderer/Core/RenderQueue.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {

    auto RenderSystem::Init() -> void {
        const RenderContextCreateInfo createInfo{
            .TargetWindow{ m_Options.TargetWindow },
            .Backend{ m_Options.Options.RendererAPI }
        };

        m_Context = RenderContext::Create(createInfo);

        MKT_ASSERT( m_Context, "RenderSystem::Init - Assertion failed. Could not create a valid Render context." );

        if (!m_Context->Init()) {
            MKT_THROW_RUNTIME_ERROR( "RenderSystem::Init - Could not initialize Render context." );
        }
    }

    auto RenderSystem::Shutdown() -> void {
        m_Context->Shutdown();
    }

    auto RenderSystem::Update() -> void {
    }

    auto RenderSystem::PrepareFrame() const -> void {
        m_Context->PrepareFrame();
    }

    auto RenderSystem::EndFrame() const -> void {
        Flush();
    }

    auto RenderSystem::Flush() const -> void {
        // Execute all the commands, render commands can include stuff not directly related to the API
        // For instance if we want to change the size of the viewport that is not a command
        // that is directly related to the API, but it is a command that is necessary for rendering
        // Even tho it is later needed to determine the size of the image we render to
        RenderQueue::Flush();

        m_Context->SubmitFrame();
    }
}
