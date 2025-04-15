//
// Created by zanet on 3/18/2025.
//

#include <utility>

#include "Renderer/Text/FontAtlas.hh"

#include <msdf-atlas-gen/glyph-generators.h>
#include <msdf-atlas-gen/msdf-atlas-gen.h>


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
        std::vector<Byte_T> Bytes{};

        MsdfGlyphGeometryList_T GlyphData{};
    };

    struct CharsetRange {
        Int32_T Start{};
        Int32_T End{};
    };


    MKT_NODISCARD static auto GenerateAtlas( const CStr_T fontFilename ) -> MsdfData {
        using namespace msdf_atlas;

        MsdfData result{};

        // Initialize instance of FreeType library
        msdfgen::FreetypeHandle *ft{ FontManager::GetMsdfFtHandle() };

        if ( ft != nullptr ) {
            msdfgen::FontHandle *font{ msdfgen::loadFont( ft, fontFilename ) };

            if ( font != nullptr ) {
                // Storage for glyph geometry and their coordinates in the atlas
                MsdfGlyphGeometryList_T glyphs{};

                // FontGeometry is a helper class that loads a set of glyphs from a single font.
                // It can also be used to get additional font metrics, kerning information, etc.
                FontGeometry fontGeometry( std::addressof( glyphs ) );

                // Load a set of character glyphs:

                std::array charsetRanges{
                    CharsetRange{ 0x0020, 0x007F },// Basic Latin
                    CharsetRange{ 0x3040, 0x309F },// Hiragana
                    CharsetRange{ 0x30A0, 0x30FF } // Katakana
                    //CharsetRange{ 0x00A0, 0x00FF },// Latin-1 Supplement
                    //CharsetRange{ 0x0100, 0x017F },// Latin Extended-A
                    //CharsetRange{ 0x0180, 0x024F },// Latin Extended-B
                    //CharsetRange{ 0x0370, 0x03FF },// Greek & Coptic
                    //CharsetRange{ 0x0400, 0x04FF } // Cyrillic
                };

                msdf_atlas::Charset charset{};
                for ( auto [Start, End]: charsetRanges ) {
                    for ( UInt32_T c = Start; c <= End; c++ ) {
                        charset.add( c );
                    }
                }

                // The second argument can be ignored unless you mix different font sizes in one atlas.
                // In the last argument, you can specify a charset other than ASCII.
                // To load specific glyph indices, use loadGlyphs instead.
                fontGeometry.loadCharset( font, 5.0, charset );

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

                // The ImmediateAtlasGenerator class facilitates the generation of the atlas bitmap.
                MsdfAtlasGen_T generator{ width, height };

                generator.setAttributes( attributes );
                generator.setThreadCount( 8 );

                // Generate atlas bitmap
                generator.generate( glyphs.data(), glyphs.size() );

                // Copy the atlas pixel data
                constexpr Int32_T channelCount{ 4 };
                std::vector<Byte_T> bytes{};
                const msdfgen::BitmapConstRef<msdfgen::byte, 4> &bitmapRef{ generator.atlasStorage() };
                for ( Int64_T index{}; index < height * width * channelCount; ++index ) {
                    bytes.emplace_back( static_cast<Byte_T>( bitmapRef.pixels[index] ) );
                }

                result.Bytes = std::move( bytes );
                result.GlyphData = std::move( glyphs );

                // Cleanup
                msdfgen::destroyFont( font );
            }
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
            .Type{ TextureType::TEXTURE_2D_TEXT },
            .Format{ TextureFormat::TEXTURE_FORMAT_RGBA8 }
        };

        m_Texture = Texture2D::Create( createInfo );
    }
}// namespace Mikoto