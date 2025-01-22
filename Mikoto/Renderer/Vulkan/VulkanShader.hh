/**
 * VulkanShader.hh
 * Created by kate on 7/3/23.
 * */

#ifndef MIKOTO_VULKAN_SHADER_HH
#define MIKOTO_VULKAN_SHADER_HH

// C++ Standard Library
#include <filesystem>

// Third-Party Libraries
#include "volk.h"

// Project Headers
#include "Common/Common.hh"
#include "Material/Core/Shader.hh"
#include "Models/ShaderCreateInfo.hh"
#include "STL/Utility/Types.hh"

namespace Mikoto {
    struct VulkanShaderInfo {
        // Entry point to the shader program
        std::string EntryPoint{ "main" };

        // Contents of the loaded SPIR-V file
        std::string Code{};

        // Vulkan structure needed to create the shader module
        VkPipelineShaderStageCreateInfo StageCreateInfo{};
    };

    class VulkanShader final : public Shader {
    public:
        explicit VulkanShader(const ShaderCreateInfo& createInfo);

        MKT_NODISCARD auto Get() const -> const VkShaderModule& { return m_Module; }
        MKT_NODISCARD static auto GetDefaultEntryPoint() -> std::string_view { return "main"; }
        MKT_NODISCARD auto GetPipelineStageCreateInfo() const -> const VkPipelineShaderStageCreateInfo& { return m_Data.StageCreateInfo; }

    private:
        auto Upload(const Path_T& src) -> void;
        static auto CreateModule(const std::string& srcCode, VkShaderModule& shaderModule) -> void;

    private:
        VulkanShaderInfo m_Data{};
        VkShaderModule m_Module{};
    };
}

#endif // MIKOTO_VULKAN_SHADER_HH
