//
// Created by zanet on 3/2/2025.
//

#include "Assets/Font.hh"

#include <Core/Logging/Logger.hh>
#include <Core/System/RenderSystem.hh>

#include "Renderer/Vulkan/VulkanFont.hh"

namespace Mikoto {

    Font::Font( const FontLoadInfo &loadInfo ) {

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