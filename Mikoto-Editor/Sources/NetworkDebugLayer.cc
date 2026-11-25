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

#include <cstdio>

#include <imgui.h>

#include <EASTL/algorithm.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/LocalizationService.hh>

#include <Platform/CoreEvents.hh>

#include <Networking/Client.hh>

#include <ImGui/Utility.hh>
#include <ImGui/ImGuiWidget.hh>

#include <Layers/NetworkDebugLayer.hh>

namespace mikoto::editor {

    static constexpr core::usize kPreviewBytes{ 64 * 1024 };

    NetworkDebugLayer::NetworkDebugLayer()
        : mName{ "NetworkDebugLayer" } {
    }

    auto NetworkDebugLayer::OnCreate() -> void {
        MKT_BEGIN_PROFILER();
        mVisible = false;
    }

    auto NetworkDebugLayer::StartRequest() -> void {
        MKT_BEGIN_PROFILER();
        mClient = eastl::make_unique<network::HttpClient>( mUrl );
        mRequestUrl = mUrl;
        mResponseReady = false;
        mClient->GetAsync();
    }

    auto NetworkDebugLayer::OnDestroy() -> void {
        MKT_BEGIN_PROFILER();
        mClient.reset();

        mBodyPreview.clear();
    }

    auto NetworkDebugLayer::OnRender( float ) -> void {
    }

    auto NetworkDebugLayer::GetName() const -> eastl::string_view {
        return mName;
    }

    auto NetworkDebugLayer::OnEvent( platform::Event &event ) -> void {
        MKT_BEGIN_PROFILER();

        if ( event.IsType( platform::EventType::KEY_PRESSED_EVENT ) ) {
            const auto &key{ static_cast<const platform::KeyPressedEvent &>( event ) };

            const auto modifiers{ key.GetModifiers() & ( platform::ModKey::eControl | platform::ModKey::eShift |
                                                        platform::ModKey::eAlt | platform::ModKey::eSuper ) };

            if ( key.GetKeyCode() == platform::KeyCode::Key_F8 && !key.IsRepeated() &&
                 modifiers == ( platform::ModKey::eControl | platform::ModKey::eShift ) ) {
                mVisible = !mVisible;
                event.SetHandled( true );
            }
        }
    }

    auto NetworkDebugLayer::OnUpdate( float ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mClient ) {
            mClient->Poll();
        }

        if ( mClient && !mResponseReady && !mClient->IsPending() ) {
            mResponseReady = true;
            const auto &body{ mClient->GetResponse().mBody };
            mBodyPreview.assign( body.data(), eastl::min( body.size(), kPreviewBytes ) );

            for ( char &character: mBodyPreview ) {
                const auto byte{ static_cast<unsigned char>( character ) };

                if ( ( byte < 32 && character != '\n' && character != '\r' && character != '\t' ) || byte == 127 ) {
                    character = '.';
                }
            }
        }

        if ( mVisible ) {
            DisplayImGuiWindow();
        }
    }

    auto NetworkDebugLayer::DisplayImGuiWindow() -> void {
        MKT_BEGIN_PROFILER();
        ImGui::SetNextWindowSize( { 700, 560 }, ImGuiCond_FirstUseEver );

        if ( !imgui::BeginWindow( imgui::widget::LocalizedLabel(
                                "network_debug_layer_network_debug_networkdebuglayer",
                                "Network Debug##NetworkDebugLayer" )
            .c_str(),
                 &mVisible ) ) {
            ImGui::End();
            return;
        }

        ImGui::TextUnformatted( MKT_LOC( "network_debug_layer_python_http_server_tester_ctrl_shift_f8_toggles_this_window" ).c_str() );
        ImGui::TextDisabled( MKT_LOC( "network_debug_layer_server_py_m_http_server_9000" ).c_str() );
        ImGui::TextWrapped( MKT_LOC( "network_debug_layer_use_localhost_127_0_0_1_or_1_the_server" ).c_str() );

        const bool pending{ mClient && mClient->IsPending() };
        ImGui::BeginDisabled( pending );
        ImGui::SetNextItemWidth( -1 );
        const bool enter{ ImGui::InputText( "##HttpUrl", mUrl, sizeof( mUrl ), ImGuiInputTextFlags_EnterReturnsTrue ) };

        if ( ImGui::Button( "GET" ) || enter ) {
            mBodyPreview.clear();
            StartRequest();
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "network_debug_layer_root", "Root /" ).c_str() ) ) {
            std::snprintf( mUrl, sizeof( mUrl ), "%s", "http://localhost:9000/" );
        }

        ImGui::SameLine();

        if ( ImGui::Button( imgui::widget::LocalizedLabel( "network_debug_layer_missing_file_404", "Missing file (404)" ).c_str() ) ) {
            std::snprintf( mUrl, sizeof( mUrl ), "%s", "http://localhost:9000/__mikoto_missing_file_404__" );
        }

        ImGui::SameLine();

        if ( ImGui::Button( "IPv6" ) ) {
            std::snprintf( mUrl, sizeof( mUrl ), "%s", "http://[::1]:9000/" );
        }

        ImGui::EndDisabled();

        if ( mClient && mClient->IsPending() ) {
            ImGui::SameLine();

            if ( ImGui::Button( imgui::widget::LocalizedLabel( "network_debug_layer_cancel", "Cancel" ).c_str() ) ) {
                mClient->Cancel();
            }
        }

        ImGui::TextDisabled( MKT_LOC( "network_debug_layer_request_limits" ).c_str() );
        ImGui::Separator();

        if ( !mClient ) {
            ImGui::TextWrapped( MKT_LOC( "network_debug_layer_start_your_server_then_press_get_a_404_response_still" ).c_str() );
            ImGui::End();
            return;
        }

        ImGui::TextWrapped( "GET %s", mRequestUrl.c_str() );
        const char *statusKey{ "network_debug_layer_unknown" };

        switch ( mClient->GetStatus() ) {
            case network::HttpClient::Status::Connecting:
                statusKey = "network_debug_layer_connecting";
                break;
            case network::HttpClient::Status::Sending:
                statusKey = "network_debug_layer_sending_get";
                break;
            case network::HttpClient::Status::Receiving:
                statusKey = "network_debug_layer_receiving";
                break;
            case network::HttpClient::Status::Complete:
                statusKey = "network_debug_layer_response_received";
                break;
            case network::HttpClient::Status::Failed:
                statusKey = "network_debug_layer_request_failed";
                break;
            case network::HttpClient::Status::Cancelled:
                statusKey = "network_debug_layer_cancelled";
                break;
            case network::HttpClient::Status::Idle:
                break;
        }

        ImGui::Text( MKT_LOC( "network_debug_layer_ms_bytes_received" ).c_str(),
            MKT_LOC( statusKey ).c_str(), mClient->GetElapsedMilliseconds(),
            static_cast<unsigned long long>( mClient->GetReceivedBytes() ) );

        if ( mResponseReady && !mClient->GetError().empty() ) {
            ImGui::TextWrapped( "%s", mClient->GetError().c_str() );
        }

        if ( mResponseReady && mClient->GetResponse().mStatus != "0" ) {
            const auto &response{ mClient->GetResponse() };
            ImGui::TextColored( response.IsStatusOK() ? ImVec4{ .3f, .85f, .45f, 1 } : ImVec4{ 1, .7f, .25f, 1 },
                "HTTP %s %s", response.mStatus.c_str(), response.mReason.c_str() );

            if ( ImGui::CollapsingHeader( imgui::widget::LocalizedLabel( "network_debug_layer_response_headers", "Response headers" ).c_str(), ImGuiTreeNodeFlags_DefaultOpen ) ) {
                ImGui::BeginChild( "Headers", { 0, 110 }, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar );

                for ( const auto &[name, value]: response.mHeaders ) {
                    ImGui::Text( "%s: %s", name.c_str(), value.c_str() );
                }

                ImGui::EndChild();
            }

            ImGui::Text( MKT_LOC( "network_debug_layer_body_bytes" ).c_str(), static_cast<unsigned long long>( response.mBody.size() ) );

            if ( response.mBody.size() > kPreviewBytes ) {
                ImGui::TextDisabled( MKT_LOC( "network_debug_layer_preview_limited_to_the_first_64_kib" ).c_str() );
            }

            ImGui::TextDisabled( MKT_LOC( "network_debug_layer_raw_text_preview_control_bytes_are_shown_as_dots" ).c_str() );
            ImGui::BeginChild( "Body", { 0, 0 }, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar );

            if ( mBodyPreview.empty() ) {
                ImGui::TextDisabled( MKT_LOC( "network_debug_layer_empty_body" ).c_str() );
            }
            else {
                ImGui::TextUnformatted( mBodyPreview.data(), mBodyPreview.data() + mBodyPreview.size() );
            }

            ImGui::EndChild();
        }

        ImGui::End();
    }
}// namespace mikoto::editor