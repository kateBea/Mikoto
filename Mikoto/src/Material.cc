/**
 * Material.cc
 * Created by kate on 6/30/23.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include <Material/Core/Material.hh>
#include <Renderer/Core/Renderer.hh>

#include "Common/RenderingUtils.hh"
#include "Models/StandardMaterialCreateData.hh"
#include "Renderer/Vulkan/VulkanStandardMaterial.hh"

namespace Mikoto {

    auto Material::CreateStandardMaterial(const std::any& spec) -> std::shared_ptr<Material> {
        switch (Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::VULKAN_API: return std::make_shared<VulkanStandardMaterial>(std::any_cast<StandardMaterialCreateData>( spec ));
        }

        return nullptr;
    }
}
