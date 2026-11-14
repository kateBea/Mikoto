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

#ifndef MIKOTO_ASSETS_ASSET_SPRITE_HH
#define MIKOTO_ASSETS_ASSET_SPRITE_HH

#include <Assets/Asset.hh>
#include <Assets/AssetTexture.hh>

namespace mikoto::asset {

    struct SpriteLoadDescription {
        filesystem::Path mPath{};
        LoadPriority mPriority{ LoadPriority::eNormal };
    };

    /** Owns the runtime resource; scenes keep AssetRef/AssetHandle instead of backend objects. */
    class Sprite final : public core::ReferenceCounted<Sprite> {
    public:
        explicit Sprite( core::Ref<Texture> resource );

        MKT_NODISCARD auto GetResource() const -> core::Ref<renderer::rhi::ITexture>;
        MKT_NODISCARD auto GetResidentMemoryUsage() const -> AssetMemoryUsage;

        ~Sprite() override;
    private:
        core::Ref<Texture> mResource{};
    };

    template<>
    struct AssetTraits<Sprite> {
        using Asset = Sprite;
        using Description = SpriteLoadDescription;

        static constexpr AssetType kType{ AssetType::eSprite };
    };
}// namespace mikoto::asset
#endif// MIKOTO_ASSETS_ASSET_SPRITE_HH
