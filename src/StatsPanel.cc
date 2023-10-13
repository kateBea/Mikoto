/**
 * StatsPanel.cc
 * Created by kate on 6/27/23.
 * */

// Third-Party Libraries
#include <imgui.h>

// Project Headers
#include <Utility/StringUtils.hh>

#include <Utility/Types.hh>
#include <Core/TimeManager.hh>
#include <Renderer/Renderer.hh>
#include <Editor/StatsPanel.hh>

#include <ImGui/IconsFontAwesome5.h>
#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/IconsMaterialDesignIcons.h>

namespace Mikoto {
    static constexpr auto GetStatsPanelName() -> std::string_view {
        return "Statistics";
    }

    StatsPanel::StatsPanel()
        :   Panel{}
    {
        m_PanelHeaderName = MakePanelName(ICON_MD_MONITOR_HEART, GetStatsPanelName());
    }

    auto StatsPanel::OnUpdate() -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin(m_PanelHeaderName.c_str(), std::addressof(m_PanelIsVisible));

            DrawStatisticsTable();

            ImGui::End();
        }
    }

    auto StatsPanel::DrawStatisticsTable() const -> void {
        static constexpr ImGuiTableFlags flags{ ImGuiTableFlags_SizingStretchSame |
                                               ImGuiTableFlags_Resizable |
                                               ImGuiTableFlags_BordersOuter |
                                               ImGuiTableFlags_BordersV |
                                               ImGuiTableFlags_ContextMenuInBody |
                                               ImGuiTableFlags_Borders |
                                               ImGuiTableFlags_RowBg };

        if (ImGui::BeginTable("Stats", m_ColumCount, flags)) {
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
}