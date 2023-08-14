//
// Created by kate on 7/3/23.
//

#ifndef VULKATE_VULKAN_SHADER_HH
#define VULKATE_VULKAN_SHADER_HH

#include <filesystem>

#include <volk.h>

#include <Utility/Common.hh>

#include <Renderer/Material/Shader.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>

namespace Mikoto {
    struct VulkanShaderInfo {
        ShaderStage                     Stage{};
        std::string                     EntryPoint{ "main" };
        std::string                     SrcPath{};
        VkPipelineShaderStageCreateInfo StageCreateInfo{};
    };

    class VulkanShader : public Shader {
    public:
        explicit VulkanShader(ShaderStage stage);

        auto Upload(const Path_T& src) -> void;
        auto OnRelease() const -> void;

    private:
        static auto GetFileData(const Path_T& path) -> std::vector<char>;

        static auto CreateShaderModule(const std::string &srcCode, VkShaderModule& shaderModule) -> void;
        static auto GetVulkanStageFromShaderStage(ShaderStage stage) -> VkShaderStageFlagBits;

    private:
        VulkanShaderInfo m_Data{};
    };
}

#endif //VULKATE_VULKAN_SHADER_HH
