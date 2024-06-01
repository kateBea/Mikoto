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
#include "Common/StringUtils.hh"
#include "Common/Types.hh"
#include "Core/TimeManager.hh"
#include "Panels/StatsPanel.hh"
#include "Renderer/Renderer.hh"

#include "GUI/ImGuiUtils.hh"

#include "GUI/IconsFontAwesome5.h"
#include "GUI/IconsMaterialDesign.h"
#include "GUI/IconsMaterialDesignIcons.h"

namespace Mikoto {
    template<typename FuncType, typename... Args>
    static auto DrawStatsSection(std::string_view title, FuncType&& func, Args&&... args) -> void {
        static constexpr ImGuiTreeNodeFlags styleFlags{ ImGuiTreeNodeFlags_AllowItemOverlap |
                                                       ImGuiTreeNodeFlags_Framed |
                                                       ImGuiTreeNodeFlags_SpanAvailWidth |
                                                       ImGuiTreeNodeFlags_FramePadding };

        if (ImGui::TreeNodeEx((void*)typeid(func).hash_code(), styleFlags, "%s", title.data())) {
            func(std::forward<Args>(args)...);
            ImGui::TreePop();
        }
    }


    static constexpr auto GetStatsPanelName() -> std::string_view {
        return "Statistics";
    }


    StatsPanel::StatsPanel()
        :   Panel{}
    {
        m_PanelHeaderName = StringUtils::MakePanelName(ICON_MD_MONITOR_HEART, GetStatsPanelName());
        m_IntervalUpdate = (float)TimeManager::GetTime();
    }


    auto StatsPanel::UpdateStatsInfo(float timeStep) -> void {
        m_FrameTime =  timeStep;
        m_FrameRate = 1.0f / m_FrameTime;

        auto timeElapsed{ TimeManager::GetTime() };
        if ((timeElapsed - m_LastTimeUpdate) >= m_IntervalUpdate) {
            m_SysInfo = GetSystemCurrentInfo();
            m_LastTimeUpdate = (float)timeElapsed;
        }
    }


    auto StatsPanel::OnUpdate(float timeStep) -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin(m_PanelHeaderName.c_str(), std::addressof(m_PanelIsVisible));

            ImGui::TextUnformatted("Statistics refresh interval (seconds)");
            if (ImGui::Button(fmt::format("{}", ICON_MD_REFRESH).c_str())) {
                m_IntervalUpdate = 3.0f;
            }

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            ImGui::SameLine();

            const float minUpdate{ 0.0f };
            const float maxUpdate{ 10.0f };
            ImGui::SliderFloat("##StatisticsRefreshInterval", std::addressof(m_IntervalUpdate), minUpdate, maxUpdate, "%.2f");

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            ImGui::SameLine();
            HelpMarker(
                    "Tells how often we want to refresh system stats,\n"
                    "updating information such as RAM usage, available\n"
                    "RAM, VRAM usage, etc. This is specially costly\n"
                    "in the case of Vulkan as fetching statistics from the\n"
                    "default allocator is slow and the retrieved data can\n"
                    "be not really consistent as there is more concurrency."
                    );

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            UpdateStatsInfo(timeStep);

            DrawPerformance();
            DrawSystemInfo();
            DrawActiveSceneInfo();
            DrawLightInfo();

            ImGui::End();
        }
    }


    auto StatsPanel::DrawPerformance() -> void {
        static std::array<float, 90> frameRateGraphCachedValues{};
        static std::array<float, 90>::size_type index{};
        static float maxFps{};

        const auto func{
                [&]() -> void {
                    frameRateGraphCachedValues[index] = m_FrameRate;

                    if (m_FrameRate > maxFps) {
                        maxFps = m_FrameRate;
                    }

                    static constexpr ImGuiTableFlags flags{};

                    if (ImGui::BeginTable("DrawPerformanceTable", m_ColumCount, flags)) {
                        ImGui::TableNextRow();

                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("FPS");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(": {:.2f}", m_FrameRate).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Frame-time");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(": {:.2f} ms", m_FrameTime * 1000.0f).c_str());

                        ImGui::EndTable();
                    }

                    const auto overlay{ fmt::format("{:.2f} FPS", m_FrameRate) };
                    ImGui::PlotLines("##FPS", frameRateGraphCachedValues.data(), frameRateGraphCachedValues.size(), 100, overlay.c_str(), 0.0f, maxFps, ImVec2(0, 80.0f));

                    ++index;
                    if (index == frameRateGraphCachedValues.size()) {
                        index = 0;
                    }
                }
        };

        DrawStatsSection("Performance", func);
    }

    auto StatsPanel::DrawSystemInfo() -> void {
        const auto func{
                [&]() -> void {

                    static constexpr ImGuiTableFlags flags{ ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_SizingStretchProp };

                    std::string apiStr{};

                    const auto& rendererStats{ Renderer::GetRendererData() };

                    switch (Renderer::GetActiveGraphicsAPI()) {
                        case GraphicsAPI::VULKAN_API:
                            apiStr = fmt::format("Vulkan");
                            break;
                    }

                    if (ImGui::BeginTable("DrawPerformanceTable", m_ColumCount, flags)) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Graphics API");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", apiStr).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("CPU");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", rendererStats.CPUName).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("RAM (Total)");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {:.2f} MB", (float)m_SysInfo.TotalRam / 1000.0f).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("RAM (Available)");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {:.2f} MB", (float)m_SysInfo.FreeRam / 1000.0f).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("RAM (Shared)");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {:.2f} MB", (float)m_SysInfo.SharedRam / 1000.0f).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("GPU");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", rendererStats.GPUName).c_str());


                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("VRAM");
                        ImGui::TableNextColumn();
                        auto& vram{ Renderer::GetRendererStatistics() };
                        ImGui::TextUnformatted(fmt::format(":    {} MB", vram.VRAMUsage / 1'000'000).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Elapsed");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", TimeManager::ToString(TimeManager::GetTime())).c_str());

                        ImGui::EndTable();
                    }

                }
        };

        DrawStatsSection("System", func);
    }

    auto StatsPanel::DrawActiveSceneInfo() const -> void {
        static constexpr ImGuiTableFlags flags{ ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_SizingStretchProp };

        const auto func{
            [&]() -> void {
                // TODO: use tables
                    auto& sceneRenderStats{ Renderer::GetSceneRenderStats() };
                    if (ImGui::BeginTable("ActiveSceneInfoTable", m_ColumCount, flags)) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Draw Calls");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", sceneRenderStats.GetDrawCallCount()).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Indices");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", sceneRenderStats.GetIndexCount()).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Vertices");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", sceneRenderStats.GetVertexCount()).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Models");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", sceneRenderStats.GetModelsCount()).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Meshes");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", sceneRenderStats.GetMeshesCount()).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Objects");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", sceneRenderStats.GetObjectsCount()).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Cameras");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", sceneRenderStats.GetSceneCamerasCount()).c_str());

                        ImGui::EndTable();
                    }
            }
        };

        DrawStatsSection("Scene", func);
    }

    auto StatsPanel::DrawLightInfo() -> void {
        static constexpr ImGuiTableFlags flags{ ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_SizingStretchProp };

        const auto func{
                [&]() -> void {
                    if (ImGui::BeginTable("DrawLightInfoTable1", m_ColumCount, flags)) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Active / Total");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {} / {}", 1, 1).c_str());
                        ImGui::EndTable();
                    }

                    ImGui::Separator();

                    if (ImGui::BeginTable("DrawLightInfoTable2", m_ColumCount, flags)) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Spot lights");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {} / {}", 1, 1).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Point lights");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {} / {}", 1, 1).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Directional lights");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {} / {}", 1, 1).c_str());

                        ImGui::EndTable();
                    }
                }
        };

        DrawStatsSection("Lights", func);
    }
}