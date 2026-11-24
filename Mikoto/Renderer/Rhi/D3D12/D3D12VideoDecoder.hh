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

#ifndef MIKOTO_RENDERER_D3D12_VIDEO_DECODER_HH
#define MIKOTO_RENDERER_D3D12_VIDEO_DECODER_HH

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/VideoDecoder.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <Platform/Windows/Windows.hh>

#include <directx/d3d12video.h>
#include <wrl.h>

#include <Renderer/Rhi/D3D12/D3D12MemoryAllocator.hh>

namespace mikoto::renderer::d3d12 {

    // Plane views retain this allocation through their last graphics submission.
    class VideoImageStorage final : public rhi::DeviceObject {
    public:
        VideoImageStorage( core::u32 width, core::u32 height, bool referenceOnly );
        MKT_NODISCARD auto GetResource() const -> ID3D12Resource*;
        ~VideoImageStorage() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

        ImageAllocation mAllocation{};
        core::u32 mWidth{};
        core::u32 mHeight{};
        bool mReferenceOnly{};
    };

    class VideoSurface final : public rhi::IVideoSurface {
    public:
        VideoSurface( core::u32 width, core::u32 height, bool referenceOnly, bool sampled );
        MKT_NODISCARD auto GetImage() const -> rhi::TextureHandle override;
        MKT_NODISCARD auto GetLumaImage() const -> rhi::TextureHandle override;
        MKT_NODISCARD auto GetChromaImage() const -> rhi::TextureHandle override;
        ~VideoSurface() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

        core::Ref<VideoImageStorage> mStorage{};
        rhi::TextureHandle mImage{};
        rhi::TextureHandle mLumaImage{};
        rhi::TextureHandle mChromaImage{};
        core::u32 mWidth{};
        core::u32 mHeight{};
        bool mReferenceOnly{};
        bool mSampled{};
    };

    class VideoDecoder final : public rhi::IVideoDecoder {
    public:
        explicit VideoDecoder( const rhi::VideoDecoderDescription& description );
        MKT_NODISCARD auto CreateSurface( bool reference ) -> rhi::VideoSurfaceHandle override;
        MKT_NODISCARD auto GetMaxReferences() const -> core::u32 override;
        MKT_NODISCARD auto GetDpbSlotCount() const -> core::u32 override;
        MKT_NODISCARD auto UsesCoincidentOutput() const -> bool override;

        MKT_NODISCARD auto GetDecoder() const -> ID3D12VideoDecoder*;
        MKT_NODISCARD auto GetHeap() const -> ID3D12VideoDecoderHeap*;
        MKT_NODISCARD auto GetColorSpace() const -> DXGI_COLOR_SPACE_TYPE;
        MKT_NODISCARD auto MatchesConfiguration( const video::H264Picture& picture ) const -> bool;
        MKT_NODISCARD auto CreateBitstream( eastl::span<const core::ubyte> bytes ) -> rhi::BufferHandle override;
        MKT_NODISCARD auto GetBitstreamLayout() const -> rhi::VideoBitstreamLayout override;
        MKT_NODISCARD auto NextFeedbackNumber() -> core::u32;
        ~VideoDecoder() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

        core::Ref<video::H264Picture> mConfiguration{};
        Microsoft::WRL::ComPtr<ID3D12VideoDecoder> mDecoder{};
        Microsoft::WRL::ComPtr<ID3D12VideoDecoderHeap> mHeap{};
        DXGI_COLOR_SPACE_TYPE mColorSpace{ DXGI_COLOR_SPACE_YCBCR_STUDIO_G22_LEFT_P709 };
        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mMaxReferences{};
        core::u32 mDpbSlotCount{};
        core::u32 mFeedbackNumber{};
        bool mReferenceOnly{};
    };
}

#endif
#endif// MIKOTO_RENDERER_D3D12_VIDEO_DECODER_HH
