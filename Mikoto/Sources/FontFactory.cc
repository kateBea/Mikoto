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

#include <fmt/format.h>

#include <vector>

// I love Windows.h defining min and max macros that break everything

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <msdf-atlas-gen/glyph-generators.h>

#include <Renderer/Text/Font.hh>
#include <Renderer/Core/FontFactory.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;
    using namespace mikoto::memory;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer::rhi;

    auto FontFactory::GenerateAtlas( eastl::string_view fontFilename, f32 fontSize, bool expensiveColoring, bool customCharset ) const -> FontProperties {
        FontProperties result{};

        if ( !mFreeTypeHandle ) {
            return result;
        }

        msdfgen::FontHandle *font{ msdfgen::loadFont( mFreeTypeHandle, fontFilename.data() ) };
        if ( !font ) {
            return result;
        }

        // Storage for glyph geometry and their coordinates in the atlas
        GeometryList glyphs{};

        std::vector<msdfgen::FontVariationAxis> axes{};
        msdfgen::listFontVariationAxes( axes, mFreeTypeHandle, font );

        // FontGeometry is a helper class that loads a set of glyphs from a single font.
        // It can also be used to get additional font metrics, kerning information, etc.

        std::vector<msdf_atlas::GlyphGeometry> v{ glyphs.begin(), glyphs.end() };
        msdf_atlas::FontGeometry fontGeometry( std::addressof( v ) );

        constexpr float fontScale{ 2.0f };

        if ( customCharset ) {
            // Specify a set of character glyphs
            // https://www.unicode.org/charts/PDF/U3000.pdf
            std::array charsetRanges{
                CharsetRange{ 0x0020, 0x007F },// Basic Latin
                CharsetRange{ 0x3040, 0x309F },// Hiragana
                CharsetRange{ 0x30A0, 0x30FF },// Katakana
                CharsetRange{ 0x00A0, 0x00FF },// Latin-1 Supplement
                //CharsetRange{ 0x4E00, 0x9FFF }, // CJK Unified Ideographs
                //CharsetRange{ 0x0100, 0x017F },// Latin Extended-A
                //CharsetRange{ 0x0180, 0x024F },// Latin Extended-B
                CharsetRange{ 0x0370, 0x03FF },// Greek & Coptic
                CharsetRange{ 0x0400, 0x04FF } // Cyrillic
            };

            msdf_atlas::Charset charset{};
            for ( auto [Start, End]: charsetRanges ) {
                for ( i32 c{ Start }; c <= End; c++ ) {
                    charset.add( c );
                }
            }

            // The second argument can be ignored unless you mix different font sizes in one atlas.
            // In the last argument, you can specify a charset other than ASCII.
            // To load specific glyph indices, use loadGlyphs instead.
            fontGeometry.loadCharset( font, fontScale, charset );
        } else {
            fontGeometry.loadCharset( font, fontScale, msdf_atlas::Charset::ASCII );
        }

        // Apply MSDF edge coloring. See edge-coloring.h for other coloring strategies.
        unsigned long long glyphSeed{ 0 };
        constexpr unsigned long long LCG_MULTIPLIER{ 6364136223846793005ull };

        if ( expensiveColoring ) {
            constexpr u64 coloringSeed{ 0 };
            constexpr unsigned long long LCG_INCREMENT{ 1442695040888963407ull };

            constexpr double maxCornerAngle{ 3.0 };

            msdf_atlas::Workload( [&glyphs, &glyphSeed]( int i, int threadNo ) -> bool {
                glyphSeed = ( LCG_MULTIPLIER * ( coloringSeed ^ i ) + LCG_INCREMENT ) * !!coloringSeed;
                glyphs[i].edgeColoring( msdfgen::edgeColoringInkTrap, maxCornerAngle, glyphSeed );
                return true;
            }, glyphs.size() )
            .finish( std::thread::hardware_concurrency() );
        } else {
            constexpr double maxCornerAngle{ 3.0 };

            for ( msdf_atlas::GlyphGeometry &glyph: glyphs ) {
                glyphSeed *= LCG_MULTIPLIER;
                glyph.edgeColoring( msdfgen::edgeColoringInkTrap, maxCornerAngle, glyphSeed );
            }
        }

        // TightAtlasPacker class computes the layout of the atlas.
        msdf_atlas::TightAtlasPacker packer{};

        // Set atlas parameters:
        // setDimensions or setDimensionsConstraint to find the best value
        packer.setDimensionsConstraint( msdf_atlas::DimensionsConstraint::SQUARE );

        // setScale for a fixed size or setMinimumScale to use the largest that fits
        packer.setScale( fontSize );

        // setPixelRange or setUnitRange
        packer.setPixelRange( 2.0f );
        packer.setMiterLimit( 1.0 );

        // Compute atlas layout - pack glyphs
        packer.pack( glyphs.data(), as<i32>( glyphs.size() ) );

        // Get final atlas dimensions
        i32 width{};
        i32 height{};
        packer.getDimensions( width, height );

        // The ImmediateAtlasGenerator class facilitates the generation of the atlas bitmap.
        AtlasGenerator generator{ width, height };

        // GeneratorAttributes can be modified to change the generator's default settings.
        msdf_atlas::GeneratorAttributes attributes{
            .config = msdfgen::MSDFGeneratorConfig{ true },
            .scanlinePass = true,
        };
        generator.setAttributes( attributes );
        generator.setThreadCount( 8 );

        // Generate atlas bitmap
        generator.generate( glyphs.data(), as<i32>( glyphs.size() ) );

        SubmitAtlasBitmapAndLayout( generator.atlasStorage(), glyphs, result, fontSize );

        // Cleanup
        msdfgen::destroyFont( font );

        return result;
    }

    auto FontFactory::SubmitAtlasBitmapAndLayout( const msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4> &atlas, GeometryList& glyphs, FontProperties &data, i32 fontSize ) const -> void {
        msdfgen::BitmapConstRef bitmap{ ( msdfgen::BitmapConstRef<unsigned char, 4> )atlas };

        i32 atlasWidth{ bitmap.width };
        i32 atlasHeight{ bitmap.height };

        for ( const auto &g: glyphs ) {
            FontGlyph glyphData{ g.getCodepoint() };

            double pLeft, pBottom, pRight, pTop;
            double aLeft, aBottom, aRight, aTop;
            g.getQuadPlaneBounds( pLeft, pBottom, pRight, pTop );
            g.getQuadAtlasBounds( aLeft, aBottom, aRight, aTop );

            glyphData.SetAtlasBounds( { aLeft, aBottom, aRight, aTop } );
            glyphData.SetPlaneBounds( { pLeft, pBottom, pRight, pTop } );
            glyphData.SetAdvanceX( g.getAdvance() );
            glyphData.SetWidth( aRight - aLeft );
            glyphData.SetHeight( aTop - aBottom );
            glyphData.SetBearingUnderline( pBottom * fontSize );

            data.mGlyphMap.insert( { g.getCodepoint(), glyphData } );

            if ( glyphData.mHeight > data.mMaxHeight ) {
                //code = g.getCodepoint();
            }

            data.mMaxHeight = std::max( data.mMaxHeight, as<i32>( as<float>( glyphData.mHeight ) + glyphData.mBearingUnderline ) );
            data.mMaxHeight = std::max( data.mMaxHeight, as<i32>( g.getAdvance() ) );
        }

        // Create texture from the raw atlas data
        constexpr u32 channelCount{ 4 };
        auto textureDesc{ TextureCreateDescription{}
            .SetWidth( atlasWidth )
            .SetHeight( atlasHeight )
            .SetBufferData( BufferSpanHandle::Spawn( rc_cast<byte_t*>( bitmap.pixels ), as<size_t>( atlasWidth * atlasHeight * channelCount ) ) )
            .SetDimensions( TextureDimension::eTexture2D )
            .SetFormat( Format::eRGBA8_UNORM ) };

        data.mAtlas = mDevice->CreateTexture( textureDesc );
    }

    auto FontLoadDescription::SetFile( FileHandle file ) -> FontLoadDescription & {
        mFile = file;
        return *this;
    }

    auto FontLoadDescription::SetSize( float pixelSize ) -> FontLoadDescription & {
        mSize = pixelSize;
        return *this;
    }

    FontFactory::FontFactory( const FontFactoryCreateInfo &options )
        : mDevice( options.mDevice ) {}

    auto FontFactory::Initialize() -> void {
        mFreeTypeHandle = msdfgen::initializeFreetype();

        mIsInitialized = true;
    }

    auto FontFactory::Shutdown() -> void {
        if ( !mIsInitialized ) { return; }

        msdfgen::deinitializeFreetype( mFreeTypeHandle );

        mIsInitialized = false;
    }

    auto FontFactory::LoadFont( const FontLoadDescription &description ) -> FontHandle {
        const eastl::string_view path{ description.mFile->GetPath() };
        const auto result{ GenerateAtlas( path, description.mSize ) };
        //result.TextureAtlas->SetDebugName(string::Format( "FontAtlas {}", description.mFile->GetPath().GetC_Str() ) );

#if false && !defined( MSDFGEN_DISABLE_PNG )
        msdfgen::FontHandle *font = msdfgen::loadFont( mFreeTypeHandle, description.FontFile->GetPath().c_str() );

        msdfgen::Shape shape;
        if (msdfgen::loadGlyph( shape, font, 'C' )) {
            shape.normalize();
            // max. angle
            msdfgen::edgeColoringSimple( shape, 3.0 );
            // image width, height
            msdfgen::Bitmap<float, 4> msdf( 32, 32 );
            // range, scale, translation
            msdfgen::generateMTSDF( msdf, shape, 4.0, 1.0, msdfgen::Vector2( 4.0, 4.0 ) );
            msdfgen::savePng( msdf, "output.png" );
        }
#endif

        // Construct font
        FontHandle newFont{ FontHandle::Spawn( result.mAtlas, description.mSize, description.mFile->GetPath() ) };

        // Fill glyph data
        for ( auto &[codepoint, glyphData]: result.mGlyphMap ) {
            if ( codepoint == 0 ) {
                continue;
            }

            newFont->RegisterGlyph( codepoint, glyphData );
        }

        newFont->SetMaxHeight( result.mMaxHeight );
        newFont->SetMaxWidth( result.mMaxHeight );

        return newFont;
    }
}// namespace mikoto::renderer