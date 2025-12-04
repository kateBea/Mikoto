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
#include <Renderer/Core/DeviceObject.hh>
#include <Common/Common.hh>
#include <Common/ReferenceCounted.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    class VulkanDevice;

    class DescriptorLayoutBuilder final {
    public:

        auto Build( const void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0 ) const -> VkDescriptorSetLayoutCreateInfo;

        // if the descriptor is an array arraySize != -1
        auto WithBinding( UInt32 binding, VkDescriptorType type,  Int32 arraySize, VkShaderStageFlags shaderStages = VK_SHADER_STAGE_VERTEX_BIT ) -> DescriptorLayoutBuilder&;

    private:
        std::vector<VkDescriptorSetLayoutBinding> m_Bindings{};
    };

    class DescriptorSetLayout final : public DeviceObject {
    public:
        using DeviceObject::Initialize;

        explicit DescriptorSetLayout( const VkDescriptorSetLayoutCreateInfo& info );

        MKT_NODISCARD auto GetNativeHandle(ObjectType type ) -> Object override;

        ~DescriptorSetLayout() override;

    protected:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        VkDescriptorSetLayout m_Layout{ VK_NULL_HANDLE };
        VkDescriptorSetLayoutCreateInfo m_CreateInfo{};
    };

    using DescriptorSetLayoutHandle = Ref<DescriptorSetLayout>;

    // Handles updating descriptor sets
    class DescriptorWriter final {
    public:
        auto WriteBuffer( UInt32 binding, VkBuffer buffer, Size size, Size offset, VkDescriptorType type ) -> DescriptorWriter&;
        auto WriteImage( UInt32 binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type, UInt32 arrayIndex = 0 ) -> DescriptorWriter&;

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
    class DescriptorAllocator final {
    public:
        struct PoolSizeRatio {
            VkDescriptorType Type{};
            float Ratio{};
        };

        auto Init(VulkanDevice* device, UInt32 initialSets, std::span<PoolSizeRatio> poolRatios) -> void;
        auto Shutdown() -> void;

        auto ClearPools() -> void;

        auto Allocate(const VkDescriptorSetLayout* layout, const void* pNext = nullptr) -> VkDescriptorSet*;

    private:
        auto GetPool() -> VkDescriptorPool;
        auto CreatePool(UInt32 setCount, std::span<PoolSizeRatio> poolRatios) -> VkDescriptorPool;


    private:
        inline static float SETS_PER_POOL_GROW_RATE{ 1.5f };
        inline static float MAX_SETS_PER_POOL{ 4092.0f };

        float m_SetsPerPool{};

        VulkanDevice* m_Device{ VK_NULL_HANDLE };

#if defined(MKT_USE_VULKAN_BINDLESS)
        const bool m_IsBindlessEnabled{ true };
#else
        const bool m_IsBindlessEnabled{ false };
#endif

        // how many sets we allocate per pool
        std::vector<PoolSizeRatio> m_Ratios{};

        std::vector<std::pair<VkDescriptorSet, VkDescriptorPool>> m_AllocatedSets{};

        // contains the pools we know we cant allocate from anymore
        std::vector<VkDescriptorPool> m_FullPools{};

        // contains the pools that can still be used, or the freshly created ones.
        std::vector<VkDescriptorPool> m_ReadyPools{};
    };
}// namespace Mikoto

#endif// MIKOTO_DESCRIPTOR_MANAGER_HH