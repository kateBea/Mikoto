#include <memory>
#include <algorithm>

#include <volk.h>

#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanDescriptorManager.hh>

namespace Mikoto {

    auto DescriptorLayoutBuilder::Clear() -> void {
        m_Bindings.clear();
    }

    auto DescriptorLayoutBuilder::WithBinding( UInt32_T binding, VkDescriptorType type, VkShaderStageFlags shaderStages ) -> DescriptorLayoutBuilder& {
        VkDescriptorSetLayoutBinding newBinding{
            .binding{ binding },
            .descriptorType{ type },
            .descriptorCount{ 1 }, // always one descriptor, not an array
            .stageFlags{ shaderStages },
            .pImmutableSamplers{}
        };

        m_Bindings.emplace_back(newBinding);

        return *this;
    }

    auto DescriptorLayoutBuilder::Build( const VkDevice device, const void* pNext, VkDescriptorSetLayoutCreateFlags flags ) const -> VkDescriptorSetLayout {
        VkDescriptorSetLayoutCreateInfo info{ VulkanHelpers::Initializers::DescriptorSetLayoutCreateInfo() };
        info.pNext = pNext;
        info.pBindings = m_Bindings.data();
        info.bindingCount = static_cast<UInt32_T>( m_Bindings.size() );
        info.flags = flags;

        VkDescriptorSetLayout set{};
        if ( vkCreateDescriptorSetLayout( device, std::addressof( info ), nullptr, std::addressof( set ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "DescriptorLayoutBuilder::Build - Failed to create descriptor set." );
        }

        return set;
    }

    auto VulkanDescriptorWriter::WriteBuffer( const UInt32_T binding, const VkBuffer buffer, const Size_T size, Size_T offset, VkDescriptorType type ) -> VulkanDescriptorWriter& {
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

    auto VulkanDescriptorWriter::WriteImage( UInt32_T binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type ) -> VulkanDescriptorWriter& {
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

    auto VulkanDescriptorWriter::Clear() -> void {
        m_ImageInfos.clear();
        m_Writes.clear();
        m_BufferInfos.clear();
    }

    auto VulkanDescriptorWriter::UpdateSet( const VkDevice device, const VkDescriptorSet set ) -> void {
        for (VkWriteDescriptorSet& write : m_Writes) {
            write.dstSet = set;
        }

        vkUpdateDescriptorSets(device, static_cast<UInt32_T>(m_Writes.size()), m_Writes.data(), 0, nullptr);
    }

    auto VulkanDescriptorAllocator::Init( const VkDevice device, UInt32_T initialSets, std::span<PoolSizeRatio> poolRatios ) -> void {
        m_Ratios.clear();

        for (const auto& poolRatio : poolRatios) {
            m_Ratios.push_back(poolRatio);
        }

        VkDescriptorPool newPool{ CreatePool(device, initialSets, poolRatios) };

        m_SetsPerPool = static_cast<float>(initialSets) * SETS_PER_POOL_GROW_RATE; //grow it next allocation

        m_ReadyPools.emplace_back(newPool);
    }

    auto VulkanDescriptorAllocator::ClearPools( const VkDevice device ) -> void {
        for (const auto& pool : m_ReadyPools) {
            // This command does not return any failure codes
            vkResetDescriptorPool(device, pool, 0);
        }

        for (const auto& pool : m_FullPools) {
            // This command does not return any failure codes
            vkResetDescriptorPool(device, pool, 0);
            m_ReadyPools.push_back(pool);
        }

        m_FullPools.clear();
    }

    auto VulkanDescriptorAllocator::DestroyPool( const VkDevice device ) -> void {
        for (const auto& pool : m_ReadyPools) {
            vkDestroyDescriptorPool(device, pool, nullptr);
        }
        m_ReadyPools.clear();

        for (const auto& pool : m_FullPools) {
            vkDestroyDescriptorPool(device, pool, nullptr);
        }
        m_FullPools.clear();
    }

    auto VulkanDescriptorAllocator::Allocate( const VkDevice device, const VkDescriptorSetLayout layout, const void *pNext ) -> VkDescriptorSet {
        //get or create a pool to allocate from
        VkDescriptorPool poolToUse{ GetPool(device) };

        VkDescriptorSetAllocateInfo allocInfo{ VulkanHelpers::Initializers::DescriptorSetAllocateInfo() };
        allocInfo.pNext = pNext;
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = poolToUse;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = std::addressof( layout );

        VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
        VkResult result{ vkAllocateDescriptorSets(device, std::addressof( allocInfo ), &descriptorSet) };

        // Allocation failed. Try again
        if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {

            m_FullPools.push_back(poolToUse);

            poolToUse = GetPool(device);
            allocInfo.descriptorPool = poolToUse;

            if (vkAllocateDescriptorSets(device, std::addressof( allocInfo ), std::addressof( descriptorSet )) != VK_SUCCESS) {
                MKT_THROW_RUNTIME_ERROR( "VulkanDescriptorAllocator::Allocate - Failed to allocate descriptor set." );
            }
        }

        m_ReadyPools.push_back(poolToUse);
        return descriptorSet;
    }

    // The allocation logic will first grab a pool from readyPools, and try to allocate from it.
    // If it succeeds, it will add the pool back into the readyPools array. If it fails, it will
    // put the pool on the fullPools array, and try to get another pool to retry. The GetPool
    // function will pick up a pool from readyPools, or create a new one.
    auto VulkanDescriptorAllocator::GetPool( const VkDevice device ) -> VkDescriptorPool {
        VkDescriptorPool newPool;
        if (!m_ReadyPools.empty()) {
            newPool = m_ReadyPools.back();

            // Depending on whether we can allocate from it or not
            // we may add it back to the ready pools or full pools
            m_ReadyPools.pop_back();
        }
        else {
            //need to create a new pool
            newPool = CreatePool(device, static_cast<UInt32_T>(m_SetsPerPool), m_Ratios);

            m_SetsPerPool = m_SetsPerPool * SETS_PER_POOL_GROW_RATE;
            m_SetsPerPool = std::min( m_SetsPerPool,  MAX_SETS_PER_POOL );
        }

        return newPool;
    }

    auto VulkanDescriptorAllocator::CreatePool( const VkDevice device, const UInt32_T setCount, const std::span<PoolSizeRatio> poolRatios ) -> VkDescriptorPool {
        std::vector<VkDescriptorPoolSize> poolSizes{};

        for (const auto& [Type, Ratio] : poolRatios) {
            poolSizes.emplace_back(VkDescriptorPoolSize{
                .type{ Type },
                .descriptorCount{ static_cast<UInt32_T>(Ratio) * setCount },
            });
        }

        VkDescriptorPoolCreateInfo poolInfo{ VulkanHelpers::Initializers::DescriptorPoolCreateInfo() };
        poolInfo.flags = 0;
        poolInfo.maxSets = setCount;
        poolInfo.poolSizeCount = static_cast<UInt32_T>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        VkDescriptorPool newPool{};
        if (vkCreateDescriptorPool(device, std::addressof( poolInfo ), nullptr, std::addressof( newPool )) != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR( "VulkanDescriptorAllocator::CreatePool - Failed to create pool." );
        }
        return newPool;
    }


}// namespace Mikoto