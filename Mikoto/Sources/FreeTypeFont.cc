//
// Created by zanet on 3/15/2025.
//

#include <ft2build.h>
#include FT_FREETYPE_H

#include "Renderer/Text/FreeTypeFont.hh"
#include "Renderer/Text/FreeTypeManager.hh"

namespace Mikoto {

    FreeTypeFont::FreeTypeFont( const FontLoadInfo &loadInfo )
        : Font{ loadInfo } {
        FT_Library defaultLibrary{ FreeTypeManager::GetLibrary() };

        auto error = FT_New_Face( defaultLibrary, loadInfo.Path.string().c_str(), 0, std::addressof( m_Face ) );
        if ( !error) {
            // Leave width at 0 as it will be computed automatically according to the specified height
            error = FT_Set_Pixel_Sizes( m_Face, 0, static_cast<FT_UInt>( m_Size ) );

            if ( error ) {
                MKT_THROW_RUNTIME_ERROR( "couldn't set pixel sizes" );
            }

        } else {
            MKT_CORE_LOGGER_ERROR( "couldn't set pixel sizes" );
        }
    }

    FreeTypeFont::~FreeTypeFont() {
        FT_Done_Face(m_Face);
    }
}// namespace Mikoto