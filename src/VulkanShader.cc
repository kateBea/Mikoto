//
// Created by kate on 7/3/23.
//

#include <filesystem>
#include <fstream>

#include <Utility/Common.hh>

#include <Core/Logger.hh>

#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanShader.hh>

namespace kaTe {
    VulkanShader::VulkanShader(const ShaderStage stage) {
        m_Data.Stage = stage;
    }

    auto VulkanShader::Upload(const Path_T& src) -> void {
        const auto srcData{ GetFileData(src) };
        m_Data.SrcPath = std::string(srcData.begin(), srcData.end());
        KATE_CORE_LOGGER_DEBUG("Loaded vertex shader data. Size {}", srcData.size());
        VkShaderModule shaderModule{};
        CreateShaderModule(m_Data.SrcPath, shaderModule);

        m_Data.StageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        m_Data.StageCreateInfo.stage = GetVulkanStageFromShaderStage(m_Data.Stage);
        m_Data.StageCreateInfo.module = shaderModule;
        m_Data.StageCreateInfo.pName = m_Data.EntryPoint.c_str();
        m_Data.StageCreateInfo.flags = 0;
        m_Data.StageCreateInfo.pNext = nullptr;
        m_Data.StageCreateInfo.pSpecializationInfo = nullptr;
    }

    auto VulkanShader::CreateShaderModule(const std::string &srcCode, VkShaderModule& shaderModule) -> void {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = srcCode.size();
        // It seems this casts is valid since the default std::vector allocator
        // ensures the data satisfies the worst case alignment
        createInfo.pCode = reinterpret_cast<const UInt32_T*>(srcCode.data());

        if (vkCreateShaderModule(VulkanContext::GetPrimaryLogicalDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
            throw std::runtime_error("Failed to create shader module");
    }

    // TODO: move to Common.hh
    auto VulkanShader::GetFileData(const std::filesystem::path& path) -> std::vector<char> {
        std::ifstream file{ path, std::ios::binary };

        if (!file.is_open())
            throw std::runtime_error("Failed to open SPR-V file");

        return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    }

    auto VulkanShader::GetVulkanStageFromShaderStage(ShaderStage stage) -> VkShaderStageFlagBits {
        switch (stage) {
            case ShaderStage::VERTEX_STAGE: return VK_SHADER_STAGE_VERTEX_BIT;
            case ShaderStage::FRAGMENT_STAGE: return VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        return VK_SHADER_STAGE_FLAG_BITS_MAX_ENUM;
    }

    auto VulkanShader::OnRelease() const -> void {
        vkDestroyShaderModule(VulkanContext::GetPrimaryLogicalDevice(), m_Data.StageCreateInfo.module, nullptr);
    }
}