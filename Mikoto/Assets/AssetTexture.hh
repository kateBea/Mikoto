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

#ifndef MIKOTO_ASSETS_ASSET_TEXTURE_HH
#define MIKOTO_ASSETS_ASSET_TEXTURE_HH

#include <Assets/Asset.hh>

#include <Renderer/Rhi/Texture.hh>

namespace mikoto::asset {

    struct TextureLoadDescription {
        filesystem::Path mPath{};
        renderer::rhi::TextureDimension mDimension{ renderer::rhi::TextureDimension::eTexture2D };

        auto SetPath( const filesystem::Path& path ) -> TextureLoadDescription&;
        auto SetDimensions( renderer::rhi::TextureDimension dim ) -> TextureLoadDescription&;
        LoadPriority mPriority{ LoadPriority::eNormal };
    };

    /** Owns the runtime resource; scenes keep AssetRef/AssetHandle instead of backend objects. */
    class Texture final : public core::ReferenceCounted<Texture> {
    public:
        explicit Texture( core::Ref<renderer::rhi::ITexture> resource, AssetRef<Texture> reference = {} );

        MKT_NODISCARD auto GetResource() const -> core::Ref<renderer::rhi::ITexture>;
        MKT_NODISCARD auto GetFormat() const -> renderer::rhi::Format;
        MKT_NODISCARD auto GetWidth() const -> core::u32;
        MKT_NODISCARD auto GetHeight() const -> core::u32;
        MKT_NODISCARD auto GetTextureUsage() const -> renderer::rhi::TextureUsageFlags;
        MKT_NODISCARD auto GetImage() const -> asset::ImageHandle;
        MKT_NODISCARD auto GetSourcePath() const -> const filesystem::Path&;
        MKT_NODISCARD auto GetSizeBytes() const -> core::usize;
        MKT_NODISCARD auto GetDimension() const -> renderer::rhi::TextureDimension;
        MKT_NODISCARD auto GetSampleCount() const -> renderer::rhi::Multisampling;
        MKT_NODISCARD auto GetMipLevelCount() const -> core::u32;
        MKT_NODISCARD auto GetStorageSizeBytes() const -> core::usize;
        MKT_NODISCARD auto CanEvict() const -> bool;
        MKT_NODISCARD auto GetResidentMemoryUsage() const -> AssetMemoryUsage;

        ~Texture() override;
    private:
        AssetRef<Texture> mReference{};
        core::Ref<renderer::rhi::ITexture> mResource{};
    };

    template<> struct AssetTraits<Texture> {
        using Asset = Texture;
        using Description = TextureLoadDescription;
        static constexpr AssetType kType{ AssetType::eTexture };
    };

    template<> struct AssetTraits<renderer::rhi::ITexture> : AssetTraits<Texture> {};
}// namespace mikoto::asset
#endif// MIKOTO_ASSETS_ASSET_TEXTURE_HH
