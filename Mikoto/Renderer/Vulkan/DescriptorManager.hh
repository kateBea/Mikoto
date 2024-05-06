//
// Created by kate on 11/3/23.
//

#ifndef MIKOTO_DESCRIPTOR_MANAGER_HH
#define MIKOTO_DESCRIPTOR_MANAGER_HH

// C++ Standard Libraries
#include <vector>
#include <unordered_map>

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
        auto Allocate(VkDescriptorSet& set, VkDescriptorSetLayout layout) -> bool;
        auto ResetPools() -> void;
        auto Cleanup() -> void;

        struct PoolSizes {
            std::vector<std::pair<VkDescriptorType, Size_T>> sizes{
                { VK_DESCRIPTOR_TYPE_SAMPLER, 1 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
                { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4 },
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1 },
                { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1 },
                { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1 }
            };
        };
    private:
        auto GrabPool() -> VkDescriptorPool;

        VkDescriptorPool m_CurrentPool{ VK_NULL_HANDLE };
        PoolSizes m_DescriptorSizes;

        std::vector<VkDescriptorPool> m_UsedPools;
        std::vector<VkDescriptorPool> m_FreePools;

    };


    /**
     * @class DescriptorLayoutCache
     * @brief Caches descriptor set layouts to avoid recreating already existing ones.
     * */
    class DescriptorLayoutCache {
    public:
        auto Cleanup() -> void;
        auto CreateDescriptorLayout(VkDescriptorSetLayoutCreateInfo* info) -> VkDescriptorSetLayout;

        struct DescriptorLayoutInfo {
            //good idea to turn this into a inlined array
            std::vector<VkDescriptorSetLayoutBinding> Bindings{};

            auto operator==(const DescriptorLayoutInfo& other) const -> bool;

            MKT_NODISCARD auto hash() const -> Size_T;
        };



    private:

        struct DescriptorLayoutHasher {

            auto operator()(const DescriptorLayoutInfo& key) const -> Size_T {
                return key.hash();
            }
        };

        std::unordered_map<DescriptorLayoutInfo, VkDescriptorSetLayout, DescriptorLayoutHasher> m_LayoutCache{};
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

    private:

        std::vector<VkWriteDescriptorSet> m_Writes{};
        std::vector<VkDescriptorSetLayoutBinding> m_Bindings{};

        DescriptorLayoutCache* cache{};
        DescriptorAllocator* alloc{};
    };
}

#endif // MIKOTO_DESCRIPTOR_MANAGER_HH