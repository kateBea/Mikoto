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

#include <Assets/Image.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {

    /**
     * Creation parameters for an RHI texture and its optional CPU initializer.
     */
    struct TextureCreateDescription {
        eastl::string mName{};

        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mMipCount{ 1 };

        bool mTrackState{ true };
        bool mKeepInitializerResources{ false };

        // Only one of these can be used, this is provided so we can
        // initialize a GPU texture from an Image or a buffer of raw bytes from CPU side
        asset::ImageHandle mImageHandle{};

        // Can init texture from a buffer as well, example usages are noise texture
        // from SSAO
        memory::BufferSpanHandle mBufferSpan{};

        TextureSubresourceSet mSubresourceSet{};

        ResourceType mResourceType{ ResourceType::eInvalid };
        ResourceStates mInitialState{ ResourceStates::eUnknown };

        Format mFormat{ Format::eRGBA8_SNORM };
        HeapType mHeapType{ HeapType::eDeviceLocal };
        Multisampling mMultisampling{ Multisampling::eMsaaX1 };
        TextureDimension mDimension{ TextureDimension::eTexture2D };

        TextureUsageFlags mUsage{ TextureUsageFlagsBits::None };

        /**
         * Sets a backend debug name.
         *
         * @param name Input value used by this operation.
         * @returns The result of SetName.
         */
        auto SetName( eastl::string_view name ) -> TextureCreateDescription&;

        /**
         * Sets the value handled by SetWidth.
         *
         * @param width Input value used by this operation.
         * @returns The result of SetWidth.
         */
        auto SetWidth( core::u32 width ) -> TextureCreateDescription&;

        /**
         * Sets the value handled by SetHeight.
         *
         * @param height Input value used by this operation.
         * @returns The result of SetHeight.
         */
        auto SetHeight( core::u32 height ) -> TextureCreateDescription&;

        /**
         * Sets the value handled by SetMipCount.
         *
         * @param count Input value used by this operation.
         * @returns The result of SetMipCount.
         */
        auto SetMipCount( core::u32 count ) -> TextureCreateDescription&;

        /**
         * Uses @p image as initialization data and enables copy-destination usage.
         *
         * @param image Input value used by this operation.
         * @returns The result of SetImageData.
         */
        auto SetImageData( asset::ImageHandle image) -> TextureCreateDescription&;

        /**
         * Uses @p buffer as initialization data and enables copy-destination usage.
         *
         * @param buffer Input value used by this operation.
         * @returns The result of SetBufferData.
         */
        auto SetBufferData( memory::BufferSpanHandle buffer) -> TextureCreateDescription&;

        /**
         * Sets the value handled by SetHeapType.
         *
         * @param heapType Input value used by this operation.
         * @returns The result of SetHeapType.
         */
        auto SetHeapType( HeapType heapType) -> TextureCreateDescription&;

        /**
         * Sets the value handled by SetMultisampling.
         *
         * @param sampleCount Input value used by this operation.
         * @returns The result of SetMultisampling.
         */
        auto SetMultisampling( Multisampling sampleCount ) -> TextureCreateDescription&;

        /**
         * Sets the value handled by SetFormat.
         *
         * @param usage Input value used by this operation.
         * @returns The result of SetFormat.
         */
        auto SetFormat( Format usage ) -> TextureCreateDescription&;

        /**
         * Sets the value handled by SetDimensions.
         *
         * @param dimensions Input value used by this operation.
         * @returns The result of SetDimensions.
         */
        auto SetDimensions( TextureDimension dimensions ) -> TextureCreateDescription&;

        /**
         * Adds one or more intended texture usages.
         *
         * @param usage Input value used by this operation.
         * @returns The result of SetUsage.
         */
        auto SetUsage( TextureUsageFlags usage ) -> TextureCreateDescription&;

        /**
         * Sets the value handled by SetResourceType.
         *
         * @param usage Input value used by this operation.
         * @returns The result of SetResourceType.
         */
        auto SetResourceType( ResourceType usage ) -> TextureCreateDescription&;

        /**
         * Sets the value handled by SetSubResources.
         *
         * @param subResources Input value used by this operation.
         * @returns The result of SetSubResources.
         */
        auto SetSubResources( const TextureSubresourceSet& subResources ) -> TextureCreateDescription&;

        /**
         * Sets the value handled by SetInitialState.
         *
         * @param value Input value used by this operation.
         * @returns The result of SetInitialState.
         */
        constexpr auto SetInitialState(ResourceStates value) -> TextureCreateDescription& { mInitialState = value; return *this; }

        /**
         * Performs the operation represented by EnableAutomaticStateTracking.
         *
         * @param initialState Input value used by this operation.
         * @returns The result of EnableAutomaticStateTracking.
         */
        constexpr auto EnableAutomaticStateTracking(ResourceStates initialState) -> TextureCreateDescription& {
            mInitialState = initialState;
            mTrackState = true;
            return *this;
        }
    };

    /**
     * Creation parameters for an RHI sampler.
     */
    struct SamplerCreateDescription {
        core::f32 mMipLevels{ 1.0f };

        CompareOp mCompareOp{ CompareOp::eNever };

        SamplerMipmapMode mMipMapMode{ SamplerMipmapMode::eLinear };

        SamplerFilter mMinFilter{ SamplerFilter::eNearest };
        SamplerFilter mMagFilter{ SamplerFilter::eNearest };
        SamplerWrapMode mWrapU{ SamplerWrapMode::eRepeat };
        SamplerWrapMode mWrapV{ SamplerWrapMode::eRepeat };
        SamplerWrapMode mWrapW{ SamplerWrapMode::eRepeat };

        Color mBorderColor{ 0.0f, 0.0f, 0.0f, 1.0f };

        /**
         * Sets the value handled by SetMipLevels.
         *
         * @param mipLevels Input value used by this operation.
         * @returns The result of SetMipLevels.
         */
        auto SetMipLevels( float mipLevels) -> SamplerCreateDescription&;

        /**
         * Sets the value handled by SetBorderColor.
         *
         * @param color Input value used by this operation.
         * @returns The result of SetBorderColor.
         */
        auto SetBorderColor( const Color& color ) -> SamplerCreateDescription&;

        /**
         * Sets the value handled by SetMipmapMode.
         *
         * @param mode Input value used by this operation.
         * @returns The result of SetMipmapMode.
         */
        auto SetMipmapMode( SamplerMipmapMode mode ) -> SamplerCreateDescription&;

        /**
         * Sets the value handled by SetFilter.
         *
         * @param filter Input value used by this operation.
         * @returns The result of SetFilter.
         */
        auto SetFilter( SamplerFilter filter ) -> SamplerCreateDescription&;

        /**
         * Sets the value handled by SetMinFilter.
         *
         * @param filter Input value used by this operation.
         * @returns The result of SetMinFilter.
         */
        auto SetMinFilter( SamplerFilter filter ) -> SamplerCreateDescription&;

        /**
         * Sets the value handled by SetMagFilter.
         *
         * @param filter Input value used by this operation.
         * @returns The result of SetMagFilter.
         */
        auto SetMagFilter( SamplerFilter filter ) -> SamplerCreateDescription&;

        /**
         * Sets the value handled by SetWrap.
         *
         * @param wrap Input value used by this operation.
         * @returns The result of SetWrap.
         */
        auto SetWrap( SamplerWrapMode wrap ) -> SamplerCreateDescription&;

        /**
         * Sets the value handled by SetWrapU.
         *
         * @param wrap Input value used by this operation.
         * @returns The result of SetWrapU.
         */
        auto SetWrapU( SamplerWrapMode wrap ) -> SamplerCreateDescription&;

        /**
         * Sets the value handled by SetWrapV.
         *
         * @param wrap Input value used by this operation.
         * @returns The result of SetWrapV.
         */
        auto SetWrapV( SamplerWrapMode wrap ) -> SamplerCreateDescription&;

        /**
         * Sets the value handled by SetWrapW.
         *
         * @param wrap Input value used by this operation.
         * @returns The result of SetWrapW.
         */
        auto SetWrapW( SamplerWrapMode wrap ) -> SamplerCreateDescription&;
    };

    /**
     * Backend-independent interface for immutable sampler state.
     */
    class ISampler : public DeviceObject {
    public:

        /**
         * Returns the minification filter.
         * @returns The result of GetMinFilter.
         */
        MKT_NODISCARD auto GetMinFilter() const -> SamplerFilter;

        /**
         * Returns the magnification filter.
         * @returns The result of GetMagFilter.
         */
        MKT_NODISCARD auto GetMagFilter() const -> SamplerFilter;

        /**
         * Returns the U-axis wrapping mode.
         * @returns The result of GetWrapU.
         */
        MKT_NODISCARD auto GetWrapU() const -> SamplerWrapMode;

        /**
         * Returns the V-axis wrapping mode.
         * @returns The result of GetWrapV.
         */
        MKT_NODISCARD auto GetWrapV() const -> SamplerWrapMode;

        /**
         * Returns the W-axis wrapping mode.
         * @returns The result of GetWrapW.
         */
        MKT_NODISCARD auto GetWrapW() const -> SamplerWrapMode;

        /**
         * Returns the maximum mip level sampled by this sampler.
         * @returns The result of GetMipLevels.
         */
        MKT_NODISCARD auto GetMipLevels() const -> float;

        using DeviceObject::Initialize;

    protected:

        /**
         * Creates immutable sampler state in the active backend.
         */
        auto Initialize() -> void override = 0;

        /**
         * Releases native sampler state.
         */
        auto Destroy() -> void override = 0;

        /**
         * Performs the operation represented by ISampler.
         *
         * @returns The result of ISampler.
         */
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

    /**
     * Backend-independent interface for a GPU texture.
     *
     * The texture may retain either an image initializer or a raw buffer
     * initializer, depending on how its creation description was populated.
     */
    class ITexture : public DeviceObject {
    public:

        /**
         * Returns the texture format.
         * @returns The result of GetFormat.
         */
        MKT_NODISCARD auto GetFormat() const -> Format;

        /**
         * Returns the base mip width in texels.
         * @returns The result of GetWidth.
         */
        MKT_NODISCARD auto GetWidth() const -> core::u32;

        /**
         * Returns the base mip height in texels.
         * @returns The result of GetHeight.
         */
        MKT_NODISCARD auto GetHeight() const -> core::u32;

        /**
         * Returns the declared texture usages.
         * @returns The result of GetTextureUsage.
         */
        MKT_NODISCARD auto GetTextureUsage() const -> TextureUsageFlags;

        /**
         * Returns the retained image initializer, if any.
         * @returns The result of GetImage.
         */
        MKT_NODISCARD auto GetImage() const -> asset::ImageHandle;

        /**
         * Returns the retained initializer byte count, or zero when no initializer was retained.
         * @returns The result of GetSizeBytes.
         */
        MKT_NODISCARD auto GetSizeBytes() const -> core::usize;

        /**
         * Returns the texture dimensionality.
         * @returns The result of GetDimension.
         */
        MKT_NODISCARD auto GetDimension() const -> TextureDimension;

        /**
         * Returns the multisampling mode.
         * @returns The result of GetSampleCount.
         */
        MKT_NODISCARD auto GetSampleCount() const -> Multisampling;

        /**
         * Returns the number of mip levels.
         * @returns The result of GetMipLevelCount.
         */
        MKT_NODISCARD auto GetMipLevelCount() const -> core::u32;

        /**
         * Destroys the texture.
         */
        ~ITexture() override = default;

    protected:

        /**
         * Constructs a texture from its creation description.
         *
         * @param desc Texture creation parameters.
         */
        explicit ITexture( const TextureCreateDescription& desc );

    protected:
        core::u32 mWidth{};
        core::u32 mHeight{};
        core::u32 mMipCount{ 1 };

        asset::ImageHandle mImageData{};
        memory::BufferSpanHandle mBufferSpan{};

        Format mFormat{ Format::eUnknown };
        TextureDimension mDimension{ TextureDimension::eInvalid };

        const ResourceStates mInitialState;

        TextureUsageFlags mTextureUsage{ TextureUsageFlagsBits::ShaderResource };

        Multisampling mMultisampling{ Multisampling::eMsaaX1 };

        TextureSubresourceSet mSubResources{};
    };

    using TextureHandle = core::Ref<ITexture>;
}

#endif//MIKOTO_RHI_TEXTURE_HH
