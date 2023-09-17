/**
 * StatsPanel.cc
 * Created by kate on 6/27/23.
 * */

// C++ Standard Library

// Third-Party Libraries
#include <imgui.h>

// Project Headers
#include <Core/TimeManager.hh>
#include <Editor/Panels/StatsPanel.hh>
#include <Renderer/Renderer.hh>

namespace Mikoto {

    StatsPanel::StatsPanel(const Path_T &iconPath)
        :   Panel{ iconPath }
    {
        m_PanelIsVisible = true;
        m_PanelIsHovered = false;
        m_PanelIsFocused = false;
    }

    auto StatsPanel::OnUpdate() -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin("Statistics");

            DrawStatisticsTable();

            ImGui::End();
        }
    }

    auto StatsPanel::DrawStatisticsTable() -> void {
        static constexpr Int32_T COLUM_COUNT{ 2 };
        static constexpr ImGuiTableFlags flags{ ImGuiTableFlags_SizingStretchSame |
                                               ImGuiTableFlags_Resizable |
                                               ImGuiTableFlags_BordersOuter |
                                               ImGuiTableFlags_BordersV |
                                               ImGuiTableFlags_ContextMenuInBody |
                                               ImGuiTableFlags_Borders |
                                               ImGuiTableFlags_RowBg };

        if (ImGui::BeginTable("Stats", COLUM_COUNT, flags)) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Elapsed");
            ImGui::TableNextColumn();
            ImGui::Text("%s", TimeManager::ToString(TimeManager::GetTime()).c_str());

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Draw Calls");
            ImGui::TableNextColumn();
            ImGui::Text("%lu", Renderer::QueryDrawCallsCount());

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Indices");
            ImGui::TableNextColumn();
            ImGui::Text("%lu", Renderer::QueryIndexCount());

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Vertices");
            ImGui::TableNextColumn();
            ImGui::Text("%lu", Renderer::QueryVertexCount());

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("FPS");
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", ImGui::GetIO().Framerate);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("Frame Time");
            ImGui::TableNextColumn();
            ImGui::Text("%.2f ms", TimeManager::GetTimeStep(TimeUnit::MILLISECONDS));

            ImGui::EndTable();
        }
    }

    auto StatsPanel::OnEvent(Event& event) -> void {
        (void)event;
    }
}