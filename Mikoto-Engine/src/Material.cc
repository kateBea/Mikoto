/**
 * Material.cc
 * Created by kate on 6/30/23.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include "Common/RenderingUtils.hh"

#include "Renderer/Renderer.hh"
#include "Renderer/Material/Material.hh"
#include "Renderer/OpenGL/OpenGLDefaultMaterial.hh"
#include "Renderer/Vulkan/VulkanStandardMaterial.hh"
#include "Renderer/Vulkan/VulkanPBRMaterial.hh"

namespace Mikoto {

    auto Material::CreateStandardMaterial(const DefaultMaterialCreateSpec& spec) -> std::shared_ptr<Material> {
        switch (Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::VULKAN_API: return std::make_shared<VulkanStandardMaterial>(spec);
            case GraphicsAPI::OPENGL_API: return std::make_shared<OpenGLDefaultMaterial>();
        }
    }
    auto Material::CreatePBRMaterial( const PBRMaterialCreateSpec& spec ) -> std::shared_ptr<Material> {
        switch (Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::VULKAN_API: return std::make_shared<VulkanPBRMaterial>(spec);
            case GraphicsAPI::OPENGL_API:
                MKT_CORE_LOGGER_WARN("PBR materials not supported for OpenGL for now");
                return nullptr;
        }
    }
}
