//
// Created by kate on 7/3/23.
//

// C++ Standard Library
#include <filesystem>
#include <fstream>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/VulkanUtils.hh>
#include <Core/Logger.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanShader.hh>

namespace Mikoto {
    VulkanShader::VulkanShader(ShaderStage stage)
        :   Shader{ stage }
    {

    }

    auto VulkanShader::Upload(const Path_T& src) -> void {
        const auto srcData{ GetFileData(src) };
        m_Data.SrcPath = std::string(srcData.begin(), srcData.end());
        MKT_CORE_LOGGER_DEBUG("Loaded vertex shader data. Size {}", srcData.size());

        VkShaderModule shaderModule{};
        CreateModule(m_Data.SrcPath, shaderModule);

        m_Data.StageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        m_Data.StageCreateInfo.stage = VulkanUtils::GetVulkanShaderStageFlag(m_Stage);
        m_Data.StageCreateInfo.module = shaderModule;
        m_Data.StageCreateInfo.pName = m_Data.EntryPoint.c_str();
        m_Data.StageCreateInfo.flags = 0;
        m_Data.StageCreateInfo.pNext = nullptr;
        m_Data.StageCreateInfo.pSpecializationInfo = nullptr;
    }

    auto VulkanShader::CreateModule(const std::string& srcCode, VkShaderModule& shaderModule) -> void {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = srcCode.size();
        // It seems this casts is valid since the default std::vector allocator
        // ensures the data satisfies the worst case alignment
        createInfo.pCode = reinterpret_cast<const UInt32_T*>(srcCode.data());

        if (vkCreateShaderModule(VulkanContext::GetPrimaryLogicalDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
            throw std::runtime_error("Failed to create shader module");
    }

    auto VulkanShader::OnRelease() const -> void {
        vkDestroyShaderModule(VulkanContext::GetPrimaryLogicalDevice(), m_Data.StageCreateInfo.module, nullptr);
    }
}