/**
 * RenderCommand.cc
 * Created by kate on 6/9/23.
 * */

// Third-Party Libraries
#include "glm/glm.hpp"

// Project Headers
#include "Core/Assert.hh"
#include "Renderer/RenderCommand.hh"
#include "Renderer/Renderer.hh"
#include <utility>

namespace Mikoto {
    auto RenderCommand::Init(RendererBackend * activeAPI) -> void {
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


    auto RenderCommand::UpdateViewPort(Int32_T x, Int32_T y, Int32_T width, Int32_T height) -> void {
        MKT_ASSERT(s_ActiveRendererAPI, "Render command active API is NULL");
        s_ActiveRendererAPI->SetViewport((float)x, (float)y, (float)width, (float)height);
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

    auto RenderCommand::AddToRenderQueue(const std::string &id, std::shared_ptr<GameObject> &&data, std::shared_ptr<Material> &&material) -> void {
        s_ActiveRendererAPI->QueueForDrawing(id, std::move(data), std::move(material));
    }

}