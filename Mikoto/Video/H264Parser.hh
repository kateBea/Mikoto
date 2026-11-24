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

#ifndef MIKOTO_VIDEO_H264_PARSER_HH
#define MIKOTO_VIDEO_H264_PARSER_HH

#include <EASTL/span.h>
#include <EASTL/array.h>

#include <h264.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Resource.hh>

#include <Video/Types.hh>

namespace mikoto::video {

    // Own every parameter referenced by a recorded decode, including multi-slice pictures.
    class H264Picture final : public core::ReferenceCounted<H264Picture> {
    public:
        MKT_NODISCARD auto GetSps() const -> const h264::SPS&;
        MKT_NODISCARD auto GetPps() const -> const h264::PPS&;

        MKT_NODISCARD auto GetSlices() const -> eastl::span<const h264::SliceHeader>;
        MKT_NODISCARD auto GetSliceOffsets() const -> eastl::span<const core::u32>;

        MKT_NODISCARD auto GetBitstream() const -> eastl::span<const core::ubyte>;
        MKT_NODISCARD auto GetSampleIndex() const -> core::u64;

        MKT_NODISCARD auto GetPresentationTime() const -> Timestamp;

        MKT_NODISCARD auto GetDuration() const -> Timestamp;

        MKT_NODISCARD auto IsReference() const -> bool;
        MKT_NODISCARD auto IsIdr() const -> bool;

    private:
        friend class H264Parser;

    private:
        h264::SPS mSps{};
        h264::PPS mPps{};
        eastl::vector<h264::SliceHeader> mSlices{};
        eastl::vector<core::u32> mSliceOffsets{};
        eastl::vector<core::ubyte> mBitstream{};
        core::u64 mSampleIndex{};
        Timestamp mPresentationTime{};
        Timestamp mDuration{};
        bool mReference{};
        bool mIdr{};
    };

    class H264Parser final : public core::INonCopyable {
    public:
        explicit H264Parser( const VideoTrackInfo& track );

        auto Reset( const VideoTrackInfo& track ) -> void;

        MKT_NODISCARD auto ParsePicture( const EncodedPacket& packet ) -> core::Ref<H264Picture>;

    private:
        auto ReadParameterSet( eastl::span<const core::ubyte> bytes ) -> void;

    private:
        eastl::vector<h264::SPS> mSequenceParameterSets{};
        eastl::vector<h264::PPS> mPictureParameterSets{};
        eastl::array<bool, h264::MAX_SPS_ID + 1> mHasSps{};
        eastl::array<bool, h264::MAX_PPS_ID + 1> mHasPps{};
        core::u32 mNalLengthBytes{};
    };
}

#endif