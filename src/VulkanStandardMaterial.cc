/**
 * VulkanStandardMaterial.cc
 * Created by kate on 7/10/2023.
 * */

// C++ Standard Library
#include <array>
#include <memory>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanStandardMaterial.hh>
#include <Renderer/Vulkan/VulkanSwapChain.hh>

namespace Mikoto {
    VulkanStandardMaterial::VulkanStandardMaterial(std::string_view name)
        :   Material{ name }
    {
        m_Texture = std::make_shared<VulkanTexture2D>("../assets/textures/lava512x512.png");

        CreateDescriptorSetLayout();
        CreatePipelineLayout();
        CreatePipeline();

        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();
    }

    auto VulkanStandardMaterial::OnRelease() const -> void {
        vkDeviceWaitIdle(VulkanContext::GetPrimaryLogicalDevice());

        vkDestroyDescriptorPool(VulkanContext::GetPrimaryLogicalDevice(), m_DescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(VulkanContext::GetPrimaryLogicalDevice(), m_DescriptorSetLayout, nullptr);

        for (std::size_t i{}; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
            // TODO: remove, we will have one uniform buffer only which is setup properly before rendering a frame we will no longer be rendering directly to the swapchain images
            vkDestroyBuffer(VulkanContext::GetPrimaryLogicalDevice(), m_UniformBuffers[i], nullptr);
            vkFreeMemory(VulkanContext::GetPrimaryLogicalDevice(), m_UniformBuffersMemory[i], nullptr);
        }

        vkDestroyPipelineLayout(VulkanContext::GetPrimaryLogicalDevice(), m_PipelineLayout, nullptr);
        m_Pipeline->OnRelease();
        m_Texture->OnRelease();
    }

    auto VulkanStandardMaterial::CreatePipelineLayout() -> void {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &m_DescriptorSetLayout;

        if (vkCreatePipelineLayout(VulkanContext::GetPrimaryLogicalDevice(), &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create pipeline layout");
    }

    auto VulkanStandardMaterial::CreatePipeline() -> void {
        auto pipelineConfig{ VulkanPipeline::GetDefaultPipelineConfigInfo() };
        pipelineConfig.RenderPass = VulkanContext::GetSwapChain()->GetRenderPass();
        pipelineConfig.PipelineLayout = m_PipelineLayout;

        m_Pipeline = std::make_shared<VulkanPipeline>("../assets/shaders/vulkan-spirv/basicVert.sprv",
                                                      "../assets/shaders/vulkan-spirv/basicFrag.sprv",
                                                      pipelineConfig);
    }

    auto VulkanStandardMaterial::BindDescriptorSets(VkCommandBuffer commandBuffer) -> void {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSets[VulkanContext::GetSwapChain()->GetCurrentFrame()], 0, nullptr);
    }

    auto VulkanStandardMaterial::CreateDescriptorSetLayout() -> void {
        VkDescriptorSetLayoutBinding transformLayoutBinding{};
        transformLayoutBinding.binding = 0;
        transformLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        transformLayoutBinding.descriptorCount = 1;
        transformLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        transformLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.pImmutableSamplers = nullptr;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings{ transformLayoutBinding, samplerLayoutBinding};

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<UInt32_T>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(VulkanContext::GetPrimaryLogicalDevice(), &layoutInfo, nullptr, &m_DescriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor set layout!");

    }

    auto VulkanStandardMaterial::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) -> void {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(VulkanContext::GetPrimaryLogicalDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
            throw std::runtime_error("failed to create buffer!");

        VkMemoryRequirements memRequirements{};
        vkGetBufferMemoryRequirements(VulkanContext::GetPrimaryLogicalDevice(), buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = VulkanContext::FindMemoryType(memRequirements.memoryTypeBits, properties, VulkanContext::GetPrimaryPhysicalDevice());

        /**
         * NOTE:
         * It should be noted that in a real world application, you're not supposed to actually call
         * vkAllocateMemory for every individual buffer. The maximum number of simultaneous memory
         * allocations is limited by the maxMemoryAllocationCount physical device limit, which may
         * be as low as 4096 even on high end hardware like an NVIDIA GTX 1080
         * See: https://vulkan-tutorial.com/Vertex_buffers/Staging_buffer
         * */
        if (vkAllocateMemory(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate buffer memory!");

        vkBindBufferMemory(VulkanContext::GetPrimaryLogicalDevice(), buffer, bufferMemory, 0);
    }

    auto VulkanStandardMaterial::CreateUniformBuffers() -> void {
        VkDeviceSize bufferSize{ sizeof(UniformBufferObject) };

        m_UniformBuffers.resize(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);
        m_UniformBuffersMemory.resize(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);
        m_UniformBuffersMapped.resize(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

        for (std::size_t i{}; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
            CreateBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_UniformBuffers[i], m_UniformBuffersMemory[i]);
            vkMapMemory(VulkanContext::GetPrimaryLogicalDevice(), m_UniformBuffersMemory[i], 0, bufferSize, 0, &m_UniformBuffersMapped[i]);
        }
    }

    auto VulkanStandardMaterial::UpdateUniformBuffers(UInt32_T frame) -> void {
        std::memcpy(m_UniformBuffersMapped[frame], &m_Transform, sizeof(m_Transform));
    }

    auto VulkanStandardMaterial::CreateDescriptorPool() -> void {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<UInt32_T>(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<UInt32_T>(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<UInt32_T>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<UInt32_T>(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(VulkanContext::GetPrimaryLogicalDevice(), &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool!");
    }

    auto VulkanStandardMaterial::CreateDescriptorSets() -> void {
        std::vector<VkDescriptorSetLayout> layouts(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT, m_DescriptorSetLayout);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = static_cast<UInt32_T>(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        m_DescriptorSets.resize(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

        if (vkAllocateDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, m_DescriptorSets.data()) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate descriptor sets!");

        for (std::size_t i{}; i < VulkanSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            imageInfo.imageView = m_Texture->GetImageView();
            imageInfo.sampler = m_Texture->GetImageSampler();

            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_UniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
            descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[0].dstSet = m_DescriptorSets[i];
            descriptorWrites[0].dstBinding = 0;
            descriptorWrites[0].dstArrayElement = 0;
            descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites[0].descriptorCount = 1;
            descriptorWrites[0].pBufferInfo = &bufferInfo;

            descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites[1].dstSet = m_DescriptorSets[i];
            descriptorWrites[1].dstBinding = 1;
            descriptorWrites[1].dstArrayElement = 0;
            descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites[1].descriptorCount = 1;
            descriptorWrites[1].pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), static_cast<UInt32_T>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }

    auto VulkanStandardMaterial::SetModelMatrix(const glm::mat4& model) -> void {
        m_Transform.Model = model;
    }

    auto VulkanStandardMaterial::SetViewMatrix(const glm::mat4 &view) -> void {
        m_Transform.View = view;
    }

    auto VulkanStandardMaterial::SetProjectionMatrix(const glm::mat4& proj) -> void {
        m_Transform.Projection = proj;
    }

    auto VulkanStandardMaterial::EnableWireframe() -> void {
        m_Pipeline->OnRelease();

        auto pipelineConfig{ VulkanPipeline::GetDefaultPipelineConfigInfo() };

        constexpr float GPU_STANDARD_LINE_WIDTH{ 1.0f };
        pipelineConfig.RasterizationInfo.polygonMode = VK_POLYGON_MODE_LINE;
        pipelineConfig.RasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        pipelineConfig.RasterizationInfo.lineWidth = pipelineConfig.RasterizationInfo.polygonMode == VK_POLYGON_MODE_LINE ? GPU_STANDARD_LINE_WIDTH : 0.0f;

        pipelineConfig.RenderPass = VulkanContext::GetSwapChain()->GetRenderPass();
        pipelineConfig.PipelineLayout = m_PipelineLayout;

        // creating a pipeline is expensive might have a material separated with a cached pipeline to draw in wireframe mode
        m_Pipeline = std::make_shared<VulkanPipeline>("../assets/basicVert.sprv", "../assets/basicFrag.sprv", pipelineConfig);
    }

    auto VulkanStandardMaterial::DisableWireframe() -> void {
        m_Pipeline->OnRelease();

        auto pipelineConfig{ VulkanPipeline::GetDefaultPipelineConfigInfo() };

        pipelineConfig.RasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
        pipelineConfig.RasterizationInfo.cullMode = VK_CULL_MODE_BACK_BIT;
        pipelineConfig.RasterizationInfo.lineWidth = 0.0f;

        pipelineConfig.RenderPass = VulkanContext::GetSwapChain()->GetRenderPass();
        pipelineConfig.PipelineLayout = m_PipelineLayout;

        m_Pipeline = std::make_shared<VulkanPipeline>("../assets/basicVert.sprv", "../assets/basicFrag.sprv", pipelineConfig);
    }
}
