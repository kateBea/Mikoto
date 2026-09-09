//    Copyright 2026 ケイト
// Licensed under the Apache License, Version 2.0 (the "License");

#include <algorithm>
#include <cstdio>

#include <imgui.h>

#include <Core/CoreEvents.hh>
#include <Layers/NetworkDebugLayer.hh>

namespace mikoto::editor {
    namespace {
        constexpr core::usize kPreviewBytes{ 64 * 1024 };

        auto StatusName( const network::HttpProbe::Status status ) -> const char* {
            using Status = network::HttpProbe::Status;
            switch ( status ) {
                case Status::Connecting: return "Connecting";
                case Status::Sending: return "Sending GET";
                case Status::Receiving: return "Receiving";
                case Status::Complete: return "Response received";
                case Status::Failed: return "Request failed";
                case Status::Cancelled: return "Cancelled";
            }
            return "Unknown";
        }
    }

    NetworkDebugLayer::NetworkDebugLayer()
        : ILayer{ "NetworkDebugLayer" }
    {

    }

    auto NetworkDebugLayer::OnCreate() -> void {
        mVisible = true;
    }

    auto NetworkDebugLayer::OnDestroy() -> void {
        mProbe.reset();

        mBodyPreview.clear();
    }

    auto NetworkDebugLayer::OnEvent( core::IEvent& event ) -> void {
        if ( event.IsType( core::EventType::KEY_PRESSED_EVENT ) ) {
            const auto& key{ static_cast<const core::KeyPressedEvent&>( event ) };
            if ( key.GetKeyCode() == core::KeyCode::Key_F8 ) {
                mVisible = !mVisible;
                event.SetHandled( true );
            }
        }
    }

    auto NetworkDebugLayer::OnUpdate( float ) -> void {
        if ( mProbe && mProbe->IsPending() ) {
            mProbe->Poll();
            if ( !mProbe->IsPending() ) {
                const auto& body{ mProbe->GetResponse().mBody };
                mBodyPreview.assign( body.data(), std::min( body.size(), kPreviewBytes ) );
                for ( char& character : mBodyPreview ) {
                    const auto byte{ static_cast<unsigned char>( character ) };
                    if ( ( byte < 32 && character != '\n' && character != '\r' && character != '\t' ) || byte == 127 ) {
                        character = '.';
                    }
                }
            }
        }

        if ( mVisible ) {
            DisplayImGuiWindow();
        }
    }

    auto NetworkDebugLayer::DisplayImGuiWindow() -> void {
        ImGui::SetNextWindowSize( { 700, 560 }, ImGuiCond_FirstUseEver );
        if ( !ImGui::Begin( "Network Debug##NetworkDebugLayer", &mVisible ) ) { ImGui::End(); return; }

        ImGui::TextUnformatted( "Python HTTP server tester | F8 toggles this window" );
        ImGui::TextDisabled( "Server: py -m http.server 9000" );
        ImGui::TextWrapped( "Use localhost, 127.0.0.1 or [::1]. The server's [::] address is a bind address." );

        const bool pending{ mProbe && mProbe->IsPending() };
        ImGui::BeginDisabled( pending );
        ImGui::SetNextItemWidth( -1 );
        const bool enter{ ImGui::InputText( "##HttpUrl", mUrl, sizeof( mUrl ), ImGuiInputTextFlags_EnterReturnsTrue ) };
        if ( ImGui::Button( "GET" ) || enter ) {
            mBodyPreview.clear();
            mProbe = eastl::make_unique<network::HttpProbe>( mUrl );
        }
        ImGui::SameLine();
        if ( ImGui::Button( "Root /" ) ) std::snprintf( mUrl, sizeof( mUrl ), "%s", "http://localhost:9000/" );
        ImGui::SameLine();
        if ( ImGui::Button( "Missing file (404)" ) ) std::snprintf( mUrl, sizeof( mUrl ), "%s", "http://localhost:9000/__mikoto_missing_file_404__" );
        ImGui::SameLine();
        if ( ImGui::Button( "IPv6" ) ) std::snprintf( mUrl, sizeof( mUrl ), "%s", "http://[::1]:9000/" );
        ImGui::EndDisabled();
        if ( mProbe && mProbe->IsPending() ) {
            ImGui::SameLine();
            if ( ImGui::Button( "Cancel" ) ) mProbe->Cancel();
        }

        ImGui::TextDisabled( "Plain HTTP GET | 5 s timeout | 1 MiB response limit | No redirect following" );
        ImGui::Separator();
        if ( !mProbe ) {
            ImGui::TextWrapped( "Start your server, then press GET. A 404 response still confirms the connection works." );
            ImGui::End();
            return;
        }

        ImGui::TextWrapped( "GET %s", mProbe->GetUrl().c_str() );
        ImGui::Text( "%s | %.1f ms | %llu bytes received", StatusName( mProbe->GetStatus() ),
                     mProbe->GetElapsedMilliseconds(), static_cast<unsigned long long>( mProbe->GetReceivedBytes() ) );
        if ( !mProbe->GetError().empty() ) {
            ImGui::TextWrapped( "%s", mProbe->GetError().c_str() );
            ImGui::End();
            return;
        }

        if ( mProbe->GetStatus() == network::HttpProbe::Status::Complete ) {
            const auto& response{ mProbe->GetResponse() };
            ImGui::TextColored( response.IsStatusOK() ? ImVec4{ .3f, .85f, .45f, 1 } : ImVec4{ 1, .7f, .25f, 1 },
                                "HTTP %s %s", response.mStatus.c_str(), response.mReason.c_str() );
            if ( ImGui::CollapsingHeader( "Response headers", ImGuiTreeNodeFlags_DefaultOpen ) ) {
                ImGui::BeginChild( "Headers", { 0, 110 }, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar );
                for ( const auto& [name, value] : response.mHeaders ) ImGui::Text( "%s: %s", name.c_str(), value.c_str() );
                ImGui::EndChild();
            }
            ImGui::Text( "Body: %llu bytes", static_cast<unsigned long long>( response.mBody.size() ) );
            if ( response.mBody.size() > kPreviewBytes ) ImGui::TextDisabled( "Preview limited to the first 64 KiB." );
            ImGui::TextDisabled( "Raw text preview; control bytes are shown as dots." );
            ImGui::BeginChild( "Body", { 0, 0 }, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar );
            if ( mBodyPreview.empty() ) ImGui::TextDisabled( "(empty body)" );
            else ImGui::TextUnformatted( mBodyPreview.data(), mBodyPreview.data() + mBodyPreview.size() );
            ImGui::EndChild();
        }
        ImGui::End();
    }
}
