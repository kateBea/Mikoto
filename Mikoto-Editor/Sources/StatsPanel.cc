/**
 * StatsPanel.cc
 * Created by kate on 6/27/23.
 * */

#include <array>
#include <string_view>
#include <typeinfo>

// Third-Party Libraries
#include "fmt/format.h"
#include "imgui.h"

// Project Headers
#include <ImGui/IconsMaterialDesign.h>

#include <Core/SystemStats.hh>
#include <Core/TimeService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <Layers/EditorLayer.hh>
#include <Panels/StatsPanel.hh>
#include <Renderer/Core/RenderService.hh>

namespace Mikoto {

    static constexpr auto GetStatsPanelName() -> std::string_view { return "Engine Dashboard"; }

    template<typename FuncType>
    static auto DrawStatsSection( const std::string_view title, FuncType&& func ) -> void {
        ImGui::PushStyleVar( ImGuiStyleVar_FramePadding, ImVec2( 6, 6 ) );
        ImGui::PushStyleColor( ImGuiCol_Header, ImVec4( 0.20f, 0.22f, 0.28f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_HeaderHovered, ImVec4( 0.25f, 0.28f, 0.35f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_HeaderActive, ImVec4( 0.30f, 0.33f, 0.40f, 1.0f ) );

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth |
                                   ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_DefaultOpen;

        if ( ImGui::TreeNodeEx( reinterpret_cast<void*>( typeid( func ).hash_code() ), flags, "%s", title.data() ) ) {
            ImGui::Spacing();
            func();
            ImGui::Spacing();
            ImGui::TreePop();
        }

        ImGui::PopStyleColor( 3 );
        ImGui::PopStyleVar();
    }

    StatsPanel::StatsPanel(const StatsPanelCreateInfo& info)
        : m_State{ info.State }
    {
        m_PanelHeaderName = ImGuiUtils::MakePanelName( ICON_MD_TABLE_CHART, GetStatsPanelName() );
    }

    auto StatsPanel::OnUpdate( float timeStep ) -> void {
        if ( !m_PanelIsVisible ) return;

        m_FrameRate = 1 / timeStep;
        m_FrameTime = timeStep;

        ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, ImVec2( 12, 12 ) );
        ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0.12f, 0.12f, 0.14f, 1.0f ) );

        ImGui::Begin( m_PanelHeaderName.c_str(), &m_PanelIsVisible,
                      ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize );

        ImGui::TextUnformatted( "Statistics refresh interval (seconds)" );
        ImGui::SameLine();
        ImGui::SetNextItemWidth( 120.0f );
        ImGui::SliderFloat( "##StatisticsRefreshInterval", &m_IntervalUpdate, 0.1f, 10.0f, "%.2f" );
        ImGuiUtils::HelpMarker( "How often system stats update. Higher = less overhead." );

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        DrawPerformance();
        DrawSystemInfo();

        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        m_State->StatsPanelVisible = m_PanelIsVisible;
    }

    auto StatsPanel::DrawPerformance() -> void {
        static std::array<float, 120> frameHistory{};
        static std::size_t index = 0;
        static float maxFps = 0.0f;

        static float frameTime = m_FrameTime;
        static float frameRate = m_FrameRate;

        if ( TimeService::Get()->GetTime() - m_LastTime >= m_IntervalUpdate ) {
            m_LastTime = TimeService::Get()->GetTime();

            frameHistory[index] = m_FrameRate;
            if ( m_FrameRate > maxFps ) maxFps = m_FrameRate;
            index = ( index + 1 ) % frameHistory.size();

            frameTime = m_FrameTime;
            frameRate = m_FrameRate;
        }

        DrawStatsSection( "Performance", [&]() {
            if ( ImGui::BeginTable( "PerformanceTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp ) ) {
                auto Row = [&]( const char* label, const std::string& value, ImVec4 color = ImVec4( 1, 1, 1, 1 ) ) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( label );
                    ImGui::TableNextColumn();
                    ImGui::TextColored( color, "%s", value.c_str() );
                };

                Row( "Frame Rate", fmt::format( "{:.2f} FPS", frameRate ), ImVec4( 0.6f, 0.9f, 0.6f, 1.0f ) );
                Row( "Frame Time", fmt::format( "{:.2f} ms", frameTime * 1000.0f ), ImVec4( 0.7f, 0.7f, 1.0f, 1.0f ) );

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::PushStyleColor( ImGuiCol_PlotLines, ImVec4( 0.3f, 0.7f, 1.0f, 1.0f ) );
            ImGui::PlotLines( "##FrameGraph", frameHistory.data(), frameHistory.size(), 0,
                              fmt::format( "{:.2f} FPS", frameRate ).c_str(),
                              0.0f, maxFps, ImVec2( -1, 80.0f ) );
            ImGui::PopStyleColor();
        } );
    }

    auto StatsPanel::DrawSystemInfo() -> void {
        DrawStatsSection( "System Overview", [&]() {
            static auto* stats = SystemStats::GetPtr();
            static double totalMB = stats->GetTotalRam() / 1'000'000.0;
            static double freeMB = stats->GetFreeRam() / 1'000'000.0;
            static double sharedMB = stats->GetSharedRam() / 1'000'000.0;
            static double appMB = stats->GetProcessRamUsage() / 1'000'000.0;
            static double cpuUsage = stats->GetCpuUsage();
            static double ramPercent = ( appMB / totalMB ) * 100.0;

            if ( TimeService::Get()->GetTime() - m_LastTime >= m_IntervalUpdate ) {
                m_LastTime = TimeService::Get()->GetTime();

                stats = SystemStats::GetPtr();
                totalMB = stats->GetTotalRam() / 1'000'000.0;
                freeMB = stats->GetFreeRam() / 1'000'000.0;
                sharedMB = stats->GetSharedRam() / 1'000'000.0;
                appMB = stats->GetProcessRamUsage() / 1'000'000.0;
                cpuUsage = stats->GetCpuUsage();
                ramPercent = ( appMB / totalMB ) * 100.0;

            }

            std::string apiStr;
            switch ( RenderService::Get()->GetActiveGraphicsApi() ) {
                case GraphicsAPI::VULKAN_API:
                    apiStr = "Vulkan";
                    break;
                default:
                    apiStr = "Unknown";
                    break;
            }

            // Make CPU usage formatting width stable
            const std::string cpuStr = fmt::format( "{} ({:05.1f} %)", stats->GetCpuName(), cpuUsage );

            if ( ImGui::BeginTable( "SystemTable", 2,
                                    ImGuiTableFlags_BordersInnerV |
                                            ImGuiTableFlags_SizingFixedFit |
                                            ImGuiTableFlags_NoHostExtendX ) ) {
                auto Row = [&]( const char* label, const std::string& value, ImVec4 color = ImVec4( 1, 1, 1, 1 ) ) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( label );
                    ImGui::TableNextColumn();
                    ImGui::TextColored( color, "%s", value.c_str() );
                };

                Row( "Graphics API", apiStr );
                Row( "CPU", cpuStr, ImVec4( 0.9f, 0.9f, 0.6f, 1.0f ) );
                Row( "RAM (Total)", fmt::format( "{:.2f} MB", totalMB ) );
                Row( "App RAM", fmt::format( "{:.2f} MB ({:4.1f}%)", appMB, ramPercent ),
                     ImVec4( 0.8f, 0.8f, 0.5f, 1.0f ) );
                Row( "RAM (Available)", fmt::format( "{:.2f} MB", freeMB ) );
                Row( "RAM (Shared)", fmt::format( "{:.2f} MB", sharedMB ) );
                Row( "GPU", fmt::format( "{}", RenderService::Get()->GetGpuDevice()->GetDeviceName() ));
                Row( "VRAM Usage", fmt::format( "{:.2f} MB", stats->GetVramUsage() / 1'000'000.0 ),
                     ImVec4( 0.7f, 0.5f, 1.0f, 1.0f ) );
                Row( "Elapsed Time", TimeService::Get()->ToString( TimeService::Get()->GetTime() ) );

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::TextColored( ImVec4( 0.6f, 0.7f, 1.0f, 1.0f ), "Memory Usage" );

            ImGui::PushStyleColor( ImGuiCol_PlotHistogram, ImVec4( 0.6f, 0.7f, 1.0f, 1.0f ) );
            ImGui::ProgressBar( ramPercent / 100.0f, ImVec2( -1, 0 ),
                                fmt::format( "{:4.1f}%% of {:.0f} MB used", ramPercent, totalMB ).c_str() );
            ImGui::PopStyleColor();
        } );
    }
}