//
// Created by kate on 1/26/2025.
//
#include <Core/Logging/Logger.hh>
#include <Core/Logging/StackTrace.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDeletionQueue.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>

namespace Mikoto {

    VulkanDevice::VulkanDevice(const VulkanDeviceCreateInfo& createInfo) {
        m_VulkanInstance = createInfo.Instance;
        m_Surface = createInfo.Surface;

        m_RequestedExtensions = std::vector<const char*>{
            createInfo.DeviceRequestedExtensions,
            createInfo.DeviceRequestedExtensions + createInfo.RequiredExtensionsCount
        };

        m_ValidationsLayers = std::vector<const char*>{
            createInfo.ValidationsLayers,
            createInfo.ValidationsLayers + createInfo.ValidationsLayersCount
        };

        m_VmaCallbacks = createInfo.VmaCallbacks;
    }

    auto VulkanDevice::Init() -> void {
        GetPrimaryPhysicalDevice();

        CreatePrimaryLogicalDevice();

        // Queue handles require a valid logical device so they are created after we have a logical device created
        GetDeviceQueues( m_LogicalDevice, m_QueueFamiliesData );

        InitMemoryAllocator();
    }

    VulkanDevice::~VulkanDevice() {
        MKT_CORE_LOGGER_INFO( "VulkanDevice::~VulkanDevice - Destroying device." );

        if (!m_IsReleased) {
            Release();
            Invalidate();
        }
    }

    auto VulkanDevice::GetPhysicalDeviceInfo(const VkPhysicalDevice& device) -> PhysicalDeviceInfo {
        PhysicalDeviceInfo result{};

        vkGetPhysicalDeviceFeatures( device, std::addressof( result.Features ) );
        vkGetPhysicalDeviceProperties( device, std::addressof(result.Properties ) );
        vkGetPhysicalDeviceMemoryProperties( device, std::addressof( result.MemoryProperties ) );

        return result;
    }

    auto VulkanDevice::CheckExtensionsSupport( const VkPhysicalDevice& device, const std::vector<const char*>& requestedExtensions ) -> bool {
        UInt32_T extensionCount{};
        vkEnumerateDeviceExtensionProperties( device, nullptr, std::addressof( extensionCount ), nullptr );

        std::vector<VkExtensionProperties> physicalDeviceSupportedExtensions( extensionCount );
        vkEnumerateDeviceExtensionProperties( device, nullptr, std::addressof( extensionCount ), physicalDeviceSupportedExtensions.data() );

        bool physicalDeviceSupportsAllRequiredExtensions{ true };
        for ( const auto& requiredExtension: requestedExtensions ) {
            physicalDeviceSupportsAllRequiredExtensions = std::ranges::any_of(physicalDeviceSupportedExtensions,
                [&requiredExtension](const VkExtensionProperties& extensionProperties) -> bool {
                    return StringUtils::Equal( requiredExtension, extensionProperties.extensionName );
                });

            if (!physicalDeviceSupportsAllRequiredExtensions) {
                break;
            }
        }

        return physicalDeviceSupportsAllRequiredExtensions;
    }

    auto VulkanDevice::CreateBuffer(const VulkanBufferCreateInfo & createInfo, VkBuffer& buffer, VmaAllocation& allocation, VmaAllocationInfo& allocationInfo ) const -> void {
        const auto result { vmaCreateBuffer(m_DefaultAllocator,
                            std::addressof( createInfo.BufferCreateInfo ),
                            std::addressof( createInfo.AllocationCreateInfo ),
                                    std::addressof( buffer ),
                            std::addressof( allocation ),
                            std::addressof( allocationInfo )) };

        if (result  != VK_SUCCESS) {
             MKT_THROW_RUNTIME_ERROR("VulkanDevice::UploadBufferFailed to create VMA buffer.");
        }
    }

    auto VulkanDevice::CreateImage(const VulkanImageCreateInfo& createInfo, const VmaAllocationCreateInfo& allocCreateInfo, VkImage& image, VmaAllocation& allocation, VmaAllocationInfo& allocationInf ) const -> void {
        const auto result{ vmaCreateImage(m_DefaultAllocator,
                                     std::addressof( createInfo.ImageCreateInfo ),
                                     std::addressof( allocCreateInfo ),
                                     std::addressof( image ),
                                     std::addressof( allocation ),
                                     std::addressof( allocationInf ) ) };

        if (result != VK_SUCCESS) {
            MKT_THROW_RUNTIME_ERROR("VulkanDevice::AllocateImage - Failed to allocate VMA Image!");
        }
    }

    auto VulkanDevice::WaitIdle() const -> void {
        vkDeviceWaitIdle(m_LogicalDevice);
    }

    auto VulkanDevice::GetDeviceMinimumOffsetAlignment() const -> VkDeviceSize {
        return m_PhysicalDeviceInfo.Properties.limits.minUniformBufferOffsetAlignment;
    }

    auto VulkanDevice::IsDeviceSuitable( const VkPhysicalDevice& device, const PhysicalDeviceRequiredFeatures& requirements ) -> bool {
        // Verify extensions support
        const bool extensionsSupported{ CheckExtensionsSupport( device, *requirements.RequestedExtensions ) };

        // Verify queue support (Present is needed if the surface is not null)
        // For now I always want a graphics queue by default for the device
        const auto& [Present, Graphics, Compute] { GetQueueFamilyIndices( device, requirements.Surface ) };
        const bool deviceSupportsRequiredQueues{ Graphics.has_value() && (requirements.Surface != nullptr && Present.has_value()) };

        // Check swapchain support
        bool deviceHasSwapchainSupport{ true };
        if ( extensionsSupported && requirements.Surface != nullptr  ) {
            const SwapChainSupportDetails swapChainSupport{ VulkanHelpers::GetSwapChainSupport( device, *requirements.Surface ) };
            deviceHasSwapchainSupport = !swapChainSupport.Formats.empty() && !swapChainSupport.PresentModes.empty();
        }

        // Check support physical device features
        VkPhysicalDeviceFeatures supportedFeatures{};
        vkGetPhysicalDeviceFeatures( device, std::addressof(supportedFeatures) );
        bool supportRequiredPhysicalFeatures{
            // Anisotropic filtering requested and supported
            (!requirements.AnysotropicFiltering || supportedFeatures.samplerAnisotropy) &&
            (!requirements.FillModeNonSolid || supportedFeatures.fillModeNonSolid)
        };

        return deviceSupportsRequiredQueues && extensionsSupported && deviceHasSwapchainSupport && supportRequiredPhysicalFeatures;
    }

    auto VulkanDevice::GetPrimaryPhysicalDevice() -> void {
        UInt32_T deviceCount{ 0 };
        vkEnumeratePhysicalDevices( *m_VulkanInstance, std::addressof(deviceCount), nullptr );
        if ( deviceCount == 0 ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanDevice::PickPrimaryPhysicalDevice - Error failed to find GPUs with Vulkan support!" );
        }

        std::vector<VkPhysicalDevice> physicalDevices( deviceCount );
        vkEnumeratePhysicalDevices( *m_VulkanInstance, std::addressof( deviceCount ), physicalDevices.data() );

        const auto it{
            std::ranges::find_if( physicalDevices,
                                  [&]( const VkPhysicalDevice& device ) -> bool {
                                      const PhysicalDeviceRequiredFeatures reqs{
                                          .AnysotropicFiltering{ true },
                                          .FillModeNonSolid{ true },
                                          .Surface{ m_Surface },
                                          .RequestedExtensions{ std::addressof( m_RequestedExtensions ) },
                                      };

                                      return IsDeviceSuitable( device, reqs );
                                  } )
        };

        if (it == physicalDevices.end() ||  *it == VK_NULL_HANDLE ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanDevice::PickPrimaryPhysicalDevice - Error could not find a suitable GPU to use as primary physical device!" );
        }

        m_PhysicalDevice = *it;
        m_PhysicalDeviceInfo = GetPhysicalDeviceInfo( m_PhysicalDevice );
    }

    auto VulkanDevice::CreatePrimaryLogicalDevice() -> void {
        m_QueueFamiliesData = GetQueueFamilyIndices( m_PhysicalDevice, m_Surface );

        const auto graphicsQueueFamilyIndex{ m_QueueFamiliesData.Graphics->FamilyIndex };
        const auto presentQueueFamilyIndex{ m_QueueFamiliesData.Present->FamilyIndex };

        const auto queueCreateInfos{ VulkanHelpers::SetupDeviceQueueCreateInfo( { graphicsQueueFamilyIndex, presentQueueFamilyIndex } ) };

        // Requested device features
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        deviceFeatures.fillModeNonSolid = VK_TRUE;// required for wireframe mode

        VkPhysicalDeviceVulkan13Features vulkan13Features{ VulkanHelpers::Initializers::PhysicalDeviceVulkan13Features() };
        vulkan13Features.synchronization2 = VK_TRUE;// required for vkCmdPipelineBarrier2 used when image transitions

        VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{ VulkanHelpers::Initializers::PhysicalDeviceFeatures2() };
        physicalDeviceFeatures2.features = deviceFeatures;
        physicalDeviceFeatures2.pNext = std::addressof( vulkan13Features );

        VkDeviceCreateInfo createInfo{ VulkanHelpers::Initializers::DeviceCreateInfo() };
        createInfo.queueCreateInfoCount = static_cast<UInt32_T>( queueCreateInfos.size() );
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = nullptr;
        createInfo.enabledExtensionCount = static_cast<UInt32_T>( m_RequestedExtensions.size() );
        createInfo.ppEnabledExtensionNames = m_RequestedExtensions.data();
        createInfo.pNext = std::addressof( physicalDeviceFeatures2 );

        // might not be necessary anymore because device-specific validation layers have been deprecated
        // even tho recommended for some backwards compatibility as they are required for some Vulkan implementations
        if ( !m_ValidationsLayers.empty() ) {
            // These two fields are ignored by up-to-date Vulkan implementations
            createInfo.enabledLayerCount = static_cast<UInt32_T>( m_ValidationsLayers.size() );
            createInfo.ppEnabledLayerNames = m_ValidationsLayers.data();
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
    }


    auto VulkanDevice::GetAllocatorStats() -> const VmaTotalStatistics& {
        vmaCalculateStatistics( m_DefaultAllocator, std::addressof( m_AllocatorStats ) );
        return m_AllocatorStats;
    }

    auto VulkanDevice::RegisterGraphicsCommand( VkCommandBuffer cmd ) -> void {
        // TODO: Thread safety
        // Senders are responsible of freeing the command buffers packed into
        // this array once these command buffers are no longer needed.
        m_GraphicsSubmitCommands.emplace_back( cmd );
    }

    auto VulkanDevice::RegisterComputeCommand( VkCommandBuffer cmd ) -> void {
        // TODO: Thread safety
        // Senders are responsible of freeing the command buffers packed into
        // this array once these command buffers are no longer needed.
        m_ComputeSubmitCommands.emplace_back( cmd );
    }

    auto VulkanDevice::SubmitCommandsGraphicsQueue( const FrameSynchronizationPrimitives& syncPrimitives ) -> void {
        if (m_GraphicsSubmitCommands.empty()) {
            return;
        }

        // Prepare the submission to the queue. We want to wait on
        // the present semaphore, which is signaled when the swapchain
        // is ready (there's image available to render to). We will
        // signal the render semaphore to signal that rendering has finished

        VkPipelineStageFlags waitStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo submit{ VulkanHelpers::Initializers::SubmitInfo() };
        submit.pNext = nullptr;

        submit.pWaitDstStageMask = std::addressof( waitStage );

        // Wait-on semaphores
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = std::addressof( syncPrimitives.PresentSemaphore );

        // Completion signal semaphores
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = std::addressof( syncPrimitives.RenderSemaphore );

        // Command buffers
        submit.commandBufferCount = m_GraphicsSubmitCommands.size();
        submit.pCommandBuffers = m_GraphicsSubmitCommands.data();

        if ( vkQueueSubmit( m_QueueFamiliesData.Graphics->Queue, 1, std::addressof( submit ), syncPrimitives.RenderFence ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanDevice::SubmitCommand - Error trying to submit commands." );
        }

        m_GraphicsSubmitCommands.clear();
    }

    auto VulkanDevice::SubmitCommandsComputeQueue( const ComputeSynchronizationPrimitives& syncPrimitives ) -> void {
        if (m_ComputeSubmitCommands.empty()) {
            return;
        }

        // Prepare the submission to the queue. We want to wait on
        // the present semaphore, which is signaled when the swapchain
        // is ready (there's image available to render to). We will
        // signal the render semaphore to signal that rendering has finished

        VkPipelineStageFlags waitStage{ VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };
        VkSubmitInfo submit{ VulkanHelpers::Initializers::SubmitInfo() };
        submit.pNext = nullptr;

        submit.pWaitDstStageMask = std::addressof( waitStage );

        // Wait-on semaphores
        submit.waitSemaphoreCount = static_cast<UInt32_T>( syncPrimitives.WaitSemaphores.size() );
        submit.pWaitSemaphores = syncPrimitives.WaitSemaphores.data();

        // Completion signal semaphores
        submit.signalSemaphoreCount = static_cast<UInt32_T>( syncPrimitives.SignalSemaphores.size() );
        submit.pSignalSemaphores = syncPrimitives.SignalSemaphores.data();

        // Command buffers
        submit.commandBufferCount = m_ComputeSubmitCommands.size();
        submit.pCommandBuffers = m_ComputeSubmitCommands.data();

        if ( vkQueueSubmit( m_QueueFamiliesData.Compute->Queue, 1, std::addressof( submit ), syncPrimitives.Fence ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanDevice::SubmitCommand - Error trying to submit commands." );
        }

        m_ComputeSubmitCommands.clear();
    }

    auto VulkanDevice::Release() -> void {
        if (m_IsReleased) {
            return;
        }

        vmaDestroyAllocator( m_DefaultAllocator );

        vkDestroyDevice( m_LogicalDevice, nullptr );

        Invalidate();
    }

    auto VulkanDevice::InitMemoryAllocator() -> void {
        VmaAllocatorCreateInfo allocatorInfo{
            .physicalDevice{ m_PhysicalDevice },
            .device{ m_LogicalDevice },
            .pVulkanFunctions{ m_VmaCallbacks },
            .instance{ *m_VulkanInstance },
            .vulkanApiVersion{ VK_MAKE_API_VERSION(MKT_VULKAN_VERSION_VARIANT, MKT_VULKAN_VERSION_MAJOR, MKT_VULKAN_VERSION_MINOR, MKT_VULKAN_VERSION_PATCH ) },
        };

        vmaCreateAllocator( std::addressof( allocatorInfo ), std::addressof( m_DefaultAllocator ) );
    }

    auto VulkanDevice::FindSupportedFormat( const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features ) const -> VkFormat {
        for ( const VkFormat format: candidates ) {
            VkFormatProperties props{};
            vkGetPhysicalDeviceFormatProperties( m_PhysicalDevice, format, std::addressof( props ) );

            if ( ( tiling == VK_IMAGE_TILING_LINEAR && ( props.linearTilingFeatures & features ) == features ) ||
                 ( tiling == VK_IMAGE_TILING_OPTIMAL && ( props.optimalTilingFeatures & features ) == features ) ) {
                return format;
            }
        }

        MKT_THROW_RUNTIME_ERROR( "Failed to find supported format!" );
    }

    auto VulkanDevice::GetQueueFamilyIndices( const VkPhysicalDevice& device, const VkSurfaceKHR* surface ) -> QueuesData {
        QueuesData result{};

        UInt32_T queueFamilyCount{};
        vkGetPhysicalDeviceQueueFamilyProperties( device, std::addressof( queueFamilyCount ), nullptr );

        std::vector<VkQueueFamilyProperties> queueFamilies( queueFamilyCount );
        vkGetPhysicalDeviceQueueFamilyProperties( device, std::addressof( queueFamilyCount ), queueFamilies.data() );

        // Vulkan does not give the family index,
        // so I need to manually keep track of it
        UInt32_T queueFamilyIndex{};
        for ( const auto& queueFamilyProperties : queueFamilies ) {
            // Check graphics queue support
            if (!result.Graphics.has_value() && VulkanHelpers::HasGraphicsQueue(queueFamilyProperties) ) {
                result.Graphics = std::make_optional( VulkanQueueData{
                    .Queue{ VK_NULL_HANDLE },
                    .FamilyIndex{ queueFamilyIndex },
                } );
            }

            // Check present queue support
            if (!result.Present.has_value() && surface != nullptr && VulkanHelpers::HasPresentQueue( device, queueFamilyIndex, *surface, queueFamilyProperties )) {
                result.Present = std::make_optional( VulkanQueueData{
                    .Queue{ VK_NULL_HANDLE },
                    .FamilyIndex{ queueFamilyIndex },
                } );
            }

            // Check compute queue
            if (!result.Compute.has_value() && VulkanHelpers::HasComputeQueue(queueFamilyProperties)) {
                result.Compute = std::make_optional( VulkanQueueData{
                    .Queue{ VK_NULL_HANDLE },
                    .FamilyIndex{ queueFamilyIndex },
                } );
            }

            ++queueFamilyIndex;
        }

        return result;
    }

    auto VulkanDevice::GetDeviceQueues( const VkDevice& device, QueuesData& queues ) -> void {
        if (queues.Graphics.has_value()) {
            vkGetDeviceQueue( device, queues.Graphics->FamilyIndex, 0, std::addressof(queues.Graphics->Queue) );
        }

        if (queues.Present.has_value()) {
            vkGetDeviceQueue( device, queues.Present->FamilyIndex, 0, std::addressof(queues.Present->Queue) );
        }

        if (queues.Compute.has_value()) {
            vkGetDeviceQueue( device, queues.Compute->FamilyIndex, 0, std::addressof(queues.Compute->Queue) );
        }
    }
}
