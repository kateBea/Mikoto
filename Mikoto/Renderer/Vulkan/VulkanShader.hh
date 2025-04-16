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
#include <Library/Utility/Types.hh>
#include <Material/ShaderModule.hh>
#include <Library/Filesystem/File.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>

namespace Mikoto {

    class VulkanShader final : public ShaderModule {
    public:
        explicit VulkanShader(const ShaderModuleDescription& createInfo);

        MKT_NODISCARD auto Get() -> VkShaderModule&;
        MKT_NODISCARD auto Get() const -> const VkShaderModule&;

        MKT_NODISCARD auto GetPipelineStageCreateInfo() const -> const VkPipelineShaderStageCreateInfo&;

        MKT_NODISCARD auto GetVulkanStage() const -> VkShaderStageFlags;

        ~VulkanShader() override;

    private:
        auto Release() -> void override;
        auto Allocate() -> void override;

    private:
        std::string m_Code{};
        std::string m_EntryPoint{};

        VkShaderModule m_Module{};
        VkPipelineShaderStageCreateInfo m_StageCreateInfo{};
    };
}

#endif // MIKOTO_VULKAN_SHADER_HH
