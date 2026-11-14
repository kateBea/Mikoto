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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Exception.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <Renderer/Rhi/D3D12/D3D12Device.hh>
#include <Renderer/Rhi/D3D12/D3D12VideoDecoder.hh>

namespace mikoto::renderer::d3d12 {
    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    static auto GetVideoColorSpace( const h264::SPS& sps ) -> DXGI_COLOR_SPACE_TYPE {
        const bool fullRange{ sps.vui.video_full_range_flag != 0 };
        const bool highDefinition{ ( sps.pic_height_in_map_units_minus1 + 1 ) * 16 > 576 };
        const auto matrix{ sps.vui.colour_description_present_flag && sps.vui.matrix_coefficients != 2
            ? sps.vui.matrix_coefficients : ( highDefinition ? 1 : 6 ) };

        if ( matrix == 5 || matrix == 6 ) {
            return fullRange ? DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P601 : DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P601;
        }

        if ( matrix == 9 || matrix == 10 ) {
            return fullRange ? DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P2020 : DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P2020;
        }

        return fullRange ? DXGI_COLOR_SPACE_YCBCR_FULL_G22_LEFT_P709 : DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709;
    }

    VideoImageStorage::VideoImageStorage( u32 width, u32 height, bool referenceOnly )
        : mWidth{ width }, mHeight{ height }, mReferenceOnly{ referenceOnly } {
    }

    auto VideoImageStorage::Initialize() -> void {
        auto* device{ core::checked_cast<Device*>( mDevice ) };
        mAllocation.mDesc = CD3DX12_RESOURCE_DESC::Tex2D( DXGI_FORMAT_NV12, mWidth, mHeight, 1, 1 );
        mAllocation.mAllocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

        if ( mReferenceOnly ) {
            mAllocation.mDesc.Flags = D3D12_RESOURCE_FLAG_VIDEO_DECODE_REFERENCE_ONLY;
        }

        ThrowIfFailed( device->GetAllocator()->AllocateImage( mAllocation ) );
        mIsAllocated = true;
    }

    auto VideoImageStorage::GetResource() const -> ID3D12Resource* {
        return mAllocation.mResource.Get();
    }

    auto VideoImageStorage::Destroy() -> void {
        core::checked_cast<Device*>( mDevice )->GetAllocator()->FreeImage( mAllocation );
        mIsAllocated = false;
    }

    VideoImageStorage::~VideoImageStorage() {
        if ( mIsAllocated ) {
            Destroy();
        }
    }

    VideoSurface::VideoSurface( u32 width, u32 height, bool referenceOnly, bool sampled )
        : mWidth{ width }, mHeight{ height }, mReferenceOnly{ referenceOnly }, mSampled{ sampled } {
    }

    auto VideoSurface::Initialize() -> void {
        auto* device{ core::checked_cast<Device*>( mDevice ) };
        mStorage = Ref<VideoImageStorage>::New( mWidth, mHeight, mReferenceOnly );
        static_cast<DeviceObject*>( mStorage.GetPtr() )->Initialize( mDevice );

        // NV12 uses two SRVs over one allocation, matching the existing presentation shader.
        // https://learn.microsoft.com/en-us/windows/win32/api/dxgiformat/ne-dxgiformat-dxgi_format (NV12)
        ExternalTextureDescription description{};
        description.mWidth = mWidth;
        description.mHeight = mHeight;
        description.mFormat = Format::eNV12;
        description.mImageResource = mStorage->GetResource();
        description.mStorageOwner = mStorage;
        description.mTextureUsage = TextureUsageFlagsBits::None;
        mImage = Ref<Texture>::New( description, *device->GetHeapResources() );
        mImage->Initialize( mDevice );
        mImage->SetResourceState( ResourceStates::eCommon );

        if ( mSampled ) {
            description.mFormat = Format::eR8_UNORM;
            description.mTextureUsage = TextureUsageFlagsBits::ShaderResource;
            mLumaImage = Ref<Texture>::New( description, *device->GetHeapResources() );
            mLumaImage->Initialize( mDevice );
            description.mWidth /= 2;
            description.mHeight /= 2;
            description.mFormat = Format::eRG8_UNORM;
            description.mPlaneSlice = 1;
            mChromaImage = Ref<Texture>::New( description, *device->GetHeapResources() );
            mChromaImage->Initialize( mDevice );
        }

        mIsAllocated = true;
    }

    auto VideoSurface::GetImage() const -> TextureHandle {
        return mImage;
    }

    auto VideoSurface::GetLumaImage() const -> TextureHandle {
        return mLumaImage;
    }

    auto VideoSurface::GetChromaImage() const -> TextureHandle {
        return mChromaImage;
    }

    auto VideoSurface::Destroy() -> void {
        mChromaImage.Reset();
        mLumaImage.Reset();
        mImage.Reset();
        mStorage.Reset();
        mIsAllocated = false;
    }

    VideoSurface::~VideoSurface() {
        Destroy();
    }

    VideoDecoder::VideoDecoder( const VideoDecoderDescription& description )
        : mConfiguration{ description.mConfiguration } {
    }

    auto VideoDecoder::Initialize() -> void {
        auto* device{ core::checked_cast<Device*>( mDevice ) };
        auto* videoDevice{ device->GetVideoDevice() };
        const auto& sps{ mConfiguration->GetSps() };

        if ( !videoDevice || !sps.frame_mbs_only_flag || sps.chroma_format_idc != 1 ||
            sps.bit_depth_luma_minus8 || sps.bit_depth_chroma_minus8 || mConfiguration->GetPps().num_slice_groups_minus1 ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 video requires progressive 8-bit H.264 4:2:0 without slice groups." );
        }

        // Probe this stream before allocating decoder state; profile support alone is insufficient.
        // https://learn.microsoft.com/en-us/windows/win32/medfound/direct3d-12-video-overview
        // https://github.com/turanszkij/WickedEngine/blob/master/WickedEngine/wiGraphicsDevice_DX12.cpp (CreateVideoDecoder)
        D3D12_VIDEO_DECODER_DESC decoder{};
        decoder.Configuration.DecodeProfile = D3D12_VIDEO_DECODE_PROFILE_H264;
        decoder.Configuration.InterlaceType = D3D12_VIDEO_FRAME_CODED_INTERLACE_TYPE_NONE;
        D3D12_FEATURE_DATA_VIDEO_DECODE_SUPPORT support{};
        support.Configuration = decoder.Configuration;
        support.Width = static_cast<UINT>( sps.pic_width_in_mbs_minus1 + 1 ) * 16;
        support.Height = static_cast<UINT>( sps.pic_height_in_map_units_minus1 + 1 ) * 16;
        support.DecodeFormat = DXGI_FORMAT_NV12;
        support.FrameRate.Denominator = 1;
        ThrowIfFailed( videoDevice->CheckFeatureSupport( D3D12_FEATURE_VIDEO_DECODE_SUPPORT, &support, sizeof( support ) ) );

        // The RHI allocates independent DPB images; tier 1 instead requires one texture array.
        // https://learn.microsoft.com/en-us/windows/win32/api/d3d12video/ne-d3d12video-d3d12_video_decode_tier
        if ( !( support.SupportFlags & D3D12_VIDEO_DECODE_SUPPORT_FLAG_SUPPORTED ) || support.DecodeTier < D3D12_VIDEO_DECODE_TIER_2 ) {
            MKT_THROW_RUNTIME_ERROR( "This H.264 stream requires D3D12 video decode tier 2 with individual DPB textures." );
        }

        mReferenceOnly = ( support.ConfigurationFlags & D3D12_VIDEO_DECODE_CONFIGURATION_FLAG_REFERENCE_ONLY_ALLOCATIONS_REQUIRED ) != 0;
        const bool aligned{ ( support.ConfigurationFlags & D3D12_VIDEO_DECODE_CONFIGURATION_FLAG_HEIGHT_ALIGNMENT_MULTIPLE_32_REQUIRED ) != 0 };
        mWidth = aligned ? core::align( support.Width, 32u ) : support.Width;
        mHeight = aligned ? core::align( support.Height, 32u ) : support.Height;
        mMaxReferences = eastl::max( static_cast<u32>( sps.num_ref_frames ), 1u );
        mDpbSlotCount = eastl::min( mMaxReferences + ( mReferenceOnly ? 1u : 8u ), 17u );
        mColorSpace = GetVideoColorSpace( sps );

        if ( mReferenceOnly ) {
            // Check the identity NV12 output path used when DPB storage cannot be sampled.
            // https://learn.microsoft.com/en-us/windows/win32/api/d3d12video/ns-d3d12video-d3d12_video_decode_conversion_arguments
            D3D12_FEATURE_DATA_VIDEO_DECODE_CONVERSION_SUPPORT conversion{};
            conversion.Configuration = decoder.Configuration;
            conversion.DecodeSample.Width = mWidth;
            conversion.DecodeSample.Height = mHeight;
            conversion.DecodeSample.Format.Format = DXGI_FORMAT_NV12;
            conversion.DecodeSample.Format.ColorSpace = mColorSpace;
            conversion.OutputFormat = conversion.DecodeSample.Format;
            conversion.FrameRate = support.FrameRate;
            ThrowIfFailed( videoDevice->CheckFeatureSupport( D3D12_FEATURE_VIDEO_DECODE_CONVERSION_SUPPORT, &conversion, sizeof( conversion ) ) );

            if ( !( conversion.SupportFlags & D3D12_VIDEO_DECODE_CONVERSION_SUPPORT_FLAG_SUPPORTED ) ) {
                MKT_THROW_RUNTIME_ERROR( "D3D12 video cannot produce a sampleable NV12 output from reference-only storage." );
            }
        }

        D3D12_VIDEO_DECODER_HEAP_DESC heap{};
        heap.Configuration = decoder.Configuration;
        heap.DecodeWidth = mWidth;
        heap.DecodeHeight = mHeight;
        heap.Format = DXGI_FORMAT_NV12;
        heap.FrameRate.Denominator = 1;
        heap.MaxDecodePictureBufferCount = mDpbSlotCount;
        ThrowIfFailed( videoDevice->CreateVideoDecoderHeap( &heap, IID_PPV_ARGS( &mHeap ) ) );
        ThrowIfFailed( videoDevice->CreateVideoDecoder( &decoder, IID_PPV_ARGS( &mDecoder ) ) );
        mIsAllocated = true;
    }

    auto VideoDecoder::CreateSurface( bool reference ) -> VideoSurfaceHandle {
        VideoSurfaceHandle surface{ Ref<VideoSurface>::New( mWidth, mHeight, reference && mReferenceOnly, !reference || !mReferenceOnly ) };
        surface->Initialize( mDevice );
        return surface;
    }

    auto VideoDecoder::CreateBitstream( const video::H264Picture& picture ) -> BufferHandle {
        // DXVA uses three-byte start codes and pads the last slice to 128 bytes.
        // Drop one leading zero per normalized NAL; offset zero satisfies the input alignment.
        // https://raw.githubusercontent.com/FFmpeg/FFmpeg/master/libavcodec/dxva2_h264.c (commit_bitstream_and_slice_buffer)
        const auto bytes{ picture.GetBitstream() };
        const auto offsets{ picture.GetSliceOffsets() };
        const usize size{ core::align( bytes.size() - offsets.size(), usize{ 128 } ) };
        auto buffer{ mDevice->CreateBuffer( BufferCreateDescription{}
            .SetName( "D3D12 video bitstream" )
            .SetByteSize( size )
            .SetHeapType( HeapType::eUpload )
            .SetCpuAccessType( AccessType::eWrite )
            .SetResourceType( ResourceType::eRawBuffer_SRV )
            .SetBufferUsage( BufferUsageFlagsBits::VideoDecodeSource ) ) };
        auto* mapped{ static_cast<ubyte*>( mDevice->Map( buffer.GetPtr() ) ) };
        std::memset( mapped, 0, size );

        for ( usize index{}; index < offsets.size(); ++index ) {
            const usize end{ index + 1 < offsets.size() ? offsets[index + 1] : bytes.size() };
            std::memcpy( mapped + offsets[index] - index, bytes.data() + offsets[index] + 1, end - offsets[index] - 1 );
        }

        mDevice->UnMap( buffer.GetPtr() );
        return buffer;
    }

    auto VideoDecoder::MatchesConfiguration( const video::H264Picture& picture ) const -> bool {
        return std::memcmp( &picture.GetSps(), &mConfiguration->GetSps(), sizeof( h264::SPS ) ) == 0 &&
            std::memcmp( &picture.GetPps(), &mConfiguration->GetPps(), sizeof( h264::PPS ) ) == 0;
    }

    auto VideoDecoder::GetMaxReferences() const -> u32 {
        return mMaxReferences;
    }

    auto VideoDecoder::GetDpbSlotCount() const -> u32 {
        return mDpbSlotCount;
    }

    auto VideoDecoder::UsesCoincidentOutput() const -> bool {
        return !mReferenceOnly;
    }

    auto VideoDecoder::GetDecoder() const -> ID3D12VideoDecoder* {
        return mDecoder.Get();
    }

    auto VideoDecoder::GetHeap() const -> ID3D12VideoDecoderHeap* {
        return mHeap.Get();
    }

    auto VideoDecoder::GetColorSpace() const -> DXGI_COLOR_SPACE_TYPE {
        return mColorSpace;
    }

    auto VideoDecoder::NextFeedbackNumber() -> u32 {
        return ++mFeedbackNumber;
    }

    auto VideoDecoder::Destroy() -> void {
        mHeap.Reset();
        mDecoder.Reset();
        mIsAllocated = false;
    }

    VideoDecoder::~VideoDecoder() {
        Destroy();
    }
}

#endif
