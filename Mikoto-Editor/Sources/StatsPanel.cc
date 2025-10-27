/**
 * StatsPanel.cc
 * Created by kate on 6/27/23.
 * */

#include <array>
#include <typeinfo>
#include <string_view>

// Third-Party Libraries
#include "fmt/format.h"
#include "imgui.h"

// Project Headers
#include <ImGui/IconsMaterialDesign.h>
#include <Core/TimeService.hh>
#include <Core/SystemStats.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Renderer/RenderService.hh>
#include <Panels/StatsPanel.hh>

namespace Mikoto {

    static constexpr auto GetStatsPanelName() -> std::string_view {
        return "Statistics";
    }

    template<typename FuncType>
    static auto DrawStatsSection(const std::string_view title, FuncType&& func) -> void {
        static constexpr ImGuiTreeNodeFlags styleFlags{
            ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding
        };

        if (ImGui::TreeNodeEx(reinterpret_cast<void*>(typeid(func).hash_code()), styleFlags, "%s", title.data())) {
            func();
            ImGui::TreePop();
        }
    }

    StatsPanel::StatsPanel() {
        m_PanelHeaderName = ImGuiUtils::MakePanelName(ICON_MD_MONITOR_HEART, GetStatsPanelName());
    }

    auto StatsPanel::OnUpdate(float timeStep) -> void {
        if (!m_PanelIsVisible) return;

        ImGui::Begin(m_PanelHeaderName.c_str(), &m_PanelIsVisible);

        ImGui::TextUnformatted("Statistics refresh interval (seconds)");
        if (ImGui::Button(fmt::format("{}", ICON_MD_REFRESH).c_str()))
            m_IntervalUpdate = 3.0f;

        if (ImGui::IsItemHovered())
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        ImGui::SameLine();

        ImGui::SliderFloat("##StatisticsRefreshInterval", &m_IntervalUpdate, 0.1f, 10.0f, "%.2f");
        ImGuiUtils::HelpMarker(
            "How often to refresh system stats (RAM, CPU, VRAM, etc.).\n"
            "Higher frequencies may increase overhead slightly."
        );

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        DrawPerformance();
        DrawSystemInfo();

        ImGui::End();
    }

    auto StatsPanel::DrawPerformance() -> void {
        static std::array<float, 90> frameRateGraph{};
        static std::size_t index = 0;
        static float maxFps = 0.0f;

        frameRateGraph[index] = m_FrameRate;
        if (m_FrameRate > maxFps) maxFps = m_FrameRate;
        index = (index + 1) % frameRateGraph.size();

        DrawStatsSection("Performance", [&]() {
            if (ImGui::BeginTable("PerformanceTable", m_ColumCount)) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("FPS");
                ImGui::TableNextColumn();
                ImGui::Text(fmt::format(": {:.2f}", m_FrameRate).c_str());

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted("Frame-time");
                ImGui::TableNextColumn();
                ImGui::Text(fmt::format(": {:.2f} ms", m_FrameTime * 1000.0f).c_str());

                ImGui::EndTable();
            }

            ImGui::PlotLines("##FPSGraph", frameRateGraph.data(), frameRateGraph.size(),
                             0, fmt::format("{:.2f} FPS", m_FrameRate).c_str(),
                             0.0f, maxFps, ImVec2(0, 80.0f));
        });
    }

    auto StatsPanel::DrawSystemInfo() -> void {
        DrawStatsSection("System", [&]() {
            const auto* stats{ SystemStats::GetPtr() };

            const auto totalMB = stats->GetTotalRam() / 1'000'000.0;
            const auto freeMB  = stats->GetFreeRam() / 1'000'000.0;
            const auto sharedMB = stats->GetSharedRam() / 1'000'000.0;
            const auto cpuUsage = stats->GetCpuUsage();

            std::string apiStr;
            switch (RenderService::Get()->GetActiveGraphicsApi()) {
                case GraphicsAPI::VULKAN_API: apiStr = "Vulkan"; break;
                default: apiStr = "Unknown"; break;
            }

            if (ImGui::BeginTable("SystemTable", m_ColumCount)) {
                ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted("Graphics API");
                ImGui::TableNextColumn(); ImGui::Text(fmt::format(": {}", apiStr).c_str());

                ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted("CPU");
                ImGui::TableNextColumn(); ImGui::Text(fmt::format(": {} ({:.1f}%%)", stats->GetCpuName(), cpuUsage).c_str());

                ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted("RAM (Total)");
                ImGui::TableNextColumn(); ImGui::Text(fmt::format(": {:.2f} MB", totalMB).c_str());

                ImGui::TableNextRow(); ImGui::TableNextColumn();
                ImGui::TextUnformatted("App RAM usage");
                ImGui::TableNextColumn();
                ImGui::Text(fmt::format(": {:.2f} MB", stats->GetProcessRamUsage() / 1'000'000.0).c_str());

                ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted("RAM (Available)");
                ImGui::TableNextColumn(); ImGui::Text(fmt::format(": {:.2f} MB", freeMB).c_str());

                ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted("RAM (Shared)");
                ImGui::TableNextColumn(); ImGui::Text(fmt::format(": {:.2f} MB", sharedMB).c_str());

                ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted("GPU");
                ImGui::TableNextColumn(); ImGui::Text(fmt::format(": {}", stats->GetGpuName()).c_str());

                ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted("VRAM");
                ImGui::TableNextColumn(); ImGui::Text(fmt::format(": {:.2f} MB", stats->GetVramUsage() / 1'000'000.0).c_str());

                ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted("Elapsed");
                ImGui::TableNextColumn(); ImGui::Text(fmt::format(": {}", TimeService::Get()->ToString(TimeService::Get()->GetTime())).c_str());

                ImGui::EndTable();
            }
        });
    }
}