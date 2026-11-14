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

#ifndef MIKOTO_TEXT_FONT_ATLAS_HH
#define MIKOTO_TEXT_FONT_ATLAS_HH

#include <span>
#include <array>
#include <cmath>
#include <vector>
#include <algorithm>
#include <stdexcept>

#include <EASTL/sort.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>

#include <ankerl/unordered_dense.h>

#include <glm/glm.hpp>

#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Renderer/Text/Font.hh>

namespace mikoto::renderer {

    // Raw immutable SlugHorn pages. Keep the third-party types out of public
    // headers; the renderer uploads these bytes with the matching RHI format.
    struct SlugHornTextureData {
        core::u32 mWidth{};
        core::u32 mHeight{};
        eastl::vector<core::ubyte> mPixels{};
    };

    struct SlugHornAtlasData {
        SlugHornTextureData mCurves{};
        SlugHornTextureData mBands{};
        SlugHornTextureData mStrokeCurves{};
    };

    struct FontAtlasData {
        SlugHornAtlasData slug;

        int width{};
        int height{};
        double lineHeight{ 1.2 };
        double ascender{ 1.0 };
        float distanceRange{ 4.0f };
        eastl::vector<core::ubyte> pixels;
        ankerl::unordered_dense::map<core::u32, FontGlyph> glyphs;
        ankerl::unordered_dense::map<core::u64, double> kerning;
    };

}// namespace mikoto::renderer

namespace mikoto::text {
    inline constexpr core::usize kMaxEmojiSequence{ 64 };

    struct ColorGlyphRequest {
        core::u32 mGlyphKey{};
        eastl::vector<core::u32> mSequence{};
    };

    inline constexpr core::usize kSlugTextureWidth{ 4096ull };

    struct SlugCurve {
        core::float2 p0{};
        core::float2 p1{};
        core::float2 p2{};
    };

    struct SlugAtlasData {
        eastl::vector<core::float4> mCurves{};
        eastl::vector<eastl::array<core::u32, 2>> mBands{};
    };

    // Per-upload cache. Font pages keep the textures alive while this is used.
    // Ref converts to bool, so converting a live handle to an integer gives 1,
    // not its identity. Float curve data and integer band data must stay distinct.
    class AtlasTextureBindings {
    public:
        template<class Handle, class BindTexture>
        auto Resolve( const Handle& texture, BindTexture&& bind ) -> core::u32 {
            MKT_BEGIN_PROFILER();
            const auto* identity = texture.GetPtr();
            auto found = mBindings.find( identity );
            if ( found == mBindings.end() ) {
                found = mBindings.emplace( identity, bind( texture ) ).first;
            }
            return found->second;
        }

        auto Clear() -> void;

    private:
        ankerl::unordered_dense::map<const void*, core::u32> mBindings;
    };
}// namespace mikoto::text

#endif// MIKOTO_TEXT_FONT_ATLAS_HH
