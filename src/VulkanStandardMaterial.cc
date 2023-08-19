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
#include <Utility/VulkanUtils.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanStandardMaterial.hh>

namespace Mikoto {
    VulkanStandardMaterial::VulkanStandardMaterial(std::string_view name)
        :   Material{ name }
    {}

    auto VulkanStandardMaterial::OnCreate(const VulkanStandardMaterialCreateInfo& createInfo) -> void {
        m_RenderPass = createInfo.RenderPass;

        m_Texture = std::make_shared<VulkanTexture2D>("../assets/textures/lava512x512.png");

        CreateUniformBuffer();

        CreateDescriptorSetLayout();
        CreatePipelineLayout();
        CreatePipeline();

        CreateDescriptorPool();
        CreateDescriptorSet();
    }

    auto VulkanStandardMaterial::OnRelease() const -> void {
        vkDeviceWaitIdle(VulkanContext::GetPrimaryLogicalDevice());

        vkDestroyDescriptorPool(VulkanContext::GetPrimaryLogicalDevice(), m_DescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(VulkanContext::GetPrimaryLogicalDevice(), m_DescriptorSetLayout, nullptr);

        m_UniformBuffer.OnRelease();

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
        pipelineConfig.RenderPass = m_RenderPass;
        pipelineConfig.PipelineLayout = m_PipelineLayout;

        m_Pipeline = std::make_shared<VulkanPipeline>("../assets/shaders/vulkan-spirv/basicVert.sprv",
                                                      "../assets/shaders/vulkan-spirv/basicFrag.sprv",
                                                      pipelineConfig);
    }

    auto VulkanStandardMaterial::BindDescriptorSets(VkCommandBuffer commandBuffer) -> void {
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_PipelineLayout, 0, 1, &m_DescriptorSet, 0, nullptr);
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

    auto VulkanStandardMaterial::CreateUniformBuffer() -> void {
        BufferAllocateInfo allocInfo{};
        allocInfo.Size = sizeof(UniformTransformData);

        allocInfo.BufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        allocInfo.BufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        allocInfo.BufferCreateInfo.size = allocInfo.Size;

        allocInfo.AllocationCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

        m_UniformBuffer.OnCreate(allocInfo);

        if (vmaMapMemory(VulkanContext::GetDefaultAllocator(), m_UniformBuffer.GetVmaAllocation(), &m_UniformBuffersMapped) != VK_SUCCESS)
            throw std::runtime_error("Failed to map memory for uniform buffer in default material!");

        vmaUnmapMemory(VulkanContext::GetDefaultAllocator(), m_UniformBuffer.GetVmaAllocation());
    }

    auto VulkanStandardMaterial::UploadUniformBuffers() -> void {
        std::memcpy(m_UniformBuffersMapped, &m_Transform, sizeof(m_Transform));
    }

    auto VulkanStandardMaterial::CreateDescriptorPool() -> void {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = 1;

        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<UInt32_T>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 10;

        if (vkCreateDescriptorPool(VulkanContext::GetPrimaryLogicalDevice(), &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool!");
    }

    auto VulkanStandardMaterial::CreateDescriptorSet() -> void {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_DescriptorSetLayout;

        if (vkAllocateDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, &m_DescriptorSet) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate descriptor sets!");

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        imageInfo.imageView = m_Texture->GetImageView();
        imageInfo.sampler = m_Texture->GetImageSampler();

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = m_UniformBuffer.Get();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(UniformTransformData);

        std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = m_DescriptorSet;
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = m_DescriptorSet;
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(VulkanContext::GetPrimaryLogicalDevice(), static_cast<UInt32_T>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }


    auto VulkanStandardMaterial::SetProjectionView(const glm::mat4 &projView) -> void {
        m_Transform.ProjectionView = projView;
    }

    auto VulkanStandardMaterial::SetTransform(const glm::mat4&transform) -> void {
        m_Transform.Transform = transform;
    }

    auto VulkanStandardMaterial::EnableWireframe() -> void {
        m_Pipeline->OnRelease();

        auto pipelineConfig{ VulkanPipeline::GetDefaultPipelineConfigInfo() };

        constexpr float GPU_STANDARD_LINE_WIDTH{ 1.0f };
        pipelineConfig.RasterizationInfo.polygonMode = VK_POLYGON_MODE_LINE;
        pipelineConfig.RasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        pipelineConfig.RasterizationInfo.lineWidth = pipelineConfig.RasterizationInfo.polygonMode == VK_POLYGON_MODE_LINE ? GPU_STANDARD_LINE_WIDTH : 0.0f;

        pipelineConfig.RenderPass = m_RenderPass;
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

        pipelineConfig.RenderPass = m_RenderPass;
        pipelineConfig.PipelineLayout = m_PipelineLayout;

        m_Pipeline = std::make_shared<VulkanPipeline>("../assets/basicVert.sprv", "../assets/basicFrag.sprv", pipelineConfig);
    }

    auto VulkanStandardMaterial::SetTiltingColor(float red, float green, float blue, float alpha) -> void {
        //m_UniformBufferData.Color.r = red;
        //m_UniformBufferData.Color.g = green;
        //m_UniformBufferData.Color.b = blue;
        //m_UniformBufferData.Color.a = alpha;
    }

    auto VulkanStandardMaterial::SetTiltingColor(const glm::vec4& color) -> void {
        //m_UniformBufferData.Color = color;
    }
}
