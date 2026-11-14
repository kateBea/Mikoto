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

#include <cstring>

#include <EASTL/algorithm.h>

#include <Core/Exception.hh>

#include <Audio/AudioDevice.hh>

#include <Video/VideoAudioStream.hh>

namespace mikoto::video {

    using namespace mikoto::core;

    // Adapt miniaudio's custom-data-source and SPSC ring-buffer examples.
    // https://miniaud.io/docs/manual/index.html#data-sources
    // https://miniaud.io/docs/manual/index.html#ring-buffers
    static auto ReadPcm( ma_data_source* source, void* output, ma_uint64 count, ma_uint64* read ) -> ma_result {
        u64 consumed{};
        const auto result{ static_cast<VideoPcmDataSource*>( source )->mStream->ReadFrames( output, count, consumed ) };
        *read = consumed;
        return result;
    }

    static auto GetPcmFormat( ma_data_source* source, ma_format* format, ma_uint32* channels,
                              ma_uint32* sampleRate, ma_channel* channelMap, size_t capacity ) -> ma_result {
        const auto& stream{ *static_cast<VideoPcmDataSource*>( source )->mStream };
        *format = ma_format_f32;
        *channels = stream.GetChannels();
        *sampleRate = stream.GetSampleRate();

        if ( channelMap ) {
            ma_channel_map_init_standard( ma_standard_channel_map_default, channelMap, capacity, *channels );
        }

        return MA_SUCCESS;
    }

    static auto GetPcmCursor( ma_data_source* source, ma_uint64* cursor ) -> ma_result {
        *cursor = static_cast<VideoPcmDataSource*>( source )->mStream->GetConsumedFrames();
        return MA_SUCCESS;
    }

    VideoAudioStream::VideoAudioStream( audio::AudioDevice& device, const AudioTrackInfo& track, const audio::SourceSettings& settings )
        : mSampleRate{ track.mSampleRate }, mChannels{ track.mChannels } {
        if ( !mSampleRate || !mChannels || ma_pcm_rb_init( ma_format_f32, mChannels, mSampleRate * 2, nullptr, nullptr, &mBuffer ) != MA_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot initialize video PCM ring buffer." );
        }

        mBufferInitialized = true;
        static const ma_data_source_vtable callbacks{ ReadPcm, nullptr, GetPcmFormat, GetPcmCursor, nullptr, nullptr, 0 };
        auto config{ ma_data_source_config_init() };
        config.vtable = &callbacks;
        mDataSource.mStream = this;

        if ( ma_data_source_init( &config, &mDataSource ) != MA_SUCCESS ) {
            ma_pcm_rb_uninit( &mBuffer );
            MKT_THROW_RUNTIME_ERROR( "Cannot initialize video PCM data source." );
        }

        mDataSourceInitialized = true;

        try {
            mSource = device.CreateSource( &mDataSource.mBase );

            if ( !mSource ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot attach video PCM to the audio engine." );
            }

            auto playback{ settings };
            playback.mLooping = false;
            playback.mPitch = 1;
            playback.mDoppler = 0;
            mSource->SetSettings( playback );
        } catch ( ... ) {
            mSource.Reset();
            ma_data_source_uninit( &mDataSource );
            ma_pcm_rb_uninit( &mBuffer );
            throw;
        }
    }

    VideoAudioStream::~VideoAudioStream() {
        // Detach and quiesce the sound before releasing storage read by its callback.
        mSource.Reset();

        if ( mDataSourceInitialized ) {
            ma_data_source_uninit( &mDataSource );
        }

        if ( mBufferInitialized ) {
            ma_pcm_rb_uninit( &mBuffer );
        }
    }

    auto VideoAudioStream::Push( const PcmBlock& block ) -> bool {
        if ( block.mSampleRate != mSampleRate || block.mChannels != mChannels || block.mSamples.size() % mChannels ) {
            MKT_THROW_RUNTIME_ERROR( "Decoded video audio changed its PCM format." );
        }

        const auto count{ static_cast<u32>( block.mSamples.size() / mChannels ) };

        if ( count > ma_pcm_rb_available_write( &mBuffer ) ) {
            return false;
        }

        if ( !mAnchored.load() ) {
            mStartTime.store( block.mPresentationTime.ToSeconds() );
            mAnchored.store( true );
        }

        u32 written{};

        while ( written < count ) {
            auto frames{ count - written };
            void* destination{};
            ma_pcm_rb_acquire_write( &mBuffer, &frames, &destination );
            std::memcpy( destination, block.mSamples.data() + static_cast<usize>( written ) * mChannels,
                static_cast<usize>( frames ) * mChannels * sizeof( float ) );
            ma_pcm_rb_commit_write( &mBuffer, frames );
            written += frames;
        }

        return true;
    }

    auto VideoAudioStream::ReadFrames( void* output, u64 count, u64& read ) -> ma_result {
        read = 0;

        while ( read < count ) {
            auto frames{ static_cast<u32>( eastl::min<u64>( count - read, ma_pcm_rb_available_read( &mBuffer ) ) ) };

            if ( !frames ) {
                break;
            }

            void* source{};
            ma_pcm_rb_acquire_read( &mBuffer, &frames, &source );

            if ( output ) {
                std::memcpy( static_cast<float*>( output ) + read * mChannels, source,
                    static_cast<usize>( frames ) * mChannels * sizeof( float ) );
            }

            ma_pcm_rb_commit_read( &mBuffer, frames );
            read += frames;
        }

        mConsumedFrames.fetch_add( read );

        if ( mFinished.load() ) {
            return read ? MA_SUCCESS : MA_AT_END;
        }

        if ( output && read < count ) {
            std::memset( static_cast<float*>( output ) + read * mChannels, 0,
                static_cast<usize>( count - read ) * mChannels * sizeof( float ) );
        }

        // Starvation emits silence while the media clock waits for more PCM.
        read = count;
        return MA_SUCCESS;
    }

    auto VideoAudioStream::Finish() -> void {
        mFinished.store( true );
    }

    auto VideoAudioStream::GetPlaybackTime() const -> eastl::optional<f64> {
        if ( !mAnchored.load() ) {
            return {};
        }

        // This is consumed media time; device buffering adds a small output latency.
        return mStartTime.load() + static_cast<f64>( mConsumedFrames.load() ) / mSampleRate;
    }

    auto VideoAudioStream::GetQueuedSeconds() -> f64 {
        return static_cast<f64>( ma_pcm_rb_available_read( &mBuffer ) ) / mSampleRate;
    }

    auto VideoAudioStream::GetSource() -> audio::AudioSource* {
        return mSource.GetPtr();
    }

    auto VideoAudioStream::GetSampleRate() const -> u32 {
        return mSampleRate;
    }

    auto VideoAudioStream::GetChannels() const -> u32 {
        return mChannels;
    }

    auto VideoAudioStream::GetConsumedFrames() const -> u64 {
        return mConsumedFrames.load();
    }

}
