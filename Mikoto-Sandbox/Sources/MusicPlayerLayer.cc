//
// Created by kate on 10/12/25.
//

#include <imgui.h>

#include <Assets/AssetsService.hh>
#include <Filesystem/FileService.hh>
#include <ImGui/ImGuiUtility.hh>
#include <MusicPlayerLayer.hh>

namespace Mikoto {
    MusicPlayerLayer::MusicPlayerLayer( std::string_view name )
        : ILayer( name ) {
    }

    auto MusicPlayerLayer::OnCreate() -> void {
        // Optionally preload some audio files
        LoadAudio( "./harajuku_8211997.mp3" );
        LoadAudio( "./vtuber_8899707_rockoTensei.mp3" );

        MKT_FILE_LOGGER_DEBUG( "Initializing Graphics Layer" );
    }

    void MusicPlayerLayer::OnDestroy() {
        m_Tracks.clear();
    }

    auto MusicPlayerLayer::OnUpdate( float /*deltaTime*/ ) -> void {
        ImGuiUtils::ImGuiScopedBorderColor borderColor{ {0, 200, 255, 255 } };
        ImGui::Begin( "\uf025 Music Player", nullptr, ImGuiWindowFlags_AlwaysAutoResize );

        DrawLoadAudioUI();

        // Track list (dynamic height)
        constexpr ImVec2 trackListMinSize{ 300, 100 };// minimum width x height
        if ( ImGui::BeginChild( "TrackList", trackListMinSize, true, ImGuiWindowFlags_AlwaysVerticalScrollbar | ImGuiWindowFlags_HorizontalScrollbar ) ) {
            for ( Size i{}; i < m_Tracks.size(); ++i ) {
                const auto& audio{ m_Tracks[i] };
                if ( ImGui::Selectable( audio->GetTrackName().c_str(), i == m_SelectedIndex ) ) {
                    m_SelectedIndex = static_cast<int>( i );
                    m_NewAudio = m_Tracks.at( m_SelectedIndex )->CreateSource();
                }
            }
            ImGui::EndChild();
        }

        ImGui::SameLine();


        // Controls
        ImGui::BeginGroup();
        if ( ImGui::Button( "\uf04b Play", ImVec2{ 80, 40 } ) ) {
            if ( !m_NewAudio.IsEmpty() ) {
                if ( !m_Target.IsEmpty() && !m_Target->IsSameAudio( m_NewAudio.GetRaw() ) ) {
                    m_Target->Stop();
                }

                m_Target = m_NewAudio;
            }

            // check, when we have never selected an audio
            if ( !m_Target.IsEmpty() ) {
                m_Target->Play();
            }
        }

        ImGui::SameLine();
        if ( ImGui::Button( "\uf04c Pause", ImVec2{ 80, 40 } ) && !m_Target.IsEmpty() ) {
            m_Target->Pause();
        }

        ImGui::SameLine();
        if ( ImGui::Button( "\uf04d Stop", ImVec2{ 80, 40 } ) && !m_Target.IsEmpty() ) {
            m_Target->Stop();
        }

        if ( !m_Target.IsEmpty() ) {
            ImGui::SliderFloat( "Volume", &m_Volume, 0.0f, 1.0f );

            m_Target->SetVolume( m_Volume );

            const float audioProgress{ m_Target->GetCurrentProgress() };
            const float totalDuration{ m_Target->GetAudioDuration() };
            const float progress{ totalDuration > 0.0f ? audioProgress / totalDuration : 0.0f };
            ImGui::ProgressBar( progress, ImVec2{ -1.0f, 0.0f } );

            auto formatTime{ []( float seconds ) {
                Int32 totalSec{ static_cast<Int32>( seconds ) };
                Int32 minutes{ totalSec / 60 };
                Int32 secs{ totalSec % 60 };
                return fmt::format( "{:02}:{:02}", minutes, secs );
            } };
            ImGui::Text( "%s / %s (s)", formatTime( audioProgress ).c_str(), formatTime( totalDuration ).c_str() );
        }
        ImGui::SameLine();

        if (!m_Target.IsEmpty()) {
            bool spatialized{ m_Target->IsSpatialized() };

            if (ImGui::Checkbox( "Spatialized", std::addressof( spatialized ))) {
                m_Target->SetSpatialization( spatialized );
            }
        }

        ImGui::EndGroup();

        ImGui::End();
    }

    auto MusicPlayerLayer::DrawLoadAudioUI() -> void {
        ImGui::InputText( "Audio Path", m_InputPath.data(), m_InputPath.size() );
        ImGui::SameLine();
        if ( ImGui::Button( "Load" ) ) {
            TaskService::Get()->Submit( [this]() -> void {
                // prepare filters for the dialog
                std::initializer_list<std::pair<std::string, std::string>> filters{
                    { "Audio files", "mp3,wav" },
                };

                const std::string songPath{ FileService::Get()->OpenDialog( filters ).string() };

                for ( Size i{}; i < std::min( m_InputPath.size(), songPath.size() ); ++i ) {
                    m_InputPath[i] = songPath[i];
                }

                if ( std::strlen( m_InputPath.data() ) > 0 ) {
                    LoadAudio( songPath );

                    // Clear input after loading
                    m_InputPath[0] = '\0';
                }
            } );
        }
    }

    auto MusicPlayerLayer::LoadAudio( std::string_view path ) -> void {

        auto file{ FileService::Get()->LoadFile( path ) };
        if ( !file ) {
            MKT_CORE_LOGGER_ERROR( "Failed to load audio file: {}", path );
            return;
        }

        const AudioLoadDescription desc{
            .AudioFile{ file },
            .Volume{ 0.5f }
        };

        AudioHandle handle{ AssetsService::Get()->LoadAsset<Audio>( desc ) };
        if ( handle.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Audio handle is empty: {}", path );
            return;
        }

        const std::string newTrackName{ Path{ path }
                                                .replace_extension()
                                                .filename()
                                                .string() };

        if ( !std::ranges::any_of( m_Tracks, [&]( const AudioHandle& track ) {
                 return track->GetTrackName().c_str() == newTrackName;
             } ) ) {
            m_Tracks.push_back( handle );

            MKT_FILE_LOGGER_DEBUG( "Registered audio track: {}", path );
        }
    }
}// namespace Mikoto