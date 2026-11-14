// Copyright 2026 ケイト
// Licensed under the Apache License, Version 2.0.

#ifndef MIKOTO_EDITOR_PANELS_AUDIO_PANEL_HH
#define MIKOTO_EDITOR_PANELS_AUDIO_PANEL_HH

#include <EASTL/shared_ptr.h>
#include <EASTL/unique_ptr.h>

#include <Filesystem/Path.hh>

#include <Audio/AudioDevice.hh>
#include <Audio/AudioSource.hh>
#include <Audio/AudioListener.hh>

#include <Renderer/Rhi/GpuDevice.hh>

#include <Panels/Panel.hh>

namespace mikoto::editor {
    struct EditorState;

    /**
     * @brief Draws the shared distance-curve editor used by audio previews and the inspector.
     * @param[in,out] settings Independent source settings whose selected curve is edited.
     * @returns True when a curve's points, interpolation or enabled state changes.
     * @details Selection and drag state are scoped to the current ImGui window and ID stack.
     */
    auto DrawAudioDistanceCurves( audio::SourceSettings& settings ) -> bool;

    struct AudioPanelCreateInfo {
        EditorState* mState{};
        renderer::rhi::IGpuDevice* mDevice{};
        eastl::string mGroup{ "Audio" };
        bool mIsToggleable{ true };
    };

    /** @brief Auditions audio files with a full-clip waveform and an independent spatial listener. */
    class AudioPanel final : public Panel {
    public:
        /** @brief Creates an initially hidden preview panel.
         * @param description Editor state, texture device and panel menu configuration.
         * @details The panel opens its own audio device lazily and never borrows scene audio objects.
         */
        explicit AudioPanel( const AudioPanelCreateInfo& description );

        /** @brief Updates waveform loading and draws playback, source and listener controls.
         * @param timeStep Elapsed editor frame time in seconds.
         */
        auto OnRender( float timeStep ) -> void override;

        /** @brief Stops preview playback and releases its source before its listener and audio device. */
        ~AudioPanel() override;

    private:
        struct WaveformLoad;

        auto LoadClip( const filesystem::Path& path ) -> void;
        auto ClearClip() -> void;
        auto RequestWaveform() -> void;
        auto PollWaveform() -> void;
        auto DrawWaveform() -> void;
        auto DrawPlayback() -> bool;
        auto DrawSpatialSettings() -> bool;
        auto ApplyPreviewPose() -> void;

        EditorState* mState{};
        renderer::rhi::IGpuDevice* mDevice{};
        filesystem::Path mPath{};
        eastl::string mError{};
        renderer::rhi::TextureHandle mWaveform{};
        eastl::shared_ptr<WaveformLoad> mWaveformLoad{};

        // Gives the preview its own playback cursor, volume, spatial settings,
        // and listener pose. Changing those controls doesn’t modify an entity’s audio component.

        // Member order keeps the device alive until all its borrowers have gone.
        eastl::unique_ptr<audio::AudioDevice> mAudioDevice{};
        eastl::unique_ptr<audio::AudioListener> mListener{};
        audio::AudioSourceHandle mSource{};
        audio::SourceSettings mSettings{};
        core::float3 mSourcePosition{ 0.0f, 0.0f, -3.0f };
        core::float3 mSourceVelocity{};
        core::float3 mListenerPosition{};
        core::float3 mListenerVelocity{};
        core::float2 mListenerAngles{};
    };
}// namespace mikoto::editor

#endif// MIKOTO_EDITOR_PANELS_AUDIO_PANEL_HH
