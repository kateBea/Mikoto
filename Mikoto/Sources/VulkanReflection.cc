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

    auto ToVkDescriptorType( SpvReflectDescriptorType type ) -> VkDescriptorType {
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

#if false
    // NOTE: About bindless descriptors, for simplicity their name will contain "bindless", e.g., "bindless_textures"
    // Helper: process descriptor sets for a single SPIR-V module and merge into `sets` and `out`
    static void ProcessDescriptorSets( SpvReflectShaderModule& mod, VkShaderStageFlagBits stage,
        std::map<UInt32, std::unordered_map<UInt32, VkDescriptorSetLayoutBinding>>& sets, ReflectedData& out ) {
        u32 setCount{};
        spvReflectEnumerateDescriptorSets( &mod, &setCount, nullptr );

        std::vector<SpvReflectDescriptorSet*> reflectedSets( setCount );
        spvReflectEnumerateDescriptorSets( &mod, &setCount, reflectedSets.data() );

        for ( auto* reflectedDescriptorSet: reflectedSets ) {
            for ( UInt32 setBinding{}; setBinding < reflectedDescriptorSet->binding_count; ++setBinding ) {
                auto* reflectedBinding{ reflectedDescriptorSet->bindings[setBinding] };

                UInt32 setIndex{ reflectedDescriptorSet->set };
                auto& bindingMap{ sets[setIndex] };

                if ( auto it{ bindingMap.find( reflectedBinding->binding ) }; it == bindingMap.end() ) {
                    // If this set does not have this binding yet, add it

                    VkDescriptorSetLayoutBinding bindingInfo{};
                    bindingInfo.binding = reflectedBinding->binding;
                    bindingInfo.descriptorType = ToVkDescriptorType( reflectedBinding->descriptor_type );

                    // BUFFER_VIEWS_SET_INDEX set uses uniform and storage dynamics
                    if ( setIndex == BUFFER_VIEWS_SET_INDEX && reflectedBinding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER ) {
                        bindingInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;

                        out.DynamicBuffersBindingCount++;
                    }
                    if ( setIndex == BUFFER_VIEWS_SET_INDEX && reflectedBinding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER ) {
                        bindingInfo.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;

                        out.DynamicBuffersBindingCount++;
                    }

                    bool isBindless{ false };

                    if ( setIndex == UNBOUNDED_IV_SAMPLERS_SET_INDEX || setIndex == UNBOUNDED_BV_SET_INDEX ) {
                        isBindless = true;
                    }

                    // IMPORTANT: bindless textures need to be the last binding if they are sharing a SET with other bindings
                    bindingInfo.descriptorCount = std::max( 1u, isBindless ? MAX_BINDLESS_GROUP_INDEX : reflectedBinding->count );

                    bindingInfo.stageFlags = stage;
                    bindingMap[bindingInfo.binding] = bindingInfo;

                    out.bindingMap[{ setIndex, bindingInfo.binding }] = ReflectedBindingInfo{
                        reflectedBinding->name,
                        setIndex,
                        bindingInfo.binding,
                        bindingInfo.descriptorType,
                        bindingInfo.descriptorCount,
                        static_cast<VkShaderStageFlags>( stage ),
                        isBindless
                    };
                } else {
                    // If this set already has this binding, just update stage flags
                    it->second.stageFlags |= stage;
                    out.bindingMap[{ reflectedDescriptorSet->set, it->second.binding }].stageFlags |= stage;
                }
            }
        }
    }

    // Helper: collect push constants from a single SPIR-V module
    static void ProcessPushConstants( SpvReflectShaderModule& mod, VkShaderStageFlagBits stage, std::vector<VkPushConstantRange>& pushConstants ) {

        // Follow same structure as graphics context, global set of constants passed per draw
        VkPushConstantRange psRange{
            .stageFlags = VK_SHADER_STAGE_ALL,
            .offset = 0,
            .size = MINIMUM_REQUIRED_PUSH_CONSTANTS_SIZE
        };

        // Push constants are globals and declared once for a single pipeline
        if ( pushConstants.empty() ) {
            pushConstants.emplace_back( psRange );
        }
    }

    static auto InferSizeInBytesSpirVAttribute( SpvReflectFormat format ) -> Size {
        switch ( format ) {
            case SPV_REFLECT_FORMAT_R32_SFLOAT:
                return 4;

            case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
                return 8;

            case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
                return 12;

            case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
                return 16;

            default:
                // If unsupported or unknown, return 0 or handle however your engine expects
                return 0;
        }
    }

    // Helper: collect vertex inputs for vertex-stage modules
    static void ProcessVertexInputs( SpvReflectShaderModule& mod, ReflectedData& out ) {
        UInt32 inputCount{};
        spvReflectEnumerateInputVariables( &mod, &inputCount, nullptr );
        std::vector<SpvReflectInterfaceVariable*> inputs( inputCount );
        spvReflectEnumerateInputVariables( &mod, &inputCount, inputs.data() );

        Size stride{};
        UInt32 binding{};
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

            switch ( v->format ) {
                case SPV_REFLECT_FORMAT_R32_SFLOAT:
                    attr.format = VK_FORMAT_R32_SFLOAT;
                    break;
                case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
                    attr.format = VK_FORMAT_R32G32_SFLOAT;
                    break;
                case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
                    attr.format = VK_FORMAT_R32G32B32_SFLOAT;
                    break;
                case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
                    attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
                    break;
                default:
                    attr.format = VK_FORMAT_UNDEFINED;
                    break;
            }

            stride += InferSizeInBytesSpirVAttribute( v->format );
            out.vertexAttributes.push_back( attr );
        }

        if ( !out.vertexAttributes.empty() ) {
            VkVertexInputBindingDescription bind{};
            bind.binding = 0;
            bind.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            // This field will simply tell
            // the minimum recorded byte offset between set of vertex attributes
            bind.stride = stride;

            out.vertexBindings.push_back( bind );
        }
    }

    // Helper: create descriptor set layouts from collected `sets`
    static VkResult CreateDescriptorSetLayouts( VkDevice device, const std::map<UInt32, std::unordered_map<UInt32, VkDescriptorSetLayoutBinding>>& sets, ReflectedData& out ) {
        for ( const auto& [setIndex, bindings]: sets ) {
            std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
            layoutBindings.reserve( bindings.size() );

            for ( const auto& binding: bindings | std::ranges::views::values ) {
                layoutBindings.push_back( binding );
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
            layoutInfo.bindingCount = static_cast<UInt32>( layoutBindings.size() );
            layoutInfo.pBindings = layoutBindings.data();

            std::vector<VkDescriptorBindingFlags> bindingFlags( layoutBindings.size(), VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT );

            // BUFFER_VIEWS_SET_INDEX need no flags, dynamic Storage buffers and dynamic uniforms cannot have update after bind bit
            if ( setIndex == BUFFER_VIEWS_SET_INDEX ) {
                for ( auto& flag: bindingFlags ) {
                    flag = VK_FLAGS_NONE;
                }
            }

            VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
            flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;

            if ( IsBindlessEnabled() ) {

                for ( Size i{}; i < layoutBindings.size(); i++ ) {
                    auto bindingNumber{ layoutBindings[i].binding };
                    auto& bindingInfo{ out.bindingMap[{ setIndex, bindingNumber }] };

                    if ( bindingInfo.IsBindless ) {
                        bindingFlags[i] =
                                VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                                VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
                    }
                }

                layoutInfo.pNext = &flagsInfo;
                layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
            }

            flagsInfo.bindingCount = static_cast<UInt32>( bindingFlags.size() );
            flagsInfo.pBindingFlags = bindingFlags.data();

            VkDescriptorSetLayout layoutHandle{};
            if ( vkCreateDescriptorSetLayout( device, &layoutInfo, nullptr, &layoutHandle ) != VK_SUCCESS ) {
                return VK_ERROR_INITIALIZATION_FAILED;
            }

            out.setLayouts.emplace( setIndex, layoutHandle );
        }

        return VK_SUCCESS;
    }

    static auto CreatePipelineLayout( VkDevice device, ReflectedData& out, std::vector<VkPushConstantRange>& pushConstants ) -> VkResult {
        // IMPORTANT:
        // In Vulkan, VkPipelineLayoutCreateInfo::pSetLayouts is an array where each element
        // corresponds to a descriptor set index in order:
        //    pSetLayouts[0] -> set = 0
        //    pSetLayouts[1] -> set = 1
        //    pSetLayouts[2] -> set = 2
        // Vulkan does NOT sort or remap them automatically. If the layouts in out.setLayouts
        // are not in the same order as the shader set indices, you will get validation errors.
        // For example, if the fragment shader uses set = 1 but out.setLayouts[1] corresponds
        // to set = 2, Vulkan will complain that the descriptor is missing.
        // Here we are just filling not used slots with empty descriptor set layouts.
        // TODO: VK_EXT_Pipeline library extension
        // [11:59:23] STDERR LOG [thread 67676] Validation Error: Validation Error: [ VUID-VkPipelineLayoutCreateInfo-graphicsPipelineLibrary-06753 ] |
        // MessageID = 0x57ab6143 | vkCreatePipelineLayout(): pCreateInfo->pSetLayouts[0] is VK_NULL_HANDLE, but VK_EXT_graphics_pipeline_library is not enabled.
        // The Vulkan spec states: If graphicsPipelineLibrary is not enabled, elements of pSetLayouts must be valid VkDescriptorSetLayout objects
        // (https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkPipelineLayoutCreateInfo-graphicsPipelineLibrary-06753)

        // Find highest set index
        UInt32 maxSet{ 0 };
        for ( const auto& setIndex: out.setLayouts | std::views::keys ) {
            maxSet = std::max( maxSet, setIndex );
        }

        // Allocate with holes
        VulkanDevice* vulkanDevice{ TO_VK_DEVICE( RenderService::Get()->GetGpuDevice() ) };
        DescriptorSetLayoutHandle emptySetLayout{ vulkanDevice->GetDummyDescriptorLayout() };

        std::vector<VkDescriptorSetLayout> setLayouts( maxSet + 1, emptySetLayout->GetNativeHandle( ObjectType::Vk_DescriptorSetLayout ) );

        // Place layouts at correct indices
        for ( const auto& [setIndex, layout]: out.setLayouts ) {
            setLayouts[setIndex] = layout;
        }


        // Add a Layout with no descriptors, not needed because we should not bind an empty Set
        // for (Size setIndex{}; setIndex < setLayouts.size(); setIndex++) {
        //     if (out.setLayouts[setIndex] == VK_NULL_HANDLE) {
        //         out.setLayouts[setIndex] = setLayouts[setIndex];
        //     }
        // }

        // Prepare list of sets for the pipeline layout
        for ( const auto& [layoutIndex, setLayout]: out.setLayouts ) {
            setLayouts[layoutIndex] = setLayout;
        }

        VkPipelineLayoutCreateInfo plInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
        plInfo.setLayoutCount = static_cast<UInt32>( setLayouts.size() );
        plInfo.pSetLayouts = setLayouts.data();

        plInfo.pushConstantRangeCount = static_cast<UInt32>( pushConstants.size() );
        plInfo.pPushConstantRanges = pushConstants.data();

        if ( vkCreatePipelineLayout( device, &plInfo, nullptr, &out.pipelineLayout ) != VK_SUCCESS ) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        out.pushConstantRanges = std::move( pushConstants );
        return VK_SUCCESS;
    }

    auto ReflectSPIRV( VkDevice device, const std::vector<std::vector<UInt32>>& spirvModules, ReflectedData& out ) -> VkResult {
        out = {};

        std::vector<VkPushConstantRange> pushConstants{};
        std::map<UInt32, std::unordered_map<UInt32, VkDescriptorSetLayoutBinding>> sets{};

        for ( const auto& moduleData: spirvModules ) {
            if ( moduleData.empty() ) {
                MKT_CORE_LOGGER_ERROR( "VulkanHelpers::Reflection::ReflectSPIRV - Empty SPIR-V module data." );
                continue;
            }

            SpvReflectShaderModule mod{};
            if ( spvReflectCreateShaderModule( moduleData.size() * sizeof( UInt32 ), moduleData.data(), &mod ) != SPV_REFLECT_RESULT_SUCCESS ) {
                return VK_ERROR_INITIALIZATION_FAILED;
            }

            VkShaderStageFlagBits stage{ static_cast<VkShaderStageFlagBits>( mod.shader_stage ) };

            ProcessDescriptorSets( mod, stage, sets, out );
            ProcessPushConstants( mod, stage, pushConstants );

            if ( stage == VK_SHADER_STAGE_VERTEX_BIT ) {
                ProcessVertexInputs( mod, out );
            }

            // Cleanup
            spvReflectDestroyShaderModule( &mod );
        }

        if ( CreateDescriptorSetLayouts( device, sets, out ) != VK_SUCCESS ) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        if ( CreatePipelineLayout( device, out, pushConstants ) != VK_SUCCESS ) {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        return VK_SUCCESS;
    }

    auto DestroyReflectedPipeline( const VkDevice device, ReflectedData& reflected ) -> void {
        if ( reflected.pipelineLayout ) {
            vkDestroyPipelineLayout( device, reflected.pipelineLayout, nullptr );
            reflected.pipelineLayout = VK_NULL_HANDLE;
        }

        for ( const auto& dsLayout: reflected.setLayouts | std::views::values ) {
            vkDestroyDescriptorSetLayout( device, dsLayout, nullptr );
        }

        reflected.setLayouts.clear();
        reflected.vertexBindings.clear();
        reflected.vertexAttributes.clear();
        reflected.bindingMap.clear();
        reflected.pushConstantRanges.clear();
    }

#endif

    static auto CreatePipelineLayout(
        Device* device,
        PipelineReflection& mPipelineReflection,
        BindingSetLayoutsMap& bindingLayoutsMap,
        VkPipelineLayout& pipelineLayout ) -> void {

        // IMPORTANT:
        // In Vulkan, VkPipelineLayoutCreateInfo::pSetLayouts is an array where each element
        // corresponds to a descriptor set index in order:
        //    pSetLayouts[0] -> set = 0
        //    pSetLayouts[1] -> set = 1
        //    pSetLayouts[2] -> set = 2
        // Vulkan does NOT sort or remap them automatically. If the layouts in out.setLayouts
        // are not in the same order as the shader set indices, you will get validation errors.
        // For example, if the fragment shader uses set = 1 but out.setLayouts[1] corresponds
        // to set = 2, Vulkan will complain that the descriptor is missing.
        // Here we are just filling not used slots with empty descriptor set layouts.

        // TODO: VK_EXT_Pipeline library extension
        // [11:59:23] STDERR LOG [thread 67676] Validation Error: Validation Error: [ VUID-VkPipelineLayoutCreateInfo-graphicsPipelineLibrary-06753 ] |
        // MessageID = 0x57ab6143 | vkCreatePipelineLayout(): pCreateInfo->pSetLayouts[0] is VK_NULL_HANDLE, but VK_EXT_graphics_pipeline_library is not enabled.
        // The Vulkan spec states: If graphicsPipelineLibrary is not enabled, elements of pSetLayouts must be valid VkDescriptorSetLayout objects
        // (https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkPipelineLayoutCreateInfo-graphicsPipelineLibrary-06753)

        // Find the highest set index
        u32 maxSet{ 0 };
        for ( const auto& setIndex: bindingLayoutsMap ) {
            maxSet = eastl::max( maxSet, setIndex.first );
        }

        // Initialize everything with "holes" and fill accordingly
        VkDescriptorSetLayout emptySetLayout{ device->GetLayoutForEmptySet() };
        eastl::vector<VkDescriptorSetLayout> setLayouts( maxSet + 1, emptySetLayout );

        // Place set layouts at correct set indices
        for ( const auto& [setIndex, layout]: bindingLayoutsMap ) {
            setLayouts[setIndex] = layout;
        }

        VkPipelineLayoutCreateInfo plInfo{ initializers::PipelineLayoutCreateInfo() };

        plInfo.setLayoutCount = as<u32>( setLayouts.size() );
        plInfo.pSetLayouts = setLayouts.data();

        plInfo.pushConstantRangeCount = as<u32>( mPipelineReflection.mPushConstantRanges.size() );
        plInfo.pPushConstantRanges = mPipelineReflection.mPushConstantRanges.data();

        MKT_VK_CHECK( vkCreatePipelineLayout( device->GetDevice(), &plInfo, nullptr, MKT_ADDRESSOF( pipelineLayout ) ) );
    }

    static auto CreateDescriptorSetLayouts(
        Device* device,
        PipelineReflection& pipelineReflection,
        BindingSetLayoutsMap& bindingLayoutsMap,
        BindingLayoutHandle bindingLayoutHandle = BindingLayoutHandle::CreateEmpty()  ) -> void {
        BindingLayout* bindingLayout{ bindingLayoutHandle.IsEmpty() ?
            nullptr : checked_cast<BindingLayout*>( bindingLayoutHandle.GetRaw() )
        };

        for ( const auto& [setIndex, setBindings]: pipelineReflection.mBindingSetsMap ) {
            if (bindingLayout && setIndex == bindingLayout->GetRegisterSpace()) {
                VkDescriptorSetLayout setLayout{ bindingLayout->GetNativeHandle( ObjectType::Vk_DescriptorSetLayout ) };
                bindingLayoutsMap[setIndex] = setLayout;
                continue;
            }

            // Get the max binding the other ones will be empty
            u32 maxBinding{0};
            for (const auto& item : setBindings) {
                maxBinding = eastl::max( maxBinding, item.second.mBinding );
            }
            std::vector<VkDescriptorSetLayoutBinding> layoutBindings{};
            layoutBindings.resize( maxBinding + 1 );

            // Initialize bindings
            for (u32 index{}; auto& item: layoutBindings ) {
                item = VkDescriptorSetLayoutBinding{
                    .binding = index++,
                    .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, // Dummy
                    .descriptorCount = 1, // Dummy
                    .stageFlags = VK_SHADER_STAGE_ALL , // Dummy
                };
            }

            // Fill accordingly
            for ( const auto& [bindingIndex, bindingInfo]: setBindings ) {
                layoutBindings[bindingIndex] = VkDescriptorSetLayoutBinding{
                    .binding = bindingIndex,
                    .descriptorType = bindingInfo.mType,
                    .descriptorCount = bindingInfo.mCount,
                    .stageFlags = bindingInfo.mStageFlags,
                };
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
            layoutInfo.bindingCount = as<u32>( layoutBindings.size() );
            layoutInfo.pBindings = layoutBindings.data();

            VkDescriptorBindingFlags bindingFlags{ VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT };

            VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
            flagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO_EXT;

            flagsInfo.bindingCount = 1;
            flagsInfo.pBindingFlags = MKT_ADDRESSOF( bindingFlags );

            MKT_VK_CHECK( vkCreateDescriptorSetLayout(
                device->GetDevice(),
                MKT_ADDRESSOF( layoutInfo ),
                nullptr,
                MKT_ADDRESSOF( bindingLayoutsMap[setIndex] ) ) );
        }
    }

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