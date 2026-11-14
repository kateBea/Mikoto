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

#include <memory>
#include <cstdlib>
#include <charconv>
#include <exception>
#include <string_view>

#include <imgui.h>

#include <Fonts.hpp>
#include <TracyView.hpp>
#include <TracyEmbed.hpp>
#include <TracyWeb.hpp>
#include <TracyMouse.hpp>
#include <TracyFileRead.hpp>
#include <TracyTexture.hpp>
#include <TracyFileselector.hpp>

#include <data/FontBold.hpp>
#include <data/FontFixed.hpp>
#include <data/FontIcons.hpp>
#include <data/FontEmoji.hpp>
#include <data/FontItalic.hpp>
#include <data/FontNormal.hpp>
#include <data/FontBoldItalic.hpp>

#include "TracyViewer.hh"

FontData g_fonts{};
float FontNormal{ 15.0f };
float FontSmall{ 10.0f };
float FontBig{ 21.0f };

namespace tracy {

    static mikoto::editor::tracy_viewer::OpenUrl gOpenUrl{};

    void OpenWebpage( const char* url ) {
        if ( gOpenUrl ) {
            gOpenUrl( url );
        }
    }

}

namespace tracy::Fileselector {

    static mikoto::editor::tracy_viewer::SelectFile gSelectFile{};

    void OpenFile( const char* ext, const char*, const std::function<void( const char* )>& callback ) {
        const auto path{ gSelectFile( false, ext ) };
        if ( !path.empty() ) {
            callback( path.c_str() );
        }
    }

    void SaveFile( const char* ext, const char*, const std::function<void( const char* )>& callback ) {
        const auto path{ gSelectFile( true, ext ) };
        if ( !path.empty() ) {
            callback( path.c_str() );
        }
    }

}

namespace mikoto::editor::tracy_viewer {

    struct Capture {
        explicit Capture( tracy::FileRead& file )
            // The editor owns the window title and attention state.
            : mView{ std::make_unique<tracy::View>( nullptr, file, []( const char* ) {}, nullptr, [] {}, nullptr ) } {}

        Capture( const char* address, uint16_t port ) {
            Reconnect( address, port );
        }

        auto Reconnect( const char* address, uint16_t port ) -> void {
            mView.reset();
            mView = std::make_unique<tracy::View>( nullptr, address, port, []( const char* ) {}, nullptr, [] {}, nullptr );
        }

        std::unique_ptr<tracy::View> mView;
    };

    auto Initialize( SelectFile selectFile, OpenUrl openUrl ) -> void {
        tracy::Fileselector::gSelectFile = selectFile;
        tracy::gOpenUrl = openUrl;
        auto& atlas{ *ImGui::GetIO().Fonts };
        if ( atlas.Fonts.contains( g_fonts.normal ) ) {
            return;
        }

        // Follow Tracy's font families and icon merges in the editor's shared atlas.
        // https://github.com/wolfpld/tracy/blob/v0.14.1/profiler/src/Fonts.cpp
        static const auto normal{ Unembed( FontNormal ) };
        static const auto mono{ Unembed( FontFixed ) };
        static const auto bold{ Unembed( FontBold ) };
        static const auto boldItalic{ Unembed( FontBoldItalic ) };
        static const auto italic{ Unembed( FontItalic ) };
        static const auto icons{ Unembed( FontIcons ) };
        static const auto emoji{ Unembed( FontEmoji ) };
        constexpr float kIconSize{ 14.0f };

        const auto addFont{ [&]( const EmbedData& data, bool fixed = false ) {
            ImFontConfig base{};
            base.FontDataOwnedByAtlas = false;
            if ( fixed ) {
                base.GlyphExtraAdvanceX = -1.0f;
            }
            auto* font{ atlas.AddFontFromMemoryTTF( const_cast<char*>( data.data() ), static_cast<int>( data.size() ), FontNormal, &base ) };

            ImFontConfig merge{};
            merge.FontDataOwnedByAtlas = false;
            merge.MergeMode = true;
            merge.DstFont = font;
            atlas.AddFontFromMemoryTTF( const_cast<char*>( icons->data() ), static_cast<int>( icons->size() ), kIconSize, &merge );
            atlas.AddFontFromMemoryTTF( const_cast<char*>( emoji->data() ), static_cast<int>( emoji->size() ), kIconSize, &merge );
            return font;
        } };

        g_fonts = {
            .normal = addFont( *normal ),
            .mono = addFont( *mono, true ),
            .bold = addFont( *bold ),
            .boldItalic = addFont( *boldItalic ),
            .italic = addFont( *italic ),
        };
    }

    auto Open( const std::filesystem::path& path, const char* utf8Path, OpenError& error ) -> Capture* {
        error = OpenError::eNone;

        try {
#ifdef _WIN32
            const std::unique_ptr<tracy::FileRead> file{ tracy::FileRead::Open( path.c_str(), utf8Path ) };
#else
            const std::unique_ptr<tracy::FileRead> file{ tracy::FileRead::Open( path.c_str() ) };
#endif
            if ( file ) {
                return new Capture{ *file };
            }

            error = OpenError::eReadFailed;
        } catch ( const tracy::UnsupportedVersion& ) {
            error = OpenError::eUnsupportedVersion;
        } catch ( const tracy::LegacyVersion& ) {
            error = OpenError::eUnsupportedVersion;
        } catch ( const tracy::NotTracyDump& ) {
            error = OpenError::eInvalidTrace;
        } catch ( const std::exception& ) {
            error = OpenError::eReadFailed;
        }

        return nullptr;
    }

    auto Connect() -> Capture* {
        // Match the profiler's default and its TRACY_PORT override.
        // https://github.com/wolfpld/tracy/blob/v0.14.1/public/client/TracyProfiler.cpp
        constexpr uint16_t kDefaultPort{ 8086 };
        auto port{ kDefaultPort };

        if ( const char* env{ std::getenv( "TRACY_PORT" ) } ) {
            const std::string_view value{ env };
            uint16_t parsed{};
            const auto result{ std::from_chars( value.data(), value.data() + value.size(), parsed ) };
            if ( result.ec == std::errc{} && result.ptr == value.data() + value.size() && parsed ) {
                port = parsed;
            }
        }

        return new Capture{ "127.0.0.1", port };
    }

    auto CanStop( const Capture& capture ) -> bool {
        return capture.mView->CanStopCapture();
    }

    auto Stop( Capture& capture ) -> void {
        capture.mView->StopCapture();
    }

    auto Close( Capture* capture ) -> void {
        delete capture;
    }

    auto CollectFrameTextures() -> void {
        tracy::CollectTextures();
    }

    auto Draw( Capture& capture ) -> bool {
        const auto size{ ImGui::GetContentRegionAvail() };
        capture.mView->NotifyRootWindowSize( size.x, size.y );

        // Refresh Tracy's mouse snapshot after the editor begins the ImGui frame.
        // https://github.com/wolfpld/tracy/blob/v0.14.1/profiler/src/main.cpp
        tracy::MouseFrame();

        ImGui::PushFont( g_fonts.normal, FontNormal );
        const bool keepOpen{ capture.mView->Draw() };
        ImGui::PopFont();

        if ( !keepOpen && capture.mView->ReconnectRequested() ) {
            const auto address{ capture.mView->GetAddress() };
            capture.Reconnect( address.c_str(), capture.mView->GetPort() );
            return true;
        }

        return keepOpen;
    }

}
