/**
 * VulkanShader.hh
 * Created by kate on 7/3/23.
 * */

#ifndef MIKOTO_VULKAN_SHADER_HH
#define MIKOTO_VULKAN_SHADER_HH

// C++ Standard Library
#include <filesystem>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Utility/Common.hh>
#include <Utility/Types.hh>
#include <Renderer/Material/Shader.hh>

namespace Mikoto {
    struct VulkanShaderInfo {
        std::string EntryPoint{ "main" };
        std::string SrcPath{};
        VkPipelineShaderStageCreateInfo StageCreateInfo{};
    };

    class VulkanShader : public Shader {
    public:
        explicit VulkanShader(ShaderStage stage);

        auto Upload(const Path_T& src) -> void;
        auto OnRelease() const -> void;

        MKT_NODISCARD auto Get() const -> const VkShaderModule& { return m_Module; }

    private:
        static auto CreateModule(const std::string& srcCode, VkShaderModule& shaderModule) -> void;

    private:
        VulkanShaderInfo m_Data{};
        VkShaderModule m_Module{};
    };
}

#endif // MIKOTO_VULKAN_SHADER_HH
