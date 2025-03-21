//
// Created by zanet on 3/15/2025.
//

#include <../../Third-Party/freetype/include/ft2build.h>
#include FT_FREETYPE_H

#include <msdf-atlas-gen/msdf-atlas-gen.h>

#include <Core/Logging/Logger.hh>
#include <Core/System/RenderSystem.hh>

#include "Renderer/Text/FreeTypeManager.hh"

namespace Mikoto {


bool generateAtlas(const char *fontFilename) {
    using namespace msdf_atlas;
    bool success = false;
    // Initialize instance of FreeType library
    if (msdfgen::FreetypeHandle *ft = msdfgen::initializeFreetype()) {
        // Load font file
        if (msdfgen::FontHandle *font = msdfgen::loadFont(ft, fontFilename)) {
            // Storage for glyph geometry and their coordinates in the atlas
            std::vector<GlyphGeometry> glyphs;
            // FontGeometry is a helper class that loads a set of glyphs from a single font.
            // It can also be used to get additional font metrics, kerning information, etc.
            FontGeometry fontGeometry(&glyphs);
            // Load a set of character glyphs:
            // The second argument can be ignored unless you mix different font sizes in one atlas.
            // In the last argument, you can specify a charset other than ASCII.
            // To load specific glyph indices, use loadGlyphs instead.
            fontGeometry.loadCharset(font, 1.0, Charset::ASCII);
            // Apply MSDF edge coloring. See edge-coloring.h for other coloring strategies.
            const double maxCornerAngle = 3.0;
            for (GlyphGeometry &glyph : glyphs)
                glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, maxCornerAngle, 0);
            // TightAtlasPacker class computes the layout of the atlas.
            TightAtlasPacker packer;
            // Set atlas parameters:
            // setDimensions or setDimensionsConstraint to find the best value
            packer.setDimensionsConstraint(DimensionsConstraint::SQUARE);
            // setScale for a fixed size or setMinimumScale to use the largest that fits
            packer.setMinimumScale(24.0);
            // setPixelRange or setUnitRange
            packer.setPixelRange(2.0);
            packer.setMiterLimit(1.0);
            // Compute atlas layout - pack glyphs
            packer.pack(glyphs.data(), glyphs.size());
            // Get final atlas dimensions
            int width = 0, height = 0;
            packer.getDimensions(width, height);
            // The ImmediateAtlasGenerator class facilitates the generation of the atlas bitmap.
            ImmediateAtlasGenerator<
                float, // pixel type of buffer for individual glyphs depends on generator function
                3, // number of atlas color channels
                msdfGenerator, // function to generate bitmaps for individual glyphs
                BitmapAtlasStorage<byte, 3> // class that stores the atlas bitmap
                // For example, a custom atlas storage class that stores it in VRAM can be used.
            > generator(width, height);
            // GeneratorAttributes can be modified to change the generator's default settings.
            GeneratorAttributes attributes;
            generator.setAttributes(attributes);
            generator.setThreadCount(4);
            // Generate atlas bitmap
            generator.generate(glyphs.data(), glyphs.size());
            // The atlas bitmap can now be retrieved via atlasStorage as a BitmapConstRef.
            // The glyphs array (or fontGeometry) contains positioning data for typesetting text.
            //success = my_project::submitAtlasBitmapAndLayout(generator.atlasStorage(), glyphs);
            // Cleanup
            msdfgen::destroyFont(font);
        }
        msdfgen::deinitializeFreetype(ft);
    }
    return success;
}


    auto FreeTypeManager::Init() -> void {
    generateAtlas( "F:\\DEV\\Mikoto\\cmake-build-debug\\Mikoto-Editor\\Resources\\Fonts\\Open_Sans\\OpenSans-VariableFont.ttf" );

        // Init Free Ttype Library
        const auto FTInit_Result{ FT_Init_FreeType(std::addressof( m_FreeTypeLibrary )) };

        if (FTInit_Result != 0) {
            MKT_CORE_LOGGER_ERROR( "FreeTypeManager::Init - Could not init FreeType Library");
            return;
        }
    }

    auto FreeTypeManager::Shutdown() -> void {
        if ( FT_Done_FreeType( m_FreeTypeLibrary ) != 0 ) {
            MKT_CORE_LOGGER_ERROR( "FreeTypeManager::Shutdown - Failed to destroy free type library" );
        }
    }

    auto FreeTypeManager::CreateTexture( Int32_T width, Int32_T height, Int32_T channelCount, const std::vector<UInt8_T> &data ) -> Scope_T<Texture> {

        Texture2DCreateInfo createInfo{
            .Name{ "Texture Glyph " },
            .Path{ "" },
            .Width{ width },
            .Height{ height },
            .ChannelCount{ channelCount },
            .BufferData{ data },
            .Type{ MapType::TEXTURE_2D_TEXT }
        };

        return Texture2D::Create( createInfo );
    }
}// namespace Mikoto