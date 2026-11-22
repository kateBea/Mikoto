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

#ifndef MIKOTO_TEXT_TEXT_LAYOUT_HH
#define MIKOTO_TEXT_TEXT_LAYOUT_HH

#include <span>
#include <cmath>
#include <algorithm>

#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Text/FontAtlas.hh>
#include <Text/UnicodeString.hh>

#include <Renderer/Text/Font.hh>

namespace mikoto::text {

    struct ScreenPlacement {
        core::float2 mPosition{ 24.0f, 24.0f };
        core::float2 mScale{ 1.0f, 1.0f };
        core::f32 mRotation{}; // Clockwise radians in the top-left, Y-down pixel space.
    };

    enum class Alignment {
        eLeft,
        eCenter,
        eRight
    };

    struct LayoutOptions {
        core::f32 mSize{ 12.0f };
        core::f32 mLetterSpacing{};
        core::f32 mLineSpacing{ 1.0f };
        Alignment mAlignment{ Alignment::eLeft };
        bool mYDown{};
    };

    struct GlyphQuad {
        core::u32 mCodepoint{};
        core::float2 mPosition{};
        core::float2 mSize{};
    };

    struct TextLayout {
        eastl::vector<GlyphQuad> mGlyphs;
        core::float2 mMinimum{};
        core::float2 mMaximum{};
        core::usize mMissingGlyphs{};
        core::usize mLineCount{ 1 };
        bool mTruncated{};
    };

}// namespace mikoto::text

#endif// MIKOTO_TEXT_TEXT_LAYOUT_HH
