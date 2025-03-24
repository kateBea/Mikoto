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

    struct MsdfData {
        Int32_T Width{};
        Int32_T Height{};
        std::vector<UInt8_T> Bytes{};

        MsdfGlyphGeometryList_T GlyphData{};
    };


    MKT_NODISCARD static auto GenerateAtlas( const CStr_T fontFilename ) -> MsdfData {
        using namespace msdf_atlas;

        MsdfData result{};

        // Initialize instance of FreeType library
        msdfgen::FreetypeHandle *ft{ msdfgen::initializeFreetype() };

        if ( ft != nullptr ) {
            msdfgen::FontHandle *font{ msdfgen::loadFont( ft, fontFilename ) };

            if ( font != nullptr ) {
                // Storage for glyph geometry and their coordinates in the atlas
                MsdfGlyphGeometryList_T glyphs{};

                // FontGeometry is a helper class that loads a set of glyphs from a single font.
                // It can also be used to get additional font metrics, kerning information, etc.
                FontGeometry fontGeometry( &glyphs );

                // Load a set of character glyphs:
                // The second argument can be ignored unless you mix different font sizes in one atlas.
                // In the last argument, you can specify a charset other than ASCII.
                // To load specific glyph indices, use loadGlyphs instead.
                fontGeometry.loadCharset( font, 5.0, Charset::ASCII );

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
                packer.pack( glyphs.data(), static_cast<int>( glyphs.size() ) );

                // Get final atlas dimensions
                Int32_T width{};
                Int32_T height{};
                packer.getDimensions( width, height );

                result.Width = width;
                result.Height = height;

                // GeneratorAttributes can be modified to change the generator's default settings.
                GeneratorAttributes attributes{
                    .config{ msdfgen::MSDFGeneratorConfig{ true } },
                    .scanlinePass{ true }
                };

                if (attributes.scanlinePass) {
                    attributes.config.errorCorrection.mode = msdfgen::ErrorCorrectionConfig::DISABLED;
                }

                // The ImmediateAtlasGenerator class facilitates the generation of the atlas bitmap.
                MsdfAtlasGen_T generator{ width, height  };

                generator.setAttributes( attributes );
                generator.setThreadCount( 8 );

                // Generate atlas bitmap
                generator.generate( glyphs.data(), glyphs.size() );


                // Copy the atlas pixel data
                std::vector<UInt8_T> bytes{};
                const msdfgen::BitmapConstRef<msdfgen::byte, 4>& bitmapRef{ generator.atlasStorage() };
                for (Int64_T y{}; y < height*width*4; ++y) {
                    bytes.emplace_back( static_cast<UInt8_T>( bitmapRef.pixels[y] ) );
                }

                result.Bytes = std::move( bytes );
                result.GlyphData = std::move( glyphs );

                // Cleanup
                msdfgen::destroyFont( font );
            }

            msdfgen::deinitializeFreetype( ft );
        }

        return result;
    }

    auto FontAtlas::Init() -> void {
        auto result{ GenerateAtlas( m_Path.string().c_str() ) };

        // Match the channel count for bitmap atlas storage
        constexpr auto channelCount{ 4 };
        const Int32_T width{ result.Width };
        const Int32_T height{ result.Height };

        // Create texture from the raw atlas data
        Texture2DCreateInfo createInfo{
            .Name{ "Texture Glyph " },
            .Path{ "" },
            .Width{ width },
            .Height{ height },
            .ChannelCount{ channelCount },
            .BufferData{ std::move( result.Bytes ) },
            .Type{ MapType::TEXTURE_2D_TEXT }
        };

        m_Texture = Texture2D::Create( createInfo );
    }
}