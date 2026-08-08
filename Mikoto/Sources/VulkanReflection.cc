//    Copyright 2025 ケイト
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

#include <volk.h>
#include <spirv_reflect.h>

#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanReflection.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    MKT_NODISCARD auto GetDescriptorType( SpvReflectDescriptorType type ) -> VkDescriptorType {
        switch ( type ) {
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
                return VK_DESCRIPTOR_TYPE_SAMPLER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
                return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
                return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;

            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
                return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
                return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;

            case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
                return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

#if defined( VK_KHR_acceleration_structure )
            case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
                return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
#endif
            default:
                return VK_DESCRIPTOR_TYPE_MAX_ENUM;
        }
    }

    static auto ProcessDescriptorSets(SpvReflectShaderModule& mod, VkShaderStageFlagBits stage, PipelineReflection& pipelineReflection) -> void {
        u32 setCount{};
        spvReflectEnumerateDescriptorSets(&mod, &setCount, nullptr);

        eastl::vector<SpvReflectDescriptorSet*> reflectedSets(setCount);
        spvReflectEnumerateDescriptorSets(&mod, &setCount, reflectedSets.data());

        // For all descriptor sets
        for (auto* reflectedDescriptorSet: reflectedSets) {
            // For all bindings within a descriptor set
            for (u32 binding{}; binding < reflectedDescriptorSet->binding_count; ++binding) {
                auto* reflectedBinding{ reflectedDescriptorSet->bindings[binding] };

                u32 setIndex{ reflectedDescriptorSet->set };
                auto& setBindings{ pipelineReflection.mBindingSetsMap[setIndex] };
                if (auto it{ setBindings.find(reflectedBinding->binding) }; it == setBindings.end()) {
                    // If this set does not have this binding yet, add it
                    VkDescriptorSetLayoutBinding bindingInfo{};
                    bindingInfo.binding = reflectedBinding->binding;
                    bindingInfo.descriptorType = GetDescriptorType( reflectedBinding->descriptor_type );
                    bindingInfo.descriptorCount = reflectedBinding->count;
                    bindingInfo.stageFlags = stage;

                    auto& bindingDescription{ setBindings[bindingInfo.binding] };
                    bindingDescription.mSet = setIndex;
                    bindingDescription.mCount =  bindingInfo.descriptorCount;
                    bindingDescription.mBinding = bindingInfo.binding;
                    bindingDescription.mName = reflectedBinding->name;
                    bindingDescription.mType = bindingInfo.descriptorType;
                    bindingDescription.mStageFlags = as<VkShaderStageFlags>( stage );

                } else {
                    // If this set already has this binding, just update stage flags
                    it->second.mStageFlags |= stage;
                    pipelineReflection.mBindingSetsMap[reflectedDescriptorSet->set][it->second.mBinding].mStageFlags |= stage;
                }
            }
        }
    }

    static auto ProcessPushConstants(SpvReflectShaderModule& mod, PipelineReflection& pipelineReflection) -> void {
        // Follow same structure as graphics context, global set of constants passed per draw
        VkPushConstantRange psRange{
            .stageFlags = VK_SHADER_STAGE_ALL,
            .offset     = 0,
            .size       = kMaxPushConstantSize
        };

        // Push constants are globals and declared once for a single pipeline
        if (pipelineReflection.mPushConstantRanges.empty()) {
            pipelineReflection.mPushConstantRanges.emplace_back( psRange );
        }
    }

    MKT_NODISCARD static auto GetAttributeByteSize(SpvReflectFormat format) -> size_t {
        switch (format) {
            // ======================
            // 32-bit float
            // ======================
            case SPV_REFLECT_FORMAT_R32_SFLOAT:               return 4;
            case SPV_REFLECT_FORMAT_R32G32_SFLOAT:            return 8;
            case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:         return 12;
            case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:      return 16;

            // ======================
            // 32-bit signed int
            // ======================
            case SPV_REFLECT_FORMAT_R32_SINT:                 return 4;
            case SPV_REFLECT_FORMAT_R32G32_SINT:              return 8;
            case SPV_REFLECT_FORMAT_R32G32B32_SINT:           return 12;
            case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:        return 16;

            // ======================
            // 32-bit unsigned int
            // ======================
            case SPV_REFLECT_FORMAT_R32_UINT:                 return 4;
            case SPV_REFLECT_FORMAT_R32G32_UINT:              return 8;
            case SPV_REFLECT_FORMAT_R32G32B32_UINT:           return 12;
            case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:        return 16;

            // ======================
            // 16-bit float
            // ======================
            case SPV_REFLECT_FORMAT_R16_SFLOAT:               return 2;
            case SPV_REFLECT_FORMAT_R16G16_SFLOAT:            return 4;
            case SPV_REFLECT_FORMAT_R16G16B16_SFLOAT:         return 6;
            case SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT:      return 8;

            // ======================
            // 16-bit signed int
            // ======================
            case SPV_REFLECT_FORMAT_R16_SINT:                 return 2;
            case SPV_REFLECT_FORMAT_R16G16_SINT:              return 4;
            case SPV_REFLECT_FORMAT_R16G16B16_SINT:           return 6;
            case SPV_REFLECT_FORMAT_R16G16B16A16_SINT:        return 8;

            // ======================
            // 16-bit unsigned int
            // ======================
            case SPV_REFLECT_FORMAT_R16_UINT:                 return 2;
            case SPV_REFLECT_FORMAT_R16G16_UINT:              return 4;
            case SPV_REFLECT_FORMAT_R16G16B16_UINT:           return 6;
            case SPV_REFLECT_FORMAT_R16G16B16A16_UINT:        return 8;

            // ======================
            default:
                return 0;
        }
    }

    static auto ProcessVertexInputs( SpvReflectShaderModule& mod, PipelineReflection& pipelineReflection ) -> void {
        u32 inputCount{};
        spvReflectEnumerateInputVariables( &mod, &inputCount, nullptr );

        eastl::vector<SpvReflectInterfaceVariable*> inputs( inputCount );
        spvReflectEnumerateInputVariables( &mod, &inputCount, inputs.data() );

        size_t stride{};
        u32 binding{};
        for ( auto* v: inputs ) {
            if ( v->decoration_flags & SPV_REFLECT_DECORATION_BUILT_IN ) {
                continue;
            }

            VkVertexInputAttributeDescription attr{};
            attr.binding = binding;
            attr.location = v->location;

            // It is the user who will decide how
            // they want to pass the data in and how
            // the attributes are layout in the buffer they will upload to the GPU
            attr.offset = 0;
            attr.format = as<VkFormat>( v->format );

            stride += GetAttributeByteSize( v->format );
            pipelineReflection.mVertexAttributes.emplace_back( attr );
        }

        if ( !pipelineReflection.mVertexAttributes.empty() ) {
            VkVertexInputBindingDescription bind{};

            // We do not know, the user is supposed to
            // specify these properties
            bind.binding = 0;
            bind.inputRate = VK_VERTEX_INPUT_RATE_MAX_ENUM;

            // This field will simply tell
            // the minimum recorded byte offset between set of vertex attributes
            bind.stride = stride;

            pipelineReflection.mVertexBindings.push_back( bind );
        }
    }

    auto PipelineReflection::Reflect( eastl::span<const ShaderModuleHandle> shaders ) -> PipelineReflection {
        PipelineReflection result{};

        for (const auto& moduleData : shaders) {
            SpvReflectShaderModule module{};
            if (spvReflectCreateShaderModule(moduleData->GetContentsByteSize(), moduleData->GetContents(), &module) != SPV_REFLECT_RESULT_SUCCESS) {
                MKT_CORE_LOGGER_ERROR( "Failed reflection for shader module." );
                break;
            }

            VkShaderStageFlagBits stage{ as<VkShaderStageFlagBits>( module.shader_stage ) };
            if ( stage == VK_SHADER_STAGE_VERTEX_BIT ) {
                ProcessVertexInputs( module, result );
            }

            ProcessPushConstants( module, result );
            ProcessDescriptorSets( module, stage, result );

            // Cleanup
            spvReflectDestroyShaderModule( MKT_ADDRESSOF( module ) );
        }

        return result;
    }
}// namespace mikoto::renderer::vulkan