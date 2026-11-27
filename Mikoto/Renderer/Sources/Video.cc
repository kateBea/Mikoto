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

#include <EASTL/algorithm.h>

#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Video/Video.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::video;
    using namespace mikoto::renderer::rhi;

    static auto PictureNumber( i32 frame, i32 current, i32 maximum ) -> i32 {
        return frame > current ? frame - maximum : frame;
    }

    auto VideoRenderData::PreparePicture( Ref<H264Picture> picture, Ref<DecodedVideoFrame> frame ) -> void {
        // Match Wicked's decode-order DPB bookkeeping; presentation still follows MP4 PTS.
        // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiVideo.cpp
        const auto &sps{ picture->GetSps() };
        const auto &slice{ picture->GetSlices().front() };
        const i32 maximumFrameNumber{ 1 << ( sps.log2_max_frame_num_minus4 + 4 ) };

        if ( picture->IsIdr() ) {
            for ( auto &slot: mDpb ) {
                slot.mUsed = false;
            }

            mPreviousPocMsb = mPreviousPocLsb = mPreviousFrameNumber = mFrameNumberOffset = 0;
        }

        i32 poc{};
        i32 pocMsb{};

        if ( sps.pic_order_cnt_type == 2 ) {
            if ( !picture->IsIdr() && mPreviousFrameNumber > slice.frame_num ) {
                mFrameNumberOffset += maximumFrameNumber;
            }

            poc = picture->IsIdr() ? 0 : 2 * ( mFrameNumberOffset + slice.frame_num ) - ( picture->IsReference() ? 0 : 1 );
        }
        else if ( sps.pic_order_cnt_type == 0 && !slice.delta_pic_order_cnt_bottom ) {
            const i32 maximumPoc{ 1 << ( sps.log2_max_pic_order_cnt_lsb_minus4 + 4 ) };
            pocMsb = mPreviousPocMsb;

            if ( slice.pic_order_cnt_lsb < mPreviousPocLsb && mPreviousPocLsb - slice.pic_order_cnt_lsb >= maximumPoc / 2 ) {
                pocMsb += maximumPoc;
            }
            else if ( slice.pic_order_cnt_lsb > mPreviousPocLsb && slice.pic_order_cnt_lsb - mPreviousPocLsb > maximumPoc / 2 ) {
                pocMsb -= maximumPoc;
            }

            poc = pocMsb + slice.pic_order_cnt_lsb;
        }
        else {
            MKT_THROW_RUNTIME_ERROR( "This H.264 picture-order mode is not supported yet." );
        }

        auto free{ mDecoder->UsesCoincidentOutput() ? mDpb.begin() + frame->mSlot : eastl::find_if( mDpb.begin(), mDpb.end(), []( const auto &slot ) {
            return !slot.mUsed;
        } ) };

        if ( free == mDpb.end() ) {
            MKT_THROW_RUNTIME_ERROR( "H.264 decoded-picture buffer has no free reconstruction slot." );
        }

        const auto slotIndex{ as<u32>( free - mDpb.begin() ) };

        VideoDecodeDescription operation{};
        operation.mDecoder = mDecoder;
        operation.mPicture = picture;
        operation.mOutput = frame->mSurface;
        operation.mReconstruction = free->mPicture;
        operation.mReconstruction.mSlot = slotIndex;
        operation.mReconstruction.mFrameNumber = slice.frame_num;
        operation.mReconstruction.mPictureOrderCount = poc;
        operation.mResetSession = mResetDecoder;

        for ( const auto &slot: mDpb ) {
            if ( slot.mUsed ) {
                operation.mReferences.push_back( slot.mPicture );
            }
        }

        // Apply marking after taking the reference snapshot needed by this picture.
        bool currentLongTerm{ picture->IsIdr() && slice.drpm.long_term_reference_flag };

        bool resetOrder{ false };
        i32 currentLongTermIndex{ 0 };

        if ( picture->IsReference() && !picture->IsIdr() && slice.drpm.adaptive_ref_pic_marking_mode_flag ) {
            for ( u32 index{}; index < h264::MAX_MEMORY_MANAGEMENT_CONTROL_OPS; ++index ) {
                const auto op{ slice.drpm.memory_management_control_operation[index] };

                if ( !op ) {
                    break;
                }

                const i32 shortNumber{ slice.frame_num - slice.drpm.difference_of_pic_nums_minus1[index] - 1 };
                const i32 longIndex{ slice.drpm.long_term_frame_idx[index] };

                for ( auto &slot: mDpb ) {
                    if ( !slot.mUsed ) {
                        continue;
                    }

                    auto &reference{ slot.mPicture };
                    const bool matchingShort{ !reference.mLongTerm &&
                                              PictureNumber( reference.mFrameNumber, slice.frame_num, maximumFrameNumber ) == shortNumber };
                    const bool replacedLong{ reference.mLongTerm && static_cast<i32>( reference.mFrameNumber ) == longIndex };

                    if ( ( op == 1 && matchingShort ) ||
                         ( op == 2 && reference.mLongTerm && static_cast<i32>( reference.mFrameNumber ) == slice.drpm.long_term_pic_num[index] ) ||
                         ( ( op == 3 || op == 6 ) && replacedLong ) ||
                         ( op == 4 && reference.mLongTerm && reference.mFrameNumber >= static_cast<u32>( slice.drpm.max_long_term_frame_idx_plus1[index] ) ) || op == 5 ) {
                        slot.mUsed = false;
                    }

                    if ( op == 3 && matchingShort ) {
                        reference.mLongTerm = true;
                        reference.mFrameNumber = longIndex;
                        slot.mUsed = true;
                    }
                }

                if ( op == 5 ) {
                    resetOrder = true;
                }
                else if ( op == 6 ) {
                    currentLongTerm = true;
                    currentLongTermIndex = longIndex;
                }
                else if ( op < 1 || op > 6 ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid H.264 reference marking operation." );
                }
            }
        }
        else if ( picture->IsReference() && !picture->IsIdr() ) {
            const auto used{ eastl::count_if( mDpb.begin(), mDpb.end(), []( const auto &slot ) {
                return slot.mUsed;
            } ) };

            if ( used >= eastl::max( sps.num_ref_frames, 1 ) ) {
                auto oldest{ mDpb.end() };

                for ( auto slot{ mDpb.begin() }; slot != mDpb.end(); ++slot ) {
                    if ( slot->mUsed && !slot->mPicture.mLongTerm &&
                         ( oldest == mDpb.end() || PictureNumber( slot->mPicture.mFrameNumber, slice.frame_num, maximumFrameNumber ) <
                                                       PictureNumber( oldest->mPicture.mFrameNumber, slice.frame_num, maximumFrameNumber ) ) ) {
                        oldest = slot;
                    }
                }

                if ( oldest == mDpb.end() ) {
                    MKT_THROW_RUNTIME_ERROR( "H.264 sliding window has no short-term reference to retire." );
                }

                oldest->mUsed = false;
            }
        }

        free->mPicture = operation.mReconstruction;
        free->mPicture.mLongTerm = currentLongTerm;
        free->mPicture.mFrameNumber = currentLongTerm ? currentLongTermIndex : ( resetOrder ? 0 : slice.frame_num );
        free->mPicture.mPictureOrderCount = resetOrder ? 0 : poc;
        free->mUsed = picture->IsReference();

        if ( picture->IsReference() ) {
            mPreviousPocMsb = resetOrder ? 0 : pocMsb;
            mPreviousPocLsb = resetOrder ? 0 : slice.pic_order_cnt_lsb;
            mPreviousFrameNumber = resetOrder ? 0 : slice.frame_num;
            mFrameNumberOffset = resetOrder ? 0 : mFrameNumberOffset;
        }

        frame->mPresentationTime = picture->GetPresentationTime();
        frame->mGeneration = ++mGeneration;
        frame->mRecorded = false;
        mDecodeWork.push_back( { eastl::move( operation ), frame } );
        mDisplayQueue.push_back( frame );
        mResetDecoder = false;
    }

    auto VideoBitstream::Create( const Ref<asset::VideoClip> &clip, IVideoDecoder *decoder ) -> Ref<VideoBitstream> {
        // Pack every access unit once, retaining per-picture offsets for seeks and reordering.
        // https://github.com/turanszkij/mini_video/blob/master/mini_video_vulkan.cpp
        auto stream{ Ref<VideoBitstream>::New() };
        stream->mClip = clip;
        const auto layout{ decoder->GetBitstreamLayout() };
        const usize removedStartCodeBytes{ 4 - layout.mStartCodeSize };
        const auto samples{ clip->GetSamples( TrackKind::eVideo ) };
        stream->mPictures.reserve( samples.size() );
        H264Parser parser{ clip->GetInfo().mVideo };
        EncodedPacket packet{};
        eastl::vector<ubyte> bytes{};

        for ( u64 sample{}; sample < samples.size(); ++sample ) {
            if ( !clip->ReadSample( TrackKind::eVideo, sample, packet ) ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot read the complete video bitstream." );
            }

            const auto picture{ parser.ParsePicture( packet ) };
            const auto source{ picture->GetBitstream() };
            const auto slices{ picture->GetSliceOffsets() };
            const usize pictureSize{ source.size() - slices.size() * removedStartCodeBytes };
            const usize maximumSize{ std::numeric_limits<usize>::max() };

            if ( bytes.size() > maximumSize - layout.mOffsetAlignment ||
                 pictureSize > maximumSize - layout.mSizeAlignment ) {
                MKT_THROW_RUNTIME_ERROR( "Video bitstream exceeds the addressable upload size." );
            }

            const usize offset{ core::align( bytes.size(), layout.mOffsetAlignment ) };
            const usize size{ core::align( pictureSize, layout.mSizeAlignment ) };

            if ( size > maximumSize - offset || size > std::numeric_limits<u32>::max() ) {
                MKT_THROW_RUNTIME_ERROR( "Video picture exceeds the supported decode range." );
            }

            // Zero the alignment gaps and final-slice padding before publishing the buffer.
            bytes.resize( offset + size, 0 );

            for ( usize index{}; index < slices.size(); ++index ) {
                const usize start{ slices[index] };
                const usize end{ index + 1 < slices.size() ? slices[index + 1] : source.size() };
                std::memcpy( bytes.data() + offset + start - index * removedStartCodeBytes,
                    source.data() + start + removedStartCodeBytes, end - start - removedStartCodeBytes );
            }

            stream->mPictures.emplace_back( offset, size );
        }

        stream->mBuffer = decoder->CreateBitstream( { bytes.data(), bytes.size() } );
        return stream;
    }

    VideoRenderData::VideoRenderData( VideoPlaybackHandle playback )
        : mPlayback{ eastl::move( playback ) } {
    }

    auto VideoRenderData::ResetDecoder() -> void {
        mDisplayQueue.clear();
        mDecodeWork.clear();
        mCurrentFrame.Reset();
        mFramePool.clear();
        mDpb.clear();
        mDecoder.Reset();
        mPreviousPocMsb = 0;
        mPreviousPocLsb = 0;
        mPreviousFrameNumber = 0;
        mFrameNumberOffset = 0;
        mResetDecoder = true;
        mEpoch = mPlayback->GetEpoch();
        ++mRevision;
    }

    auto VideoRenderData::Prepare() -> void {
        if ( !mPlayback->IsRegistered() || mPlayback->GetState() == PlaybackState::eFailed || !mPlayback->GetClip() ) {
            return;
        }

        if ( mEpoch != mPlayback->GetEpoch() ) {
            ResetDecoder();
        }

        if ( mPlayback->IsPosterFrameOnly() && mCurrentFrame ) {
            return;
        }

        try {
            if ( !mDecoder && mPlayback->GetPicture() ) {
                const VideoDecoderDescription description{ .mConfiguration = mPlayback->GetPicture() };
                mDecoder = RenderSystem::Get()->GetGpuDevice()->CreateVideoDecoder( description );

                if ( !mDecoder ) {
                    MKT_THROW_RUNTIME_ERROR( "Cannot create an H.264 hardware decoder for this video." );
                }

                mDpb.resize( mDecoder->GetDpbSlotCount() );

                for ( u32 slot{}; slot < mDpb.size(); ++slot ) {
                    mDpb[slot].mPicture.mSurface = mDecoder->CreateSurface( true );
                    mDpb[slot].mPicture.mSlot = slot;

                    if ( mDecoder->UsesCoincidentOutput() ) {
                        auto frame{ Ref<DecodedVideoFrame>::New() };
                        frame->mSurface = mDpb[slot].mPicture.mSurface;
                        frame->mSlot = slot;
                        mFramePool.push_back( eastl::move( frame ) );
                    }
                }

                if ( !mOutput ) {
                    const auto &info{ mPlayback->GetClip()->GetInfo().mVideo };
                    mOutput = RenderSystem::Get()->GetGpuDevice()->CreateTexture( TextureCreateDescription{}
                        .SetName( "Video material output" )
                        .SetWidth( info.mWidth )
                        .SetHeight( info.mHeight )
                        .SetFormat( Format::eRGBA16_FLOAT )
                        .SetUsage( TextureUsageFlagsBits::ShaderResource | TextureUsageFlagsBits::UnorderedAccess ) );
                }
            }

            while ( mDecoder && mPlayback->GetPicture() && mDecodeWork.size() < 4 ) {
                auto free{ eastl::find_if( mFramePool.begin(), mFramePool.end(), [&]( const auto &frame ) {
                    // A coincident picture is reusable only after both reference and display users release it.
                    return frame->GetRefCount() == 1 &&
                           ( !mDecoder->UsesCoincidentOutput() || !mDpb[frame->mSlot].mUsed );
                } ) };

                if ( free == mFramePool.end() ) {
                    if ( mDecoder->UsesCoincidentOutput() || mFramePool.size() >= 8 ) {
                        break;
                    }

                    auto frame{ Ref<DecodedVideoFrame>::New() };
                    frame->mSurface = mDecoder->CreateSurface( false );
                    mFramePool.push_back( eastl::move( frame ) );
                    free = mFramePool.end() - 1;
                }

                PreparePicture( mPlayback->GetPicture(), *free );
                mPlayback->ConsumePicture();
            }

            // Select by presentation timestamp after
            // preparing every required decode reference.
            Ref<DecodedVideoFrame> selected{};
            const auto samples{ mPlayback->GetClip()->GetSamples( TrackKind::eVideo ) };
            const auto firstFrameTime{ samples[mPlayback->GetPreviewSample()].mPresentationTime.ToSeconds() };
            const bool showFirstFrame{ mPlayback->IsPosterFrameOnly() || mPlayback->GetTimeSeconds() < firstFrameTime };

            for ( auto entry{ mDisplayQueue.begin() }; entry != mDisplayQueue.end(); ) {
                if ( showFirstFrame ) {
                    // A paused scrub before a positive first PTS still shows
                    // that first picture; retain later pictures for resume.
                    if ( ( *entry )->mPresentationTime.ToSeconds() == firstFrameTime ) {
                        selected = *entry;
                        entry = mDisplayQueue.erase( entry );
                    }
                    else if ( mPlayback->IsPosterFrameOnly() ) {
                        entry = mDisplayQueue.erase( entry );
                    }
                    else {
                        ++entry;
                    }
                    continue;
                }

                if ( ( *entry )->mPresentationTime.ToSeconds() <= mPlayback->GetTimeSeconds() ) {
                    if ( !selected || ( *entry )->mPresentationTime.ToSeconds() >= selected->mPresentationTime.ToSeconds() ) {
                        selected = *entry;
                    }

                    entry = mDisplayQueue.erase( entry );
                }
                else {
                    ++entry;
                }
            }

            if ( selected && ( !mCurrentFrame || selected->mPresentationTime.ToSeconds() >= mCurrentFrame->mPresentationTime.ToSeconds() ) &&
                 selected != mCurrentFrame ) {
                mCurrentFrame = eastl::move( selected );
                ++mRevision;
            }
        } catch ( const std::exception &error ) {
            mPlayback->SetFailure( error.what() );
            MKT_CORE_LOGGER_ERROR( "Prepare video: {}", error.what() );
        }
    }

    auto VideoRenderData::UploadBitstream() -> void {
        if ( mPlayback->GetState() == PlaybackState::eFailed || !mDecoder || mBitstream ) {
            return;
        }

        try {
            mBitstream = RenderSystem::Get()->AcquireVideoBitstream( mPlayback->GetClip(), mDecoder.GetPtr() );
        } catch ( const std::exception &error ) {
            mPlayback->SetFailure( error.what() );
            mDecodeWork.clear();
            MKT_CORE_LOGGER_ERROR( "Upload video: {}", error.what() );
        }
    }

    auto VideoRenderData::UpdateMaterial( material::SurfaceMaterialHandle source, VideoOutputMap output ) -> void {
        if ( !source || !mCurrentFrame || !mOutput || !HasPresented( mCurrentFrame->mGeneration ) ) {
            return;
        }

        // Rendering owns the dynamic clone so authored material assets keep their source bindings.
        if ( !mPlaybackMaterial || source != mOriginalMaterial || source->GetRevision() != mMaterialRevision || output != mOutputMap ) {
            mOriginalMaterial = source;
            mMaterialRevision = source->GetRevision();
            mOutputMap = output;
            mPlaybackMaterial = material::SurfaceMaterialHandle::New( *source );
            ++mRevision;
        }

        const auto map{ output == VideoOutputMap::eEmissive ? material::MapType::eEmissive : material::MapType::eBaseColor };

        if ( mPlaybackMaterial->GetTexture( map ) != mOutput ) {
            mPlaybackMaterial->SetTexture( map, mOutput );
            mPlaybackMaterial->UseTexture( map );
            ++mRevision;
        }
    }

    auto VideoRenderData::MarkPresented( u64 generation ) -> void {
        mPresentedGeneration = generation;
    }

    auto VideoRenderData::HasPresented( u64 generation ) const -> bool {
        return mPresentedGeneration == generation;
    }

    auto VideoRenderData::GetPlayback() const -> const VideoPlaybackHandle & {
        return mPlayback;
    }

    auto VideoRenderData::GetCurrentFrame() const -> Ref<DecodedVideoFrame> {
        return mCurrentFrame;
    }

    auto VideoRenderData::GetOutputTexture() const -> TextureHandle {
        return mOutput;
    }

    auto VideoRenderData::GetMaterial() const -> material::MaterialHandle {
        return mPlaybackMaterial;
    }

    auto VideoRenderData::GetRevision() const -> u64 {
        return mRevision;
    }
}// namespace mikoto::renderer
