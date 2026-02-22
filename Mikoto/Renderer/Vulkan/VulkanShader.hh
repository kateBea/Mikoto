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
#include <slang.h>
#include <slang-com-ptr.h>

// Project Headers
#include <Common/Common.hh>
#include <Material/ShaderModule.hh>

namespace Mikoto {

    class VulkanShader final : public ShaderModule {
    public:
        explicit VulkanShader(const ShaderModuleDescription& createInfo);


        MKT_NODISCARD auto GetVulkanStage() const -> VkShaderStageFlags;
        MKT_NODISCARD auto GetNativeHandle( ObjectType ) -> Object override;
        MKT_NODISCARD auto GetPipelineStageCreateInfo() const -> const VkPipelineShaderStageCreateInfo&;

        ~VulkanShader() override;

    private:
        auto LoadSlang( const Path& path ) -> void;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        std::string m_Path{};
        std::string m_EntryPoint{ "main" };

        Slang::ComPtr<ISlangBlob> m_SlangSpirv{};
        Slang::ComPtr<slang::IModule> m_SlangModule{};

        VkShaderModule m_Module{};
        VkPipelineShaderStageCreateInfo m_StageCreateInfo{};
    };
}

#endif // MIKOTO_VULKAN_SHADER_HH
