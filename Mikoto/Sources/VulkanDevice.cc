//
// Created by kate on 1/26/2025.
//
#include <spirv_reflect.h>

#include <ankerl/unordered_dense.h>

#include <Logging/Logger.hh>
#include <Renderer/RenderService.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>

namespace Mikoto {

    // Device extensions standard
    static constexpr std::array DEVICE_EXTENSIONS{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,

        // Passing your vertex data just like in OpenGL, using the same state (as the pipeline setup)
        // and Shaders as in OpenGL, your scene will likely not display as you’d expect.
        // The viewport’s origin in OpenGL is in the lower left of the screen, with Y pointing up.
        // In Vulkan the origin is in the top left of the screen, with Y pointing downwards.
        // Starting from Vulkan 1.1 though, this feature is part of core Vulkan, so checking for it is not really needed.
        // See: https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
        VK_KHR_MAINTENANCE1_EXTENSION_NAME,
    };

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
        vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);

        std::vector<VkExtensionProperties> available(count);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

        ankerl::unordered_dense::set<std::string_view> names{};
        names.reserve(available.size());

        for ( const auto& [extensionName, specVersion] : available) {
            names.emplace(extensionName);
        }

        // We simply check that all requested extensions are in the available ones
        return std::ranges::all_of(requested, [&](auto r) { return names.contains(r); });
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
                result.Graphics = std::make_optional(VulkanQueueData{
                        .Queue{ VK_NULL_HANDLE },
                        .FamilyIndex{ queueFamilyIndex },
                });
            }

            // Check present queue support
            if ( !result.Present.has_value() && surface != nullptr && VulkanHelpers::HasPresentQueue( device, queueFamilyIndex, *surface, queueFamilyProperties ) ) {
                result.Present = std::make_optional(VulkanQueueData{
                        .Queue{ VK_NULL_HANDLE },
                        .FamilyIndex{ queueFamilyIndex },
                });
            }

            // Check compute queue
            if ( !result.Compute.has_value() && VulkanHelpers::HasComputeQueue( queueFamilyProperties ) ) {
                result.Compute = std::make_optional(VulkanQueueData{
                        .Queue{ VK_NULL_HANDLE },
                        .FamilyIndex{ queueFamilyIndex },
                });
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

        return extensionsSupported && deviceSupportsRequiredQueues && deviceHasSwapchainSupport && supportRequiredPhysicalFeatures;
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
        CreatePrimaryLogicalDevice();

        FetchDeviceQueues( m_LogicalDevice, m_Queues );

        InitMemoryAllocator();

        // Init pools
        m_Buffers.Init( 100 );
        m_Textures.Init( 100 );

        m_IsInitialized = true;
    }

    auto VulkanDevice::InitMemoryAllocator() -> void {
        m_GpuAllocator = GpuAllocator::Create(this);
        if (!m_GpuAllocator) {
            MKT_THROW_RUNTIME_ERROR("VulkanDevice::InitMemoryAllocator - Could not create GPU Allocator.");
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
                                          .Surface{ std::addressof(VulkanContext::Get()->GetSurface()) },
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

        // Get the queue family indices for the selected physical device, we will need them to create the logical device
        m_Queues = GetQueueFamilyIndices( m_PhysicalDevice, std::addressof(VulkanContext::Get()->GetSurface()) );

        MKT_CORE_LOGGER_DEBUG( "VulkanDevice::GetPrimaryPhysicalDevice - Selected GPU: {}", m_PhysicalDeviceInfo.Properties.deviceName );
    }

    auto VulkanDevice::CreatePrimaryLogicalDevice() -> void {
        if ( !m_Queues.Graphics.has_value() && !m_Queues.Present.has_value() ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanDevice::CreatePrimaryLogicalDevice - No graphics or present queue found!" );
        }

        const auto graphicsQueueFamilyIndex{ m_Queues.Graphics->FamilyIndex };
        const auto presentQueueFamilyIndex{ m_Queues.Present->FamilyIndex };
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
        createInfo.queueCreateInfoCount = static_cast<UInt32>( queueCreateInfos.size() );
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = nullptr;
        createInfo.enabledExtensionCount = static_cast<UInt32>( m_RequestedExtensions.size() );
        createInfo.ppEnabledExtensionNames = m_RequestedExtensions.data();
        createInfo.pNext = std::addressof( physicalDeviceFeatures2 );

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


    auto VulkanDevice::Shutdown() -> void {
        if ( !m_IsInitialized ) {
            return;
        }

        // Clear resources (pools, etc)
        m_Textures.Shutdown();
        m_Buffers.Shutdown();

        MKT_CORE_LOGGER_INFO( "VulkanDevice::Shutdown - Shutting down Vulkan Device." );

        if (m_GpuAllocator) {
            m_GpuAllocator->Shutdown();
            m_GpuAllocator = nullptr;
        }

        vkDestroyDevice( m_LogicalDevice, nullptr );
    }

    auto VulkanDevice::WaitIdle() const -> void {
        vkDeviceWaitIdle( m_LogicalDevice );

    }

    auto VulkanDevice::CreateTexture( const TextureDescription& description ) -> TextureHandle {
        Texture* texture{ m_Textures.Allocate( description ) };
        if ( texture == nullptr ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreateBuffer - Failed to allocate buffer resource." );
            return TextureHandle::CreateEmpty();
        }

        texture->Allocate( this );

        return TextureHandle::Create(m_Textures.Get( texture->GetHandle()) );
    }

    auto VulkanDevice::RunGarbageCollection() -> void {
        for ( const auto& texture: m_Textures | std::views::values ) {
            if ( texture && texture->GetRefCount() == 1 ) {
                m_Textures.Release( texture->GetHandle() );
            }
        }

        for ( const auto& buffer: m_Buffers | std::views::values ) {
            if ( buffer && buffer->GetRefCount() == 1 ) {
                m_Buffers.Release( buffer->GetHandle() );
            }
        }
    }

    auto VulkanDevice::CreateCommandList() -> CommandListHandle {
        // Get an available command pool or create a new command list

        return CommandListHandle::CreateEmpty();
    }

    auto VulkanDevice::CreateBuffer( const BufferDescription& description ) -> BufferHandle {
        Buffer* buffer{ m_Buffers.Allocate( description ) };
        if ( buffer == nullptr ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreateBuffer - Failed to allocate buffer resource." );
            return BufferHandle::CreateEmpty();
        }

        buffer->Allocate( this );

        return BufferHandle::Create(m_Buffers.Get( buffer->GetHandle()) );
    }

    auto VulkanDevice::GetUniformBufferMinOffsetAlignment() const -> VkDeviceSize {
        return m_PhysicalDeviceInfo.Properties.limits.minUniformBufferOffsetAlignment;
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

}// namespace Mikoto
