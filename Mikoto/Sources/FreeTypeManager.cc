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

    auto FreeTypeManager::Init() -> void {
        // Init Free Ttype Library
        const auto FTInit_Result{ FT_Init_FreeType( std::addressof( m_FreeTypeLibrary ) ) };

        if ( FTInit_Result != 0 ) {
            MKT_CORE_LOGGER_ERROR( "FreeTypeManager::Init - Could not init FreeType Library" );
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