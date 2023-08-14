//
// Created by kate on 6/27/23.
//

#include <memory>

#include <imgui.h>

#include <Core/TimeManager.hh>

#include <Renderer/Renderer.hh>

#include "Editor/Panels/StatsPanel.hh"

namespace Mikoto {

    StatsPanel::StatsPanel(const std::shared_ptr<StatsPanelData> &data, const Path_T &iconPath)
        :   Panel{ iconPath }, m_Visible{ true }, m_Hovered{ false }, m_Focused{ false }, m_Data{ data }
    {

    }

    auto StatsPanel::OnUpdate() -> void {
        if (m_Visible) {
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