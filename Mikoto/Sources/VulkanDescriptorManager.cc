#include <memory>
#include <algorithm>

#include <volk.h>

#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanDescriptorManager.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>

namespace Mikoto {

    auto DescriptorSetLayout::Release() -> void {
        vkDestroyDescriptorSetLayout(VK_DEVICE(m_Device), m_Layout, nullptr);

        m_IsAllocated = false;
    }

    DescriptorSetLayout::DescriptorSetLayout( const VkDescriptorSetLayoutCreateInfo& info )
        : m_CreateInfo{ info }
    {}

    auto DescriptorSetLayout::GetNativeHandle( ObjectType type )-> Object {
        switch (type) {
            case ObjectType::Vk_DescriptorSetLayout:
                return Object(std::addressof( m_Layout ));
            default:;
        }

        return Object(nullptr);
    }

    DescriptorSetLayout::~DescriptorSetLayout() {
        if (m_IsAllocated) {
            Release();
        }
    }

    auto DescriptorSetLayout::Initialize() -> void {

        if ( vkCreateDescriptorSetLayout( VK_DEVICE(m_Device), std::addressof( m_CreateInfo ), nullptr, std::addressof( m_Layout ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "DescriptorLayoutBuilder::Initialize - Failed to create descriptor set layout." );
        }
    }

    auto DescriptorLayoutBuilder::WithBinding( UInt32 binding, VkDescriptorType type,  Int32 arraySize, VkShaderStageFlags shaderStages ) -> DescriptorLayoutBuilder& {
        VkDescriptorSetLayoutBinding newBinding{
            .binding{ binding },
            .descriptorType{ type },
            .descriptorCount{ static_cast<UInt32>(arraySize != -1 ? arraySize : 1) },
            .stageFlags{ shaderStages },
            .pImmutableSamplers{}
        };

        m_Bindings.emplace_back(newBinding);

        return *this;
    }

    auto DescriptorLayoutBuilder::Build( const void* pNext, const VkDescriptorSetLayoutCreateFlags flags ) const -> VkDescriptorSetLayoutCreateInfo {
        VkDescriptorSetLayoutCreateInfo result{ VulkanHelpers::Initializers::DescriptorSetLayoutCreateInfo() };
        result.pNext = pNext;
        result.pBindings = m_Bindings.data();
        result.bindingCount = static_cast<UInt32>( m_Bindings.size() );
        result.flags = flags;

        return result;
    }

    auto DescriptorWriter::WriteBuffer( const UInt32 binding, const VkBuffer buffer, const Size size, Size offset, VkDescriptorType type ) -> DescriptorWriter& {
        // Descriptor types allowed for a buffer
        // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
        // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC

        // When we want to bind one or the other type into a shader, we set the correct type here.
        // Remember that it needs to match the usage when allocating the VkBuffer

        VkDescriptorBufferInfo& info{ m_BufferInfos.emplace_back(VkDescriptorBufferInfo{
            .buffer{ buffer },
            .offset{ offset },
            .range{ size },
            })
        };

        VkWriteDescriptorSet write{ VulkanHelpers::Initializers::WriteDescriptorSet() };

        write.dstBinding = binding;
        write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
        write.descriptorCount = 1;
        write.descriptorType = type;
        write.pBufferInfo = std::addressof( info );

        m_Writes.push_back(write);

        return *this;
    }

    auto DescriptorWriter::WriteImage( UInt32 binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type ) -> DescriptorWriter& {
        // The layout is going to be almost always either VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        // the best layout to use for accessing textures in the shaders, or VK_IMAGE_LAYOUT_GENERAL
        // when we are using them from compute shaders and writing them.

        VkDescriptorImageInfo& info{ m_ImageInfos.emplace_back(VkDescriptorImageInfo{
            .sampler{ sampler },
            .imageView{ image },
            .imageLayout{ layout }
            })
        };

        VkWriteDescriptorSet write{ VulkanHelpers::Initializers::WriteDescriptorSet() };

        write.dstBinding = binding;
        write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
        write.descriptorCount = 1;
        write.descriptorType = type;
        write.pImageInfo = std::addressof( info );

        m_Writes.push_back(write);

        return *this;
    }

    auto DescriptorWriter::Clear() -> void {
        m_ImageInfos.clear();
        m_Writes.clear();
        m_BufferInfos.clear();
    }

    auto DescriptorWriter::UpdateSet( const VkDevice device, const VkDescriptorSet set ) -> void {
        for (VkWriteDescriptorSet& write : m_Writes) {
            write.dstSet = set;
        }

        vkUpdateDescriptorSets(device, static_cast<UInt32>(m_Writes.size()), m_Writes.data(), 0, nullptr);
    }

    auto DescriptorAllocator::Init( const VkDevice device, UInt32 initialSets, std::span<PoolSizeRatio> poolRatios ) -> void {
        m_Device = device;

        m_Ratios.clear();

        for (const auto& poolRatio : poolRatios) {
            m_Ratios.push_back(poolRatio);
        }

        VkDescriptorPool newPool{ CreatePool(initialSets, poolRatios) };

        m_SetsPerPool = static_cast<float>(initialSets) * SETS_PER_POOL_GROW_RATE; //grow it next allocation

        m_ReadyPools.emplace_back(newPool);
    }

    auto DescriptorAllocator::ClearPools() -> void {
        for (const auto& pool : m_ReadyPools) {
            // This command does not return any failure codes
            vkResetDescriptorPool(m_Device, pool, 0);
        }

        for (const auto& pool : m_FullPools) {
            // This command does not return any failure codes
            vkResetDescriptorPool(m_Device, pool, 0);
            m_ReadyPools.push_back(pool);
        }

        m_FullPools.clear();
    }

    auto DescriptorAllocator::Shutdown() -> void {
        for (const auto& [descriptorSet, descriptorPool] : m_AllocatedSets) {
            vkFreeDescriptorSets(m_Device, descriptorPool, 1, std::addressof(descriptorSet));
        }

        for (const auto& pool : m_ReadyPools) {
            vkDestroyDescriptorPool(m_Device, pool, nullptr);
        }
        m_ReadyPools.clear();

        for (const auto& pool : m_FullPools) {
            vkDestroyDescriptorPool(m_Device, pool, nullptr);
        }
        m_FullPools.clear();
    }

    auto DescriptorAllocator::Allocate( const VkDescriptorSetLayout layout, const void *pNext ) -> VkDescriptorSet* {
        //get or create a pool to allocate from
        VkDescriptorPool poolToUse{ GetPool() };

        VkDescriptorSetAllocateInfo allocInfo{ VulkanHelpers::Initializers::DescriptorSetAllocateInfo() };
        allocInfo.pNext = pNext;
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = poolToUse;
        allocInfo.descriptorSetCount = 1;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = std::addressof( layout );

        VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
        VkResult result{ vkAllocateDescriptorSets(m_Device, std::addressof( allocInfo ), &descriptorSet) };

        // Allocation failed. Try again
        if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {

            m_FullPools.push_back(poolToUse);

            poolToUse = GetPool();
            allocInfo.descriptorPool = poolToUse;

            if (vkAllocateDescriptorSets(m_Device, std::addressof( allocInfo ), std::addressof( descriptorSet )) != VK_SUCCESS) {
                MKT_THROW_RUNTIME_ERROR( "VulkanDescriptorAllocator::Allocate - Failed to allocate descriptor set." );
            }
        }

        m_ReadyPools.push_back(poolToUse);
        m_AllocatedSets.emplace_back(std::make_pair( descriptorSet, poolToUse ));
        return std::addressof( m_AllocatedSets.back().first );
    }

    // The allocation logic will first grab a pool from readyPools, and try to allocate from it.
    // If it succeeds, it will add the pool back into the readyPools array. If it fails, it will
    // put the pool on the fullPools array, and try to get another pool to retry. The GetPool
    // function will pick up a pool from readyPools, or create a new one.
    auto DescriptorAllocator::GetPool() -> VkDescriptorPool {
        VkDescriptorPool newPool;
        if (!m_ReadyPools.empty()) {
            newPool = m_ReadyPools.back();

            // Depending on whether we can allocate from it or not
            // we may add it back to the ready pools or full pools
            m_ReadyPools.pop_back();
        }
        else {
            //need to create a new pool
            newPool = CreatePool(static_cast<UInt32>(m_SetsPerPool), m_Ratios);

            m_SetsPerPool = m_SetsPerPool * SETS_PER_POOL_GROW_RATE;
            m_SetsPerPool = std::min( m_SetsPerPool,  MAX_SETS_PER_POOL );
        }

        return newPool;
    }

    auto DescriptorAllocator::CreatePool( const UInt32 setCount, const std::span<PoolSizeRatio> poolRatios ) -> VkDescriptorPool {
        std::vector<VkDescriptorPoolSize> poolSizes{};

        for (const auto& [Type, Ratio] : poolRatios) {
            poolSizes.emplace_back(VkDescriptorPoolSize{
                .type{ Type },
                .descriptorCount{ static_cast<UInt32>(Ratio) * setCount },
            });
        }

        VkDescriptorPoolCreateInfo poolInfo{ VulkanHelpers::Initializers::DescriptorPoolCreateInfo() };
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = setCount;
        poolInfo.poolSizeCount = static_cast<UInt32>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        VkDescriptorPool newPool{};
        if (vkCreateDescriptorPool(m_Device, std::addressof( poolInfo ), nullptr, std::addressof( newPool )) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR( "VulkanDescriptorAllocator::CreatePool - Failed to create pool." );
        }
        return newPool;
    }


}// namespace Mikoto