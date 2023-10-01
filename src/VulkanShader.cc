/**
 * VulkanShader.cc
 * Created by kate on 7/3/23.
 * */

// C++ Standard Library
#include <filesystem>
#include <fstream>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Utility/Types.hh>
#include <Utility/Common.hh>
#include <Utility/VulkanUtils.hh>
#include <Core/Logger.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanShader.hh>

namespace Mikoto {
    VulkanShader::VulkanShader(const ShaderCreateInfo& createInfo)
        :   Shader{ createInfo.Stage }, m_Data{ }
    {
        Upload(createInfo.Directory);
    }

    auto VulkanShader::Upload(const Path_T& src) -> void {
        const auto srcData{ GetFileData(src) };

        m_Data.Code = std::string(srcData.begin(), srcData.end());
        MKT_CORE_LOGGER_DEBUG("Loaded SPIR-V. Size {}", srcData.size());

        CreateModule(m_Data.Code, m_Module);

        // This data is needed later within the pipeline
        m_Data.StageCreateInfo = VulkanUtils::Initializers::PipelineShaderStageCreateInfo();
        m_Data.StageCreateInfo.stage = VulkanUtils::GetVulkanShaderStageFlag(m_Stage);
        m_Data.StageCreateInfo.module = m_Module;
        m_Data.StageCreateInfo.pName = GetDefaultEntryPoint().data();
        m_Data.StageCreateInfo.flags = 0;
        m_Data.StageCreateInfo.pNext = nullptr;
        m_Data.StageCreateInfo.pSpecializationInfo = nullptr;
    }

    auto VulkanShader::CreateModule(const std::string& srcCode, VkShaderModule& shaderModule) -> void {
        VkShaderModuleCreateInfo createInfo{ VulkanUtils::Initializers::ShaderModuleCreateInfo() };
        createInfo.codeSize = srcCode.size();

        // It seems this casts is valid since the default std::vector allocator
        // ensures the data satisfies the worst case alignment
        createInfo.pCode = reinterpret_cast<const UInt32_T*>(srcCode.data());

        if (vkCreateShaderModule(VulkanContext::GetPrimaryLogicalDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create shader module");
        }
    }

    auto VulkanShader::OnRelease() const -> void {
        vkDestroyShaderModule(VulkanContext::GetPrimaryLogicalDevice(), m_Module, nullptr);
    }
}