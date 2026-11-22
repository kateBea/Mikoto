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

#ifndef MIKOTO_TEXT_UTILITY_HH
#define MIKOTO_TEXT_UTILITY_HH

#include <span>

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Text/FontAtlas.hh>
#include <Text/TextLayout.hh>
#include <Text/UnicodeString.hh>

namespace mikoto::text {

    /**
     * @brief Decodes UTF-8 into Mikoto Unicode storage.
     * @param ut8 Borrowed UTF-8 byte range.
     * @returns Decoded text; malformed sequences become U+FFFD.
     */
    MKT_NODISCARD auto ConvertFromUtf8( eastl::string_view ut8 ) -> UnicodeString;

    /**
     * @brief Decodes UTF-8 into numeric Unicode code points.
     * @param ut8 Borrowed UTF-8 byte range.
     * @returns Decoded code points; malformed sequences become U+FFFD.
     */
    MKT_NODISCARD auto GetUnicodeFromUtf8( eastl::string_view ut8 ) -> eastl::vector<core::u32>;

    // Shared decoder for UnicodeString and the legacy vector API. Malformed
    // input consumes one byte and emits U+FFFD, matching the renderer's policy.
    /**
     * @brief Visits decoded UTF-8 code points without allocating a result.
     * @tparam Consumer Callable accepting a numeric Unicode code point.
     * @param utf8 Borrowed UTF-8 byte range.
     * @param consumer Callback receiving each numeric code point; malformed input yields U+FFFD.
     */
    template<typename Consumer>
    auto ForEachUtf8CodePoint( eastl::string_view utf8, Consumer&& consumer ) -> void {
        MKT_BEGIN_PROFILER();
        for ( core::usize i{}; i < utf8.size(); ) {
            const auto lead{ static_cast<core::ubyte>( utf8[i] ) };
            if ( lead < 0x80 ) {
                consumer( core::u32{ lead } );
                ++i;
                continue;
            }
            core::u32 count{};
            if ( lead >= 0xC2 && lead <= 0xDF ) {
                count = 2;
            } else if ( lead >= 0xE0 && lead <= 0xEF ) {
                count = 3;
            } else if ( lead >= 0xF0 && lead <= 0xF4 ) {
                count = 4;
            }

            core::u32 point{ count ? core::u32{ lead } & ( 0x7Fu >> count ) : 0u };
            bool valid{ count && count <= utf8.size() - i };
            for ( core::u32 j{ 1 }; valid && j < count; ++j ) {
                const auto next{ static_cast<core::ubyte>( utf8[i + j] ) };
                valid = ( next & 0xC0 ) == 0x80;
                point = ( point << 6 ) | ( next & 0x3F );
            }
            const core::u32 minimum{ count == 2 ? 0x80u : count == 3 ? 0x800u : 0x10000u };
            valid = valid && point >= minimum && point <= 0x10FFFF && !( point >= 0xD800 && point <= 0xDFFF );
            consumer( valid ? point : 0xFFFDu );
            i += valid ? count : 1;
        }
    }

    /**
     * @brief Builds a model matrix for screen-space text.
     * @param placement Screen-space position, scale and clockwise rotation in radians.
     * @returns Placement matrix in top-left, Y-down pixel coordinates.
     */
    auto ScreenTextModel( const ScreenPlacement& placement ) -> core::float4x4;

    // Screen overlays always occupy a valid clip-depth plane. Disabling depth
    // testing alone does not disable Vulkan's vertex clipping against 0 <= z <= w.
    /**
     * @brief Builds a projection for screen-space text.
     * @param width Width in pixels.
     * @param height Height in pixels.
     * @returns Orthographic projection for the supplied pixel dimensions.
     */
    auto ScreenTextProjection( float width, float height ) -> core::float4x4;

    /**
     * @brief Extracts editable text placement from a model matrix.
     * @param placement Screen-space position, scale and clockwise rotation in radians.
     * @param model Model matrix to decompose into placement.
     * @returns True when the matrix can be represented by placement fields.
     */
    auto SetScreenTextModel( ScreenPlacement& placement, const core::float4x4& model ) -> bool;

    /**
     * @brief  Identifies emoji candidates without redirecting ordinary letters to the fallback font.
     * @param codepoint Unicode code point to inspect.
     * @returns True for a supported emoji base.
     */
    MKT_NODISCARD auto IsEmojiBase( core::u32 codepoint ) -> bool;

    /**
     * @brief  Returns the bounded candidate cluster length, including presentation selectors and joiners.
     * @param codepoints Borrowed Unicode code-point range.
     * @returns Length of the leading sequence.
     */
    MKT_NODISCARD auto EmojiSequenceLength( std::span<const core::u32> codepoints ) -> core::usize;

    /**
     * @brief  Reports colour tables supported by the selected FreeType face.
     * @param path Source file path.
     * @returns True when supported color glyphs are available.
     */
    MKT_NODISCARD auto HasColorGlyphs( const char* path ) -> bool;

    /**
     * @brief Shapes complete emoji clusters and packs premultiplied RGBA pixels bottom-up,
     * matching the outline atlas convention. Unsupported clusters are omitted.
     * Each invocation owns its FreeType and HarfBuzz state; requests remain read-only.
     * @param path Source file path.
     * @param pixelSize Requested glyph size in pixels.
     * @param requests Glyph keys and Unicode sequences to rasterize.
     * @returns Atlas pixels and glyph metadata.
     */
    MKT_NODISCARD auto GenerateColorGlyphAtlas( const char* path, core::f32 pixelSize,
                                                std::span<const ColorGlyphRequest> requests ) -> renderer::FontAtlasData;

    // Degree reduction with a bounded error in em space for PostScript/CFF outlines.
    /**
     * @brief Approximates a cubic Bezier segment with quadratic curves.
     * @param out Destination curve list to append to.
     * @param a Start point of the cubic Bezier segment.
     * @param b First cubic control point.
     * @param c Second cubic control point.
     * @param d End point of the cubic Bezier segment.
     * @param depth Current subdivision depth; callers normally leave this at zero.
     */
    auto AppendSlugCubic( eastl::vector<SlugCurve>& out, glm::dvec2 a, glm::dvec2 b,
                                 glm::dvec2 c, glm::dvec2 d, unsigned depth = 0 ) -> void;

    // Implements the official Slug texture contract. Header/list rows and each
    // curve's two texels stay on one row because the shader reads x + index.
    /**
     * @brief Adds a glyph's curves and spatial bands to a Slug atlas.
     * @param[in,out] atlas Atlas curve and band data to extend.
     * @param curves Quadratic segments describing the glyph.
     * @param[in,out] glyph Glyph whose atlas offsets and bounds are updated.
     */
    auto AppendSlugGlyph( SlugAtlasData& atlas, eastl::span<const SlugCurve> curves,
                                 renderer::FontGlyph& glyph ) -> void;

    /**
     * @brief Checks for carriage-return or line-feed code points.
     * @param cp Numeric Unicode code point.
     * @returns True for CR or LF.
     */
    auto IsLineBreak( core::u32 cp ) -> bool;

    /**
     * @brief Retrieves the numeric Unicode value of a code point.
     * @param value Value to inspect or edit.
     * @returns Unicode value represented by the input.
     */
    auto CodePointValue( core::u32 value ) -> core::u32;

    /**
     * @brief Retrieves the numeric Unicode value of a code point.
     * @param value Value to inspect or edit.
     * @returns Unicode value represented by the input.
     */
    auto CodePointValue( CodePoint value ) -> core::u32;

    // Walk decoded code points in text order. Advances and pair kerning move
    // the pen; glyph plane bounds produce quads in local coordinates. Complete
    // each line before applying alignment so its final width is known. World
    // text moves the baseline toward negative Y for new lines; screen text uses
    // Y-down coordinates and an ascender offset from its top anchor.
    // https://freetype.org/freetype2/docs/glyphs/glyphs-3.html
    // https://freetype.org/freetype2/docs/glyphs/glyphs-5.html
    /**
     * @brief Computes glyph placement while reusing an existing result buffer.
     * @tparam Point Code-point representation accepted by CodePointValue.
     * @param font Font supplying glyph metrics and kerning.
     * @param codepoints Borrowed Unicode code-point range.
     * @param options Text size, spacing, alignment and coordinate orientation.
     * @param[in,out] result Layout storage to clear and refill, retaining capacity.
     * @param maxGlyphs Maximum number of emitted glyph quads.
     */
    template<typename Point>
    auto LayoutTextInto( const renderer::Font& font, std::span<const Point> codepoints,
                         const LayoutOptions& options, TextLayout& result, core::usize maxGlyphs = 65536 ) -> void {
        MKT_BEGIN_PROFILER();
        // Reuse glyph capacity, resetting all other layout state. Final GPU
        // draw data is copied out before another text uses this scratch space.
        result.mGlyphs.clear();
        result.mMinimum = result.mMaximum = {};
        result.mMissingGlyphs = 0;
        result.mLineCount = 1;
        result.mTruncated = false;
        if ( options.mSize <= 0.0f || options.mLineSpacing <= 0.0f ) {
            return;
        }
        core::f64 x{};
        core::f64 baseline = options.mYDown ? font.GetAscender() * options.mSize : 0.0;
        const core::f64 lineHeight = font.GetLineHeight() * options.mSize * options.mLineSpacing;
        const auto* space = font.FindGlyph( ' ' );
        const core::f64 tabWidth = eastl::max( 0.001, ( space ? space->mAdvanceX : 0.5 ) * options.mSize * 4.0 );
        core::usize lineStart{};
        core::u32 previous{};
        bool hasCharacter{};
        const auto finishLine = [&] {
            const core::f64 width = eastl::max( 0.0, x - ( hasCharacter ? options.mLetterSpacing : 0.0 ) );
            const core::f64 offset = options.mAlignment == Alignment::eCenter ? -width * 0.5 : options.mAlignment == Alignment::eRight ? -width
                                                                                                                                   : 0.0;
            for ( core::usize i = lineStart; i < result.mGlyphs.size(); ++i ) {
                result.mGlyphs[i].mPosition.x += static_cast<core::f32>( offset );
            }
            lineStart = result.mGlyphs.size();
        };
        for ( core::usize i = 0; i < codepoints.size(); ++i ) {
            auto cp = CodePointValue( codepoints[i] );
            if ( IsLineBreak( cp ) ) {
                if ( cp == '\r' && i + 1 < codepoints.size() && CodePointValue( codepoints[i + 1] ) == '\n' ) {
                    ++i;
                }
                finishLine();
                x = 0.0;
                baseline += options.mYDown ? lineHeight : -lineHeight;
                previous = 0;
                hasCharacter = false;
                ++result.mLineCount;
                continue;
            }

            if ( cp == '\t' ) {
                x = ( glm::floor( x / tabWidth ) + 1 ) * tabWidth;
                previous = 0;
                hasCharacter = false;
                continue;
            }

            if ( cp < 32 || cp == 0x7F ) {
                continue;
            }
            const renderer::FontGlyph* glyph{};
            if ( IsEmojiBase( cp ) ) {
                eastl::array<core::u32, kMaxEmojiSequence> sequence{};
                const auto available{ eastl::min( sequence.size(), codepoints.size() - i ) };
                for ( core::usize offset{}; offset < available; ++offset ) {
                    sequence[offset] = CodePointValue( codepoints[i + offset] );
                }

                const auto length{ EmojiSequenceLength( { sequence.data(), available } ) };
                if ( length ) {
                    glyph = font.FindColorGlyph( { sequence.data(), length } );
                    if ( glyph ) {
                        i += length - 1;
                    } else if ( length > 1 && sequence[1] != 0xFE0E ) {
                        glyph = font.FindColorGlyph( { sequence.data(), 1 } );
                    }
                }
            }
            // Presentation selectors and joiners never become visible replacement
            // boxes when an unsupported sequence falls back to separate glyphs.
            if ( !glyph && ( cp == 0xFE0E || cp == 0xFE0F || cp == 0x200D || ( cp >= 0xE0020 && cp <= 0xE007F ) ) ) {
                continue;
            }

            if ( !glyph ) {
                glyph = font.FindGlyph( cp );
            }

            if ( !glyph && cp == 0xA0 ) {
                glyph = space;
            }

            if ( !glyph ) {
                glyph = font.GetFallbackGlyph();
                ++result.mMissingGlyphs;
            }

            if ( !glyph ) {
                x += options.mSize * 0.5;
                previous = 0;
                continue;
            }

            if ( previous && glyph->mImageType == renderer::GlyphImageType::eOutline ) {
                x += font.GetKerning( previous, glyph->mCodepoint ) * options.mSize;
            }

            const auto& bounds = glyph->mPlaneBounds;
            const core::float2 size{ ( bounds.z - bounds.x ) * options.mSize, ( bounds.w - bounds.y ) * options.mSize };
            if ( size.x > 0.0f && size.y > 0.0f ) {
                if ( result.mGlyphs.size() >= maxGlyphs ) {
                    result.mTruncated = true;
                    break;
                }

                result.mGlyphs.push_back( {
                    glyph->mCodepoint,
                    { static_cast<core::f32>( x + bounds.x * options.mSize ),
                        static_cast<core::f32>( baseline + ( options.mYDown ? -bounds.w : bounds.y ) * options.mSize ) },
                    size } );
            }
            x += glyph->mAdvanceX * options.mSize + options.mLetterSpacing;
            previous = glyph->mImageType == renderer::GlyphImageType::eOutline ? glyph->mCodepoint : 0;
            hasCharacter = true;
        }
        finishLine();
        if ( !result.mGlyphs.empty() ) {
            result.mMinimum = result.mGlyphs.front().mPosition;
            result.mMaximum = result.mMinimum;
            for ( const auto& glyph: result.mGlyphs ) {
                result.mMinimum = glm::min( result.mMinimum, glyph.mPosition );
                result.mMaximum = glm::max( result.mMaximum, glyph.mPosition + glyph.mSize );
            }
        }
    }

    /**
     * @brief Computes glyph placement, bounds and missing-glyph statistics.
     * @param font Font supplying glyph metrics and kerning.
     * @param codepoints Borrowed Unicode code-point range.
     * @param options Text size, spacing, alignment and coordinate orientation.
     * @param maxGlyphs Maximum number of emitted glyph quads.
     * @returns Layout for the supplied text and options.
     */
    auto LayoutText( const renderer::Font& font, std::span<const core::u32> codepoints, const LayoutOptions& options, core::usize maxGlyphs = 65536 ) -> TextLayout;

    /**
     * @brief Computes glyph placement, bounds and missing-glyph statistics.
     * @param font Font supplying glyph metrics and kerning.
     * @param contents Unicode text to lay out.
     * @param options Text size, spacing, alignment and coordinate orientation.
     * @param maxGlyphs Maximum number of emitted glyph quads.
     * @returns Layout for the supplied text and options.
     */
    auto LayoutText( const renderer::Font& font, const UnicodeString& contents, const LayoutOptions& options, core::usize maxGlyphs = 65536 ) -> TextLayout;

    // CPU-only generation, shared by the factory and real-font regression tests.
    /**
     * @brief Builds glyphs, metrics and atlas pages for requested code points.
     * @param path Source file path.
     * @param pixelSize Requested glyph size in pixels.
     * @param requested Code points whose glyphs are needed.
     * @param previous Code points whose existing glyphs must be retained.
     * @returns CPU atlas data and glyph metadata.
     */
    auto GenerateFontAtlas(
        const char* path,
        float pixelSize,
        std::span<const core::u32> requested,
        std::span<const core::u32> previous = {} ) -> renderer::FontAtlasData;

}// namespace mikoto::text

#endif// MIKOTO_TEXT_UTILITY_HH
