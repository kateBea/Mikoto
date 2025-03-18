//
// Created by zanet on 3/15/2025.
//

#include <../../Third-Party/freetype/include/ft2build.h>
#include FT_FREETYPE_H

#include <msdfgen/msdfgen.h>
#include <Renderer/Text/import-font.h>

#include <Core/Logging/Logger.hh>
#include <Core/System/RenderSystem.hh>

#include "Renderer/Text/FreeTypeManager.hh"

namespace Mikoto {

    static auto TestMsdfGen() -> void {
        using namespace msdfgen;

        if (FreetypeHandle *ft = initializeFreetype()) {
            if (FontHandle *font = loadFont(ft, "F:\\DEV\\Mikoto\\cmake-build-debug\\Mikoto-Editor\\Resources\\Fonts\\Open_Sans\\OpenSans-VariableFont.ttf")) {
                Shape shape;
                if (loadGlyph(shape, font, 'A', FONT_SCALING_EM_NORMALIZED)) {
                    shape.normalize();
                    //                      max. angle
                    edgeColoringSimple(shape, 3.0);
                    //          output width, height
                    Bitmap<float, 3> msdf(32, 32);
                    //                            scale, translation (in em's)
                    SDFTransformation t(Projection(32.0, Vector2(0.125, 0.125)), Range(0.125));
                    generateMSDF(msdf, shape, t);
                }
                destroyFont(font);
            }
            deinitializeFreetype(ft);
        }
    }

    auto FreeTypeManager::Init() -> void {
        TestMsdfGen();

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