/**
 * StatsPanel.cc
 * Created by kate on 6/27/23.
 * */

// C++ Standard Library
#include <memory>

// Third-Party Libraries
#include <imgui.h>

// Project Headers
#include <Core/TimeManager.hh>
#include <Renderer/Renderer.hh>
#include <Editor/Panels/StatsPanel.hh>

namespace Mikoto {

    StatsPanel::StatsPanel(const std::shared_ptr<StatsPanelData> &data, const Path_T &iconPath)
        :   Panel{ iconPath }, m_Data{ data }
    {
        m_PanelIsVisible = true;
        m_PanelIsHovered = false;
        m_PanelIsFocused = false;
    }

    auto StatsPanel::OnUpdate() -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin("Statistics");

            ImGui::Text("Elapsed: %s", TimeManager::ToString(TimeManager::GetTime()).c_str());
            ImGui::Text("Draw calls count: %d", Renderer::QueryDrawCallsCount());
            ImGui::Text("Quad count: %d", Renderer::QueryQuadCount());
            ImGui::Text("Index count: %d", Renderer::QueryIndexCount());
            ImGui::Text("Vertex count: %d", Renderer::QueryVertexCount());
            ImGui::Text("Frame rate: %.1f", ImGui::GetIO().Framerate);

            ImGui::End();
        }
    }

    auto StatsPanel::OnEvent(Event &event) -> void {

    }
}