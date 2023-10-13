/**
 * StatsPanel.cc
 * Created by kate on 6/27/23.
 * */

#include <array>
#include <typeinfo>
#include <string_view>

// Third-Party Libraries
#include <imgui.h>
#include <fmt/format.h>

// Project Headers
#include <Utility/StringUtils.hh>
#include <Utility/Types.hh>
#include <Core/TimeManager.hh>
#include <Core/TimeManager.hh>
#include <Renderer/Renderer.hh>
#include <Editor/StatsPanel.hh>

#include <ImGui/IconsFontAwesome5.h>
#include <ImGui/IconsMaterialDesign.h>
#include <ImGui/IconsMaterialDesignIcons.h>

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
    }

    auto StatsPanel::OnUpdate(float timeStep) -> void {
        if (m_PanelIsVisible) {
            ImGui::Begin(m_PanelHeaderName.c_str(), std::addressof(m_PanelIsVisible));

            m_FrameTime =  timeStep;
            m_FrameRate = 1.0f / m_FrameTime;

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

                    const auto overlay{ fmt::format("{:.2f}", m_FrameRate) };
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
                        case GraphicsAPI::OPENGL_API:
                            apiStr = fmt::format("Open GL");
                            break;
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
                        ImGui::TextUnformatted("GPU");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", rendererStats.GPUName).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("RAM");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {:.2f} MB", rendererStats.RAMSize).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("VRAM");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {:.2f} MB", rendererStats.VRAMSize).c_str());

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
                    if (ImGui::BeginTable("ActiveSceneInfoTable", m_ColumCount, flags)) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Draw Calls");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", Renderer::QueryDrawCallsCount()).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Indices");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", Renderer::QueryIndexCount()).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Vertices");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", Renderer::QueryVertexCount()).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Models");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", 0).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Meshes");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", 0).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Objects");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", 0).c_str());

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted("Cameras");
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(fmt::format(":    {}", 0).c_str());

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