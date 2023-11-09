//
// Created by kate on 11/3/23.
//

#ifndef MIKOTO_DESCRIPTOR_MANAGER_HH
#define MIKOTO_DESCRIPTOR_MANAGER_HH

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Common/Types.hh>
#include <Common/Common.hh>

namespace Mikoto {
    /**
     * @class DescriptorAllocator
     * @brief Manages allocation of new descriptor sets.
     * Creates new descriptor pools as the are filled.
     * */
    class DescriptorAllocator {
    public:
        auto Init() -> void;
        auto Allocate() -> void;
        auto ResetPools() -> void;
        auto Cleanup() -> void;

    };


    /**
     * @class DescriptorLayoutCache
     * @brief Caches descriptor set layouts to avoid recreating already existing ones.
     * */
    class DescriptorLayoutCache {
    public:
        auto Init() -> void;
        auto Cleanup() -> void;
        auto CreateDescriptorLayout() -> void;
    };


    /**
     * @class DescriptorBuilder
     * @brief Allocates and writes new descriptor sets.
     * */
    class DescriptorBuilder {
        static DescriptorBuilder Begin(DescriptorLayoutCache& layoutCache, DescriptorAllocator& allocator );

        auto BindBuffer( UInt32_T binding, VkDescriptorBufferInfo& bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags ) -> DescriptorBuilder&;

        auto BindImage( UInt32_T binding, VkDescriptorImageInfo& imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags ) -> DescriptorBuilder&;

        MKT_NODISCARD auto Build( VkDescriptorSet& set, VkDescriptorSetLayout& layout ) -> bool;
        MKT_NODISCARD auto Build( VkDescriptorSet& set ) -> bool;
    };
}

#endif // MIKOTO_DESCRIPTOR_MANAGER_HH
