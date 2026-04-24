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

#ifndef MIKOTO_REFLECTION_HH
#define MIKOTO_REFLECTION_HH

#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/utility.h>
#include <EASTL/vector.h>
#include <EASTL/fixed_hash_map.h>

#include <ankerl/unordered_dense.h>

#include <volk.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/GpuDevice.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;

    static constexpr u32 kMaxBindingsDescriptorSetLayouts{ 32 };

    struct PipelineReflection {
        struct BindingDescription {
            u32 mSet{};
            u32 mCount{};
            u32 mBinding{};
            eastl::string mName{};
            VkDescriptorType mType{};
            VkShaderStageFlags mStageFlags{};
        };

        eastl::vector<VkPushConstantRange> mPushConstantRanges{};

        static constexpr u32 kMaxBindingsPerSet{ 36 };
        eastl::fixed_hash_map<u32,
            eastl::fixed_hash_map<u32,
                BindingDescription, kMaxBindingsPerSet>,
                kMaxBindingsDescriptorSetLayouts> mBindingSetsMap{};

        // Optional vertex input data (only filled if vertex shader provided)
        std::vector<VkVertexInputBindingDescription> mVertexBindings{};
        std::vector<VkVertexInputAttributeDescription> mVertexAttributes{};

        MKT_NODISCARD static auto Reflect( eastl::span<const ShaderModuleHandle> shaders ) ->  PipelineReflection;
    };
}

#endif//MIKOTO_REFLECTION_HH
