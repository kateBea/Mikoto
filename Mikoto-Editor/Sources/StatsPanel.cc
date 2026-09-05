//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <imgui.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/SystemStats.hh>
#include <Core/TimeService.hh>

#include <Memory/Allocator.hh>

#include <ImGui/ImGuiWidget.hh>
#include <ImGui/ImGuiUtility.hh>
#include <ImGui/IconsMaterialDesign.h>

#include <Layers/EditorLayer.hh>

#include <Panels/StatsPanel.hh>

#include <Renderer/Core/RenderSystem.hh>

namespace mikoto::editor {

    using namespace mikoto::imgui;
    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    template<typename FuncType>
    static auto DrawStatsSection( const eastl::string_view title, FuncType&& func ) -> void {
        ImGui::PushStyleColor( ImGuiCol_Header, ImVec4( 0.20f, 0.22f, 0.28f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_HeaderHovered, ImVec4( 0.25f, 0.28f, 0.35f, 1.0f ) );
        ImGui::PushStyleColor( ImGuiCol_HeaderActive, ImVec4( 0.30f, 0.33f, 0.40f, 1.0f ) );

        ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth |
                                   ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_DefaultOpen };
        if ( ImGui::TreeNodeEx( reinterpret_cast<void*>( typeid( func ).hash_code() ), flags, "%s", title.data() ) ) {
            SetCursorHandOnLastItemHovered();
            ImGui::Spacing();

            func();

            ImGui::Spacing();
            ImGui::TreePop();
        }

        SetCursorHandOnLastItemHovered();

        ImGui::PopStyleColor( 3 );
    }

    StatsPanel::StatsPanel(const StatsPanelCreateInfo& info)
        : Panel{ "Engine Dashboard" }, mState{ info.mState }
    {
        mPanelHeaderName = widget::MakeIconTitle( ICON_MD_TABLE_CHART, mPanelName );
    }

    auto StatsPanel::OnRender( float timeStep ) -> void {
        if ( !mPanelIsVisible ) {
            return;
        }

        ImGuiScopedStyleVar windowPadding{ ImGuiStyleVar_WindowPadding, ImVec2( 12, 12 ) };

        ImGui::Begin( mPanelHeaderName.c_str(), MKT_ADDRESSOF( mPanelIsVisible ),
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize );

        // Update frame times
        mCurrentTime = TimeService::Get()->GetTime();
        if ( mCurrentTime - mLastTime >= mIntervalUpdate ) {
            mLastTime = mCurrentTime;

            mFrameRate = 1.0f / timeStep;
            mFrameTime = timeStep;
        }

        DrawUpdateInfo();

        DrawSystemInfo();
        DrawPerformance( timeStep );

        ImGui::End();
    }

    auto StatsPanel::DrawUpdateInfo() -> void {
        ImGui::TextUnformatted( "Statistics refresh interval (seconds)" );
        ImGui::SameLine();
        ImGui::SetNextItemWidth( 120.0f );
        ImGui::SliderFloat( "##StatisticsRefreshInterval", &mIntervalUpdate, 0.0f, 1.0f, "%.2f" );

        ImGui::SameLine();
        widget::MakeHelpPopUp( "How often system stats update (in seconds). Higher = less overhead." );

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    auto StatsPanel::DrawPerformance( float timeStep ) -> void {
        static usize index{};
        static float maxFps{ 0.0f };
        static eastl::array<float, 120> frameHistory{};

        if ( mCurrentTime - mLastTime >= mIntervalUpdate ) {
            frameHistory[index] = mFrameRate;
            if ( mFrameRate > maxFps ) maxFps = mFrameRate;
            index = ( index + 1 ) % frameHistory.size();
        }

        DrawStatsSection( "Performance", [&]() {
            if ( ImGui::BeginTable( "PerformanceTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp ) ) {
                auto Row = [&]( const char* label, const eastl::string& value, ImVec4 color = ImVec4{ 1, 1, 1, 1 } ) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( label );
                    ImGui::TableNextColumn();
                    ImGui::TextColored( color, "%s", value.c_str() );
                };

                Row( "Frame Rate", string::Format( "{:.2f} FPS", mFrameRate ), ImVec4( 0.6f, 0.9f, 0.6f, 1.0f ) );
                Row( "Frame Time", string::Format( "{:.2f} ms", mFrameTime * 1000.0f ), ImVec4( 0.7f, 0.7f, 1.0f, 1.0f ) );

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::PushStyleColor( ImGuiCol_PlotLines, ImVec4( 0.3f, 0.7f, 1.0f, 1.0f ) );
            ImGui::PlotLines( "##FrameGraph", frameHistory.data(), frameHistory.size(), 0,
                              string::Format( "{:.2f} FPS", mFrameRate ).c_str(),
                              0.0f, maxFps, ImVec2( -1, 80.0f ) );
            ImGui::PopStyleColor();
        } );
    }

    auto StatsPanel::DrawSystemInfo() -> void {
        DrawStatsSection( "System Overview", [&]() {
            static double totalMB = SystemStats::Get()->GetTotalRam() / 1'000'000.0;
            static double freeMB = SystemStats::Get()->GetFreeRam() / 1'000'000.0;
            static double sharedMB = SystemStats::Get()->GetSharedRam() / 1'000'000.0;
            static double appMB = SystemStats::Get()->GetProcessRamUsage() / 1'000'000.0;
            static double ramPercent = ( appMB / totalMB ) * 100.0;

            if ( mCurrentTime - mLastTime >= mIntervalUpdate ) {
                totalMB = SystemStats::Get()->GetTotalRam() / 1'000'000.0;
                freeMB = SystemStats::Get()->GetFreeRam() / 1'000'000.0;
                sharedMB = SystemStats::Get()->GetSharedRam() / 1'000'000.0;
                appMB = SystemStats::Get()->GetProcessRamUsage() / 1'000'000.0;
                ramPercent = ( appMB / totalMB ) * 100.0;
            }

            eastl::string apiStr{};
            switch ( RenderSystem::Get()->GetActiveGraphicsApi() ) {
                case GraphicsAPI::eVulkan:
                    apiStr = "Vulkan";
                    break;
                case GraphicsAPI::eD3D12:
                    apiStr = "DirectX 12";
                    break;
                case GraphicsAPI::eD3D11:
                    apiStr = "DirectX 11";
                    break;
                default:
                    apiStr = "Unknown";
                    break;
            }

            // Make CPU usage formatting width stable
            const eastl::string cpuStr{ string::Format( "{}", SystemStats::Get()->GetCpuName() ) };

            if ( ImGui::BeginTable( "SystemTable", 2,
                                    ImGuiTableFlags_BordersInner |
                                            ImGuiTableFlags_SizingFixedFit |
                                            ImGuiTableFlags_NoHostExtendX ) ) {
                auto Row = [&]( const char* label, const eastl::string& value, ImVec4 color = ImVec4( 1, 1, 1, 1 ) ) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted( label );
                    ImGui::TableNextColumn();
                    ImGui::TextColored( color, "%s", value.c_str() );
                };

                Row( "Graphics API", apiStr );
                Row( "CPU", cpuStr, ImVec4( 0.9f, 0.9f, 0.6f, 1.0f ) );
                Row( "RAM (Total)", string::Format( "{:.2f} MB", totalMB ) );
                Row( "App RAM", string::Format( "{:.2f} MB ({:4.1f}%)", appMB, ramPercent ),
                     ImVec4( 0.8f, 0.8f, 0.5f, 1.0f ) );
                Row( "RAM (Available)", string::Format( "{:.2f} MB", freeMB ) );
                Row( "RAM (Shared)", string::Format( "{:.2f} MB", sharedMB ) );
                Row( "GPU Device", string::Format( "{}", RenderSystem::Get()->GetGpuDevice()->GetDeviceName() ));
                Row( "GPU V-RAM Usage", string::Format( "{:.2f} MB", RenderSystem::Get()->GetGpuDevice()->GetMemoryUsage() / 1'000'000.0 ),
                     ImVec4( 0.7f, 0.5f, 1.0f, 1.0f ) );
                Row( "Elapsed Time", TimeService::Get()->ToString( TimeService::Get()->GetTime() ) );

                ImGui::EndTable();
            }

            ImGui::Spacing();
            ImGui::TextColored( ImVec4( 0.6f, 0.7f, 1.0f, 1.0f ), "Memory Usage" );

            ImGui::PushStyleColor( ImGuiCol_PlotHistogram, ImVec4( 0.6f, 0.7f, 1.0f, 1.0f ) );

            f64 systemUsedRamPercent{ ( ( totalMB - freeMB ) / totalMB ) * 100.0 };
            ImGui::ProgressBar( systemUsedRamPercent / 100.0f, ImVec2( -1, 0 ),
                                string::Format( "{:4.1f}%% of {:.0f} GB used", systemUsedRamPercent, totalMB / 1000 ).c_str() );
            ImGui::PopStyleColor();
        } );
    }
}