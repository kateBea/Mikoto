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

#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/exponential.hpp>

#include <Audio/AudioBackend.hh>

// Adapted from David Reid's miniaudio Steam Audio custom-node example:
// https://miniaud.io/docs/examples/engine_steamaudio.html
// The buffering and listener-space updates below also handle short reads and
// moving listeners without writing shared, non-atomic direction fields.

namespace mikoto::audio {

    // https://valvesoftware.github.io/steam-audio/doc/capi/integration.html

    static auto ResetHistory( SteamAudioNode& node ) -> void {
        iplBinauralEffectReset( node.mEffect );
        node.mInput.fill( 0.0f );
        node.mOutput.fill( {} );
        node.mCursor = 0;
        node.mDry.fill( {} );
        node.mLowPassHistory.fill( 0.0f );
        node.mReverbDamping.fill( {} );

        for ( auto& channel: node.mReverb ) {
            for ( auto& delay: channel ) {
                eastl::fill( delay.begin(), delay.end(), 0.f );
            }
        }
    }

    static auto RenderBlock( SteamAudioNode& node ) -> void {
        eastl::array<float*, 1> input{ node.mInput.data() };
        eastl::array<float*, 2> output{ node.mOutput[0].data(), node.mOutput[1].data() };
        IPLAudioBuffer source{ 1, kAudioBlockFrames, input.data() };
        IPLAudioBuffer destination{ 2, kAudioBlockFrames, output.data() };

        // Read miniaudio's atomic pose fields on the mixing thread so a
        // stationary source still follows listener movement and rotation.
        auto* engine{ ma_sound_get_engine( node.mSound ) };
        const auto listener{ ma_sound_get_listener_index( node.mSound ) };
        const auto position{ ma_sound_get_position( node.mSound ) };
        const auto origin{ ma_engine_listener_get_position( engine, listener ) };
        const auto ahead{ ma_engine_listener_get_direction( engine, listener ) };
        const auto up{ ma_engine_listener_get_world_up( engine, listener ) };

        IPLBinauralEffectParams params{};
        params.direction = iplCalculateRelativeDirection( node.mContext,
            { position.x, position.y, position.z }, { origin.x, origin.y, origin.z },
            { ahead.x, ahead.y, ahead.z }, { up.x, up.y, up.z } );
        const float lengthSquared{ params.direction.x * params.direction.x +
                                   params.direction.y * params.direction.y + params.direction.z * params.direction.z };

        if ( lengthSquared < 0.000001f ) {
            params.direction = { 0, 0, -1 };
        }

        params.interpolation = IPL_HRTFINTERPOLATION_BILINEAR;
        params.spatialBlend = 1.0f;
        params.hrtf = node.mHrtf;

        // Silent voices need no convolution once their filter tail ends.
        const bool silent{ eastl::all_of( input[0], input[0] + kAudioBlockFrames,
            []( float value ) {
            return value == 0.0f;
        } ) };

        if ( !silent ) {
            iplBinauralEffectApply( node.mEffect, &params, &source, &destination );
        }
        else if ( iplBinauralEffectGetTailSize( node.mEffect ) > 0 ) {
            iplBinauralEffectGetTail( node.mEffect, &destination );
        }
        else {
            node.mOutput.fill( {} );
        }

        // Evaluate distance curves once per block so every sample uses the same spatial mix.
        const float dx{ position.x - origin.x }, dy{ position.y - origin.y }, dz{ position.z - origin.z };
        const float distance{ glm::sqrt( dx * dx + dy * dy + dz * dz ) / node.mSettings.mMaxDistance };
        const auto evaluate = [&]( DistanceParameter parameter, float fallback ) {
            const auto& curve{ node.mSettings.mCurves[static_cast<size_t>( parameter )] };
            return curve.mEnabled ? curve.Evaluate( distance ) : fallback;
        };

        const float gain{ evaluate( DistanceParameter::eVolume, 1 ) };
        const float blend{ evaluate( DistanceParameter::eSpatialBlend, 1 ) };
        const float width{ 1 + evaluate( DistanceParameter::eSpread, 0 ) };

        // Keep the low-pass cutoff below Nyquist when the source moves close to the listener.
        const float cutoff{ 20.f * glm::pow( 1000.f, evaluate( DistanceParameter::eLowPass, 1 ) ) };
        const bool filtered{ node.mSettings.mCurves[static_cast<size_t>( DistanceParameter::eLowPass )].mEnabled };
        const float coefficient{ 1 - glm::exp( -6.2831853f * eastl::min( cutoff, node.mSampleRate * .45f ) / node.mSampleRate ) };
        const float wet{ evaluate( DistanceParameter::eReverb, 0 ) };

        for ( ma_uint32 i{}; i < kAudioBlockFrames; ++i ) {
            const float mid{ ( node.mOutput[0][i] + node.mOutput[1][i] ) * .5f };
            const float side{ ( node.mOutput[0][i] - node.mOutput[1][i] ) * .5f * width };

            for ( size_t ch{}; ch < 2; ++ch ) {
                float sample{ gain * ( blend * ( mid + ( ch ? -side : side ) ) + ( 1 - blend ) * node.mDry[ch][i] ) };
                node.mLowPassHistory[ch] += coefficient * ( sample - node.mLowPassHistory[ch] );

                if ( filtered ) {
                    sample = node.mLowPassHistory[ch];
                }

                float reverberated{};

                for ( size_t comb{}; comb < 4; ++comb ) {
                    auto& delay{ node.mReverb[ch][comb] };
                    auto& cursor{ node.mReverbCursor[ch][comb] };
                    const float echo{ delay[cursor] };
                    auto& damped{ node.mReverbDamping[ch][comb] };
                    damped = .8f * echo + .2f * damped;
                    delay[cursor] = sample * wet + .78f * damped;
                    cursor = ( cursor + 1 ) % delay.size();
                    reverberated += echo * .25f;
                }

                for ( size_t diffuser{ 4 }; diffuser < 6; ++diffuser ) {
                    auto& delay{ node.mReverb[ch][diffuser] };
                    auto& cursor{ node.mReverbCursor[ch][diffuser] };
                    const float echo{ delay[cursor] };
                    delay[cursor] = reverberated + .5f * echo;
                    reverberated = echo - .5f * delay[cursor];
                    cursor = ( cursor + 1 ) % delay.size();
                }

                const float panGain{ ch == 0 ? 1 - eastl::max( node.mSettings.mPan, 0.f ) : 1 + eastl::min( node.mSettings.mPan, 0.f ) };
                node.mOutput[ch][i] = node.mSettings.mMuted ? 0.f : ( sample + reverberated ) * panGain;
            }
        }
    }

    static auto Process( ma_node* base, const float** input, ma_uint32* inputFrames,
        float** output, ma_uint32* outputFrames ) -> void {
        auto& node{ *reinterpret_cast<SteamAudioNode*>( base ) };
        // Never block the mixer thread when the inspector publishes settings.
        if ( std::unique_lock lock{ node.mSettingsMutex, std::try_to_lock }; lock.owns_lock() ) {
            node.mSettings = node.mPendingSettings;
        }

        const bool spatialized{ ma_sound_is_spatialization_enabled( node.mSound ) == MA_TRUE };
        const auto reset{ node.mResetRevision.load( eastl::memory_order_relaxed ) };

        if ( reset != node.mAppliedReset || spatialized != node.mWasSpatialized ) {
            // Reset on the audio thread: seeking/stopping must never race
            // Steam Audio's convolution, or replay samples from the old cursor.
            ResetHistory( node );
            node.mAppliedReset = reset;
            node.mWasSpatialized = spatialized;
        }

        for ( ma_uint32 frame{}; frame < *outputFrames; ++frame ) {
            const bool available{ input && input[0] && frame < *inputFrames };
            const float left{ available ? input[0][frame * 2] : 0.0f };
            const float right{ available ? input[0][frame * 2 + 1] : 0.0f };

            if ( !spatialized ) {
                // Music/UI retain their original stereo image and incur no
                // extra block latency from the HRTF adapter.
                output[0][frame * 2] = left;
                output[0][frame * 2 + 1] = right;
                continue;
            }

            // Accumulate exactly one FFT block while playing the previous
            // result. This adds 256 frames of latency only to 3D voices and
            // remains correct for partial reads, EOF and offline rendering.
            output[0][frame * 2] = node.mOutput[0][node.mCursor];
            output[0][frame * 2 + 1] = node.mOutput[1][node.mCursor];
            node.mInput[node.mCursor] = ( left + right ) * 0.5f;
            node.mDry[0][node.mCursor] = left;
            node.mDry[1][node.mCursor] = right;

            if ( ++node.mCursor == kAudioBlockFrames ) {
                RenderBlock( node );
                node.mCursor = 0;
            }
        }
    }

    static const ma_node_vtable kBinauralVTable{
        Process, nullptr, 1, 1, MA_NODE_FLAG_CONTINUOUS_PROCESSING
    };


    auto SteamAudioNode::Initialize( ma_engine& engine, ma_sound& sound, IPLContext context, IPLHRTF hrtf ) -> bool {
        if ( mInitialized ) {
            return false;
        }

        mSound = &sound;
        mContext = context;
        mHrtf = hrtf;
        mSampleRate = float( ma_engine_get_sample_rate( &engine ) );
        constexpr float delays[]{ .0297f, .0371f, .0411f, .0437f, .005f, .0017f };

        for ( size_t ch{}; ch < 2; ++ch ) {
            for ( size_t i{}; i < 6; ++i ) {
                mReverb[ch][i].resize( eastl::max( size_t{ 1 }, size_t( ( delays[i] + ch * .0007f ) * mSampleRate ) ) );
            }
        }

        IPLAudioSettings audio{ static_cast<IPLint32>( ma_engine_get_sample_rate( &engine ) ), kAudioBlockFrames };
        IPLBinauralEffectSettings settings{ hrtf };

        if ( iplBinauralEffectCreate( context, &audio, &settings, &mEffect ) != IPL_STATUS_SUCCESS ) {
            return false;
        }

        ResetHistory( *this );

        const ma_uint32 channels{ 2 };
        auto config{ ma_node_config_init() };
        config.vtable = &kBinauralVTable;
        config.pInputChannels = &channels;
        config.pOutputChannels = &channels;

        if ( ma_node_init( ma_engine_get_node_graph( &engine ), &config, nullptr, &mNode ) != MA_SUCCESS ) {
            iplBinauralEffectRelease( &mEffect );
            return false;
        }

        mInitialized = true;

        // Publish the graph only after the effect and its scratch memory exist.
        if ( ma_node_attach_output_bus( &sound, 0, &mNode, 0 ) != MA_SUCCESS ||
             ma_node_attach_output_bus( &mNode, 0, ma_engine_get_endpoint( &engine ), 0 ) != MA_SUCCESS ) {
            Shutdown();
            return false;
        }

        return true;
    }

    auto SteamAudioNode::Shutdown() -> void {
        if ( !mInitialized ) {
            return;
        }
        // Detach and wait for graph readers before releasing anything that the
        // callback can touch, including the source's ma_sound.
        ma_node_uninit( &mNode, nullptr );
        iplBinauralEffectRelease( &mEffect );
        mInitialized = false;
    }

    auto SteamAudioNode::RequestReset() -> void {
        mResetRevision.fetch_add( 1, eastl::memory_order_relaxed );
    }
}
