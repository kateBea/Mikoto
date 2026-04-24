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

#include <Renderer/Text/Font.hh>

namespace mikoto::renderer {

    Font::Font( TextureHandle fontAtlas, f32 pixelSize, Path path )
        : mPath{ path },
          mAtlas{ fontAtlas },
          mPixelSize{ pixelSize } {}

    auto Font::GetPath() const -> const Path& {
        return mPath;
    }

    auto Font::GetName() const -> eastl::string_view {
        return mPath.GetStem();
    }

    auto Font::GetAtlas() const -> TextureHandle {
        return mAtlas;
    }

    auto Font::GetGlyph( const u32 characterCode ) const -> const FontGlyph& {
        return mGlyphs.at( characterCode );
    }

    auto Font::GetMaxHeight() const -> f64 {
        return mMaxHeight;
    }

    auto Font::GetMaxWidth() const -> f64 {
        return mMaxWidth;
    }

    auto Font::SetMaxHeight( const f64 maxHeight ) -> void {
        mMaxHeight = maxHeight;
    }

    auto Font::SetMaxWidth( const f64 maxWidth ) -> void {
        mMaxWidth = maxWidth;
    }

    auto Font::HasGlyph( const u32 unicodePoint ) const -> bool {
        return mGlyphs.contains( unicodePoint );
    }

    auto Font::RegisterGlyph( u32 characterCode, const FontGlyph& glyph ) -> void {
        mGlyphs.emplace( characterCode, glyph );
    }

    auto Font::GetGlyphCount() const -> size_t {
        return mGlyphs.size();
    }

    auto Font::GetSize() const -> f64 {
        return mPixelSize;
    }
}// namespace mikoto::renderer