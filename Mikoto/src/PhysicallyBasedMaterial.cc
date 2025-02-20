//
// Created by kate on 11/11/23.
//

#include <Core/System/RenderSystem.hh>
#include <Material/Material/PBRMaterial.hh>
#include <Renderer/Vulkan/VulkanPBRMaterial.hh>

namespace Mikoto {
    auto PBRMaterial::Create( const PBRMaterialCreateSpec& createInfo ) -> Scope_T<PBRMaterial> {
        switch (Engine::GetSystem<RenderSystem>().GetDefaultApi()) {
            case GraphicsAPI::VULKAN_API:
                return CreateScope<VulkanPBRMaterial>(createInfo);
            default:
                MKT_CORE_LOGGER_ERROR( "Material::Create - Trying to create material for non-available graphics API." );
        }

        return nullptr;
    }

}