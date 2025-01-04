/**
 * VertexBuffer.cc
 * Created by kate on 6/5/23.
 * */

// C++ Standard Library
#include <vector>
#include <memory>

// Project Headers
#include <Renderer/Core/Renderer.hh>

#include "Common/RenderingUtils.hh"
#include "Core/Logger.hh"
#include "Models/VertexBufferCreateInfo.hh"
#include "Renderer/Vulkan/VulkanVertexBuffer.hh"

namespace Mikoto {
    auto VertexBuffer::Create(const std::vector<float>& data, const BufferLayout& layout) -> std::shared_ptr<VertexBuffer> {
        VertexBufferCreateInfo createInfo{};

        createInfo.Data = data;
        createInfo.RetainData = false; // for future use
        createInfo.Layout = layout;

        switch(Renderer::GetActiveGraphicsAPI()) {
            case GraphicsAPI::VULKAN_API:
                return std::make_shared<VulkanVertexBuffer>(std::move(createInfo));
            default:
                MKT_CORE_LOGGER_CRITICAL("Unsupported renderer API");
                return nullptr;
        }
    }
}