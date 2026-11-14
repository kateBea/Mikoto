// Copyright 2026 ケイト. Licensed under the Apache License, Version 2.0.
#include <cstring>
#include <limits>

#include <EASTL/algorithm.h>
#include <EASTL/unique_ptr.h>

#include <Core/Exception.hh>

#include <Assets/AssetVideo.hh>

#include <Video/H264Parser.hh>

#define MINIMP4_IMPLEMENTATION
#include <Vendor/minimp4/minimp4.h>

namespace mikoto::asset {

    using namespace mikoto::core;
    using namespace mikoto::video;

    // Index once through minimp4; playback reads only our immutable sample table.
    // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiVideo.cpp
    // https://github.com/lieff/minimp4/blob/2a3014a7753cfcba4442adeb469431967238fe6a/minimp4.h
    static auto ReadSource( int64_t offset, void* destination, size_t size, void* token ) -> int {
        const auto& source{ *static_cast<const eastl::string*>( token ) };

        if ( offset < 0 || static_cast<u64>( offset ) > source.size() ||
             size > source.size() - static_cast<usize>( offset ) ) {
            return 1;
        }

        std::memcpy( destination, source.data() + offset, size );
        return 0;
    }

    static auto DestroyDemuxer( MP4D_demux_t* demuxer ) -> void {
        MP4D_close( demuxer );
        delete demuxer;
    }

    static auto ReadNalLength( const ubyte* bytes, u32 count ) -> u32 {
        u32 length{};

        for ( u32 index{}; index < count; ++index ) {
            length = ( length << 8 ) | bytes[index];
        }

        return length;
    }

    static auto ContainsIdr( eastl::span<const ubyte> bytes, u32 lengthBytes ) -> bool {
        bool idr{};
        usize offset{};

        while ( offset < bytes.size() ) {
            if ( bytes.size() - offset < lengthBytes ) {
                MKT_THROW_RUNTIME_ERROR( "Truncated H.264 NAL length in MP4 sample." );
            }

            const auto length{ ReadNalLength( bytes.data() + offset, lengthBytes ) };
            offset += lengthBytes;

            if ( !length || length > bytes.size() - offset ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid H.264 NAL extent in MP4 sample." );
            }

            idr |= ( bytes[offset] & 31 ) == h264::NAL_UNIT_TYPE_CODED_SLICE_IDR;
            offset += length;
        }

        return idr;
    }

    static auto ReadParameterSets( const MP4D_demux_t& demuxer, VideoTrackInfo& info ) -> void {
        for ( int kind{}; kind < 2; ++kind ) {
            auto& destination{ kind ? info.mPictureParameterSets : info.mSequenceParameterSets };

            for ( int index{}; ; ++index ) {
                int size{};
                const auto* bytes{ static_cast<const ubyte*>( kind ?
                    MP4D_read_pps( &demuxer, info.mTrackIndex, index, &size ) :
                    MP4D_read_sps( &demuxer, info.mTrackIndex, index, &size ) ) };

                if ( !bytes ) {
                    break;
                }

                if ( size <= 0 || index > ( kind ? h264::MAX_PPS_ID : h264::MAX_SPS_ID ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid H.264 parameter sets in MP4." );
                }

                destination.emplace_back( bytes, bytes + size );
            }
        }

        if ( info.mSequenceParameterSets.empty() || info.mPictureParameterSets.empty() ) {
            MKT_THROW_RUNTIME_ERROR( "MP4 H.264 track has no SPS/PPS configuration." );
        }
    }

    static auto RescaleEditTime( u64 ticks, u32 sourceScale, u32 targetScale ) -> i64 {
        if ( !sourceScale || ticks / sourceScale > static_cast<u64>( std::numeric_limits<i64>::max() ) / targetScale ) {
            MKT_THROW_RUNTIME_ERROR( "MP4 edit timing exceeds the supported timeline." );
        }

        const u64 whole{ ticks / sourceScale * targetScale };
        const u64 remainder{ ticks % sourceScale * targetScale / sourceScale };

        if ( whole > static_cast<u64>( std::numeric_limits<i64>::max() ) - remainder ) {
            MKT_THROW_RUNTIME_ERROR( "MP4 edit timing exceeds the supported timeline." );
        }

        return static_cast<i64>( whole + remainder );
    }

    static auto IndexSamples( const MP4D_demux_t& demuxer, u32 trackIndex,
                              const eastl::string& source, u32 nalLengthBytes ) -> eastl::vector<SampleInfo> {
        const auto& track{ demuxer.track[trackIndex] };

        if ( !track.timescale || !track.sample_count || track.timestamp_count != track.sample_count ) {
            MKT_THROW_RUNTIME_ERROR( "MP4 track has no complete sample timing table." );
        }

        eastl::vector<SampleInfo> samples{};
        samples.reserve( track.sample_count );

        const i64 emptyEdit{ RescaleEditTime( track.empty_edit_duration, demuxer.timescale, track.timescale ) };
        i64 decodeTime{};

        for ( u32 index{}; index < track.sample_count; ++index ) {
            u32 size{}, timestamp{}, duration{};
            const auto offset{ MP4D_frame_offset( &demuxer, trackIndex, index, &size, &timestamp, &duration ) };

            if ( !size || !duration || offset > source.size() || size > source.size() - offset ) {
                MKT_THROW_RUNTIME_ERROR( "MP4 sample points outside its source or has invalid duration." );
            }

            // Keep container PTS for reordered pictures; never infer it from FPS.
            const i64 composition{ track.composition_offset ? track.composition_offset[index] : 0 };
            const i64 presentation{ decodeTime + composition - track.edit_media_time + emptyEdit };
            const eastl::span<const ubyte> bytes{ reinterpret_cast<const ubyte*>( source.data() + offset ), size };

            samples.push_back( SampleInfo{
                .mOffset = offset,
                .mSize = size,
                .mDecodeTime = { decodeTime, track.timescale },
                .mPresentationTime = { presentation, track.timescale },
                .mDuration = { duration, track.timescale },
                .mRandomAccess = nalLengthBytes ? ContainsIdr( bytes, nalLengthBytes ) : false } );

            decodeTime += duration;
        }

        return samples;
    }

    VideoClip::VideoClip( const VideoLoadDescription& description ) {
        const auto file{ description.mFile ? description.mFile : filesystem::FileHandle::New( description.mPath ) };
        mPath = file->GetPath();
        mSourceBytes = file->GetContentsString();

        // Freeze compressed bytes against file reloads; no decoded frames live in the asset.
        eastl::unique_ptr<MP4D_demux_t, void ( * )( MP4D_demux_t* )> demuxer{ new MP4D_demux_t{}, DestroyDemuxer };

        if ( mSourceBytes.size() > static_cast<u64>( std::numeric_limits<i64>::max() ) ||
             !MP4D_open( demuxer.get(), ReadSource, &mSourceBytes, static_cast<i64>( mSourceBytes.size() ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot index MP4 video source." );
        }

        bool foundVideo{};

        for ( u32 index{}; index < demuxer->track_count; ++index ) {
            const auto& track{ demuxer->track[index] };

            if ( track.handler_type == MP4D_HANDLER_TYPE_VIDE && !foundVideo && track.object_type_indication == MP4_OBJECT_TYPE_AVC ) {
                auto& video{ mInfo.mVideo };
                video.mTrackIndex = index;
                video.mWidth = track.SampleDescription.video.width;
                video.mHeight = track.SampleDescription.video.height;
                video.mTimeScale = track.timescale;
                video.mNalLengthBytes = track.nal_length_bytes;

                if ( !video.mWidth || !video.mHeight || !video.mNalLengthBytes || video.mNalLengthBytes > 4 ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid MP4 H.264 video configuration." );
                }

                ReadParameterSets( *demuxer, video );
                mVideoSamples = IndexSamples( *demuxer, index, mSourceBytes, video.mNalLengthBytes );
                foundVideo = true;
            }

            if ( track.handler_type == MP4D_HANDLER_TYPE_SOUN && mInfo.mAudio.mCodec == AudioCodec::eNone ) {
                auto& audio{ mInfo.mAudio };
                audio.mCodec = track.object_type_indication == MP4_OBJECT_TYPE_AUDIO_ISO_IEC_14496_3 ? AudioCodec::eAac : AudioCodec::eUnsupported;
                audio.mTrackIndex = index;
                audio.mSampleRate = track.SampleDescription.audio.samplerate_hz;
                audio.mChannels = track.SampleDescription.audio.channelcount;
                audio.mTimeScale = track.timescale;

                if ( track.dsi_bytes ) {
                    audio.mCodecConfiguration.assign( track.dsi, track.dsi + track.dsi_bytes );
                }

                mAudioSamples = IndexSamples( *demuxer, index, mSourceBytes, 0 );
            }
        }

        if ( !foundVideo || !mVideoSamples.front().mRandomAccess ) {
            MKT_THROW_RUNTIME_ERROR( "Expected an MP4 H.264 track starting at an IDR picture." );
        }

        for ( const auto& sample: mVideoSamples ) {
            mInfo.mDurationSeconds = eastl::max( mInfo.mDurationSeconds,
                sample.mPresentationTime.ToSeconds() + sample.mDuration.ToSeconds() );
        }

        // Validate parser configuration during import, before publishing a ready asset.
        H264Parser parser{ mInfo.mVideo };
        EncodedPacket packet{};
        ReadSample( TrackKind::eVideo, 0, packet );
        const auto picture{ parser.ParsePicture( packet ) };
        const auto& sps{ picture->GetSps() };
        mInfo.mVideo.mCodedWidth = static_cast<u32>( sps.pic_width_in_mbs_minus1 + 1 ) * 16;
        mInfo.mVideo.mCodedHeight = static_cast<u32>( sps.pic_height_in_map_units_minus1 + 1 ) * 16;
        mInfo.mVideo.mCropLeft = sps.frame_cropping_flag ? sps.frame_crop_left_offset * 2 : 0;
        mInfo.mVideo.mCropTop = sps.frame_cropping_flag ? sps.frame_crop_top_offset * 2 : 0;
        mInfo.mVideo.mTransferCharacteristics = sps.vui.colour_description_present_flag && sps.vui.transfer_characteristics != 2 ?
            sps.vui.transfer_characteristics : 1;
        mInfo.mVideo.mMatrixCoefficients = sps.vui.colour_description_present_flag && sps.vui.matrix_coefficients != 2 ?
            sps.vui.matrix_coefficients : ( mInfo.mVideo.mHeight > 576 ? 1 : 6 );
        mInfo.mVideo.mFullRange = sps.vui.video_full_range_flag != 0;

        // H.264 4:2:0 chroma locations, expressed as offsets in luma texels.
        const auto location{ sps.vui.chroma_loc_info_present_flag ? sps.vui.chroma_sample_loc_type_top_field : 0 };
        mInfo.mVideo.mChromaOffset = { location == 1 || location == 3 || location == 5 ? 0.0f : 0.5f,
            location < 2 ? 0.0f : ( location < 4 ? 0.5f : -0.5f ) };

        if ( mInfo.mVideo.mWidth + mInfo.mVideo.mCropLeft > mInfo.mVideo.mCodedWidth ||
             mInfo.mVideo.mHeight + mInfo.mVideo.mCropTop > mInfo.mVideo.mCodedHeight ) {
            MKT_THROW_RUNTIME_ERROR( "MP4 display dimensions exceed the decoded picture." );
        }
    }

    auto VideoClip::GetInfo() const -> const ClipInfo& {
        return mInfo;
    }

    auto VideoClip::GetPath() const -> const filesystem::Path& {
        return mPath;
    }

    auto VideoClip::GetSamples( TrackKind track ) const -> eastl::span<const SampleInfo> {
        const auto& samples{ track == TrackKind::eVideo ? mVideoSamples : mAudioSamples };
        return { samples.data(), samples.size() };
    }

    auto VideoClip::ReadSample( TrackKind track, u64 index, EncodedPacket& packet ) const -> bool {
        const auto samples{ GetSamples( track ) };

        if ( index >= samples.size() ) {
            return false;
        }

        const auto& sample{ samples[index] };
        packet.mTrack = track;
        packet.mSampleIndex = index;
        packet.mDecodeTime = sample.mDecodeTime;
        packet.mPresentationTime = sample.mPresentationTime;
        packet.mDuration = sample.mDuration;
        packet.mRandomAccess = sample.mRandomAccess;

        const auto* bytes{ reinterpret_cast<const ubyte*>( mSourceBytes.data() + sample.mOffset ) };
        packet.mBytes.assign( bytes, bytes + sample.mSize );
        return true;
    }

    auto VideoClip::FindRandomAccessSample( f64 seconds ) const -> u64 {
        u64 result{};

        for ( u64 index{}; index < mVideoSamples.size(); ++index ) {
            const auto& sample{ mVideoSamples[index] };

            if ( sample.mRandomAccess && sample.mPresentationTime.ToSeconds() <= seconds ) {
                result = index;
            }
        }

        return result;
    }

    auto VideoClip::CanEvict() const -> bool {
        return GetRefCount() == 1;
    }

    auto VideoClip::GetResidentMemoryUsage() const -> AssetMemoryUsage {
        usize bytes{ sizeof( *this ) + mSourceBytes.capacity() + mVideoSamples.capacity() * sizeof( SampleInfo ) +
                     mAudioSamples.capacity() * sizeof( SampleInfo ) + mInfo.mAudio.mCodecConfiguration.capacity() };

        for ( const auto* sets: { &mInfo.mVideo.mSequenceParameterSets, &mInfo.mVideo.mPictureParameterSets } ) {
            bytes += sets->capacity() * sizeof( eastl::vector<ubyte> );

            for ( const auto& set: *sets ) {
                bytes += set.capacity();
            }
        }

        return { bytes, 0 };
    }
}
