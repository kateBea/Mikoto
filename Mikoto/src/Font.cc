//
// Created by zanet on 3/2/2025.
//

#include "Assets/Font.hh"

#include <Core/Logging/Logger.hh>
#include <Core/System/RenderSystem.hh>

#include "Renderer/Vulkan/VulkanFont.hh"

namespace Mikoto {

    Font::Font( const FontLoadInfo &loadInfo )
        :m_Path{ loadInfo.Path },
            m_Name{ loadInfo.Path.stem().string() },
            m_Size{ loadInfo.Size }
    {
        m_Atlas = CreateScope<FontAtlas>( m_Path );

        if (!m_Atlas) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate memory for font atlas" );
        } else {
            m_Atlas->Init();
        }
    }

    auto Font::Create( const FontLoadInfo &loadInfo ) -> Scope_T<Font>{
        RenderSystem& renderSystem{ Engine::GetSystem<RenderSystem>() };

        switch (renderSystem.GetDefaultApi()) {

            case GraphicsAPI::VULKAN_API:
                return CreateScope<VulkanFont>( loadInfo );
            default:
                MKT_CORE_LOGGER_ERROR( "Font::Create - No font implementation for the given API." );
                break;
        }

        return nullptr;
    }

}