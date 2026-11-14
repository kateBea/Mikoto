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

#include <Core/Exception.hh>

#include <Video/AacDecoder.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <mfapi.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#endif

namespace mikoto::video {

    using namespace mikoto::core;

#if defined( MIKOTO_PLATFORM_WINDOWS )

    using Microsoft::WRL::ComPtr;

    // minimp4 demuxes AAC; the OS decoder produces PCM for the existing audio engine.
    // https://learn.microsoft.com/en-us/windows/win32/medfound/aac-decoder
    // https://learn.microsoft.com/en-us/windows/win32/medfound/basic-mft-processing-model
    static auto CheckMediaResult( HRESULT result, cstr operation ) -> void {
        if ( FAILED( result ) ) {
            MKT_THROW_RUNTIME_ERROR( string::Format( "{} (HRESULT 0x{:08x})", operation, static_cast<u32>( result ) ) );
        }
    }

    AacDecoder::AacDecoder( const AudioTrackInfo& track ) : mTrack{ track } {
        CheckMediaResult( CoCreateInstance( CLSID_CMSAACDecMFT, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS( mDecoder.GetAddressOf() ) ), "Create AAC decoder" );

        ComPtr<IMFMediaType> input{}, output{};
        CheckMediaResult( MFCreateMediaType( &input ), "Create AAC input type" );
        CheckMediaResult( input->SetGUID( MF_MT_MAJOR_TYPE, MFMediaType_Audio ), "Set AAC media type" );
        CheckMediaResult( input->SetGUID( MF_MT_SUBTYPE, MEDIASUBTYPE_RAW_AAC1 ), "Set raw AAC subtype" );
        CheckMediaResult( input->SetUINT32( MF_MT_AUDIO_NUM_CHANNELS, track.mChannels ), "Set AAC channels" );
        CheckMediaResult( input->SetUINT32( MF_MT_AUDIO_SAMPLES_PER_SECOND, track.mSampleRate ), "Set AAC sample rate" );
        CheckMediaResult( input->SetUINT32( MF_MT_AUDIO_BITS_PER_SAMPLE, 32 ), "Request floating-point AAC output" );
        CheckMediaResult( input->SetBlob( MF_MT_USER_DATA, track.mCodecConfiguration.data(),
            static_cast<u32>( track.mCodecConfiguration.size() ) ), "Set AAC AudioSpecificConfig" );
        CheckMediaResult( mDecoder->SetInputType( 0, input.Get(), 0 ), "Configure AAC input" );

        // Use the decoder's advertised media type, including its native layout attributes.
        // Synthesizing an equivalent-looking PCM type can omit required MFT attributes.
        for ( DWORD index{}; ; ++index ) {
            ComPtr<IMFMediaType> candidate{};
            CheckMediaResult( mDecoder->GetOutputAvailableType( 0, index, &candidate ), "Find floating-point AAC output" );
            GUID subtype{};
            UINT32 channels{}, rate{}, bits{};
            candidate->GetGUID( MF_MT_SUBTYPE, &subtype );
            candidate->GetUINT32( MF_MT_AUDIO_NUM_CHANNELS, &channels );
            candidate->GetUINT32( MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate );
            candidate->GetUINT32( MF_MT_AUDIO_BITS_PER_SAMPLE, &bits );

            if ( subtype == MFAudioFormat_Float && bits == 32 && channels == track.mChannels && rate == track.mSampleRate ) {
                output = candidate;
                break;
            }
        }

        CheckMediaResult( mDecoder->SetOutputType( 0, output.Get(), 0 ), "Configure AAC PCM output" );

        MFT_OUTPUT_STREAM_INFO info{};
        CheckMediaResult( mDecoder->GetOutputStreamInfo( 0, &info ), "Query AAC output size" );
        mOutputBufferBytes = info.cbSize;
        CheckMediaResult( mDecoder->ProcessMessage( MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0 ), "Start AAC streaming" );
        CheckMediaResult( mDecoder->ProcessMessage( MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0 ), "Start AAC stream" );
    }

    auto AacDecoder::Decode( const EncodedPacket& packet ) -> eastl::vector<PcmBlock> {
        ComPtr<IMFSample> sample{};
        ComPtr<IMFMediaBuffer> buffer{};
        CheckMediaResult( MFCreateSample( &sample ), "Create AAC sample" );
        CheckMediaResult( MFCreateMemoryBuffer( static_cast<u32>( packet.mBytes.size() ), &buffer ), "Create AAC input buffer" );

        BYTE* bytes{};
        CheckMediaResult( buffer->Lock( &bytes, nullptr, nullptr ), "Map AAC sample" );
        std::memcpy( bytes, packet.mBytes.data(), packet.mBytes.size() );
        buffer->Unlock();
        CheckMediaResult( buffer->SetCurrentLength( static_cast<u32>( packet.mBytes.size() ) ), "Set AAC sample length" );
        CheckMediaResult( sample->AddBuffer( buffer.Get() ), "Attach AAC sample buffer" );

        const auto time{ static_cast<i64>( packet.mPresentationTime.ToSeconds() * 10000000 ) };
        sample->SetSampleTime( time );
        sample->SetSampleDuration( static_cast<i64>( packet.mDuration.ToSeconds() * 10000000 ) );
        mNextOutputTime = time;
        CheckMediaResult( mDecoder->ProcessInput( 0, sample.Get(), 0 ), "Decode AAC packet" );
        return ReadOutput();
    }

    auto AacDecoder::ReadOutput() -> eastl::vector<PcmBlock> {
        eastl::vector<PcmBlock> blocks{};

        for ( ;; ) {
            ComPtr<IMFSample> sample{};
            ComPtr<IMFMediaBuffer> buffer{};
            CheckMediaResult( MFCreateSample( &sample ), "Create PCM sample" );
            CheckMediaResult( MFCreateMemoryBuffer( mOutputBufferBytes, &buffer ), "Create PCM output buffer" );
            CheckMediaResult( sample->AddBuffer( buffer.Get() ), "Attach PCM output buffer" );

            MFT_OUTPUT_DATA_BUFFER output{};
            output.pSample = sample.Get();
            DWORD status{};
            const auto result{ mDecoder->ProcessOutput( 0, 1, &output, &status ) };

            if ( output.pEvents ) {
                output.pEvents->Release();
            }

            if ( result == MF_E_TRANSFORM_NEED_MORE_INPUT ) {
                break;
            }

            CheckMediaResult( result, "Read decoded AAC PCM" );
            CheckMediaResult( sample->ConvertToContiguousBuffer( &buffer ), "Collect PCM sample" );

            BYTE* samples{};
            DWORD length{};
            CheckMediaResult( buffer->Lock( &samples, nullptr, &length ), "Map decoded PCM" );
            PcmBlock block{};
            block.mSampleRate = mTrack.mSampleRate;
            block.mChannels = mTrack.mChannels;
            block.mSamples.resize( length / sizeof( float ) );
            std::memcpy( block.mSamples.data(), samples, length );
            buffer->Unlock();

            LONGLONG time{ mNextOutputTime };
            sample->GetSampleTime( &time );
            block.mPresentationTime = { time, 10000000 };
            mNextOutputTime = time + static_cast<i64>( block.mSamples.size() / mTrack.mChannels ) * 10000000 / mTrack.mSampleRate;
            blocks.push_back( eastl::move( block ) );
        }

        return blocks;
    }

    auto AacDecoder::Drain() -> eastl::vector<PcmBlock> {
        CheckMediaResult( mDecoder->ProcessMessage( MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0 ), "End AAC input" );
        CheckMediaResult( mDecoder->ProcessMessage( MFT_MESSAGE_COMMAND_DRAIN, 0 ), "Drain AAC decoder" );
        return ReadOutput();
    }

#else

    AacDecoder::AacDecoder( const AudioTrackInfo& track ) : mTrack{ track } {
        MKT_THROW_RUNTIME_ERROR( "AAC video audio currently requires the Windows Media Foundation decoder." );
    }

    auto AacDecoder::Decode( const EncodedPacket& ) -> eastl::vector<PcmBlock> {
        return {};
    }
    auto AacDecoder::ReadOutput() -> eastl::vector<PcmBlock> {
        return {};
    }
    auto AacDecoder::Drain() -> eastl::vector<PcmBlock> {
        return {};
    }

#endif
}
