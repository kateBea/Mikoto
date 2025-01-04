//
// Created by kate on 11/3/23.
//

#include <memory>
#include <algorithm>

#include <volk.h>

#include <Renderer/Vulkan/VulkanUtils.hh>

#include <Renderer/Vulkan/DescriptorManager.hh>
#include <Renderer/Vulkan/VulkanContext.hh>

namespace Mikoto {
    static auto CreatePool( const DescriptorAllocator::PoolSizes& poolSizes, int count, VkDescriptorPoolCreateFlags flags ) -> VkDescriptorPool {
        std::vector<VkDescriptorPoolSize> sizes{};
        sizes.reserve( poolSizes.sizes.size() );

        for ( auto sz : poolSizes.sizes ) {
            sizes.push_back( { sz.first, UInt32_T ( sz.second * count ) } );
        }

        VkDescriptorPoolCreateInfo poolInfo{ VulkanUtils::Initializers::DescriptorPoolCreateInfo() };
        poolInfo.flags = flags;
        poolInfo.maxSets = count;
        poolInfo.poolSizeCount = ( UInt32_T )sizes.size();
        poolInfo.pPoolSizes = sizes.data();

        VkDescriptorPool descriptorPool{};
        vkCreateDescriptorPool( VulkanContext::GetPrimaryLogicalDevice(), std::addressof(poolInfo), nullptr, std::addressof(descriptorPool) );

        return descriptorPool;
    }


    auto DescriptorAllocator::Cleanup() -> void {
        for ( auto pool : m_FreePools ) {
            vkDestroyDescriptorPool( VulkanContext::GetPrimaryLogicalDevice(), pool, nullptr );
        }

        for ( auto& pool : m_UsedPools ) {
            vkDestroyDescriptorPool( VulkanContext::GetPrimaryLogicalDevice(), pool, nullptr );
        }
    }


    auto DescriptorAllocator::GrabPool() -> VkDescriptorPool {
        // there are reusable pools available
        if ( !m_FreePools.empty() ) {
            // grab the pool from the back of the vector and remove it from there.
            VkDescriptorPool pool{ m_FreePools.back() };
            m_FreePools.pop_back();

            return pool;
        }
        else {
            // no pools available, so create a new one
            return CreatePool( m_DescriptorSizes, 1000, 0 );
        }
    }


    auto DescriptorAllocator::Allocate( VkDescriptorSet& set, VkDescriptorSetLayout layout ) -> bool {
        // initialize the currentPool handle if it's null
        if ( m_CurrentPool == VK_NULL_HANDLE ) {
            m_CurrentPool = GrabPool();
            m_UsedPools.push_back( m_CurrentPool );
        }

        VkDescriptorSetAllocateInfo allocInfo{ VulkanUtils::Initializers::DescriptorSetAllocateInfo() };
        allocInfo.pSetLayouts = std::addressof(layout);
        allocInfo.descriptorPool = m_CurrentPool;
        allocInfo.descriptorSetCount = 1;

        //try to allocate the descriptor set
        VkResult allocResult{ vkAllocateDescriptorSets( VulkanContext::GetPrimaryLogicalDevice(), std::addressof(allocInfo), std::addressof( set ) ) };
        bool needReallocate{ false };

        switch ( allocResult ) {
            case VK_SUCCESS:
                return true;
            case VK_ERROR_FRAGMENTED_POOL:
            case VK_ERROR_OUT_OF_POOL_MEMORY:
                needReallocate = true;
                break;
            default:
                // unrecoverable error
                return false;
        }

        if ( needReallocate ) {
            // allocate a new pool and retry
            m_CurrentPool = GrabPool();
            m_UsedPools.push_back( m_CurrentPool );

            allocResult = vkAllocateDescriptorSets( VulkanContext::GetPrimaryLogicalDevice(), &allocInfo, std::addressof( set ) );

            // if it still fails, then we have big issues
            if ( allocResult == VK_SUCCESS ) {
                return true;
            }
        }

        return false;
    }


    auto DescriptorAllocator::ResetPools() -> void {
        // reset all used pools and add them to the free pools
        for ( auto pool : m_UsedPools ) {
            vkResetDescriptorPool( VulkanContext::GetPrimaryLogicalDevice(), pool, 0 );
            m_FreePools.push_back( pool );
        }

        // clear the used pools, since we've put them all in the free pools
        m_UsedPools.clear();

        // reset the current pool handle back to null
        m_CurrentPool = VK_NULL_HANDLE;
    }


    auto DescriptorLayoutCache::Cleanup() -> void {
        // delete every descriptor layout held
        for ( auto& [layoutInfo, layoutSet] : m_LayoutCache ) {
            vkDestroyDescriptorSetLayout( VulkanContext::GetPrimaryLogicalDevice(), layoutSet, nullptr );
        }
    }


    auto DescriptorLayoutCache::CreateDescriptorLayout( VkDescriptorSetLayoutCreateInfo* info ) -> VkDescriptorSetLayout {
        DescriptorLayoutInfo layoutInfo{};

        layoutInfo.Bindings.reserve( info->bindingCount );
        bool isSorted{ true };
        Int32_T lastBinding{ -1 }; // TODO: review

        // copy from the direct info struct into our own one
        for ( Size_T index{}; index < info->bindingCount; index++ ) {
            layoutInfo.Bindings.push_back( info->pBindings[index] );

            // check that the bindings are in strict increasing order
            Int32_T currentBinding{ static_cast<Int32_T>(info->pBindings[index].binding) };
            if ( currentBinding > lastBinding ) {
                lastBinding = currentBinding;
            }
            else {
                isSorted = false;
            }
        }

        // sort the bindings if they aren't in order
        if ( !isSorted ) {
            std::sort( layoutInfo.Bindings.begin(), layoutInfo.Bindings.end(), []( const VkDescriptorSetLayoutBinding& lhs, const VkDescriptorSetLayoutBinding& rhs ) {
                return lhs.binding < rhs.binding;
            } );
        }

        // try to grab from cache
        auto it{ m_LayoutCache.find( layoutInfo ) };
        if ( it != m_LayoutCache.end() ) {
            return it->second;
        } else {
            // create a new one (not found)
            VkDescriptorSetLayout layout{};
            vkCreateDescriptorSetLayout( VulkanContext::GetPrimaryLogicalDevice(), info, nullptr, std::addressof(layout) );

            // add to cache
            return ( m_LayoutCache[layoutInfo] = layout );
        }
    }


    auto DescriptorLayoutCache::DescriptorLayoutInfo::operator==( const DescriptorLayoutInfo& other ) const -> bool {
        if ( other.Bindings.size() != Bindings.size() ) {
            return false;
        }
        else {
            // Compare each of the bindings is the same. Bindings are sorted so they will match
            for ( Size_T index{}; index < Bindings.size(); index++ ) {
                if ( other.Bindings[index].binding != Bindings[index].binding ) {
                    return false;
                }

                if ( other.Bindings[index].descriptorType != Bindings[index].descriptorType ) {
                    return false;
                }

                if ( other.Bindings[index].descriptorCount != Bindings[index].descriptorCount ) {
                    return false;
                }

                if ( other.Bindings[index].stageFlags != Bindings[index].stageFlags ) {
                    return false;
                }
            }

            return true;
        }
    }


    auto DescriptorLayoutCache::DescriptorLayoutInfo::hash() const -> Size_T {
        Size_T result{ std::hash<Size_T>()( Bindings.size() ) };

        for ( const VkDescriptorSetLayoutBinding& binding: Bindings ) {
            // Pack the binding data into a single int64. Not fully correct but it's ok
            UInt64_T bindingHash{ binding.binding | binding.descriptorType << 8 | binding.descriptorCount << 16 | binding.stageFlags << 24 };

            // Shuffle the packed binding data and xor it with the main hash
            result ^= std::hash<Size_T>()( bindingHash );
        }

        return result;
    }


    auto DescriptorBuilder::Begin( DescriptorLayoutCache& layoutCache, DescriptorAllocator& allocator ) -> DescriptorBuilder {
        DescriptorBuilder builder{};

        builder.cache = std::addressof( layoutCache );
        builder.alloc = std::addressof( allocator );

        return builder;
    }


    auto DescriptorBuilder::BindBuffer( UInt32_T binding, VkDescriptorBufferInfo& bufferInfo, VkDescriptorType type, VkShaderStageFlags stageFlags ) -> DescriptorBuilder& {
        // create the descriptor binding for the layout
        VkDescriptorSetLayoutBinding newBinding{};

        newBinding.descriptorCount = 1;
        newBinding.descriptorType = type;
        newBinding.pImmutableSamplers = nullptr;
        newBinding.stageFlags = stageFlags;
        newBinding.binding = binding;

        m_Bindings.push_back( newBinding );

        // create the descriptor write
        VkWriteDescriptorSet newWrite{ VulkanUtils::Initializers::WriteDescriptorSet()};
        newWrite.descriptorCount = 1;
        newWrite.descriptorType = type;
        newWrite.pBufferInfo = std::addressof( bufferInfo );
        newWrite.dstBinding = binding;

        m_Writes.push_back( newWrite );

        return *this;
    }


    auto DescriptorBuilder::BindImage( UInt32_T binding, VkDescriptorImageInfo& imageInfo, VkDescriptorType type, VkShaderStageFlags stageFlags ) -> DescriptorBuilder& {
        VkDescriptorSetLayoutBinding newBinding{};

        newBinding.descriptorCount = 1;
        newBinding.descriptorType = type;
        newBinding.pImmutableSamplers = nullptr;
        newBinding.stageFlags = stageFlags;
        newBinding.binding = binding;

        m_Bindings.push_back(newBinding);

        VkWriteDescriptorSet newWrite{ VulkanUtils::Initializers::WriteDescriptorSet() };
        newWrite.descriptorCount = 1;
        newWrite.descriptorType = type;
        newWrite.pImageInfo = std::addressof(imageInfo);
        newWrite.dstBinding = binding;

        m_Writes.push_back(newWrite);

        return *this;
    }


    auto DescriptorBuilder::Build( VkDescriptorSet& set, VkDescriptorSetLayout& layout ) -> bool {
        //build layout first
        VkDescriptorSetLayoutCreateInfo layoutInfo{ VulkanUtils::Initializers::DescriptorSetLayoutCreateInfo() };
        layoutInfo.pBindings = m_Bindings.data();
        layoutInfo.bindingCount = m_Bindings.size();

        layout = cache->CreateDescriptorLayout( std::addressof( layoutInfo ) );

        //allocate descriptor
        bool success{ alloc->Allocate( set, layout ) };
        if ( !success ) {
            return false;
        }

        //write descriptor
        for ( VkWriteDescriptorSet& write: m_Writes ) {
            write.dstSet = set;
        }

        vkUpdateDescriptorSets( VulkanContext::GetPrimaryLogicalDevice(), m_Writes.size(), m_Writes.data(), 0, nullptr );

        return true;
    }


    auto DescriptorBuilder::Build( VkDescriptorSet& set ) -> bool {
        VkDescriptorSetLayout layout{};

        return Build( set, layout );
    }
}