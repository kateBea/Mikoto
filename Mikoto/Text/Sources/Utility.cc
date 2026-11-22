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

#include <cmath>
#include <memory>
#include <thread>
#include <algorithm>

#include <EASTL/utility.h>
#include <EASTL/sort.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <hb.h>
#include <hb-ot.h>

#include <msdf-atlas-gen/msdf-atlas-gen.h>

#include <slughorn/freetype.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Text/Utility.hh>
#include <Text/FontAtlas.hh>
#include <Text/TextLayout.hh>

namespace mikoto::text {

    using namespace mikoto::core;


    static auto BuildSlugHornAtlas( const char* path, renderer::FontAtlasData& data ) -> void {
        // Follow SlugHorn's atlas example: load glyphs, freeze the atlas,
        // then upload its raw texture descriptors and per-shape band data.
        // Each page owns its FreeType state so concurrent font jobs are safe.
        FT_Library library{};

        if ( FT_Init_FreeType( &library ) ) {
            MKT_THROW_RUNTIME_ERROR( "Could not initialize FreeType for SlugHorn" );
        }

        eastl::unique_ptr<FT_LibraryRec_, decltype( &FT_Done_FreeType )> libraryOwner{ library, FT_Done_FreeType };

        FT_Face face{};

        if ( FT_New_Face( library, path, 0, &face ) ) {
            MKT_THROW_RUNTIME_ERROR( "Could not open the font for SlugHorn" );
        }

        eastl::unique_ptr<FT_FaceRec_, decltype( &FT_Done_Face )> faceOwner{ face, FT_Done_Face };

        slughorn::Atlas atlas;
        static_assert( slughorn::Atlas::DEFAULT_TEXTURE_WIDTH == 512 );
        static_assert( slughorn::Atlas::INDIRECTION_SIZE == 32 );
        std::vector<uint32_t> codepoints;
        codepoints.reserve( data.glyphs.size() );

        for ( const auto& [codepoint, glyph]: data.glyphs ) {
            if ( codepoint ) {
                codepoints.push_back( codepoint );
            }
        }

        slughorn::freetype::loadGlyphs( face, codepoints, atlas );

        // Codepoint zero is Mikoto's .notdef fallback, not a charmap entry.
        // Preserve it by explicitly decomposing glyph index zero.
        if ( data.glyphs.contains( 0 ) && face->units_per_EM &&
             FT_Load_Glyph( face, 0, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING | FT_LOAD_NO_BITMAP ) == 0 ) {
            slughorn::Atlas::ShapeInfo fallback;
            const auto scale = slughorn::slug_t{ 1 } / face->units_per_EM;

            if ( slughorn::freetype::decomposeGlyph( face, scale, face->glyph->advance.x * scale, nullptr, fallback ) ) {
                atlas.addShape( uint32_t{ 0 }, fallback );
            }
        }

        atlas.build();

        const auto copyTexture = []( const slughorn::Atlas::TextureData& source,
                                     slughorn::Atlas::TextureData::Format expected ) -> renderer::SlugHornTextureData {
            if ( source.empty() ) {
                return {};
            }

            if ( source.format != expected || source.depth || source.width > 4096 || source.height > 4096 ) {
                MKT_THROW_RUNTIME_ERROR( "Unsupported SlugHorn texture page" );
            }

            renderer::SlugHornTextureData texture;
            texture.mWidth = source.width;
            texture.mHeight = source.height;
            texture.mPixels.assign( source.bytes.data(), source.bytes.data() + source.bytes.size() );
            return texture;
        };
        using Format = slughorn::Atlas::TextureData::Format;
        data.slug.mCurves = copyTexture( atlas.getCurveTextureData(), Format::RGBA32F );
        data.slug.mBands = copyTexture( atlas.getBandTextureData(), Format::RG16UI );

        // SlugHorn's scanline-sweeper data drops horizontal segments. Stroke
        // distances need those segments too, so upload its retained original
        // curves as a simple two-texel list. No decomposition is duplicated.
        eastl::vector<core::float4> strokeCurves;

        for ( auto& [codepoint, glyph]: data.glyphs ) {
            const auto shape = atlas.getShape( codepoint );

            if ( !shape ) {
                continue;// Whitespace has advance but no vector geometry.
            }

            glyph.mSlugBandTransform = { shape->bandScaleX, shape->bandScaleY, shape->bandOffsetX, shape->bandOffsetY };
            glyph.mSlugGlyph[0] = shape->bandTexX;
            glyph.mSlugGlyph[1] = shape->bandTexY;
            glyph.mSlugGlyph[2] = shape->bandMaxX;
            glyph.mSlugGlyph[3] = shape->bandMaxY;
            glyph.mSlugStrokeStart = core::as<core::u32>( strokeCurves.size() );
            glyph.mSlugStrokeCount = core::as<core::u32>( shape->curves.size() );

            for ( const auto& curve: shape->curves ) {
                strokeCurves.push_back( { curve.x1, curve.y1, curve.x2, curve.y2 } );
                strokeCurves.push_back( { curve.x3, curve.y3, 0, 0 } );
            }
        }

        if ( !strokeCurves.empty() ) {
            auto& stroke = data.slug.mStrokeCurves;
            stroke.mWidth = slughorn::Atlas::DEFAULT_TEXTURE_WIDTH;
            stroke.mHeight = core::as<core::u32>( ( strokeCurves.size() + stroke.mWidth - 1 ) / stroke.mWidth );

            if ( stroke.mHeight > 4096 ) {
                MKT_THROW_RUNTIME_ERROR( "SlugHorn stroke texture page exceeds device limits" );
            }

            const auto* bytes = reinterpret_cast<const core::ubyte*>( strokeCurves.data() );
            stroke.mPixels.assign( bytes, bytes + strokeCurves.size() * sizeof( core::float4 ) );
            stroke.mPixels.resize( core::usize{ stroke.mWidth } * stroke.mHeight * sizeof( core::float4 ), 0 );
        }
    }

    auto GenerateFontAtlas( const char* path, float pixelSize, std::span<const core::u32> requested,
        std::span<const core::u32> previous ) -> renderer::FontAtlasData {
        MKT_BEGIN_PROFILER();
        renderer::FontAtlasData result;

        if ( !path || pixelSize <= 0.0f ) {
            return result;
        }

        // Each generation owns its FreeType library, so asynchronous font loads do not share FT state.
        eastl::unique_ptr<msdfgen::FreetypeHandle, decltype( &msdfgen::deinitializeFreetype )>
            library{ msdfgen::initializeFreetype(), msdfgen::deinitializeFreetype };

        if ( !library ) {
            return result;
        }

        eastl::unique_ptr<msdfgen::FontHandle, decltype( &msdfgen::destroyFont )>
            font{ msdfgen::loadFont( library.get(), path ), msdfgen::destroyFont };

        if ( !font ) {
            return result;
        }

        std::vector<msdf_atlas::GlyphGeometry> glyphs;
        msdf_atlas::FontGeometry geometry{ &glyphs };
        msdf_atlas::Charset charset;

        for ( auto cp: requested ) {
            charset.add( cp );
        }

        geometry.loadCharset( font.get(), 1.0, charset, true, false );

        if ( previous.empty() ) {
            geometry.loadGlyphRange( font.get(), 1.0, 0, 1, true, false );
        }

        if ( glyphs.empty() ) {
            return result;
        }
        // Reserve distance-field padding for up to 1/8 em of outside outline
        // plus an antialiasing texel. Both text backends share these bounds.
        result.distanceRange = static_cast<float>( eastl::max( 4.0, glm::ceil( pixelSize * 0.25 + 2.0 ) ) );
        result.lineHeight = geometry.getMetrics().lineHeight;
        result.ascender = geometry.getMetrics().ascenderY;

        for ( auto& glyph: glyphs ) {
            // Separate nearby edges into different channels, including complex
            // glyphs. This costs generation time only; sampling is unchanged.
            glyph.edgeColoring( msdfgen::edgeColoringByDistance, 3.0, 0 );
        }

        msdf_atlas::TightAtlasPacker packer;
        packer.setDimensionsConstraint( msdf_atlas::DimensionsConstraint::SQUARE );
        packer.setScale( pixelSize );
        packer.setPixelRange( result.distanceRange );
        packer.setMiterLimit( 1.0 );

        if ( packer.pack( glyphs.data(), static_cast<int>( glyphs.size() ) ) != 0 ) {
            return {};
        }

        packer.getDimensions( result.width, result.height );
        // A page containing only spaces still carries metrics, but has no packed rectangles.
        // Keep a valid minimal texture so these advances can join the immutable page cache.
        if ( eastl::all_of( glyphs.begin(), glyphs.end(), []( const auto& glyph ) {
            return glyph.isWhitespace();
        } ) ) {
            result.width = result.height = 1;
        }

        if ( result.width <= 0 || result.height <= 0 || result.width > 4096 || result.height > 4096 ) {
            MKT_THROW_RUNTIME_ERROR( "Font atlas page exceeds the 4096 pixel limit" );
        }

        using Generator = msdf_atlas::ImmediateAtlasGenerator<float, 4, msdf_atlas::mtsdfGenerator,
            msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>>;
        Generator generator{ result.width, result.height };
        msdf_atlas::GeneratorAttributes attributes;
        attributes.config = msdfgen::MSDFGeneratorConfig{ true };
        attributes.scanlinePass = true;
        generator.setAttributes( attributes );
        generator.setThreadCount( eastl::max( 1u, eastl::min( 8u, std::thread::hardware_concurrency() ) ) );
        generator.generate( glyphs.data(), static_cast<int>( glyphs.size() ) );
        const msdfgen::BitmapConstRef<unsigned char, 4> bitmap = generator.atlasStorage();
        result.pixels.assign( bitmap.pixels, bitmap.pixels + size_t( result.width ) * result.height * 4 );

        for ( const auto& glyph: glyphs ) {
            renderer::FontGlyph value{ glyph.getCodepoint() };
            double l{};
            double b{};
            double r{};
            double t{};
            glyph.getQuadPlaneBounds( l, b, r, t );
            value.mPlaneBounds = { l, b, r, t };
            glyph.getQuadAtlasBounds( l, b, r, t );
            value.mAtlasBounds = { l, b, r, t };
            value.mWidth = r - l;
            value.mHeight = t - b;
            value.mAdvanceX = glyph.getAdvance();
            result.glyphs.emplace( value.mCodepoint, value );
        }

        BuildSlugHornAtlas( path, result );

        // Include pairs across atlas pages; kerning is a font metric, not an atlas metric.
        ankerl::unordered_dense::map<core::u32, msdfgen::GlyphIndex> indices;

        for ( auto cp: previous ) {
            msdfgen::GlyphIndex index;

            if ( msdfgen::getGlyphIndex( index, font.get(), cp ) ) {
                indices.emplace( cp, index );
            }
        }

        for ( const auto& [cp, glyph]: result.glyphs ) {
            msdfgen::GlyphIndex index;

            if ( cp && msdfgen::getGlyphIndex( index, font.get(), cp ) ) {
                indices.emplace( cp, index );
            }
        }

        const auto addPair = [&]( core::u32 left, core::u32 right, msdfgen::GlyphIndex a, msdfgen::GlyphIndex b ) {
            double adjustment{};

            if ( msdfgen::getKerning( adjustment, font.get(), a, b, msdfgen::FONT_SCALING_EM_NORMALIZED ) && adjustment != 0.0 ) {
                result.kerning[( core::u64{ left } << 32 ) | right] = adjustment;
            }
        };

        for ( const auto& [cp, glyph]: result.glyphs ) {
            const auto current = indices.find( cp );

            if ( current == indices.end() ) {
                continue;
            }

            for ( const auto& [other, index]: indices ) {
                addPair( cp, other, current->second, index );
                addPair( other, cp, index, current->second );
            }
        }

        return result;
    }


    auto ConvertFromUtf8( eastl::string_view utf8 ) -> UnicodeString {
        MKT_BEGIN_PROFILER();
        return UnicodeString{ utf8 };
    }

    auto GetUnicodeFromUtf8( eastl::string_view utf8 ) -> eastl::vector<core::u32> {
        MKT_BEGIN_PROFILER();
        eastl::vector<core::u32> codepoints{};
        codepoints.reserve( utf8.size() );

        ForEachUtf8CodePoint( utf8,
            [&]( core::u32 point ) {
            codepoints.push_back( point );
        } );

        return codepoints;
    }

    auto ScreenTextModel( const ScreenPlacement& placement ) -> core::float4x4 {
        MKT_BEGIN_PROFILER();
        const float c = glm::cos( placement.mRotation );
        const float s = glm::sin( placement.mRotation );
        core::float4x4 result{ 1.0f };
        result[0] = { c * placement.mScale.x, s * placement.mScale.x, 0, 0 };
        result[1] = { -s * placement.mScale.y, c * placement.mScale.y, 0, 0 };
        result[3] = { placement.mPosition, 0, 1 };
        return result;
    }

    auto ScreenTextProjection( float width, float height ) -> core::float4x4 {
        MKT_BEGIN_PROFILER();
        core::float4x4 result{ 0.0f };
        result[0][0] = 2.0f / eastl::max( width, 1.0f );
        result[1][1] = -2.0f / eastl::max( height, 1.0f );
        result[3] = { -1, 1, 0.5f, 1 };
        return result;
    }

    auto SetScreenTextModel( ScreenPlacement& placement, const core::float4x4& model ) -> bool {
        MKT_BEGIN_PROFILER();
        const float sx = glm::length( core::float2{ model[0] } );

        if ( sx < 0.00001f ) {
            return false;
        }

        const float sy = ( model[0][0] * model[1][1] - model[0][1] * model[1][0] ) / sx;

        if ( glm::abs( sy ) < 0.00001f ) {
            return false;
        }

        placement.mPosition = core::float2{ model[3] };
        placement.mScale = { sx, sy };
        placement.mRotation = glm::atan2( model[0][1], model[0][0] );
        return true;
    }


    struct FontResources {
        FT_Library mLibrary{};
        FT_Face mFace{};
        hb_blob_t* mBlob{};
        hb_face_t* mShapingFace{};
        hb_font_t* mFont{};
        hb_buffer_t* mBuffer{};

        ~FontResources() {
            if ( mBuffer ) {
                hb_buffer_destroy( mBuffer );
            }

            if ( mFont ) {
                hb_font_destroy( mFont );
            }

            if ( mShapingFace ) {
                hb_face_destroy( mShapingFace );
            }

            if ( mBlob ) {
                hb_blob_destroy( mBlob );
            }

            if ( mFace ) {
                FT_Done_Face( mFace );
            }

            if ( mLibrary ) {
                FT_Done_FreeType( mLibrary );
            }
        }

        auto Open( const char* path ) -> bool {
            return path && !FT_Init_FreeType( &mLibrary ) && !FT_New_Face( mLibrary, path, 0, &mFace );
        }
    };


    auto IsEmojiBase( core::u32 cp ) -> bool {
        return ( cp >= 0x1F000 && cp <= 0x1FAFF ) || ( cp >= 0x2600 && cp <= 0x27BF ) ||
               ( cp >= 0x2194 && cp <= 0x21FF ) || ( cp >= 0x2300 && cp <= 0x23FF ) ||
               ( cp >= 0x2B00 && cp <= 0x2BFF ) || cp == 0xA9 || cp == 0xAE || cp == 0x203C ||
               cp == 0x2049 || cp == 0x2122 || cp == 0x2139 || cp == 0x3030 || cp == 0x303D ||
               cp == 0x3297 || cp == 0x3299 || cp == '#' || cp == '*' || ( cp >= '0' && cp <= '9' );
    }

    auto EmojiSequenceLength( std::span<const core::u32> points ) -> core::usize {
        if ( points.empty() || !IsEmojiBase( points.front() ) ) {
            return 0;
        }

        const core::usize limit{ eastl::min( points.size(), kMaxEmojiSequence ) };
        core::usize length{ 1 };

        if ( points[0] >= 0x1F1E6 && points[0] <= 0x1F1FF && limit > 1 &&
             points[1] >= 0x1F1E6 && points[1] <= 0x1F1FF ) {
            ++length;
        }
        while ( length < limit ) {
            const auto cp{ points[length] };

            if ( cp == 0xFE0F || cp == 0xFE0E || cp == 0x20E3 || ( cp >= 0x1F3FB && cp <= 0x1F3FF ) ||
                 ( cp >= 0xE0020 && cp <= 0xE007F ) ) {
                ++length;
            }
            else if ( cp == 0x200D && length + 1 < limit && IsEmojiBase( points[length + 1] ) ) {
                length += 2;
            }
            else {
                break;
            }
        }
        // Digits and punctuation become emoji only in a keycap sequence.
        if ( points[0] < 128 && points[length - 1] != 0x20E3 ) {
            return 0;
        }

        return length;
    }

    auto HasColorGlyphs( const char* path ) -> bool {
        FontResources resources{};
        return resources.Open( path ) && FT_HAS_COLOR( resources.mFace );
    }

    auto GenerateColorGlyphAtlas( const char* path, core::f32 pixelSize,
        std::span<const ColorGlyphRequest> requests ) -> renderer::FontAtlasData {
        renderer::FontAtlasData result{};
        FontResources resources{};

        if ( pixelSize <= 0 || requests.empty() ||
             !resources.Open( path ) || !FT_HAS_COLOR( resources.mFace ) ) {
            return result;
        }

        auto* face{ resources.mFace };

        if ( FT_Set_Pixel_Sizes( face, 0, core::as<core::u32>( eastl::clamp( pixelSize, 8.0f, 256.0f ) ) ) ) {
            // Bitmap-only fonts expose discrete strikes. Normalize their metrics
            // by the chosen strike, rather than treating a 109px emoji as 48px.
            if ( face->num_fixed_sizes <= 0 ) {
                return result;
            }

            core::i32 selected{};

            for ( core::i32 i{ 1 }; i < face->num_fixed_sizes; ++i ) {
                if ( glm::abs( face->available_sizes[i].y_ppem / 64.0 - pixelSize ) <
                     glm::abs( face->available_sizes[selected].y_ppem / 64.0 - pixelSize ) ) {
                    selected = i;
                }
            }

            if ( FT_Select_Size( face, selected ) ) {
                return result;
            }
        }

        const core::f32 em{ core::as<core::f32>( face->size->metrics.y_ppem ) };

        if ( em <= 0 ) {
            return result;
        }

        resources.mBlob = hb_blob_create_from_file_or_fail( path );

        if ( !resources.mBlob ) {
            return result;
        }

        resources.mShapingFace = hb_face_create( resources.mBlob, 0 );
        resources.mFont = hb_font_create( resources.mShapingFace );
        hb_ot_font_set_funcs( resources.mFont );
        resources.mBuffer = hb_buffer_create();

        // A bounded batch and shelf packing keep temporary CPU/GPU allocations
        // predictable. One transparent texel separates neighbouring glyphs.
        constexpr core::u32 width{ 2048 };
        constexpr core::u32 maximumHeight{ 4096 };
        result.width = width;
        result.lineHeight = face->size->metrics.height / ( 64.0 * em );
        result.ascender = face->size->metrics.ascender / ( 64.0 * em );
        core::u32 x{ 1 };
        core::u32 y{ 1 };
        core::u32 rowHeight{};

        for ( const auto& request: requests ) {
            if ( request.mSequence.empty() || request.mSequence.size() > kMaxEmojiSequence ||
                 eastl::find( request.mSequence.begin(), request.mSequence.end(), 0xFE0Eu ) != request.mSequence.end() ) {
                continue;
            }

            hb_buffer_reset( resources.mBuffer );
            hb_buffer_set_flags( resources.mBuffer, HB_BUFFER_FLAG_REMOVE_DEFAULT_IGNORABLES );
            hb_buffer_add_utf32( resources.mBuffer, request.mSequence.data(), core::as<core::i32>( request.mSequence.size() ), 0, core::as<core::i32>( request.mSequence.size() ) );
            hb_buffer_guess_segment_properties( resources.mBuffer );
            hb_shape( resources.mFont, resources.mBuffer, nullptr, 0 );
            core::u32 count{};
            const auto* glyphs{ hb_buffer_get_glyph_infos( resources.mBuffer, &count ) };
            // Keep unsupported combinations decomposable instead of caching a
            // misleading replacement glyph as a successfully joined emoji.
            if ( count != 1 || !glyphs[0].codepoint ||
                 FT_Load_Glyph( face, glyphs[0].codepoint, FT_LOAD_COLOR | FT_LOAD_RENDER ) ) {
                continue;
            }

            const auto& bitmap{ face->glyph->bitmap };

            if ( bitmap.pixel_mode != FT_PIXEL_MODE_BGRA || !bitmap.width || !bitmap.rows || bitmap.width + 2 > width ) {
                continue;
            }

            if ( x + bitmap.width + 1 > width ) {
                x = 1;
                y += rowHeight + 2;
                rowHeight = 0;
            }

            if ( y + bitmap.rows + 1 > maximumHeight ) {
                break;
            }

            rowHeight = eastl::max( rowHeight, bitmap.rows );
            result.height = y + rowHeight + 1;
            result.pixels.resize( core::usize{ width } * result.height * 4, 0 );

            for ( core::u32 row{}; row < bitmap.rows; ++row ) {
                const auto* source{ bitmap.buffer + ( bitmap.pitch >= 0 ? row : bitmap.rows - 1 - row ) * glm::abs( bitmap.pitch ) };
                auto* target{ result.pixels.data() + ( core::usize{ y + bitmap.rows - 1 - row } * width + x ) * 4 };

                for ( core::u32 column{}; column < bitmap.width; ++column ) {
                    target[4 * column] = source[4 * column + 2];
                    target[4 * column + 1] = source[4 * column + 1];
                    target[4 * column + 2] = source[4 * column];
                    target[4 * column + 3] = source[4 * column + 3];
                }
            }

            renderer::FontGlyph glyph{ request.mGlyphKey };
            glyph.mImageType = renderer::GlyphImageType::eColor;
            glyph.mAdvanceX = face->glyph->advance.x / ( 64.0 * em );
            glyph.mPlaneBounds = { face->glyph->bitmap_left / em, ( face->glyph->bitmap_top - core::as<core::i32>( bitmap.rows ) ) / em,
                ( face->glyph->bitmap_left + core::as<core::i32>( bitmap.width ) ) / em, face->glyph->bitmap_top / em };
            glyph.mAtlasBounds = { x, y, x + bitmap.width, y + bitmap.rows };
            glyph.mWidth = bitmap.width;
            glyph.mHeight = bitmap.rows;
            result.glyphs.emplace( request.mGlyphKey, glyph );
            x += bitmap.width + 2;
        }

        return result;
    }

    auto AppendSlugCubic( eastl::vector<SlugCurve>& out, glm::dvec2 a, glm::dvec2 b, glm::dvec2 c, glm::dvec2 d, unsigned depth ) -> void {
        MKT_BEGIN_PROFILER();
        constexpr double tolerance = 1.0 / 16384.0;
        const auto error = glm::length( -a + 3.0 * b - 3.0 * c + d ) / ( 12.0 * glm::sqrt( 3.0 ) );

        if ( error <= tolerance || depth == 16 ) {
            out.push_back( { a, ( -a + 3.0 * b + 3.0 * c - d ) * 0.25, d } );
            return;
        }

        const auto ab = ( a + b ) * 0.5;
        const auto bc = ( b + c ) * 0.5;
        const auto cd = ( c + d ) * 0.5;
        const auto abc = ( ab + bc ) * 0.5;
        const auto bcd = ( bc + cd ) * 0.5;
        const auto middle = ( abc + bcd ) * 0.5;
        AppendSlugCubic( out, a, ab, abc, middle, depth + 1 );
        AppendSlugCubic( out, middle, bcd, cd, d, depth + 1 );
    }

    auto AppendSlugGlyph( SlugAtlasData& atlas, eastl::span<const SlugCurve> curves, renderer::FontGlyph& glyph ) -> void {
        MKT_BEGIN_PROFILER();

        if ( curves.empty() ) {
            return;
        }

        if ( curves.size() > kSlugTextureWidth ) {
            MKT_THROW_RUNTIME_ERROR( "Slug glyph exceeds 4096 curves" );
        }

        using namespace core;
        eastl::vector<core::u32> locations{};
        locations.reserve( curves.size() );

        core::float2 lo = curves.front().p0;
        core::float2 hi = lo;

        for ( const auto& curve: curves ) {
            if ( atlas.mCurves.size() % kSlugTextureWidth == kSlugTextureWidth - 1 ) {
                atlas.mCurves.push_back( {} );
            }

            locations.push_back( static_cast<core::u32>( atlas.mCurves.size() ) );
            atlas.mCurves.push_back( { curve.p0, curve.p1 } );
            atlas.mCurves.push_back( { curve.p2, 0, 0 } );

            for ( const auto& p: { curve.p0, curve.p1, curve.p2 } ) {
                lo = glm::min( lo, p );
                hi = glm::max( hi, p );
            }
        }

        const core::u32 bandCount{ glm::clamp( static_cast<core::u32>( glm::ceil( glm::sqrt( double( curves.size() ) ) ) ), 1u, 16u ) };

        const auto reserveRow = [&]( core::usize count ) {
            const core::usize column = atlas.mBands.size() % kSlugTextureWidth;

            if ( column + count > kSlugTextureWidth ) {
                atlas.mBands.resize( atlas.mBands.size() + kSlugTextureWidth - column );
            }

            const core::usize offset{ atlas.mBands.size() };
            atlas.mBands.resize( offset + count );
            return offset;
        };

        // Keep the reference fill headers first. A third set of horizontal bands
        // includes horizontal/degenerate edges as well, for distance-based strokes.
        const core::usize header = reserveRow( bandCount * 3 );
        glyph.mSlugGlyph[0] = static_cast<core::u32>( header % kSlugTextureWidth );
        glyph.mSlugGlyph[1] = static_cast<core::u32>( header / kSlugTextureWidth );
        glyph.mSlugGlyph[2] = glyph.mSlugGlyph[3] = bandCount - 1;
        const auto extent{ glm::max( hi - lo, core::float2{ 1e-6f } ) };
        glyph.mSlugBandTransform = { float( bandCount ) / extent.x, float( bandCount ) / extent.y,
            -lo.x * bandCount / extent.x, -lo.y * bandCount / extent.y };

        for ( core::i32 axis{}; axis < 3; ++axis ) {
            // Horizontal ray bands split y; vertical ray bands split x.
            const int split = axis == 1 ? 0 : 1;
            const int sort = 1 - split;

            for ( core::u32 band = 0; band < bandCount; ++band ) {
                const float low = lo[split] + extent[split] * band / bandCount - 1.0f / 1024;
                const float high = lo[split] + extent[split] * ( band + 1 ) / bandCount + 1.0f / 1024;
                eastl::vector<core::u32> indices{};

                for ( core::u32 i = 0; i < curves.size(); ++i ) {
                    const auto& c = curves[i];
                    const float minimum = eastl::min( { c.p0[split], c.p1[split], c.p2[split] } );
                    const float maximum = eastl::max( { c.p0[split], c.p1[split], c.p2[split] } );

                    if ( ( axis == 2 || minimum != maximum ) && maximum >= low && minimum <= high ) {
                        indices.push_back( i );
                    }
                }

                eastl::sort( indices.begin(), indices.end(), [&]( core::u32 a, core::u32 b ) {
                    const auto& x = curves[a];
                    const auto& y = curves[b];
                    return eastl::max( { x.p0[sort], x.p1[sort], x.p2[sort] } ) > eastl::max( { y.p0[sort], y.p1[sort], y.p2[sort] } );
                } );
                const core::usize list = reserveRow( indices.size() );
                atlas.mBands[header + axis * bandCount + band] = { static_cast<core::u32>( indices.size() ), static_cast<core::u32>( list - header ) };

                for ( core::usize j = 0; j < indices.size(); ++j ) {
                    const core::usize address = locations[indices[j]];
                    atlas.mBands[list + j] = { static_cast<core::u32>( address % kSlugTextureWidth ), static_cast<core::u32>( address / kSlugTextureWidth ) };
                }
            }
        }
    }

    auto IsLineBreak( core::u32 cp ) -> bool {
        MKT_BEGIN_PROFILER();
        return cp == '\r' || cp == '\n' || cp == 0x85 || cp == 0x2028 || cp == 0x2029;
    }

    auto CodePointValue( core::u32 value ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        return value;
    }

    auto CodePointValue( CodePoint value ) -> core::u32 {
        MKT_BEGIN_PROFILER();
        return value.GetValue();
    }

    auto LayoutText( const renderer::Font& font, std::span<const core::u32> codepoints, const LayoutOptions& options, core::usize maxGlyphs ) -> TextLayout {
        MKT_BEGIN_PROFILER();
        TextLayout result;
        LayoutTextInto( font, codepoints, options, result, maxGlyphs );
        return result;
    }

    auto LayoutText( const renderer::Font& font, const UnicodeString& contents, const LayoutOptions& options, core::usize maxGlyphs ) -> TextLayout {
        MKT_BEGIN_PROFILER();
        TextLayout result;
        const auto points{ contents.GetCodePoints() };
        LayoutTextInto( font, std::span<const CodePoint>{ points.data(), points.size() }, options, result, maxGlyphs );
        return result;
    }

    auto AtlasTextureBindings::Clear() -> void {
        MKT_BEGIN_PROFILER();
        mBindings.clear();
    }

}// namespace mikoto::text
