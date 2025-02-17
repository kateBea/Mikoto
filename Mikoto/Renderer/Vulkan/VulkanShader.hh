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
#include <Common/Common.hh>
#include <Material/Core/Shader.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Vulkan/VulkanObject.hh>

namespace Mikoto {

    struct VulkanShaderCreateInfo {
        Path_T FilePath{};
        ShaderStage Stage{ VERTEX_STAGE };
    };

    class VulkanShader final : public VulkanObject, public Shader {
    public:
        explicit VulkanShader(const VulkanShaderCreateInfo& createInfo);

        MKT_NODISCARD auto Get() const -> const VkShaderModule& { return m_Module; }
        MKT_NODISCARD auto GetPipelineStageCreateInfo() const -> const VkPipelineShaderStageCreateInfo& { return m_StageCreateInfo; }

        auto Release() -> void override;

        ~VulkanShader() override;

    private:
        auto Upload(const VulkanShaderCreateInfo& createInfo) -> void;

    private:
        std::string m_Code{};
        std::string m_EntryPoint{};
        VkShaderModule m_Module{};
        VkPipelineShaderStageCreateInfo m_StageCreateInfo{};
    };
}

#endif // MIKOTO_VULKAN_SHADER_HH
