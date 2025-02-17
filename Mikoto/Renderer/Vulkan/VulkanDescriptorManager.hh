//
// Created by kate on 11/3/23.
//

#ifndef MIKOTO_DESCRIPTOR_MANAGER_HH
#define MIKOTO_DESCRIPTOR_MANAGER_HH

// C++ Standard Libraries
#include <vector>
#include <span>
#include <deque>

// Third-Party Libraries
#include <volk.h>

// Project Headers
#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    class DescriptorLayoutBuilder final {
    public:

        auto Clear() -> void;
        auto WithBinding( UInt32_T binding, VkDescriptorType type, VkShaderStageFlags shaderStages = VK_SHADER_STAGE_VERTEX_BIT ) -> DescriptorLayoutBuilder&;
        auto Build( VkDevice device, const void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0 ) const -> VkDescriptorSetLayout;

    private:
        std::vector<VkDescriptorSetLayoutBinding> m_Bindings{};
    };

    // Handles updating descriptor sets
    class VulkanDescriptorWriter final {
    public:
        auto WriteBuffer( UInt32_T binding, VkBuffer buffer, Size_T size, Size_T offset, VkDescriptorType type ) -> VulkanDescriptorWriter&;
        auto WriteImage( UInt32_T binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type ) -> VulkanDescriptorWriter&;

        auto Clear() -> void;
        auto UpdateSet(VkDevice device, VkDescriptorSet set) -> void;

    private:
        // std::deque is guaranteed to keep references to elements valid

        std::deque<VkDescriptorImageInfo> m_ImageInfos{};
        std::deque<VkDescriptorBufferInfo> m_BufferInfos{};
        std::vector<VkWriteDescriptorSet> m_Writes{};
    };

    // Handles allocating descriptor pools
    // Allocates a new one if needed
    class VulkanDescriptorAllocator final {
    public:
        struct PoolSizeRatio {
            VkDescriptorType Type{};
            float Ratio{};
        };

        auto Init(VkDevice device, UInt32_T initialSets, std::span<PoolSizeRatio> poolRatios) -> void;
        auto ClearPools(VkDevice device) -> void;
        auto DestroyPool(VkDevice device) -> void;

        auto Allocate(VkDevice device, VkDescriptorSetLayout layout, const void* pNext = nullptr) -> VkDescriptorSet;

    private:
        auto GetPool(VkDevice device) -> VkDescriptorPool;
        static auto CreatePool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios) -> VkDescriptorPool;


    private:
        inline static float SETS_PER_POOL_GROW_RATE{ 1.5f };
        inline static float MAX_SETS_PER_POOL{ 4092.0f };

        float m_SetsPerPool{};

        // how many sets we allocate per pool
        std::vector<PoolSizeRatio> m_Ratios{};

        // contains the pools we know we cant allocate from anymore
        std::vector<VkDescriptorPool> m_FullPools{};

        // contains the pools that can still be used, or the freshly created ones.
        std::vector<VkDescriptorPool> m_ReadyPools{};
    };
}// namespace Mikoto

#endif// MIKOTO_DESCRIPTOR_MANAGER_HH