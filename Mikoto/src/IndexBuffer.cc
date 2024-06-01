/**
 * IndexBuffer.cc
 * Created by kate on 6/5/23.
 * */

// C++ Standard Library
#include <memory>
#include <vector>

// Project Headers
#include "Common/Common.hh"
#include "Common/RenderingUtils.hh"
#include "Core/Logger.hh"
#include "Renderer/IndexBuffer.hh"
#include "Renderer/Renderer.hh"
#include "Renderer/Vulkan/VulkanIndexBuffer.hh"

namespace Mikoto {
    auto IndexBuffer::Create(const std::vector<UInt32_T>& data) -> std::shared_ptr<IndexBuffer> {
        switch(Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::VULKAN_API:
                return std::make_shared<VulkanIndexBuffer>(data);
            default:
                MKT_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                return nullptr;
        }
    }
}


