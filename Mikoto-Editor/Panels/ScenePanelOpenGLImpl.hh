/**
 * ScenePanel_OpenGLImpl.hh
 * Created by kate on 8/26/23.
 * */

#ifndef MIKOTO_SCENE_PANEL_OPENGL_IMPL_HH
#define MIKOTO_SCENE_PANEL_OPENGL_IMPL_HH

// Third-Party Libraries
#include "ImGuizmo.h"
#include "backends/imgui_impl_opengl3.h"
#include "imgui.h"

// Project Headers
#include "Common/RenderingUtils.hh"
#include "Core/Event.hh"
#include "Renderer/OpenGL/OpenGLRenderer.hh"
#include "Renderer/Renderer.hh"
#include "Scene/SceneManager.hh"
#include "ScenePanel.hh"

namespace Mikoto {
    class ScenePanel_OGLImpl : public ScenePanelInterface {
    private:
        auto Init_Impl(ScenePanelData&& data) -> void override {
            m_Data = std::move(data);
            m_SceneRenderer = dynamic_cast<OpenGLRenderer*>(Renderer::GetActiveGraphicsAPIPtr());
            m_ActiveManipulationMode = ManipulationMode::TRANSLATION;
        }

        auto OnUpdate_Impl() -> void override {
            const auto viewPortDimensions{ ImGui::GetContentRegionAvail() };
            auto& currentlyActiveScene{ SceneManager::GetActiveScene() };

            if (m_Data.ViewPortWidth != viewPortDimensions.x || m_Data.ViewPortHeight != viewPortDimensions.y) {
                m_SceneRenderer->GetColorAttachment().Resize((UInt32_T)viewPortDimensions.x, (UInt32_T)viewPortDimensions.y);

                m_Data.ViewPortWidth = viewPortDimensions.x;
                m_Data.ViewPortHeight = viewPortDimensions.y;
                currentlyActiveScene.OnViewPortResize((UInt32_T)viewPortDimensions.x, (UInt32_T)viewPortDimensions.y);
            }

            const ImTextureID textId{ reinterpret_cast<ImTextureID>(m_SceneRenderer->GetColorAttachment().GetColorAttachmentId()) };
            const float frameWidth{ static_cast<float>(m_SceneRenderer->GetColorAttachment().GetFrameBufferProperties().width) };
            const float frameHeight{ static_cast<float>(m_SceneRenderer->GetColorAttachment().GetFrameBufferProperties().height) };
            ImGui::Image(textId, ImVec2{ frameWidth, frameHeight }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });

            HandleGuizmos();
        }

    private:
        OpenGLRenderer* m_SceneRenderer{};

    };
}

#endif // MIKOTO_SCENE_PANEL_OPENGL_IMPL_HH
