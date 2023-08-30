/**
 * ScenePanel.cc
 * Created by kate on 6/27/23.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include <Core/Application.hh>
#include <Editor/Panels/ScenePanel.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Editor/Panels/ScenePanel_VulkanImpl.hh>
#include <Editor/Panels/ScenePanel_OpenGLImpl.hh>

namespace Mikoto {

    ScenePanel::ScenePanel(const std::shared_ptr<ScenePanelData>& data, const Path_T& iconPath)
        :   Panel{ iconPath }
    {
        m_PanelIsVisible = true;
        m_PanelIsHovered = false;
        m_PanelIsFocused = false;

        // Set scene panel implementation
        switch (Renderer::GetActiveGraphicsAPI()) {
        case GraphicsAPI::OPENGL_API:
            m_Implementation = std::make_shared<ScenePanel_OGLImpl>();
            break;
        case GraphicsAPI::VULKAN_API:
            m_Implementation = std::make_shared<ScenePanel_VkImpl>();
            break;
        }

        // Initialize implementation
        m_Implementation->Init_Impl(data);
    }

    auto ScenePanel::OnUpdate() -> void {
        if (m_PanelIsVisible)
            m_Implementation->OnUpdate_Impl();

        m_PanelIsHovered = m_Implementation->IsHovered();
        m_PanelIsFocused = m_Implementation->IsFocused();
    }

    auto ScenePanel::OnEvent(Event& event) -> void {
        m_Implementation->OnEvent_Impl(event);
    }

    auto ScenePanel::MakeVisible(bool value) -> void {
        m_PanelIsVisible = value;
    }
}
