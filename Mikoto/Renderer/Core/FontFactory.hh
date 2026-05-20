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

#ifndef MIKOTO_FONT_FACTORY_HH
#define MIKOTO_FONT_FACTORY_HH

// I love Windows.h defining min and max macros that break everything
#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <EASTL/vector.h>
#include <EASTL/string_view.h>
#include <EASTL/fixed_vector.h>

#include <ankerl/unordered_dense.h>
#include <msdf-atlas-gen/msdf-atlas-gen.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Filesystem/File.hh>

#include <Renderer/Text/Font.hh>
#include <Renderer/Core/GpuDevice.hh>

namespace mikoto::renderer {

    struct FontLoadDescription {
        FileHandle mFile{};
        float mSize{ 48 };

        auto SetFile( FileHandle file ) -> FontLoadDescription&;
        auto SetSize( float pixelSize ) -> FontLoadDescription&;
    };

    struct FontFactoryCreateInfo {
        GpuDevice *mDevice{ nullptr };
    };

    class FontFactory final : public IService, public Singleton<FontFactory> {
    public:
        explicit FontFactory( const FontFactoryCreateInfo &options );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto LoadFont(const FontLoadDescription& description ) -> FontHandle;

    private:

        using GeometryList = eastl::vector<msdf_atlas::GlyphGeometry>;
        using AtlasGenerator = msdf_atlas::ImmediateAtlasGenerator<float, 4,
            msdf_atlas::mtsdfGenerator, msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>>;
        using BitmapAtlasStorage = msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>;

        struct CharsetRange {
            i32 mStart{};
            i32 mEnd{};

            MKT_NODISCARD static auto GetBasicLatinRange() -> CharsetRange;
            MKT_NODISCARD static auto GetLatin1SupplementRange() -> CharsetRange;
            MKT_NODISCARD static auto GetLatinExtendedARange() -> CharsetRange;
            MKT_NODISCARD static auto GetCyrillicRange() -> CharsetRange;
            MKT_NODISCARD static auto GetGreekRange() -> CharsetRange;
            MKT_NODISCARD static auto GetHiraganaRange() -> CharsetRange;
            MKT_NODISCARD static auto GetKatakanaRange() -> CharsetRange;
            MKT_NODISCARD static auto GetKanjiCommonRange() -> CharsetRange;
        };

        struct FontProperties {
            i32 mMaxHeight{};
            i32 mMaxWidth{};
            TextureHandle mAtlas{};
            AtlasGenerator mAtlasProperties{};

            ankerl::unordered_dense::map<msdf_atlas::unicode_t, FontGlyph> mGlyphMap{};
        };

        struct AtlasGenerateDescription {
            eastl::string_view mFilename{};

            f32 mFontSize{};
            f32 mFontScale{};

            bool mUseCustomCharSet{ true }; // If false we use ASCII only
            bool mExpensiveColoring{ false };

            eastl::fixed_vector<CharsetRange, 10> mCharacterSetRanges{};
        };

    private:
        MKT_NODISCARD auto GenerateAtlas( eastl::string_view fontFilename, i32 fontSize, bool expensiveColoring = true ) const -> FontProperties;
        auto SubmitAtlasBitmapAndLayout(const BitmapAtlasStorage& atlas, GeometryList& glyphs, FontProperties& data, i32 fontSize ) const -> void;

    private:
        GpuDevice *mDevice{ nullptr };
        msdfgen::FreetypeHandle *mFreeTypeHandle{ nullptr };
    };
}



#endif //MIKOTO_FONT_FACTORY_HH
