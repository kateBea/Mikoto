/**
 * Material.cc
 * Created by kate on 6/30/23.
 * */

// C++ Standard Library
#include <memory>

// Project Headers
#include <Renderer/Renderer.hh>
#include <Renderer/RenderingUtilities.hh>
#include <Renderer/Material/Material.hh>
#include <Renderer/Vulkan/VulkanStandardMaterial.hh>
#include <Renderer/OpenGL/OpenGLDefaultMaterial.hh>

namespace Mikoto {

    auto Material::Create(Type matType) -> std::shared_ptr<Material> {
        switch(matType) {
            case Type::STANDARD:
                switch (Renderer::GetActiveGraphicsAPI()) {
                    case GraphicsAPI::VULKAN_API: return std::make_shared<VulkanStandardMaterial>();
                    case GraphicsAPI::OPENGL_API: return std::make_shared<OpenGLDefaultMaterial>();
                }

            case Type::NONE:
            case Type::COUNT: return nullptr;
        }
    }
}
