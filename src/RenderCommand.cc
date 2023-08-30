/**
 * RenderCommand.cc
 * Created by kate on 6/9/23.
 * */

// Third-Party Libraries
#include <glm/glm.hpp>

// Project Headers
#include <Core/Assert.hh>
#include <Renderer/Renderer.hh>
#include <Renderer/RenderCommand.hh>
#include <utility>

namespace Mikoto {
    auto RenderCommand::Init(RendererAPI* activeAPI) -> void {
        s_ActiveRendererAPI = activeAPI;
    }

    auto RenderCommand::SetClearColor(const glm::vec4& color) -> void {
        MKT_ASSERT(s_ActiveRendererAPI, "Render command active API is NULL");
        s_ActiveRendererAPI->SetClearColor(color);
    }

    auto RenderCommand::SetClearColor(float red, float green, float blue, float alpha) -> void {
        MKT_ASSERT(s_ActiveRendererAPI, "Render command active API is NULL");
        s_ActiveRendererAPI->SetClearColor(red, green, blue, alpha);
    }


    auto RenderCommand::UpdateViewPort(UInt32_T x, UInt32_T y, UInt32_T width, UInt32_T height) -> void {
        MKT_ASSERT(s_ActiveRendererAPI, "Render command active API is NULL");
        s_ActiveRendererAPI->SetViewport(x, y, width, height);
    }

    auto RenderCommand::EnableWireframeMode() -> void {
        s_ActiveRendererAPI->EnableWireframeMode();
    }

    auto RenderCommand::DisableWireframeMode() -> void {
        s_ActiveRendererAPI->DisableWireframeMode();
    }

    auto RenderCommand::Flush() -> void {
        s_ActiveRendererAPI->Flush();
    }

    auto RenderCommand::AddToRenderQueue(std::shared_ptr<DrawData> data) -> void {
        s_ActiveRendererAPI->QueueForDrawing(std::move(data));
    }

}