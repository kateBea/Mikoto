/**
 * Texture2D.cc
 * Created by kate on 6/8/23.
 * */

// C++ Standard Library

// Project Headers
#include <Core/Engine.hh>
#include <Core/Logging/Logger.hh>
#include <Core/System/RenderSystem.hh>
#include <Library/Utility/Types.hh>
#include <Material/Texture/Texture2D.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanTexture2D.hh>


namespace Mikoto {
    auto Texture2D::Create(const Path_T& path, MapType type) -> Scope_T<Texture2D> {
        auto& renderSystem{ Engine::GetSystem<RenderSystem>() };
        switch(renderSystem.GetDefaultApi()) {
            case GraphicsAPI::VULKAN_API:
                return VulkanTexture2D::Create( VulkanTexture2DCreateInfo{
                    .Path{ path },
                    .Type{ type },
                    .RetainFileData{ false }
                } );
            default:
                MKT_CORE_LOGGER_CRITICAL("Texture2D::Create - Unsupported renderer API");
            break;
        }

        return nullptr;
    }
}