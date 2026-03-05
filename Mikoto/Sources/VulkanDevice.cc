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
#include <vk_mem_alloc.h>

// must include Vulkan headers before including TracyVulkan.hpp
#include <Assets/AssetsService.hh>
#include <Core/Profiler.hh>
#include <Filesystem/FileService.hh>
#include <Logging/Logger.hh>
#include <Renderer/Core/RenderService.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDescriptorManager.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>
#include <tracy/TracyVulkan.hpp>

#include <Library/Math/Math.hh>

namespace Mikoto {

    // Device extensions standard
    static constexpr std::array DEVICE_EXTENSIONS{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    struct PhysicalDeviceRequiredFeatures {
        // Support for Anisotropic filtering
        bool AnysotropicFiltering{ true };

        // Support for wireframe mode
        bool FillModeNonSolid{ true };

        // Not null if we want the device to support presentation
        const VkSurfaceKHR* Surface{ nullptr };

        // List of extensions to support
        const std::vector<const char*>* RequestedExtensions{};
    };

    static auto CheckExtensionSupport( const VkPhysicalDevice& device, const std::vector<const char*>& requested ) -> bool {
        UInt32 count{};
        vkEnumerateDeviceExtensionProperties( device, nullptr, &count, nullptr );

        std::vector<VkExtensionProperties> available( count );
        vkEnumerateDeviceExtensionProperties( device, nullptr, &count, available.data() );

        ankerl::unordered_dense::set<std::string_view> names{};
        names.reserve( available.size() );

        for ( const auto& [extensionName, specVersion]: available ) {
            names.emplace( extensionName );
        }

        // We simply check that all requested extensions are in the available ones
        return std::ranges::all_of( requested, [&]( auto r ) { return names.contains( r ); } );
    }

    static auto FetchDeviceQueues( const VkDevice& device, QueuesData& queues ) -> void {
        if ( queues.Graphics.has_value() ) {
            vkGetDeviceQueue( device, queues.Graphics->FamilyIndex, 0, std::addressof( queues.Graphics->Queue ) );
        }

        if ( queues.Present.has_value() ) {
            vkGetDeviceQueue( device, queues.Present->FamilyIndex, 0, std::addressof( queues.Present->Queue ) );
        }

        if ( queues.Compute.has_value() ) {
            vkGetDeviceQueue( device, queues.Compute->FamilyIndex, 0, std::addressof( queues.Compute->Queue ) );
        }
    }

    static auto GetQueueFamilyIndices( const VkPhysicalDevice& device, const VkSurfaceKHR* surface ) -> QueuesData {
        QueuesData result{};

        UInt32 queueFamilyCount{};
        vkGetPhysicalDeviceQueueFamilyProperties( device, std::addressof( queueFamilyCount ), nullptr );

        std::vector<VkQueueFamilyProperties> queueFamilies( queueFamilyCount );
        vkGetPhysicalDeviceQueueFamilyProperties( device, std::addressof( queueFamilyCount ), queueFamilies.data() );

        // Vulkan does not give the family index,
        // so I need to manually keep track of it
        UInt32 queueFamilyIndex{};
        for ( const auto& queueFamilyProperties: queueFamilies ) {
            // Check graphics queue support
            if ( !result.Graphics.has_value() && VulkanHelpers::HasGraphicsQueue( queueFamilyProperties ) ) {
                result.Graphics = std::make_optional( VulkanQueueData{
                        .Queue{ VK_NULL_HANDLE },
                        .FamilyIndex{ queueFamilyIndex },
                } );
            }

            // Check present queue support
            if ( !result.Present.has_value() && surface != nullptr && VulkanHelpers::HasPresentQueue( device, queueFamilyIndex, *surface, queueFamilyProperties ) ) {
                result.Present = std::make_optional( VulkanQueueData{
                        .Queue{ VK_NULL_HANDLE },
                        .FamilyIndex{ queueFamilyIndex },
                } );
            }

            // Check compute queue
            if ( !result.Compute.has_value() && VulkanHelpers::HasComputeQueue( queueFamilyProperties ) ) {
                result.Compute = std::make_optional( VulkanQueueData{
                        .Queue{ VK_NULL_HANDLE },
                        .FamilyIndex{ queueFamilyIndex },
                } );
            }

            ++queueFamilyIndex;
        }

        return result;
    }

    static auto IsDeviceSuitable( const VkPhysicalDevice& device, const PhysicalDeviceRequiredFeatures& requirements ) -> bool {
        // Verify extensions support
        const bool extensionsSupported{ CheckExtensionSupport( device, *requirements.RequestedExtensions ) };

        // Verify queue support (Present is needed if the surface is not null)
        // For now I always want a graphics queue by default for the device
        const auto& [Present, Graphics, Compute]{ GetQueueFamilyIndices( device, requirements.Surface ) };
        const bool deviceSupportsRequiredQueues{ Graphics.has_value() && ( requirements.Surface != nullptr && Present.has_value() ) };

        // Check swapchain support
        bool deviceHasSwapchainSupport{ true };
        if ( extensionsSupported && requirements.Surface != nullptr ) {
            const SwapChainSupportDetails swapChainSupport{ VulkanHelpers::GetSwapChainSupport( device, *requirements.Surface ) };
            deviceHasSwapchainSupport = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
        }

        // Check support physical device features
        VkPhysicalDeviceFeatures supportedFeatures{};
        vkGetPhysicalDeviceFeatures( device, std::addressof( supportedFeatures ) );
        bool supportRequiredPhysicalFeatures{
            // Anisotropic filtering requested and supported
            ( !requirements.AnysotropicFiltering || supportedFeatures.samplerAnisotropy ) &&
            ( !requirements.FillModeNonSolid || supportedFeatures.fillModeNonSolid )
        };

        // Check for dynamic rendering if requested

#if defined( MKT_USE_VULKAN_DYNAMIC_RENDERING )
        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES
        };

        VkPhysicalDeviceFeatures2 features2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &dynamicRenderingFeature
        };

        vkGetPhysicalDeviceFeatures2( device, &features2 );

        supportRequiredPhysicalFeatures = supportRequiredPhysicalFeatures &&
                                          dynamicRenderingFeature.dynamicRendering == VK_TRUE;
#endif


        return extensionsSupported && deviceSupportsRequiredQueues && deviceHasSwapchainSupport && supportRequiredPhysicalFeatures;
    }

    VulkanDeletionQueue::VulkanDeletionQueue( GpuDevice* device ) {
        m_Device = TO_VK_DEVICE( device );
        m_Context = MKT_VK_CTX( RenderService::Get()->GetContext() );
    }

    auto VulkanDeletionQueue::Flush() -> void {
        // Disabled for now
        return;

        const UInt32 currentFrame{ m_Context->GetCurrentFrameIndex() };
        for (const auto& callback : m_Callbacks[currentFrame] ) {
            callback( m_Device );
        }

        m_Callbacks[currentFrame].clear();
    }

    auto VulkanDeletionQueue::Push( std::function<void(GpuDevice*)>&& callback ) -> void {
        std::lock_guard lock{ m_PushMutex };

        const UInt32 currentFrame{ m_Context->GetCurrentFrameIndex() };
        m_Callbacks[currentFrame].emplace_back( std::move( callback ) );
    }

    auto VulkanDeletionQueue::Shutdown() -> void {
        // Run pending cleanup
        for (auto& callbackQueue : m_Callbacks | std::ranges::views::values ) {
            for (const auto& callback : callbackQueue ) {
                callback( m_Device );
            }

            callbackQueue.clear();
        }
    }

    VulkanQueue::VulkanQueue( VulkanDevice* device, QueueType type )
        : m_Device{ device }, m_QueueType{ type }
    {}

    auto VulkanQueue::Init() -> void {

    }

    auto VulkanQueue::Shutdown() -> void {

    }

    auto VulkanQueue::Flush() -> void {
        // Wake submission thread
        // You could use std::vector::swap to move pending command lists to
        // another vector so you do not block threads that are submitting commands
    }

    auto VulkanQueue::AllocateCommandList() -> CommandListHandle {
        return CommandListHandle::CreateEmpty();
    }

    auto VulkanQueue::SubmitCommandList( CommandListHandle cmd ) -> void {

    }

    VulkanDevice::VulkanDevice( const GpuDeviceCreateInfo& createInfo )
        : GpuDevice{ createInfo.Api },
          // For now, we use all these. The idea is to enable extensions based on user request
          // parameters that can be specified in the GpuDeviceCreateInfo struct
          m_RequestedExtensions{ DEVICE_EXTENSIONS.begin(), DEVICE_EXTENSIONS.end() } {
    }

    auto VulkanDevice::Init() -> void {
        MKT_CORE_LOGGER_INFO( "VulkanDevice::Init - Initializing Vulkan Device." );

        GetPrimaryPhysicalDevice();

        // Get the queue family indices for the selected physical device, we will need them to create the logical device
        m_Queues = GetQueueFamilyIndices( m_PhysicalDevice, std::addressof( VulkanContext::Get()->GetSurface() ) );

        CreatePrimaryLogicalDevice();

        FetchDeviceQueues( m_LogicalDevice, m_Queues );

        InitMemoryAllocator();

        // Init pools
        m_Buffers.Init( 10 );
        m_Textures.Init( 10 );
        m_TexturesCube.Init( 10 );
        m_Framebuffers.Init( 10 );
        m_Swapchains.Init( 10 );
        m_GraphicsPipelines.Init( 10 );
        m_ComputePipelines.Init( 10 );
        m_Shaders.Init( 10 );
        m_Samplers.Init( 10 );
        m_DescriptorSetLayouts.Init( 10 );

        m_MainTimeSubmitPool = m_CmdPools.Allocate( QueueType::GRAPHICS_QUEUE, 100 );
        m_MainTimeSubmitPool->Initialize( this );

        m_OneTimeSubmitPool = m_CmdPools.Allocate( QueueType::GRAPHICS_QUEUE, 100 );
        m_OneTimeSubmitPool->Initialize( this );

        InitDescriptorAllocator();

        InitTracyContext();

        CreateDummyResources();

        m_DeletionQueue = CreateScope<VulkanDeletionQueue>( this );

        m_IsInitialized = true;
    }

    auto VulkanDevice::InitTracyContext() -> void {
        VkCommandPoolCreateInfo poolInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = m_Queues.Graphics->FamilyIndex,
        };
        vkCreateCommandPool(m_LogicalDevice, &poolInfo, nullptr, &m_TracyPool);

        VkCommandBufferAllocateInfo allocInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_TracyPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        vkAllocateCommandBuffers(m_LogicalDevice, &allocInfo, &m_TracyCmd);

        VulkanHelpers::SetObjectDebugName( m_LogicalDevice, VK_OBJECT_TYPE_COMMAND_POOL, reinterpret_cast<UInt64>(m_TracyPool), "Mikoto Tracy VkCommandPool" );
        VulkanHelpers::SetObjectDebugName( m_LogicalDevice, VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<UInt64>(m_TracyCmd), "Mikoto Tract VkCommandBuffer" );

        m_TracyContext = TracyVkContext(m_PhysicalDevice, m_LogicalDevice, m_Queues.Graphics->Queue, m_TracyCmd);
    }

    auto VulkanDevice::InitDescriptorAllocator() -> void {
        std::vector<DescriptorAllocator::PoolSizeRatio> sizes{
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };

        m_DescriptorAllocator.Init( this, 1000, sizes );
    }

    auto VulkanDevice::InitMemoryAllocator() -> void {
        m_GpuAllocator = GpuAllocator::Create( this );
        if ( !m_GpuAllocator ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanDevice::InitMemoryAllocator - Could not create GPU Allocator." );
        }

        m_GpuAllocator->Init();
    }

    auto VulkanDevice::GetPrimaryPhysicalDevice() -> void {
        UInt32 deviceCount{ 0 };
        vkEnumeratePhysicalDevices( VulkanContext::Get()->GetInstance(), std::addressof( deviceCount ), nullptr );

        if ( deviceCount == 0 ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanDevice::GetPrimaryPhysicalDevice - Error failed to find GPUs with Vulkan support!" );
        }

        std::vector<VkPhysicalDevice> physicalDevices( deviceCount );
        vkEnumeratePhysicalDevices( VulkanContext::Get()->GetInstance(), std::addressof( deviceCount ), physicalDevices.data() );

        const auto it{
            std::ranges::find_if( physicalDevices,
                                  [&]( const VkPhysicalDevice& device ) -> bool {
                                      const PhysicalDeviceRequiredFeatures reqs{
                                          .AnysotropicFiltering{ true },
                                          .FillModeNonSolid{ true },
                                          .Surface{ std::addressof( VulkanContext::Get()->GetSurface() ) },
                                          .RequestedExtensions{ std::addressof( m_RequestedExtensions ) },
                                      };

                                      return IsDeviceSuitable( device, reqs );
                                  } )
        };

        if ( it == physicalDevices.end() || *it == VK_NULL_HANDLE ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanDevice::GetPrimaryPhysicalDevice - Error could not find a suitable GPU to use as primary physical device!" );
        }

        m_PhysicalDevice = *it;

        // Get the properties and features of the selected physical device
        vkGetPhysicalDeviceFeatures( m_PhysicalDevice, std::addressof( m_PhysicalDeviceInfo.Features ) );
        vkGetPhysicalDeviceProperties( m_PhysicalDevice, std::addressof( m_PhysicalDeviceInfo.Properties ) );
        vkGetPhysicalDeviceMemoryProperties( m_PhysicalDevice, std::addressof( m_PhysicalDeviceInfo.MemoryProperties ) );

        MKT_CORE_LOGGER_DEBUG( "VulkanDevice::GetPrimaryPhysicalDevice - Selected GPU: {}", m_PhysicalDeviceInfo.Properties.deviceName );
    }

    auto VulkanDevice::CreatePrimaryLogicalDevice() -> void {
        if ( !m_Queues.Graphics.has_value() && !m_Queues.Present.has_value() ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanDevice::CreatePrimaryLogicalDevice - No graphics or present queue found!" );
        }

        const auto graphicsQueueFamilyIndex{ m_Queues.Graphics->FamilyIndex };
        const auto presentQueueFamilyIndex{ m_Queues.Present->FamilyIndex };
        const auto queueCreateInfos{ VulkanHelpers::SetupDeviceQueueCreateInfo( { graphicsQueueFamilyIndex, presentQueueFamilyIndex } ) };

        // --- Vulkan 1.3 Features ---
        VkPhysicalDeviceVulkan13Features vulkan13Features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
            .synchronization2 = VK_TRUE,
#if defined( MKT_USE_VULKAN_DYNAMIC_RENDERING )
            .dynamicRendering = VK_TRUE,
#endif
        };

        // --- Vulkan 1.2 Features ---
        VkPhysicalDeviceVulkan12Features vulkan12Features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
            .pNext = &vulkan13Features,
            .descriptorIndexing = VK_TRUE,
            .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
            .descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE,
            .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
            .descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
            .descriptorBindingPartiallyBound = VK_TRUE,
            .descriptorBindingVariableDescriptorCount = VK_TRUE,
            .runtimeDescriptorArray = VK_TRUE,
            .scalarBlockLayout = VK_TRUE,
        };

        // --- Vulkan 1.1 Features 
        VkPhysicalDeviceVulkan11Features vulkan11Features{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
            .shaderDrawParameters = VK_TRUE // Because Slang requires it
        };

        // Link 1.1 to 1.2
        vulkan11Features.pNext = &vulkan12Features;

        // --- Core device features ---
        VkPhysicalDeviceFeatures deviceFeatures{
            .sampleRateShading = VK_TRUE,
            .multiDrawIndirect = VK_TRUE,
            .fillModeNonSolid = VK_TRUE,
            .wideLines = VK_TRUE,
            .samplerAnisotropy = VK_TRUE,
        };

        // --- Final root features struct ---
        VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &vulkan11Features,
            .features = deviceFeatures,
        };

        VkDeviceCreateInfo createInfo{ VulkanHelpers::Initializers::DeviceCreateInfo() };
        createInfo.queueCreateInfoCount = static_cast<UInt32>( queueCreateInfos.size() );
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = nullptr;
        createInfo.enabledExtensionCount = static_cast<UInt32>( m_RequestedExtensions.size() );
        createInfo.ppEnabledExtensionNames = m_RequestedExtensions.data();
        createInfo.pNext = std::addressof( physicalDeviceFeatures2 );

        // Store currently enabled 1.2 features to query them later
        m_Vulkan12EnabledFeatures = vulkan12Features;

        // might not be necessary anymore because device-specific validation layers have been deprecated
        // even tho recommended for some backwards compatibility as they are required for some Vulkan implementations
        const auto& validationLayers{ VulkanContext::Get()->GetValidationLayers() };
        if ( !validationLayers.empty() ) {
            // These two fields are ignored by up-to-date Vulkan implementations
            createInfo.enabledLayerCount = static_cast<UInt32>( validationLayers.size() );
            createInfo.ppEnabledLayerNames = validationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if ( vkCreateDevice( m_PhysicalDevice, std::addressof( createInfo ), nullptr, std::addressof( m_LogicalDevice ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanDevice::CreatePrimaryLogicalDevice - Failed to create primary logical device!" );
        }

        /**
          * [...] all device-related function calls, such as vkCmdDraw, will go through Vulkan loader dispatch code.
          * This allows you to transparently support multiple VkDevice objects in the same application, but comes at
          * a price of dispatch overhead which can be as high as 7% depending on the driver and application.
          *
          * To avoid this, For applications that use just one VkDevice object, load device-related
          * Vulkan entry-points directly from the driver with void volkLoadDevice(VkDevice device);
          * See: https://github.com/zeux/volk
          * */
        volkLoadDevice( m_LogicalDevice );

        MKT_CORE_LOGGER_DEBUG( "VulkanDevice::CreatePrimaryLogicalDevice - Created primary logical device." );
    }

    auto VulkanDevice::ShutdownTracyContext() -> void {
        vkFreeCommandBuffers(m_LogicalDevice, m_TracyPool, 1, &m_TracyCmd);
        vkDestroyCommandPool(m_LogicalDevice, m_TracyPool, nullptr);
        TracyVkDestroy(m_TracyContext);
    }


    auto VulkanDevice::Shutdown() -> void {
        if ( !m_IsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "VulkanDevice::Shutdown - Shutting down Vulkan Device." );

        // Wait for pending operations
        WaitQueuesIdle();

        DestroyDummyResources();

        m_AvailableGraphicsCommandLists.clear();
        m_PendingGraphicsCommandLists.clear();
        m_SubmittedGraphicsCommandLists.clear();

        m_FrameFences.clear();

        // Clear resources pools
        m_Textures.Shutdown();
        m_TexturesCube.Shutdown();
        m_Buffers.Shutdown();
        m_CmdPools.Shutdown();
        m_Framebuffers.Shutdown();
        m_Swapchains.Shutdown();
        m_GraphicsPipelines.Shutdown();
        m_ComputePipelines.Shutdown();
        m_Shaders.Shutdown();
        m_Samplers.Shutdown();
        m_DescriptorSetLayouts.Shutdown();

        m_DescriptorAllocator.Shutdown();

        m_DeletionQueue->Shutdown();
        m_DeletionQueue = nullptr;

        if ( m_GpuAllocator ) {
            m_GpuAllocator->Shutdown();
            m_GpuAllocator = nullptr;
        }

        ShutdownTracyContext();

        m_MainTimeSubmitPool.Reset();
        m_OneTimeSubmitPool.Reset();

        vkDestroyDevice( m_LogicalDevice, nullptr );

        m_IsInitialized = false;
    }

    auto VulkanDevice::CreateTexture( const TextureCubeCreateDescription &description ) -> TextureHandle {
        m_TextureCubePoolMutex.lock();
        TextureHandle texture{ m_TexturesCube.Allocate( description ) };
        m_TextureCubePoolMutex.unlock();

        if ( texture.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreateTextureCube - Failed to allocate texture cube resource." );
            return TextureHandle::CreateEmpty();
        }

        texture->Initialize( this );

        return texture;
    }

    auto VulkanDevice::WaitIdle() const -> void {
        vkDeviceWaitIdle( m_LogicalDevice );
    }

    auto VulkanDevice::WaitQueuesIdle() const -> void {
        if ( m_Queues.Graphics.has_value() ) {
            vkQueueWaitIdle( m_Queues.Graphics->Queue );
        }

        if ( m_Queues.Compute.has_value() ) {
            vkQueueWaitIdle( m_Queues.Compute->Queue );
        }

        if ( m_Queues.Present.has_value() ) {
            vkQueueWaitIdle( m_Queues.Present->Queue );
        }
    }

    auto VulkanDevice::GetTracyContext() -> TracyVkCtx& {
        return m_TracyContext;
    }

    auto VulkanDevice::SetCurrentFrameIndex( const UInt32 frameIndex ) -> void {
        m_CurrentFrameIndex = frameIndex;

        std::ranges::move(
            m_SubmittedGraphicsCommandLists[m_CurrentFrameIndex],
            std::back_inserter(m_AvailableGraphicsCommandLists[m_CurrentFrameIndex]));

        m_SubmittedGraphicsCommandLists[m_CurrentFrameIndex].clear();

        m_DeletionQueue->Flush();
    }

    auto VulkanDevice::SubmitDeletion( std::function<void(GpuDevice*)> &&callback ) -> void {
        m_DeletionQueue->Push( std::move(callback) );
    }

    auto VulkanDevice::FlushImmediateCommands() -> void {
        std::vector<CommandListHandle> submisionList{};
        {
            std::lock_guard lock{ m_OneTimeSubmitMutex };
            submisionList.swap( m_ImmediateSubmitCmds );
        }

        std::vector<VkCommandBufferSubmitInfo> cmds{};
        for ( const auto& cmd: submisionList ) {
            VkCommandBufferSubmitInfo cmdInfo{
                .sType{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO },
                .commandBuffer{ cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ) },
                .deviceMask{ 0 }
            };

            cmds.emplace_back( cmdInfo );
        }

        if (cmds.empty()) {
            return;
        }

        VkSubmitInfo2 submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .commandBufferInfoCount = static_cast<UInt32>( cmds.size() ),
            .pCommandBufferInfos = cmds.data(),
        };

        MKT_VK_CHECK( vkQueueSubmit2( m_Queues.Graphics->Queue, 1, &submitInfo, VK_NULL_HANDLE ) );
    }

    auto VulkanDevice::CreateTexture( const TextureDescription& description ) -> TextureHandle {
        m_TexturePoolMutex.lock();
        TextureHandle texture{ m_Textures.Allocate( description ) };
        m_TexturePoolMutex.unlock();

        if ( texture.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreateTexture - Failed to allocate texture resource." );
            return TextureHandle::CreateEmpty();
        }

        texture->Initialize( this );

        return texture;
    }

    auto VulkanDevice::GetNativeHandle( ObjectType type ) -> Object {
        return Object( m_LogicalDevice );
    }


    auto VulkanDevice::GetMemoryUsage() const -> Size {
        return m_GpuAllocator->GetMemoryUsage();
    }

    auto VulkanDevice::GetMemoryTotal() const -> Size {
        return m_GpuAllocator->GetMemoryTotal();
    }

    auto VulkanDevice::GetMemoryAvailable() const -> Size {
        return m_GpuAllocator->GetMemoryAvailable();
    }

    auto VulkanDevice::GetDummySampler() const -> SamplerHandle {
        return m_sampler;
    }

    auto VulkanDevice::CreateStaging( const void* src, Size size ) -> BufferHandle {
        m_StagingBufferPoolMutex.lock();
        BufferHandle buffer{ m_Buffers.Allocate( src, size ) };
        m_StagingBufferPoolMutex.unlock();

        if ( buffer.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreateStaging - Failed to allocate buffer resource." );
            return BufferHandle::CreateEmpty();
        }

        buffer->Initialize( this );

        return buffer;
    }

    auto VulkanDevice::GetDummyDescriptorLayout() -> DescriptorSetLayoutHandle {
        return m_EmptyDescriptorSetLayout;
    }

    auto VulkanDevice::CreateDummyResources() -> void {
        // Descriptor set layout
        VkDescriptorSetLayoutCreateInfo emptyInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 0,
            .pBindings = nullptr
        };

        m_EmptyDescriptorSetLayout = AllocateDescriptorSetLayout( emptyInfo );

        // Sampler
        m_sampler = CreateSampler( SamplerDescription{} );
    }

    auto VulkanDevice::DestroyDummyResources() -> void {
        m_sampler.Reset();
        m_EmptyDescriptorSetLayout.Reset();
    }

    auto VulkanDevice::CreateCommandList( QueueType queueType, bool immediate ) -> CommandListHandle {
        CommandListHandle resultCommandList{};

        if (!immediate) {
            std::lock_guard lock{ m_CommandCreateMutex };
            // FIXME: Command pools cannot be shared, also command buffers from same pool cannot be used by multiple threads in paralel

            auto& currentFrameCmdLists{ m_AvailableGraphicsCommandLists[m_CurrentFrameIndex] };

            if (currentFrameCmdLists.empty() ) {
                constexpr UInt32 growCount{ 10 };

                currentFrameCmdLists.reserve( growCount );

                for (UInt32 count{}; count < growCount; ++count) {
                    CommandListHandle cmd{};

                    // TODO: Support other queue types
                    if ( !m_MainTimeSubmitPool.IsEmpty() ) {
                        cmd = m_MainTimeSubmitPool->AllocateCmdList( immediate );

                        if ( cmd.IsEmpty() ) {
                            MKT_THROW_RUNTIME_ERROR( "VulkanDevice::CreateCommandList - Failed to allocate command list." );
                        }

                        currentFrameCmdLists.emplace_back( cmd );
                    }
                }
            }

            resultCommandList = currentFrameCmdLists.back();
            currentFrameCmdLists.pop_back();
        } else {
            std::lock_guard lock{ m_OneTimeSubmitMutex };
            resultCommandList = m_OneTimeSubmitPool->AllocateCmdList( immediate );
        }

        return resultCommandList;
    }

    auto VulkanDevice::CreateBuffer( const BufferDescription& description ) -> BufferHandle {
        std::lock_guard lock{ m_BufferPoolMutex };

        BufferHandle buffer{ m_Buffers.Allocate( description ) };
        if ( buffer.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreateBuffer - Failed to allocate buffer resource." );
            return BufferHandle::CreateEmpty();
        }

        buffer->Initialize( this );

        return buffer;
    }

    auto VulkanDevice::CreateFrameBuffer( const FramebufferDescription& description ) -> FramebufferHandle {
        std::lock_guard lock{ m_FramebufferPoolMutex };

        FramebufferHandle framebuffer{ m_Framebuffers.Allocate( description ) };
        if ( framebuffer.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreateFrameBuffer - Failed to allocate framebuffer resource." );
            return FramebufferHandle::CreateEmpty();
        }

        framebuffer->Initialize( this );

        return framebuffer;
    }

    auto VulkanDevice::CreateSampler( const SamplerDescription& description ) -> SamplerHandle {
        std::lock_guard lock{ m_SamplerPoolMutex };

        SamplerHandle sampler{ m_Samplers.Allocate( description ) };
        if ( sampler.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreateSampler - Failed to allocate sampler resource." );
            return SamplerHandle::CreateEmpty();
        }

        sampler->Initialize( this );

        return sampler;
    }

    auto VulkanDevice::CreatePipeline( const ComputePipelineDescription& description ) -> PipelineHandle {
        std::lock_guard lock{ m_ComputePipelinePoolMutex };

        PipelineHandle computePipeline{ m_ComputePipelines.Allocate( description ) };
        if ( computePipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreatePipeline - Failed to allocate compute pipeline resource." );
            return PipelineHandle::CreateEmpty();
        }

        computePipeline->Initialize( this );

        return computePipeline;
    }

    auto VulkanDevice::CreatePipeline( const GraphicsPipelineDescription& description ) -> PipelineHandle {
        std::lock_guard lock{ m_GraphicsPipelinePoolMutex };

        VulkanGraphicsPipelineDescription defaultInfo{
            .Desc{ description }
        };

        PipelineHandle graphicsPipeline{ m_GraphicsPipelines.Allocate( defaultInfo ).As<Pipeline>() };
        if ( graphicsPipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreatePipeline - Failed to allocate graphics pipeline resource." );
            return PipelineHandle::CreateEmpty();
        }

        graphicsPipeline->Initialize( this );

        return graphicsPipeline;
    }

    auto VulkanDevice::LoadShader( const Path& path, ShaderStage stage ) -> ShaderModuleHandle {
        std::lock_guard lock{ m_ShaderPoolMutex };

        ShaderModuleHandle result{ ShaderModuleHandle::CreateEmpty() };
        if ( const File * shaderFile{ FileService::Get()->LoadFile( path ) } ) {
            ShaderModuleDescription description{
                .ShaderFile{ shaderFile },
                .Stage{ stage }
            };

            result = m_Shaders.Allocate( description ).As<ShaderModule>();
            if ( !result.IsEmpty() ) {
                result->Initialize( this );
            } else {
                MKT_CORE_LOGGER_ERROR( "VulkanDevice::LoadShader - Failed to create shader." );
            }
        }

        return result;
    }

    auto VulkanDevice::GetDeviceName() const -> std::string_view {
        return m_PhysicalDeviceInfo.Properties.deviceName;
    }

    auto VulkanDevice::GetUniformBufferMinOffsetAlignment() const -> VkDeviceSize {
        return m_PhysicalDeviceInfo.Properties.limits.minUniformBufferOffsetAlignment;
    }

    auto VulkanDevice::GetStorageBufferMinOffsetAlignment() const -> VkDeviceSize {
        return m_PhysicalDeviceInfo.Properties.limits.minStorageBufferOffsetAlignment;
    }

    auto VulkanDevice::GetPhysicalDevice() const -> const VkPhysicalDevice& {
        return m_PhysicalDevice;
    }

    auto VulkanDevice::GetPhysicalDeviceFeatures() const -> const VkPhysicalDeviceFeatures& {
        return m_PhysicalDeviceInfo.Features;
    }

    auto VulkanDevice::GetPhysicalDeviceProperties() const -> const VkPhysicalDeviceProperties& {
        return m_PhysicalDeviceInfo.Properties;
    }

    auto VulkanDevice::GetPhysicalDeviceMemoryProperties() const -> const VkPhysicalDeviceMemoryProperties& {
        return m_PhysicalDeviceInfo.MemoryProperties;
    }

    auto VulkanDevice::GetAllocator() -> GpuAllocator* {
        return m_GpuAllocator.get();
    }

    auto VulkanDevice::GetAllocator() const -> const GpuAllocator* {
        return m_GpuAllocator.get();
    }

    auto VulkanDevice::GetLogicalDevice() const -> const VkDevice& {
        return m_LogicalDevice;
    }

    auto VulkanDevice::GetLogicalDeviceQueues() const -> const QueuesData& {
        return m_Queues;
    }

    auto VulkanDevice::IsScalarBlockLayoutEnabled() const -> bool {
        return m_Vulkan12EnabledFeatures.scalarBlockLayout == VK_TRUE;
    }

    auto VulkanDevice::AllocateDescriptorSet( const VkDescriptorSetLayout* layout, const void* pNext ) -> VkDescriptorSet {
        VkDescriptorSet result{ *m_DescriptorAllocator.Allocate( layout, pNext ) };
        if ( result != VK_NULL_HANDLE ) {
            return result;
        }

        return VK_NULL_HANDLE;
    }

    auto VulkanDevice::AllocateDescriptorSetLayout( const VkDescriptorSetLayoutCreateInfo& layout ) -> DescriptorSetLayoutHandle {
        std::lock_guard lock{ m_DescriptorSetLayoutPoolMutex };

        DescriptorSetLayoutHandle setLayout{ m_DescriptorSetLayouts.Allocate( layout ) };
        if ( setLayout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::AllocateDescriptorSetLayout - Failed to allocate texture resource with VkImageViewCreateInfo." );
            return DescriptorSetLayoutHandle::CreateEmpty();
        }

        setLayout->Initialize( this );

        return setLayout;
    }

    auto VulkanDevice::SubmitCommands( CommandListHandle cmd ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (cmd->IsImmediate()) {
            std::lock_guard lock{ m_OneTimeSubmitMutex };
            m_ImmediateSubmitCmds.push_back( cmd );
        } else {
            std::lock_guard lock{ m_CommandSubmitMutex };
            m_PendingGraphicsCommandLists[m_CurrentFrameIndex].emplace_back( cmd );
        }
    }

    auto VulkanDevice::CreateSwapChain( const VulkanSwapChainCreateInfo& createInfo ) -> SwapChainHandle {
        SwapChainHandle result{ m_Swapchains.Allocate( createInfo ) };
        if ( !result.IsEmpty() ) {
            result->Initialize( this );
        }

        return result;
    }

    auto VulkanDevice::CreateSwapChainTextures( const VkImageViewCreateInfo& createInfo, VkExtent2D extent ) -> TextureHandle {
        TextureHandle texture{ m_Textures.Allocate( createInfo, extent ) };
        if ( texture.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreateBuffer - Failed to allocate texture resource with VkImageViewCreateInfo." );
            return TextureHandle::CreateEmpty();
        }

        texture->Initialize( this );

        return texture;
    }

    auto VulkanDevice::IsBindlessEnabled() const -> bool {
        return m_IsBindlessEnabled;
    }

    auto VulkanDevice::RunGarbageCollection() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // Check reference counting and delete those
        // that are being held by the pool exclusively
        m_Buffers.RunGarbageCollection();
    }

    auto VulkanDevice::FlushPendingCommands( const FrameSynchronizationPrimitives& syncPrimitives ) -> void {
        MKT_BEGIN_PROFILER_NAMED();

        // This fence will be used to know the state of the commands submitted in this call
        m_FrameFences[m_CurrentFrameIndex] = syncPrimitives.RenderFence;

        std::vector<VkCommandBufferSubmitInfo> cmdInfos{};
        cmdInfos.reserve( m_PendingGraphicsCommandLists[m_CurrentFrameIndex].size() );

        for ( auto& commandBuffer: m_PendingGraphicsCommandLists[m_CurrentFrameIndex] ) {
            cmdInfos.push_back( VkCommandBufferSubmitInfo{
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                    .pNext = nullptr,
                    .commandBuffer = commandBuffer->GetNativeHandle( ObjectType::Vk_CmdBuffer ),
                    .deviceMask = 0 } );
        }

        // Wait on image-available semaphore
        VkSemaphoreSubmitInfo waitSemaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = syncPrimitives.ImageAvailableSemaphore,
            .value = 0,
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .deviceIndex = 0
        };

        // Signal render-finished semaphore
        VkSemaphoreSubmitInfo signalSemaphoreInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = syncPrimitives.RenderFinishedSemaphore,
            .value = 0,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
            .deviceIndex = 0
        };

        VkSubmitInfo2 submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &waitSemaphoreInfo,
            .commandBufferInfoCount = static_cast<UInt32>( cmdInfos.size() ),
            .pCommandBufferInfos = cmdInfos.data(),
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signalSemaphoreInfo
        };

        {

        }
        MKT_VK_CHECK( vkQueueSubmit2( m_Queues.Graphics->Queue, 1, &submitInfo, syncPrimitives.RenderFence ) );

        std::ranges::move(
            m_PendingGraphicsCommandLists[m_CurrentFrameIndex],
            std::back_inserter(m_SubmittedGraphicsCommandLists[m_CurrentFrameIndex]));

        m_PendingGraphicsCommandLists[m_CurrentFrameIndex].clear();
    }

    VulkanCmdList::VulkanCmdList( const VkCommandBufferAllocateInfo& createInfo, QueueType type, bool immediate )
        : ICommandList{ immediate, type }, m_AllocInfo{ createInfo } {
    }

    auto VulkanCmdList::Begin() -> void {
        // Begin recording command buffer
        VkCommandBufferBeginInfo beginInfo{ VulkanHelpers::Initializers::CommandBufferBeginInfo() };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

        if (m_IsImmediate) {
            // Overwrite flag, one time submit and simultaneous are exclusive
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        }

        if ( vkBeginCommandBuffer( m_CmdBuffer, std::addressof( beginInfo ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanCmdList::Close - Failed to begin recording ImGui command buffer" );
        }

        //TracyVkZone(TO_VK_DEVICE( m_Device )->GetTracyContext(), m_CmdBuffer, "VulkanCmdList::Begin");
    }

    auto VulkanCmdList::End() -> void {
        //TracyVkCollect(TO_VK_DEVICE( m_Device )->GetTracyContext(), m_CmdBuffer);

        if ( vkEndCommandBuffer( m_CmdBuffer ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanCmdList::Close - Failed to record command buffer!" );
        }
    }

    auto VulkanCmdList::BeginRender( RenderInfo &info ) -> void {
        // This path requires dynamic re
        std::vector<VkRenderingAttachmentInfo> colorImages{};

        for (auto &colorImage: info.ColorRenderTargets) {
            VkAttachmentLoadOp loadOp{ info.ColorLoadOp == LoadOp::CLEAR ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD };
            VkRenderingAttachmentInfo &colorAttachment{ colorImages.emplace_back( VkRenderingAttachmentInfo{} ) };
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = colorImage->GetNativeHandle( ObjectType::Vk_ImageView );
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = loadOp;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = { info.ClearColor.r, info.ClearColor.g, info.ClearColor.b, info.ClearColor.a };
        }

        VkRenderingAttachmentInfo depthAttachment{};
        if (!info.DepthRenderTarget.IsEmpty()) {
            VkAttachmentLoadOp loadOp{ info.DephtLoadOp == LoadOp::CLEAR ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD };

            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = info.DepthRenderTarget->GetNativeHandle( ObjectType::Vk_ImageView );
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = loadOp;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachment.clearValue.depthStencil = { 1.0f, 0 };
        }

        // Dimensions
        const UInt32 width{ static_cast<UInt32>( static_cast<float>( info.ColorRenderTargets.front()->GetWidth() ) ) };
        const UInt32 height{ static_cast<UInt32>( static_cast<float>( info.ColorRenderTargets.front()->GetHeight() ) ) };

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = { { 0, 0 }, { width, height } };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = static_cast<UInt32>( colorImages.size() );
        renderingInfo.pColorAttachments = colorImages.data();
        renderingInfo.pDepthAttachment = info.DepthRenderTarget.IsEmpty() ? nullptr : std::addressof( depthAttachment );

        vkCmdBeginRendering( m_CmdBuffer, std::addressof( renderingInfo ) );
    }

    auto VulkanCmdList::EndRender(RenderInfo& info) -> void {
        vkCmdEndRendering( m_CmdBuffer );
    }

    auto VulkanCmdList::FillTexture( Buffer* src, Texture* dest ) -> void {

        if (dest != nullptr && dest->GetTextureUsage() == TextureUsage::CUBE) {
            FillCubeTexture( src, dest );
        }

        // Cast to Vulkan-specific implementations
        auto* vkSrc{ dynamic_cast<VulkanBuffer*>( src ) };
        auto* vkDest{ dynamic_cast<VulkanTexture*>( dest ) };

        if ( !vkSrc || !vkDest ) {
            return;
        }

        // Ensure the destination image is in TRANSFER_DST layout
        vkDest->SubmitLayoutTransition( VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_CmdBuffer );

        // Describe the region to copy
        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;  // Tightly packed
        copyRegion.bufferImageHeight = 0;// Tightly packed

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;

        copyRegion.imageOffset = { 0, 0, 0 };
        copyRegion.imageExtent = {
            vkDest->GetCreateInfo().extent.width,
            vkDest->GetCreateInfo().extent.height,
            1
        };

        if (src->GetUsage() == BufferUsage::INDEX) {
            MKT_CORE_LOGGER_ERROR( "VulkanCmdList::FillTexture - Trying to fill a texture from an index buffer. This is not supported." );
        }

        // Issue the copy command
        vkCmdCopyBufferToImage(
                m_CmdBuffer,
                vkSrc->GetNativeHandle(ObjectType::Vk_Buffer),
                vkDest->GetNativeHandle(ObjectType::Vk_Image),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copyRegion );

        // Transition the image to SHADER_READ_ONLY for sampling
        vkDest->SubmitLayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_CmdBuffer );
    }

    auto VulkanCmdList::FillTexture( const void* src, Size size, Texture* dest ) -> void {
        BufferHandle staging{ TO_VK_DEVICE( m_Device )->CreateStaging( src, size ) };
        this->FillTexture( staging.GetRaw(), dest );
    }

    auto VulkanCmdList::CopyBuffer( Buffer* src, Buffer* dest ) -> void {
        VkBufferCopy copy{
            .srcOffset{ 0 },
            .dstOffset{ 0 },
            .size{ src->GetSizeBytes() },
        };

        vkCmdCopyBuffer(
            m_CmdBuffer,
            src->GetNativeHandle(ObjectType::Vk_Buffer),
            dest->GetNativeHandle(ObjectType::Vk_Buffer),
            1,
            std::addressof(copy));

        VkAccessFlags accessFlags{ VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT };

        // It is either vertex or index
        if (src->IsUsage(BufferUsage::INDEX)) {
            accessFlags = VK_ACCESS_INDEX_READ_BIT;
        }

        // VK_QUEUE_FAMILY_IGNORED Because queue family indices are
        // unified see https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
        // Otherwise we would need to specify the indices explicitly

        const VkBufferMemoryBarrier barrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = accessFlags,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = dest->GetNativeHandle(ObjectType::Vk_Buffer),
            .offset = 0,
            .size = src->GetSizeBytes()
        };

        vkCmdPipelineBarrier(
            m_CmdBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
            VK_FLAGS_NONE,
            0, nullptr,
            1, &barrier,
            0, nullptr
        );
    }

    auto VulkanCmdList::CopyBuffer( const void* src, Size size, Buffer* dest ) -> void {
        BufferHandle staging{ TO_VK_DEVICE( m_Device )->CreateStaging( src, size ) };
        this->CopyBuffer( staging.GetRaw(), dest );
    }

    auto VulkanCmdList::CopyTexture( Texture* srcTexture, Texture* destTexture ) -> void {
        const auto src{ dynamic_cast<VulkanTexture*>( srcTexture ) };
        const auto dest{ dynamic_cast<VulkanTexture*>( destTexture ) };

        // Layout transition for transfer optimal
        src->SubmitLayoutTransition( VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_CmdBuffer );
        dest->SubmitLayoutTransition( VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_CmdBuffer );

        // Define copy region
        VkExtent3D srcExtent{};
        srcExtent.width = src->GetWidth();
        srcExtent.height = src->GetHeight();
        srcExtent.depth = 1;

        VkExtent3D dstExtent{};
        dstExtent.width = dest->GetWidth();
        dstExtent.height = dest->GetHeight();
        dstExtent.depth = 1;

        VulkanHelpers::CopyImageToImage( m_CmdBuffer, src->GetNativeHandle(ObjectType::Vk_Image), dest->GetNativeHandle(ObjectType::Vk_Image), srcExtent, dstExtent );

        // Reset layout
        src->SubmitLayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_CmdBuffer );

        if ( !dest->IsSwapChainImage() ) {
            dest->SubmitLayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_CmdBuffer );
        } else {
            dest->SubmitLayoutTransition( VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, m_CmdBuffer );
        }
    }

    auto VulkanCmdList::CopyTexture( Texture2D* src, TextureCube* dest, UInt32 mipLevel, UInt32 face ) -> void {
        const auto vulkanSrc{ dynamic_cast<VulkanTexture*>( src ) };
        const auto vulkanDest{ dynamic_cast<VulkanTextureCube*>( dest ) };

        auto srcOriginalLayout{ vulkanSrc->GetCurrentLayout() };
        auto destOriginalLayout{ vulkanDest->GetCurrentLayout() };

        // Transition src to SRC/DST optimal
        vulkanSrc->SubmitLayoutTransition( VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_CmdBuffer );
        vulkanDest->SubmitLayoutTransition( VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_CmdBuffer );

        // Copy
        VkImageCopy copyRegion{};
        copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.srcSubresource.baseArrayLayer = 0;
        copyRegion.srcSubresource.mipLevel = 0;
        copyRegion.srcSubresource.layerCount = 1;
        copyRegion.srcOffset = { 0, 0, 0 };

        copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.dstSubresource.baseArrayLayer = face;
        copyRegion.dstSubresource.mipLevel = mipLevel;
        copyRegion.dstSubresource.layerCount = 1;
        copyRegion.dstOffset = { 0, 0, 0 };

        float width{  static_cast<float>(vulkanSrc->GetWidth() * std::pow(0.5f, mipLevel)) };
        float height{  static_cast<float>(vulkanSrc->GetHeight() * std::pow(0.5f, mipLevel)) };

        copyRegion.extent.width = static_cast<UInt32>(width);
        copyRegion.extent.height = static_cast<UInt32>(height);
        copyRegion.extent.depth = 1;

        vkCmdCopyImage(
            m_CmdBuffer,
            src->GetNativeHandle(ObjectType::Vk_Image),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            dest->GetNativeHandle(ObjectType::Vk_Image),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            std::addressof( copyRegion ) );

        // Transition SRC/DST back
        vulkanSrc->SubmitLayoutTransition( srcOriginalLayout, m_CmdBuffer );
        vulkanDest->SubmitLayoutTransition( destOriginalLayout, m_CmdBuffer );
    }

    auto VulkanCmdList::SetPolygonLineWidth( float value ) -> void {
        // Check
        const auto& physicalProperties{ TO_VK_DEVICE( m_Device )->GetPhysicalDeviceProperties() };
        const auto& deviceFeatures{ TO_VK_DEVICE( m_Device )->GetPhysicalDeviceFeatures() };

        float minLineWidth{ physicalProperties.limits.lineWidthRange[0] };
        float maxLineWidth{ physicalProperties.limits.lineWidthRange[1] };

        if (!Math::IsBetween(value, minLineWidth, maxLineWidth)) {
            MKT_CORE_LOGGER_ERROR( "Trying to use polygon line width '{}' out of device limits [{}, {}]", value, minLineWidth, maxLineWidth );
            value = VulkanHelpers::STANDARD_POLYGON_LINE_WIDTH;
        }

        if (!deviceFeatures.wideLines) {
            MKT_CORE_LOGGER_WARN( "Wide lines not supported by the device" );
            value = VulkanHelpers::STANDARD_POLYGON_LINE_WIDTH;
        }

        vkCmdSetLineWidth(m_CmdBuffer, value);
    }

    auto VulkanCmdList::WriteBuffer( Buffer* target, Byte* data, Size size ) -> void {
        // Can be device local data
        // Here I can encapsulate all the logic about creating staging buffers etc.
    }

    auto VulkanCmdList::WriteTexture( Texture* target, Byte* data, Size size ) -> void {
        // Can be device local data
        // Here I can encapsulate all the logic about creating staging buffers, etc.
    }

    auto VulkanCmdList::SetViewport( Int32 x, Int32 y, Int32 width, Int32 height ) -> void {
        VkViewport viewport{};

        // Mikoto defaults to Vulkan 1.3 where this feature is core

        viewport.x = x;
        viewport.y = height;
        viewport.width = width;
        viewport.height = -height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        vkCmdSetViewport( m_CmdBuffer, 0, 1, std::addressof( viewport ) );
    }

    auto VulkanCmdList::SetViewport( Int32 x, Int32 y, Int32 width, Int32 height, bool flip ) -> void {
        VkViewport viewport{};

        if (flip) {
            SetViewport(x, y, width, height);
        } else {
            viewport.x = x;
            viewport.y = y;
            viewport.width = width;
            viewport.height = height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            std::array viewports{ viewport };

            vkCmdSetViewport( m_CmdBuffer, 0, ( UInt32 )viewports.size(), viewports.data() );
        }

    }

    auto VulkanCmdList::SetScissor( Int32 x, Int32 y, Int32 width, Int32 height ) -> void {
        VkRect2D scissor{};
        scissor.offset = { x, x };
        scissor.extent = { static_cast<UInt32>( width ), static_cast<UInt32>( height ) };

        vkCmdSetScissor( m_CmdBuffer, 0, 1, std::addressof( scissor ) );
    }

    auto VulkanCmdList::Dispatch( UInt32 x, UInt32 y, UInt32 z ) -> void {
        vkCmdDispatch(m_CmdBuffer, x, y, z);
    }

    auto VulkanCmdList::BindIndexBuffer( BufferHandle indexBuffer ) -> void {
        // TODO: infer index buffer data type instead of hardcoded VK_INDEX_TYPE_UINT32
        vkCmdBindIndexBuffer( m_CmdBuffer, indexBuffer->GetNativeHandle( ObjectType::Vk_Buffer ), 0, VK_INDEX_TYPE_UINT32 );
    }

    auto VulkanCmdList::BindVertexBuffer( BufferHandle vertexBuffer, const UInt32 binding ) -> void {
        constexpr std::array<VkDeviceSize, 1> offsets{};
        const std::array<VkBuffer, 1> vertexBuffers{ vertexBuffer->GetNativeHandle( ObjectType::Vk_Buffer ) };

        vkCmdBindVertexBuffers( m_CmdBuffer, binding, 1, vertexBuffers.data(), offsets.data() );
    }

    auto VulkanCmdList::Draw( UInt32 vertexCount, UInt32 instanceCount, UInt32 firstVertex, UInt32 firstInstance ) -> void {
        vkCmdDraw( m_CmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance );
    }

    auto VulkanCmdList::DrawIndexed( Size indexCount, UInt32 instanceCount, UInt32 firstIndex, UInt32 vertexOffset, UInt32 firstInstance ) -> void {
        vkCmdDrawIndexed( m_CmdBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance );
    }

    auto VulkanCmdList::DrawIndexedIndirect(BufferHandle indexBuffer, UInt32 offset, UInt32 drawCount, UInt32 stride) -> void {
        vkCmdDrawIndexedIndirect( m_CmdBuffer, indexBuffer->GetNativeHandle( ObjectType::Vk_Buffer ), offset, drawCount, stride );
    }

    auto VulkanCmdList::BindPipeline( PipelineHandle pipeline ) -> void {
        VkPipelineBindPoint bindPoint{ VK_PIPELINE_BIND_POINT_MAX_ENUM };

        switch (pipeline->GetPipelineType()) {
            case PipelineType::GRAPHICS_PIPELINE:
                bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                break;
            case PipelineType::COMPUTE_PIPELINE:
                bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
                break;
            case PipelineType::RAY_TRACING_PIPELINE:
            default:
                MKT_CORE_LOGGER_WARN( "VulkanGraphicsContext::BindPipeline - Unsupported pipeline type." );
                break;
        }

        vkCmdBindPipeline( m_CmdBuffer, bindPoint, pipeline->GetNativeHandle( ObjectType::Vk_Pipeline ) );
    }

    auto VulkanCmdList::SetDebugName( const std::string_view name ) -> void {
        m_DebugName = name;
        VulkanHelpers::SetObjectDebugName( VK_DEVICE(m_Device), VK_OBJECT_TYPE_COMMAND_BUFFER, reinterpret_cast<UInt64>(m_CmdBuffer), m_DebugName.data() );
    }

    auto VulkanCmdList::GetNativeHandle( ObjectType type ) -> Object {
        if ( type != ObjectType::Vk_CmdBuffer ) {
            return Object( nullptr );
        }

        return Object( m_CmdBuffer );
    }

    VulkanCmdList::~VulkanCmdList() {
        if ( m_IsAllocated ) {
            Release();
        }
    }

    auto VulkanCmdList::Initialize() -> void {
        if ( vkAllocateCommandBuffers(
                     VK_DEVICE( m_Device ),
                     std::addressof( m_AllocInfo ),
                     std::addressof( m_CmdBuffer ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanCmdList::Allocate - Failed to allocate command buffer" );
        }

        m_IsAllocated = true;

        // Set a debug name when this command buffer is ready to use
        SetDebugName( fmt::format( "Mikoto VkCommandList - ResourceID: {}", GetHandle() ) );

        VulkanHelpers::SetObjectDebugName(
                VK_DEVICE( m_Device ),
                VK_OBJECT_TYPE_COMMAND_BUFFER,
                reinterpret_cast<UInt64>( m_CmdBuffer ),
                m_DebugName.c_str() );
    }

    auto VulkanCmdList::Release() -> void {
        vkFreeCommandBuffers( VK_DEVICE( m_Device ), m_AllocInfo.commandPool, 1, std::addressof( m_CmdBuffer ) );
        m_IsAllocated = false;
    }

    auto VulkanCmdList::FillCubeTexture(Buffer* src, Texture* dest) -> void {
        // Cast to Vulkan-specific implementations
        auto* vkSrc{ dynamic_cast<VulkanBuffer*>( src ) };
        auto* vkDest{ dynamic_cast<VulkanTextureCube*>( dest ) };

        if ( !vkSrc || !vkDest ) {
            return;
        }

        // Ensure the destination image is in TRANSFER_DST layout
        vkDest->SubmitLayoutTransition( VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_CmdBuffer );

        // Describe the region to copy
        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;  // Tightly packed
        copyRegion.bufferImageHeight = 0;// Tightly packed

        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 6;

        copyRegion.imageOffset = { 0, 0, 0 };
        copyRegion.imageExtent = {
            vkDest->GetCreateInfo().extent.width,
            vkDest->GetCreateInfo().extent.height,
            1
        };

        if (src->GetUsage() == BufferUsage::INDEX) {
            MKT_CORE_LOGGER_ERROR( "VulkanCmdList::FillTexture - Trying to fill a texture from an index buffer. This is not supported." );
        }

        // Issue the copy command
        vkCmdCopyBufferToImage(
                m_CmdBuffer,
                vkSrc->GetNativeHandle(ObjectType::Vk_Buffer),
                vkDest->GetNativeHandle(ObjectType::Vk_Image),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copyRegion );

        // Transition the image to SHADER_READ_ONLY for sampling
        vkDest->SubmitLayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_CmdBuffer );
    }

    VulkanCommandPool::VulkanCommandPool( QueueType queue, Size initialCmdListCount )
        : m_QueueType{ queue } {
        m_CmdLists.Init( initialCmdListCount );
    }

    auto VulkanCommandPool::GetNativeHandle( ObjectType type ) -> Object {
        if ( type != ObjectType::Vk_CmdPool ) {
            return Object( nullptr );
        }

        return Object( m_Pool );
    }

    auto VulkanCommandPool::AllocateCmdList(bool immediate) -> CommandListHandle {
        MKT_BEGIN_PROFILER_NAMED();
        VkCommandBufferAllocateInfo allocInfo{ VulkanHelpers::Initializers::CommandBufferAllocateInfo() };
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_Pool;
        allocInfo.commandBufferCount = 1;

        CommandListHandle handle{ m_CmdLists.Allocate( allocInfo, m_QueueType, immediate ) };
        if ( handle.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanCommandPool::AllocateCmdList - Failed to allocate  command list." );
        } 

        handle->Initialize( m_Device );

        return handle;
    }

    auto VulkanCommandPool::SubmitCommandList() -> void {
        // if we have submitted all command lists we can mark thius pool as free
    }

    auto VulkanCommandPool::Clear() -> void {
        m_CmdLists.Clear();
    }

    VulkanCommandPool::~VulkanCommandPool() {
        if ( m_IsAllocated ) {
            Release();
        }
    }
    auto VulkanCommandPool::DestroyCommandList( CommandListHandle cmd ) -> void {
        m_CmdLists.Release( cmd->GetHandle() );
    }

    auto VulkanCommandPool::RunGarbageCollection() -> void {
        for (auto it = m_CmdLists.begin(); it != m_CmdLists.end(); ++it ) {
            if (!it->second.IsEmpty() && it->second->GetRefCount() == 1) {
                m_CmdLists.Release( it->second->GetHandle() );
            }
        }
    }

    auto VulkanCommandPool::DetermineQueueIndex( const QueuesData& queues, QueueType queue ) -> UInt32 {
        if ( queue == QueueType::GRAPHICS_QUEUE && queues.Graphics.has_value() ) {
            return queues.Graphics->FamilyIndex;
        }

        if ( queue == QueueType::COMPUTE_QUEUE && queues.Compute.has_value() ) {
            return queues.Compute->FamilyIndex;
        }

        if ( queue == QueueType::PRESENT_QUEUE && queues.Present.has_value() ) {
            return queues.Present->FamilyIndex;
        }

        // Invalid queue?
        return std::numeric_limits<UInt32>::max();
    }

    auto VulkanCommandPool::Initialize() -> void {
        const auto& queues{ TO_VK_DEVICE( m_Device )->GetLogicalDeviceQueues() };

        // Command pool to allocate command buffers for compute queue operations
        VkCommandPoolCreateInfo createInfo{ VulkanHelpers::Initializers::CommandPoolCreateInfo() };
        createInfo.flags = 0;
        createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = DetermineQueueIndex( queues, m_QueueType );

        if ( vkCreateCommandPool( VK_DEVICE( m_Device ), std::addressof( createInfo ), nullptr, std::addressof( m_Pool ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanCommandPool::Create - ailed to create command pool!" );
        }

        m_IsAllocated = true;
    }

    auto VulkanCommandPool::Release() -> void {
        m_CmdLists.Shutdown();

        vkDestroyCommandPool( VK_DEVICE( m_Device ), m_Pool, nullptr );

        m_IsAllocated = false;
    }
}
