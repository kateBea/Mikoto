/**
 * Material.cc
 * Created by kate on 6/30/23.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include <Core/Logging/Logger.hh>
#include <Library/Utility/Types.hh>
#include <Material/Core/Material.hh>
#include <Core/System/RenderSystem.hh>
#include <Renderer/Vulkan/VulkanStandardMaterial.hh>

namespace Mikoto {

    auto StandardMaterial::Create(const StandardMaterialCreateInfo& createInfo) -> Scope_T<StandardMaterial> {
        switch (Engine::GetSystem<RenderSystem>().GetDefaultApi()) {
            case GraphicsAPI::VULKAN_API:
                return CreateScope<VulkanStandardMaterial>(createInfo);
            default:
                MKT_CORE_LOGGER_ERROR( "Material::CreateStandardMaterial - Trying to create material for non-available graphics API." );
        }

        return nullptr;
    }
}
