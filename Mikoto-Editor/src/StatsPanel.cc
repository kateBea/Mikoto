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
#include <Core/System/RenderSystem.hh>
#include <Core/System/TimeSystem.hh>
#include <Library/String/String.hh>

#include "GUI/IconsFontAwesome5.h"
#include "GUI/IconsMaterialDesign.h"
#include "GUI/IconsMaterialDesignIcons.h"
#include "GUI/ImGuiUtils.hh"
#include "Library/Utility/Types.hh"
#include "Panels/StatsPanel.hh"

namespace Mikoto {

    static constexpr auto GetStatsPanelName() -> std::string_view {
        return "Statistics";
    }

    template<typename FuncType, typename... Args>
    static auto DrawStatsSection( const std::string_view title, FuncType&& func, Args&&... args ) -> void {
        static constexpr ImGuiTreeNodeFlags styleFlags{ ImGuiTreeNodeFlags_AllowItemOverlap |
                                                        ImGuiTreeNodeFlags_Framed |
                                                        ImGuiTreeNodeFlags_SpanAvailWidth |
                                                        ImGuiTreeNodeFlags_FramePadding };

        if ( ImGui::TreeNodeEx( reinterpret_cast<void*>( typeid( func ).hash_code() ), styleFlags, "%s", title.data() ) ) {
            func( std::forward<Args>( args )... );
            ImGui::TreePop();
        }
    }

    static auto DrawSystemInformation(/* system info object*/) -> void {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("Graphics API");
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(fmt::format(":    ").c_str());
    }


    StatsPanel::StatsPanel()
        :   Panel{}
    {
        TimeSystem& timeSystem{ Engine::GetSystem<TimeSystem>() };

        m_PanelHeaderName = StringUtils::MakePanelName(ICON_MD_MONITOR_HEART, GetStatsPanelName());
        m_IntervalUpdate = static_cast<float>( timeSystem.GetTime() );
    }

    auto StatsPanel::UpdateStatsInfo( float timeStep ) -> void {

        m_FrameTime = timeStep;
        m_FrameRate = 1.0f / m_FrameTime;

        if ( ( m_FrameTime - m_LastTimeUpdate ) >= m_IntervalUpdate ) {
            m_SysInfo = GetSystemCurrentInfo();
            m_LastTimeUpdate = m_FrameTime;
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

            constexpr float minUpdate{ 0.0f };
            constexpr float maxUpdate{ 10.0f };
            ImGui::SliderFloat("##StatisticsRefreshInterval", std::addressof(m_IntervalUpdate), minUpdate, maxUpdate, "%.2f");

            if (ImGui::IsItemHovered()) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

            ImGui::SameLine();
            ImGuiUtils::HelpMarker(
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

                    RenderSystem& renderSystem{ Engine::GetSystem<RenderSystem>() };
                    TimeSystem& timeSystem{ Engine::GetSystem<TimeSystem>() };

                    switch (renderSystem.GetDefaultApi()) {
                        case GraphicsAPI::VULKAN_API:
                            apiStr = fmt::format("Vulkan");
                            break;
                    }

                    if (ImGui::BeginTable("DrawPerformanceTable", m_ColumCount, flags)) {

                        // For each system info object in system info list
                        // draw system info

                        DrawSystemInformation();


                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Graphics API");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", apiStr).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("CPU");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", "CPUNAMEGOESHERE").c_str());

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
                        ImGui::TextUnformatted(fmt::format(":    {}", "GPUNAMEGOESHERE").c_str());


                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("VRAM");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {} MB", 0 / 1'000'000).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Elapsed");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", timeSystem.ToString(timeSystem.GetTime())).c_str());

                        ImGui::EndTable();
                    }

                }
        };

        DrawStatsSection("System", func);
    }
}