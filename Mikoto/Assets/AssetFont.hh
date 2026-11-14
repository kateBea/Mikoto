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

#ifndef MIKOTO_ASSETS_ASSET_FONT_HH
#define MIKOTO_ASSETS_ASSET_FONT_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Assets/Asset.hh>

#include <Renderer/Text/Font.hh>

namespace mikoto::asset {

    struct FontLoadDescription {
        filesystem::FileHandle mFile{};
        float mSize{ 48 };

        /** Optional color fallback;
         * defaults to the bundled Noto Color Emoji font. */
        filesystem::Path mColorFont{};

        filesystem::Path mPath{};
        LoadPriority mPriority{ LoadPriority::eNormal };

        auto SetFile( filesystem::FileHandle file ) -> FontLoadDescription&;
        auto SetSize( float pixelSize ) -> FontLoadDescription&;
    };

    /** Owns the runtime resource; scenes keep AssetRef/AssetHandle instead of backend objects. */
    class Font final : public core::ReferenceCounted<Font> {
    public:
        explicit Font( core::Ref<renderer::Font> resource );

        MKT_NODISCARD auto GetResource() const -> core::Ref<renderer::Font>;

        MKT_NODISCARD auto CanEvict() const -> bool;
        MKT_NODISCARD auto GetResidentMemoryUsage() const -> AssetMemoryUsage;

        ~Font() override;
    private:
        core::Ref<renderer::Font> mResource{};
    };

    template<>
    struct AssetTraits<Font> {
        using Asset = Font;
        using Description = FontLoadDescription;

        static constexpr AssetType kType{ AssetType::eFont };
    };

    template<>
    struct AssetTraits<renderer::Font> : public AssetTraits<Font> {

    };
}// namespace mikoto::asset
#endif// MIKOTO_ASSETS_ASSET_FONT_HH
