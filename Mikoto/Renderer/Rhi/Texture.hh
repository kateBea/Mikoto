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

#ifndef MIKOTO_RHI_TEXTURE_HH
#define MIKOTO_RHI_TEXTURE_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Memory/BufferSpan.hh>
#include <Core/ResourcePool.hh>

#include <Memory/BufferSpan.hh>

#include <Assets/Image.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {

    struct TextureCreateDescription {
        eastl::string mName{};

        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mMipCount{ 1 };

        bool mKeepInitializerResources{ false };

        // Only one of these can be used, this is provided so we can
        // initialize a GPU texture from an Image or a buffer of raw bytes from CPU side
        asset::ImageHandle mImageHandle{};

        // Can init texture from a buffer as well, example usages are noise texture
        // from SSAO
        memory::BufferSpanHandle mBufferSpan{};

        bool mTrackState{ true };
        ResourceStates mInitialState{ ResourceStates::eUnknown };

        HeapType mHeapType{ HeapType::eDeviceLocal };

        Multisampling mMSAA{ Multisampling::eMsaaX1 };

        Format mFormat{ Format::eRGBA8_SNORM };
        TextureDimension mDimension{ TextureDimension::eTexture2D };

        TextureUsageFlags mUsage{ TextureUsageFlagsBits::kNone };

        ResourceType mResourceType{ ResourceType::eInvalid };

        TextureSubresourceSet mSubresourceSet{};

        auto SetName( eastl::string_view name ) -> TextureCreateDescription&;
        auto SetWidth( core::u32 width ) -> TextureCreateDescription&;
        auto SetHeight( core::u32 height ) -> TextureCreateDescription&;
        auto SetMipCount( core::u32 count ) -> TextureCreateDescription&;

        auto SetKeepInitializerResources( bool value ) -> TextureCreateDescription&;

        auto SetImageData( asset::ImageHandle image) -> TextureCreateDescription&;
        auto SetBufferData( memory::BufferSpanHandle buffer) -> TextureCreateDescription&;

        auto SetHeapType( HeapType heapType) -> TextureCreateDescription&;

        auto SetMultisampling( Multisampling sampleCount ) -> TextureCreateDescription&;

        auto SetFormat( Format usage ) -> TextureCreateDescription&;
        auto SetDimensions( TextureDimension dimensions ) -> TextureCreateDescription&;

        auto SetUsage( TextureUsageFlags usage ) -> TextureCreateDescription&;

        auto SetResourceType( ResourceType usage ) -> TextureCreateDescription&;

        auto SetSubResources( const TextureSubresourceSet& subResources ) -> TextureCreateDescription&;

        constexpr auto SetInitialState(ResourceStates value) -> TextureCreateDescription& { mInitialState = value; return *this; }
        constexpr auto EnableAutomaticStateTracking(ResourceStates initialState) -> TextureCreateDescription& {
            mInitialState = initialState;
            mTrackState = true;
            return *this;
        }
    };

    struct SamplerCreateDescription {
        core::f32 mMipLevels{ 1.0f };

        SamplerFilter mMinFilter{ SamplerFilter::eNearest };
        SamplerFilter mMagFilter{ SamplerFilter::eNearest };
        SamplerWrapMode mWrapU{ SamplerWrapMode::eRepeat };
        SamplerWrapMode mWrapV{ SamplerWrapMode::eRepeat };
        SamplerWrapMode mWrapW{ SamplerWrapMode::eRepeat };

        Color mBorderColor{ 0.0f, 0.0f, 0.0f, 1.0f };

        auto SetMipLevels( float mipLevels) -> SamplerCreateDescription&;

        auto SetBorderColor( const Color& color ) -> SamplerCreateDescription&;

        auto SetFilter( SamplerFilter filter ) -> SamplerCreateDescription&;
        auto SetMinFilter( SamplerFilter filter ) -> SamplerCreateDescription&;
        auto SetMagFilter( SamplerFilter filter ) -> SamplerCreateDescription&;

        auto SetWrap( SamplerWrapMode wrap ) -> SamplerCreateDescription&;
        auto SetWrapU( SamplerWrapMode wrap ) -> SamplerCreateDescription&;
        auto SetWrapV( SamplerWrapMode wrap ) -> SamplerCreateDescription&;
        auto SetWrapW( SamplerWrapMode wrap ) -> SamplerCreateDescription&;
    };

    class ISampler : public DeviceObject {
    public:

        MKT_NODISCARD auto GetMinFilter() const -> SamplerFilter { return mMinFilter; }
        MKT_NODISCARD auto GetMagFilter() const -> SamplerFilter { return mMagFilter; }

        MKT_NODISCARD auto GetWrapU() const -> SamplerWrapMode { return mWrapU; }
        MKT_NODISCARD auto GetWrapV() const -> SamplerWrapMode { return mWrapV; }
        MKT_NODISCARD auto GetWrapW() const -> SamplerWrapMode { return mWrapW; }

        MKT_NODISCARD auto GetMipLevels() const -> float { return mMipLevels; }

        using DeviceObject::Initialize;

    protected:
        auto Initialize() -> void override = 0;
        auto Release() -> void override = 0;

        explicit ISampler( const SamplerCreateDescription& desc )
            : mMipLevels{ desc.mMipLevels},
            mMinFilter{ desc.mMinFilter},
            mMagFilter{ desc.mMagFilter},
            mWrapU{ desc.mWrapU},
            mWrapV{ desc.mWrapV},
            mWrapW{ desc.mWrapW},
            mBorderColor{ desc.mBorderColor }
        {}

    protected:
        float mMipLevels{ 1.0f };

        SamplerFilter mMinFilter{ SamplerFilter::eNearest };
        SamplerFilter mMagFilter{ SamplerFilter::eNearest };
        SamplerWrapMode mWrapU{ SamplerWrapMode::eRepeat };
        SamplerWrapMode mWrapV{ SamplerWrapMode::eRepeat };
        SamplerWrapMode mWrapW{ SamplerWrapMode::eRepeat };

        Color mBorderColor{};
    };

    using SamplerHandle = core::Ref<ISampler>;

    class ITexture : public DeviceObject {
    public:
        MKT_NODISCARD auto GetFormat() const -> Format {
            return mFormat;
        }

        MKT_NODISCARD auto GetWidth() const -> core::u32 {
            return mWidth;
        }

        MKT_NODISCARD auto GetHeight() const -> core::u32 {
            return mHeight;
        }

        MKT_NODISCARD auto GetTextureUsage() const -> TextureUsageFlags {
            return mTextureUsage;
        }

        MKT_NODISCARD auto GetImage() const -> asset::ImageHandle { return mImageData; }
        MKT_NODISCARD auto GetSizeBytes() const -> size_t { return mImageData->mBufferSpan->GetSize(); }

        MKT_NODISCARD auto GetDimension() const -> TextureDimension { return mDimension; }
        MKT_NODISCARD auto GetSampleCount() const -> Multisampling { return mMultisampling; }
        MKT_NODISCARD auto GetMipLevelCount() const -> core::u32 { return mMipCount; }

        ~ITexture() override = default;

    protected:
        explicit ITexture( const TextureCreateDescription& desc )
            : DeviceObject{ desc.mHeapType, desc.mResourceType },
              mWidth{ desc.mWidth },
              mHeight{ desc.mHeight },
              mMipCount{ desc.mMipCount },
              mImageData{ desc.mImageHandle },
              mBufferSpan{ desc.mBufferSpan },
              mFormat{ desc.mFormat },
              mDimension{ desc.mDimension },
              mTextureUsage{ desc.mUsage },
              mMultisampling{ desc.mMSAA }, mSubResources{ desc.mSubresourceSet } {}

    protected:
        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mMipCount{ 1 };

        asset::ImageHandle mImageData{};
        memory::BufferSpanHandle mBufferSpan{};

        Format mFormat{ Format::eUnknown };
        TextureDimension mDimension{ TextureDimension::eInvalid };

        TextureUsageFlags mTextureUsage{ TextureUsageFlagsBits::kShaderResource };

        Multisampling mMultisampling{ Multisampling::eMsaaX1 };

        TextureSubresourceSet mSubResources{};
    };

    using TextureHandle = core::Ref<ITexture>;
}

#endif//MIKOTO_RHI_TEXTURE_HH
