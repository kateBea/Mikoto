//
// Created by zanet on 3/18/2025.
//

#include "Renderer/Text/FontAtlas.hh"

#include <msdf-atlas-gen/glyph-generators.h>
#include <msdf-atlas-gen/msdf-atlas-gen.h>

#include <utility>


namespace Mikoto {

    using MsdfAtlasGen_T = msdf_atlas::ImmediateAtlasGenerator<
            // pixel type of buffer for individual glyphs depends on generator function
            float,

            // number of atlas color channels
            4,

            // function to generate bitmaps for individual glyphs
            msdf_atlas::mtsdfGenerator,

            // Class that stores the atlas bitmap. For example, a custom atlas storage class that stores it in VRAM can be used.
            msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>>;

    using MsdfGlyphGeometryList_T = std::vector<msdf_atlas::GlyphGeometry>;

    MKT_NODISCARD static auto GenerateAtlas( const CStr_T fontFilename ) -> decltype( auto ) {
        using namespace msdf_atlas;

        std::pair<Scope_T<MsdfAtlasGen_T>, Scope_T<MsdfGlyphGeometryList_T>> result{};
        result.second = CreateScope<MsdfGlyphGeometryList_T>();

        // Initialize instance of FreeType library
        msdfgen::FreetypeHandle *ft{ msdfgen::initializeFreetype() };

        if ( ft != nullptr ) {
            msdfgen::FontHandle *font{ msdfgen::loadFont( ft, fontFilename ) };

            if ( font != nullptr ) {
                // Storage for glyph geometry and their coordinates in the atlas
                MsdfGlyphGeometryList_T &glyphs{ *result.second };

                // FontGeometry is a helper class that loads a set of glyphs from a single font.
                // It can also be used to get additional font metrics, kerning information, etc.
                FontGeometry fontGeometry( &glyphs );

                // Load a set of character glyphs:
                // The second argument can be ignored unless you mix different font sizes in one atlas.
                // In the last argument, you can specify a charset other than ASCII.
                // To load specific glyph indices, use loadGlyphs instead.
                fontGeometry.loadCharset( font, 1.0, Charset::ASCII );

                // Apply MSDF edge coloring. See edge-coloring.h for other coloring strategies.
                constexpr double maxCornerAngle{ 3.0 };

                for ( GlyphGeometry &glyph: glyphs ) {
                    glyph.edgeColoring( &msdfgen::edgeColoringInkTrap, maxCornerAngle, 0 );
                }

                // TightAtlasPacker class computes the layout of the atlas.
                TightAtlasPacker packer{};

                // Set atlas parameters:
                // setDimensions or setDimensionsConstraint to find the best value
                packer.setDimensionsConstraint( DimensionsConstraint::SQUARE );

                // setScale for a fixed size or setMinimumScale to use the largest that fits
                packer.setMinimumScale( 24.0 );

                // setPixelRange or setUnitRange
                packer.setPixelRange( 2.0 );
                packer.setMiterLimit( 1.0 );

                // Compute atlas layout - pack glyphs
                packer.pack( glyphs.data(), glyphs.size() );

                // Get final atlas dimensions
                Int32_T width{};
                Int32_T height{};
                packer.getDimensions( width, height );

                // The ImmediateAtlasGenerator class facilitates the generation of the atlas bitmap.
                result.first = CreateScope<MsdfAtlasGen_T>( width, height );
                auto generator{ *result.first };

                // GeneratorAttributes can be modified to change the generator's default settings.
                GeneratorAttributes attributes{};
                attributes.config.overlapSupport = true;
                attributes.scanlinePass = true;

                generator.setAttributes( attributes );
                generator.setThreadCount( 8 );

                // Generate atlas bitmap
                generator.generate( glyphs.data(), glyphs.size() );

                // Cleanup
                msdfgen::destroyFont( font );
            }

            msdfgen::deinitializeFreetype( ft );
        }

        return result;
    }

    auto FontAtlas::Init() -> void {
        const auto result{ GenerateAtlas( m_Path.string().c_str() ) };

        // Retrieve the generated atlas bitmap
        const msdfgen::BitmapConstRef<msdf_atlas::byte, 4> &atlas{ result.first->atlasStorage() };

        // Match the channel count for bitmap atlas storage
        constexpr auto channelCount{ 4 };
        const auto width = atlas.width;
        const auto height = atlas.height;

        int subpixels = channelCount * width * height;
        std::vector<msdf_atlas::byte> bytePixels( subpixels );
        for ( int i = 0; i < subpixels; ++i )
            bytePixels[i] = msdfgen::pixelFloatToByte( atlas.pixels[i] );

        // Copy the atlas pixel data
        std::vector<UInt8_T> bytes( atlas.pixels, atlas.pixels + ( width * height * channelCount ) );

        // Create texture from the raw atlas data
        Texture2DCreateInfo createInfo{
            .Name{ "Texture Glyph " },
            .Path{ "" },
            .Width{ width },
            .Height{ height },
            .ChannelCount{ channelCount },
            .BufferData{ bytes },
            .Type{ MapType::TEXTURE_2D_TEXT }
        };

        m_Texture = Texture2D::Create( createInfo );
    }
}