/**
 * ScenePanel_OpenGLImpl.hh
 * Created by kate on 8/26/23.
 * */

#ifndef MIKOTO_SCENE_PANEL_OPENGL_IMPL_HH
#define MIKOTO_SCENE_PANEL_OPENGL_IMPL_HH

// Third-Party Libraries
#include <imgui.h>
#include <backends/imgui_impl_opengl3.h>

// Project Headers
#include <Core/Application.hh>
#include <Core/Events/Event.hh>
#include <Renderer/Renderer.hh>
#include <Editor/Panels/Panel.hh>
#include <Editor/Panels/ScenePanel.hh>
#include <Renderer/RenderingUtilities.hh>
#include <Renderer/OpenGL/OpenGLRenderer.hh>

namespace Mikoto {
    class ScenePanel_OGLImpl : public ScenePanelInterface {
    private:
        auto Init_Impl(std::shared_ptr<ScenePanelData> data) -> void override {
            m_Data = std::move(data);
            m_SceneRenderer = dynamic_cast<OpenGLRenderer*>(Renderer::GetActiveGraphicsAPIPtr());
        }

        auto OnUpdate_Impl() -> void override {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{0,0});
            ImGui::Begin("Scene");
            m_Focused = ImGui::IsWindowFocused();
            m_Hovered = ImGui::IsWindowHovered();

            // TODO: review
            Application::GetPtr()->BlockImGuiLayerEvents(!m_Focused || !m_Hovered);
            auto viewPortDimensions{ ImGui::GetContentRegionAvail() };

            if (m_Data->ViewPortWidth != viewPortDimensions.x || m_Data->ViewPortHeight != viewPortDimensions.y) {
                m_SceneRenderer->GetColorAttachment().Resize((UInt32_T)viewPortDimensions.x, (UInt32_T)viewPortDimensions.y);

                m_Data->ViewPortWidth = viewPortDimensions.x;
                m_Data->ViewPortHeight = viewPortDimensions.y;
                m_Data->Viewport->OnViewPortResize((UInt32_T)viewPortDimensions.x, (UInt32_T)viewPortDimensions.y);
            }

            ImTextureID textId{ reinterpret_cast<ImTextureID>(m_SceneRenderer->GetColorAttachment().GetColorAttachmentId()) };
            float frameWidth{ static_cast<float>(m_SceneRenderer->GetColorAttachment().GetFrameBufferProperties().width) };
            float frameHeight{ static_cast<float>(m_SceneRenderer->GetColorAttachment().GetFrameBufferProperties().height) };
            ImGui::Image((ImTextureID)textId, ImVec2{ frameWidth, frameHeight }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

            ImGui::End();
            ImGui::PopStyleVar();
        }

        auto OnEvent_Impl(Event& event) -> void override {

        }

    private:
        OpenGLRenderer* m_SceneRenderer{};
    };
}

#endif//MIKOTO_SCENE_PANEL_OPENGL_IMPL_HH
