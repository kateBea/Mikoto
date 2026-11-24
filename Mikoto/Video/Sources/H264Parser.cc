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

#include <limits>

#include <Core/Exception.hh>

#include <Video/H264Parser.hh>

namespace mikoto::video {

    using namespace mikoto::core;

    // Adapt Wicked's header parser; keep Annex B bytes separately for hardware decode.
    // https://github.com/turanszkij/WickedEngine/blob/cee8135f9a00e6f74e6a18326653ef4cc9c99ba3/WickedEngine/Utility/h264.h
    static auto MakeRbsp( eastl::span<const ubyte> nal ) -> eastl::vector<ubyte> {
        eastl::vector<ubyte> bytes{};
        bytes.reserve( nal.size() );

        u32 zeroCount{};

        for ( usize index{}; index < nal.size(); ++index ) {
            const auto value{ nal[index] };

            if ( zeroCount == 2 && value == 3 ) {
                if ( index + 1 == nal.size() || nal[index + 1] > 3 ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid H.264 emulation-prevention byte." );
                }

                zeroCount = 0;
                continue;
            }

            bytes.push_back( value );
            zeroCount = value == 0 ? zeroCount + 1 : 0;
        }

        return bytes;
    }

    static auto ReadHeader( h264::Bitstream& stream ) -> h264::NALHeader {
        h264::NALHeader header{};

        if ( !h264::read_nal_header( &header, &stream ) ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid H.264 NAL header." );
        }

        return header;
    }

    static auto ValidateSps( const h264::SPS& sps ) -> void {
        if ( sps.seq_parameter_set_id < 0 || sps.seq_parameter_set_id > h264::MAX_SPS_ID ||
             sps.chroma_format_idc != 1 || sps.bit_depth_luma_minus8 || sps.bit_depth_chroma_minus8 ||
             !sps.frame_mbs_only_flag || sps.separate_colour_plane_flag ) {
            MKT_THROW_RUNTIME_ERROR( "Video currently accepts progressive 8-bit H.264 with 4:2:0 chroma." );
        }

        if ( sps.log2_max_frame_num_minus4 < 0 || sps.log2_max_frame_num_minus4 > h264::MAX_LOG2_MINUS4 ||
             sps.log2_max_pic_order_cnt_lsb_minus4 < 0 || sps.log2_max_pic_order_cnt_lsb_minus4 > h264::MAX_LOG2_MINUS4 ||
             sps.pic_order_cnt_type < 0 || sps.pic_order_cnt_type > h264::MAX_PIC_ORDER_CNT_TYPE ||
             sps.num_ref_frames < 0 || sps.num_ref_frames > h264::MAX_NUM_REF_FRAMES ||
             sps.pic_width_in_mbs_minus1 < 0 || sps.pic_height_in_map_units_minus1 < 0 ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid H.264 sequence parameters." );
        }
    }

    static auto ValidatePps( const h264::PPS& pps ) -> void {
        if ( pps.pic_parameter_set_id < 0 || pps.pic_parameter_set_id > h264::MAX_PPS_ID ||
             pps.seq_parameter_set_id < 0 || pps.seq_parameter_set_id > h264::MAX_SPS_ID ||
             pps.num_slice_groups_minus1 || pps.num_ref_idx_l0_active_minus1 < 0 ||
             pps.num_ref_idx_l0_active_minus1 > h264::MAX_NUM_REF_IDX_ACTIVE_MINUS1 ||
             pps.num_ref_idx_l1_active_minus1 < 0 || pps.num_ref_idx_l1_active_minus1 > h264::MAX_NUM_REF_IDX_ACTIVE_MINUS1 ) {
            MKT_THROW_RUNTIME_ERROR( "Unsupported or invalid H.264 picture parameters." );
        }
    }

    H264Parser::H264Parser( const VideoTrackInfo& track ) {
        Reset( track );
    }

    auto H264Parser::Reset( const VideoTrackInfo& track ) -> void {
        if ( !track.mNalLengthBytes || track.mNalLengthBytes > 4 ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid H.264 NAL length field size." );
        }

        mNalLengthBytes = track.mNalLengthBytes;
        mSequenceParameterSets.assign( h264::MAX_SPS_ID + 1, h264::SPS{} );
        mPictureParameterSets.assign( h264::MAX_PPS_ID + 1, h264::PPS{} );
        mHasSps.fill( false );
        mHasPps.fill( false );

        for ( const auto* sets: { &track.mSequenceParameterSets, &track.mPictureParameterSets } ) {
            for ( const auto& bytes: *sets ) {
                ReadParameterSet( { bytes.data(), bytes.size() } );
            }
        }
    }

    auto H264Parser::ReadParameterSet( eastl::span<const ubyte> bytes ) -> void {
        if ( bytes.empty() ) {
            MKT_THROW_RUNTIME_ERROR( "Empty H.264 parameter set." );
        }

        const auto rbsp{ MakeRbsp( bytes ) };
        h264::Bitstream stream{};
        stream.init( rbsp.data(), rbsp.size() );

        const auto header{ ReadHeader( stream ) };

        if ( header.type == h264::NAL_UNIT_TYPE_SPS ) {
            h264::SPS sps{};
            h264::read_sps( &sps, &stream );
            ValidateSps( sps );

            mSequenceParameterSets[sps.seq_parameter_set_id] = sps;
            mHasSps[sps.seq_parameter_set_id] = true;
        } else if ( header.type == h264::NAL_UNIT_TYPE_PPS ) {
            h264::PPS pps{};
            h264::read_pps( &pps, &stream );
            ValidatePps( pps );

            if ( !mHasSps[pps.seq_parameter_set_id] ) {
                MKT_THROW_RUNTIME_ERROR( "H.264 PPS references an absent SPS." );
            }

            mPictureParameterSets[pps.pic_parameter_set_id] = pps;
            mHasPps[pps.pic_parameter_set_id] = true;
        } else {
            MKT_THROW_RUNTIME_ERROR( "Expected an H.264 SPS or PPS NAL unit." );
        }
    }

    auto H264Parser::ParsePicture( const EncodedPacket& packet ) -> Ref<H264Picture> {
        if ( packet.mTrack != TrackKind::eVideo ) {
            MKT_THROW_INVALID_ARGUMENT( "H.264 parser requires a video packet." );
        }

        auto picture{ Ref<H264Picture>::New() };
        picture->mSampleIndex = packet.mSampleIndex;
        picture->mPresentationTime = packet.mPresentationTime;
        picture->mDuration = packet.mDuration;

        usize offset{};

        while ( offset < packet.mBytes.size() ) {
            if ( packet.mBytes.size() - offset < mNalLengthBytes ) {
                MKT_THROW_RUNTIME_ERROR( "Truncated MP4 H.264 NAL length." );
            }

            u32 length{};

            for ( u32 index{}; index < mNalLengthBytes; ++index ) {
                length = ( length << 8 ) | packet.mBytes[offset++];
            }

            if ( !length || length > packet.mBytes.size() - offset ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid MP4 H.264 NAL extent." );
            }

            const eastl::span<const ubyte> nal{ packet.mBytes.data() + offset, length };
            offset += length;

            const auto rbsp{ MakeRbsp( nal ) };
            h264::Bitstream stream{};
            stream.init( rbsp.data(), rbsp.size() );

            auto header{ ReadHeader( stream ) };

            if ( header.type == h264::NAL_UNIT_TYPE_SPS || header.type == h264::NAL_UNIT_TYPE_PPS ) {
                ReadParameterSet( nal );
                continue;
            }

            if ( header.type != h264::NAL_UNIT_TYPE_CODED_SLICE_IDR && header.type != h264::NAL_UNIT_TYPE_CODED_SLICE_NON_IDR ) {
                continue;
            }

            // Resolve IDs before handing arrays to the upstream slice reader.
            auto prefix{ stream };
            prefix.ue();
            const auto sliceType{ prefix.ue() };
            const auto ppsId{ prefix.ue() };

            if ( sliceType > h264::MAX_SLICE_TYPE || ppsId > h264::MAX_PPS_ID || !mHasPps[ppsId] ) {
                MKT_THROW_RUNTIME_ERROR( "H.264 slice references an invalid picture parameter set." );
            }

            const auto& pps{ mPictureParameterSets[ppsId] };
            const auto& sps{ mSequenceParameterSets[pps.seq_parameter_set_id] };
            h264::SliceHeader slice{};
            h264::read_slice_header( &slice, &header, mPictureParameterSets.data(), mSequenceParameterSets.data(), &stream );

            const bool idr{ header.type == h264::NAL_UNIT_TYPE_CODED_SLICE_IDR };
            const bool reference{ header.idc != 0 };

            if ( !picture->mSlices.empty() &&
                 ( slice.frame_num != picture->mSlices.front().frame_num || ppsId != static_cast<u32>( picture->mPps.pic_parameter_set_id ) ||
                   idr != picture->mIdr || reference != picture->mReference ||
                   slice.idr_pic_id != picture->mSlices.front().idr_pic_id ||
                   slice.pic_order_cnt_lsb != picture->mSlices.front().pic_order_cnt_lsb ) ) {
                MKT_THROW_RUNTIME_ERROR( "MP4 sample contains more than one H.264 picture." );
            }

            if ( picture->mBitstream.size() > std::numeric_limits<u32>::max() - 4ull - length ) {
                MKT_THROW_RUNTIME_ERROR( "H.264 picture exceeds the supported bitstream size." );
            }

            // Vulkan's slice offsets point to start codes in the normalized bitstream.
            picture->mSliceOffsets.push_back( static_cast<u32>( picture->mBitstream.size() ) );
            picture->mBitstream.insert( picture->mBitstream.end(), { 0, 0, 0, 1 } );
            picture->mBitstream.insert( picture->mBitstream.end(), nal.begin(), nal.end() );
            picture->mSlices.push_back( slice );

            picture->mSps = sps;
            picture->mPps = pps;
            picture->mIdr = idr;
            picture->mReference = reference;
        }

        if ( picture->mSlices.empty() ) {
            MKT_THROW_RUNTIME_ERROR( "MP4 sample contains no supported H.264 picture." );
        }

        return picture;
    }

    auto H264Picture::GetSampleIndex() const -> u64 {
        return mSampleIndex;
    }

    auto H264Picture::GetSps() const -> const h264::SPS& {
        return mSps;
    }

    auto H264Picture::GetPps() const -> const h264::PPS& {
        return mPps;
    }

    auto H264Picture::GetSlices() const -> eastl::span<const h264::SliceHeader> {
        return { mSlices.data(), mSlices.size() };
    }

    auto H264Picture::GetSliceOffsets() const -> eastl::span<const u32> {
        return { mSliceOffsets.data(), mSliceOffsets.size() };
    }

    auto H264Picture::GetBitstream() const -> eastl::span<const ubyte> {
        return { mBitstream.data(), mBitstream.size() };
    }

    auto H264Picture::GetPresentationTime() const -> Timestamp {
        return mPresentationTime;
    }

    auto H264Picture::GetDuration() const -> Timestamp {
        return mDuration;
    }

    auto H264Picture::IsReference() const -> bool {
        return mReference;
    }

    auto H264Picture::IsIdr() const -> bool {
        return mIdr;
    }

}
