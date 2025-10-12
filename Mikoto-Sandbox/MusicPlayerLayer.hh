//
// Created by kate on 10/12/25.
//

#ifndef MUSICPLAYERLAYER_H
#define MUSICPLAYERLAYER_H
#include <Assets/Audio.hh>
#include <Audio/AudioDevice.hh>
#include <Core/LayerStack.hh>
#include <string_view>
#include <array>

namespace Mikoto {
    class MusicPlayerLayer final : public ILayer {
    public:
        explicit MusicPlayerLayer( std::string_view name );

        auto OnCreate() -> void override;
        auto OnDestroy() -> void override;
        auto OnUpdate( float deltaTime ) -> void override;

    private:
        auto LoadAudio( std::string_view path ) -> void;
        auto DrawLoadAudioUI() -> void;

    private:
        std::vector<AudioHandle> m_Tracks{};

        AudioSourceHandle m_Target{};
        AudioSourceHandle m_NewAudio{ };

        int m_SelectedIndex{ -1 };
        float m_Volume{ 0.5f };

        std::array<char, 512> m_InputPath{};
    };
}// namespace Mikoto

#endif //MUSICPLAYERLAYER_H
