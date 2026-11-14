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

#include <Core/Profiler.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Text/Font.hh>
#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer::rhi;

    Font::Font( renderer::rhi::TextureHandle fontAtlas, core::f32 pixelSize, filesystem::Path path )
        : mPath{ path },
          mAtlas{ fontAtlas },
          mPixelSize{ pixelSize } {
        MKT_BEGIN_PROFILER();

        if ( !fontAtlas.IsEmpty() ) {
            mAtlasPages.push_back( fontAtlas );
        }
    }

    auto Font::GetPath() const -> const filesystem::Path& {
        MKT_BEGIN_PROFILER();
        return mPath;
    }

    auto Font::GetName() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mPath.GetStem();
    }

    auto Font::GetAtlas() const -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        return mAtlas;
    }

    auto Font::GetGlyph( const core::u32 characterCode ) const -> const FontGlyph& {
        MKT_BEGIN_PROFILER();

        if ( const auto* glyph = FindGlyph( characterCode ) ) {
            return *glyph;
        }

        if ( const auto* fallback = GetFallbackGlyph() ) {
            return *fallback;
        }

        static const FontGlyph empty{ 0xFFFD };
        return empty;
    }

    auto Font::FindColorGlyph( eastl::span<const core::u32> sequence ) const -> const FontGlyph* {
        if ( sequence.empty() ) {
            return nullptr;
        }

        const eastl::string key{ reinterpret_cast<const char*>( sequence.data() ), sequence.size_bytes() };
        const auto found{ mColorGlyphs.find( key ) };
        return found != mColorGlyphs.end() && found->second ? FindGlyph( found->second ) : nullptr;
    }

    auto Font::FindGlyph( core::u32 codepoint ) const -> const FontGlyph* {
        MKT_BEGIN_PROFILER();
        const auto it = mGlyphs.find( codepoint );
        return it == mGlyphs.end() ? nullptr : &it->second;
    }

    auto Font::GetFallbackGlyph() const -> const FontGlyph* {
        MKT_BEGIN_PROFILER();

        if ( const auto* glyph = FindGlyph( 0xFFFD ) ) {
            return glyph;
        }

        if ( const auto* glyph = FindGlyph( '?' ) ) {
            return glyph;
        }

        return FindGlyph( 0 );
    }

    auto Font::GetAtlas( core::usize page ) const -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        return page < mAtlasPages.size() ? mAtlasPages[page] : renderer::rhi::TextureHandle::CreateEmpty();
    }

    auto Font::GetKerning( core::u32 left, core::u32 right ) const -> double {
        MKT_BEGIN_PROFILER();
        const auto it = mKerning.find( ( core::u64{ left } << 32 ) | right );
        return it == mKerning.end() ? 0.0 : it->second;
    }

    auto Font::GetMaxHeight() const -> core::f64 {
        MKT_BEGIN_PROFILER();
        return mMaxHeight;
    }

    auto Font::GetMaxWidth() const -> core::f64 {
        MKT_BEGIN_PROFILER();
        return mMaxWidth;
    }

    auto Font::SetMaxHeight( const core::f64 maxHeight ) -> void {
        MKT_BEGIN_PROFILER();
        mMaxHeight = maxHeight;
    }

    auto Font::SetMaxWidth( const core::f64 maxWidth ) -> void {
        MKT_BEGIN_PROFILER();
        mMaxWidth = maxWidth;
    }

    auto Font::HasGlyph( const core::u32 unicodePoint ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mGlyphs.contains( unicodePoint );
    }

    auto Font::RegisterGlyph( core::u32 characterCode, const FontGlyph& glyph ) -> void {
        MKT_BEGIN_PROFILER();
        mGlyphs.emplace( characterCode, glyph );
    }

    auto Font::GetGlyphCount() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mGlyphs.size();
    }

    auto Font::GetSize() const -> core::f64 {
        MKT_BEGIN_PROFILER();
        return mPixelSize;
    }

    FontGlyph::FontGlyph( const core::u32 characterCode ) noexcept
        : mCodepoint{ characterCode } {
    }


    auto FontGlyph::SetCodepoint( const core::u32 cp ) noexcept -> void {
        MKT_BEGIN_PROFILER();
        mCodepoint = cp;
    }


    auto FontGlyph::SetWidth( const core::f64 width ) noexcept -> void {
        MKT_BEGIN_PROFILER();
        mWidth = width;
    }


    auto FontGlyph::SetHeight( const core::f64 height ) noexcept -> void {
        MKT_BEGIN_PROFILER();
        mHeight = height;
    }


    auto FontGlyph::SetBearingX( const core::f64 bx ) noexcept -> void {
        MKT_BEGIN_PROFILER();
        mBearingX = bx;
    }


    auto FontGlyph::SetBearingY( const core::f64 by ) noexcept -> void {
        MKT_BEGIN_PROFILER();
        mBearingY = by;
    }


    auto FontGlyph::SetAdvanceX( const core::f64 adv ) noexcept -> void {
        MKT_BEGIN_PROFILER();
        mAdvanceX = adv;
    }


    auto FontGlyph::SetBearingUnderline( const core::f64 bu ) noexcept -> void {
        MKT_BEGIN_PROFILER();
        mBearingUnderline = bu;
    }


    auto FontGlyph::SetAtlasBounds( const core::float4& bounds ) noexcept -> void {
        MKT_BEGIN_PROFILER();
        mAtlasBounds = bounds;
    }


    auto FontGlyph::SetPlaneBounds( const core::float4& bounds ) noexcept -> void {
        MKT_BEGIN_PROFILER();
        mPlaneBounds = bounds;
    }


    auto Font::GetAtlasPageCount() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mAtlasPages.size();
    }


    auto Font::GetSlugPage( core::usize page ) const -> SlugTexturePage {
        MKT_BEGIN_PROFILER();
        return page < mSlugPages.size() ? mSlugPages[page] : SlugTexturePage{};
    }


    auto Font::GetLineHeight() const -> double {
        MKT_BEGIN_PROFILER();
        return mLineHeight;
    }


    auto Font::GetAscender() const -> double {
        MKT_BEGIN_PROFILER();
        return mAscender;
    }


    auto Font::GetDistanceRange() const -> float {
        MKT_BEGIN_PROFILER();
        return mDistanceRange;
    }
}
