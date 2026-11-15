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

#include <mutex>
#include <string>
#include <exception>
#include <filesystem>
#include <initializer_list>

#include <imgui.h>

#include <EASTL/utility.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/unique_ptr.h>

#include <TracyViewer.hh>

#include <Core/Profiler.hh>
#include <Core/LocalizationService.hh>

#include <Filesystem/Path.hh>
#include <Filesystem/FileSystem.hh>

#include <Threading/TaskService.hh>

#include <ImGui/Utility.hh>

#include <Panels/InstrumentationPanel.hh>

namespace mikoto::editor {

    struct CaptureDeleter {
        auto operator()( tracy_viewer::Capture* capture ) const -> void {
            tracy_viewer::Close( capture );
        }
    };

    struct InstrumentationPanel::State {
        using CaptureHandle = eastl::unique_ptr<tracy_viewer::Capture, CaptureDeleter>;

        struct LoadResult {
            std::mutex mMutex{};
            CaptureHandle mCapture{};
            tracy_viewer::OpenError mError{};
            bool mReady{};
        };

        eastl::shared_ptr<LoadResult> mPending{};
        CaptureHandle mCapture{};
        tracy_viewer::OpenError mError{};
    };

    static auto GetCaptureErrorKey( tracy_viewer::OpenError error ) -> core::cstr {
        using tracy_viewer::OpenError;

        switch ( error ) {
            case OpenError::eConnectFailed:
                return "instrumentation_connect_failed";
            case OpenError::eInvalidTrace:
                return "instrumentation_invalid_capture";
            case OpenError::eUnsupportedVersion:
                return "instrumentation_unsupported_capture";
            default:
                return "instrumentation_read_failed";
        }
    }

    InstrumentationPanel::InstrumentationPanel()
        : Panel{ "Instrumentation", "Instrumentation", "Diagnostics", true },
          mState{ eastl::make_unique<State>() } {
        mPanelIsVisible = false;

        tracy_viewer::Initialize( []( bool save, const char* extension ) -> std::string {
            const auto label{ MKT_LOC( "instrumentation_capture_files" ) };
            const std::initializer_list<filesystem::FileDialogPair> filters{ { label.c_str(), extension } };
            const auto path{ save ? filesystem::SaveFileDialog( "Capture.tracy", filters ) : filesystem::OpenFileDialog( filters ) };
            return path.IsEmpty() ? std::string{} : std::string{ path.GetC_Str() };
        }, []( const char* url ) {
            filesystem::OpenInBrowser( url );
        } );
    }

    InstrumentationPanel::~InstrumentationPanel() = default;

    auto InstrumentationPanel::OnRender( core::f32 ) -> void {
        MKT_BEGIN_PROFILER();
        PollCapture();
        tracy_viewer::CollectFrameTextures();

        if ( !mPanelIsVisible ) {
            return;
        }

        ImGui::SetNextWindowSize( { 960.0f, 600.0f }, ImGuiCond_FirstUseEver );

        if ( !imgui::BeginWindow( GetHeaderName().data(), &mPanelIsVisible ) ) {
            ImGui::End();
            return;
        }

        ImGui::BeginDisabled( static_cast<bool>( mState->mPending ) );

        if ( ImGui::Button( MKT_LOC( "instrumentation_open_capture" ).c_str() ) ) {
            OpenCapture();
        }

#if defined( MIKOTO_ENABLE_TRACY )
        if ( !mState->mCapture ) {
            ImGui::SameLine();

            if ( ImGui::Button( MKT_LOC( "instrumentation_start_capture" ).c_str() ) ) {
                try {
                    mState->mCapture.reset( tracy_viewer::Connect() );
                    mState->mError = tracy_viewer::OpenError::eNone;
                } catch ( const std::exception& ) {
                    mState->mError = tracy_viewer::OpenError::eConnectFailed;
                }
            }
        }
#endif

        ImGui::EndDisabled();

        if ( mState->mCapture ) {
            if ( tracy_viewer::CanStop( *mState->mCapture ) ) {
                ImGui::SameLine();

                if ( ImGui::Button( MKT_LOC( "instrumentation_stop_capture" ).c_str() ) ) {
                    tracy_viewer::Stop( *mState->mCapture );
                }
            }

            ImGui::SameLine();

            if ( ImGui::Button( MKT_LOC( "instrumentation_close_capture" ).c_str() ) ) {
                mState->mCapture.reset();
                mState->mError = tracy_viewer::OpenError::eNone;
            }
        }

        if ( mState->mPending ) {
            ImGui::TextUnformatted( MKT_LOC( "instrumentation_loading_capture" ).c_str() );
        }

        if ( mState->mError != tracy_viewer::OpenError::eNone ) {
            ImGui::TextWrapped( "%s", MKT_LOC( GetCaptureErrorKey( mState->mError ) ).c_str() );
        }

        if ( mState->mCapture ) {
            ImGui::Separator();
            if ( !tracy_viewer::Draw( *mState->mCapture ) ) {
                mState->mCapture.reset();
            }
        }

        ImGui::End();
    }

    auto InstrumentationPanel::OpenCapture() -> void {
        const auto path{ filesystem::OpenFileDialog( { { MKT_LOC( "instrumentation_capture_files" ).c_str(), "tracy" } } ) };

        if ( path.IsEmpty() ) {
            return;
        }

        const auto pending{ eastl::make_shared<State::LoadResult>() };
        mState->mError = tracy_viewer::OpenError::eNone;

        try {
            const auto nativePath{ path.GetPathTyped<std::filesystem::path>() };

            // Trace decompression can be expensive. The worker owns the mailbox if the panel closes.
            threading::TaskService::Get()->ExecuteWorkerTask( [pending, path, nativePath] {
                tracy_viewer::OpenError error{};
                State::CaptureHandle capture{ tracy_viewer::Open( nativePath, path.GetC_Str(), error ) };

                const std::lock_guard lock{ pending->mMutex };
                pending->mCapture = eastl::move( capture );
                pending->mError = error;
                pending->mReady = true;
            } );
            mState->mPending = pending;
        } catch ( const std::exception& ) {
            mState->mError = tracy_viewer::OpenError::eReadFailed;
        }
    }

    auto InstrumentationPanel::PollCapture() -> void {
        const auto pending{ mState->mPending };

        if ( !pending ) {
            return;
        }

        State::CaptureHandle capture{};
        {
            const std::lock_guard lock{ pending->mMutex };

            if ( !pending->mReady ) {
                return;
            }

            capture = eastl::move( pending->mCapture );
            mState->mError = pending->mError;
        }

        mState->mPending.reset();

        if ( capture ) {
            mState->mCapture = eastl::move( capture );
        }
    }

}
