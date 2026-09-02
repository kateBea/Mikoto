//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_VULKAN_SHADER_HH
#define MIKOTO_VULKAN_SHADER_HH

#include <EASTL/string.h>

#include <volk.h>
#include <slang.h>
#include <slang-com-ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Shader.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    class Shader final : public rhi::IShaderModule {
    public:
        explicit Shader(const ShaderModuleCreateDescription& desc);

        auto DumpShaderCode() -> void override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType object ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType object ) const -> Object override;

        MKT_NODISCARD auto GetPipelineInfo() const -> const VkPipelineShaderStageCreateInfo&;

        MKT_NODISCARD auto GetContents() const -> const void* override;
        MKT_NODISCARD auto GetContentsByteSize() const -> core::usize override;

        ~Shader() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

        // [Internal usage]
        auto CompileForSlang() -> void;
        auto CompileForGlsl() -> void;
        auto CompileForSpirv() -> void;

    private:
        // Slang stuff
        eastl::string mSlangContents{};
        eastl::string mModulePath{ "" };
        eastl::string mModuleName{ "" };

        Slang::ComPtr<ISlangBlob> mSlangSpirv{};
        Slang::ComPtr<slang::IModule> mSlangModule{};

        VkShaderModule mModule{};
        VkPipelineShaderStageCreateInfo mStageCreateInfo{};

        // SpirV
        eastl::vector<core::u32> mSpirvContents{};

#if !defined(NDEBUG)
        eastl::string mShaderCode{};
#endif
    };
}

#endif // MIKOTO_VULKAN_SHADER_HH
