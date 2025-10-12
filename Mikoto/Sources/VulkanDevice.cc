//
// Created by kate on 1/26/2025.
//

#include <ankerl/unordered_dense.h>
#include <spirv_reflect.h>

#include <Logging/Logger.hh>
#include <Renderer/RenderService.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>

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

        // Get the queue family indices for the selected physical device, we will need them to create the logical device
        m_Queues = GetQueueFamilyIndices( m_PhysicalDevice, std::addressof( VulkanContext::Get()->GetSurface() ) );

        CreatePrimaryLogicalDevice();

        FetchDeviceQueues( m_LogicalDevice, m_Queues );

        InitMemoryAllocator();

        // Init pools
        m_Buffers.Init( 100 );
        m_Textures.Init( 100 );
        m_Framebuffers.Init( 10 );

        // Pre-initialize available pools
        UInt32 poolCount{ 5 };
        m_CmdPools.Init( poolCount );
        for (auto count{ 0 }; count < poolCount; ++count ) {
            // Temporary on my machine this queue is powerful xdd
            // This will depend on command recording to avoid resizing the pool often
            auto pool{ m_CmdPools.Allocate( QueueType::GRAPHICS_QUEUE, 200 ) };
            pool.As<DeviceObject>()->Initialize(this);
        }

        m_IsInitialized = true;
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

        // Wait for pending operations
        WaitIdle();

        m_PendingCmdLists.clear();

        // Clear resources (pools, etc)
        m_Textures.Shutdown();
        m_Buffers.Shutdown();
        m_CmdPools.Shutdown();
        m_Framebuffers.Shutdown();

        MKT_CORE_LOGGER_INFO( "VulkanDevice::Shutdown - Shutting down Vulkan Device." );

        if ( m_GpuAllocator ) {
            m_GpuAllocator->Shutdown();
            m_GpuAllocator = nullptr;
        }

        vkDestroyDevice( m_LogicalDevice, nullptr );

        m_IsInitialized = false;
    }

    auto VulkanDevice::WaitIdle() const -> void {
        vkDeviceWaitIdle( m_LogicalDevice );
    }

    auto VulkanDevice::CreateTexture( const TextureDescription& description ) -> TextureHandle {
        TextureHandle texture{ m_Textures.Allocate( description ).As<Texture>() };
        if ( texture.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreateTexture - Failed to allocate texture resource." );
            return TextureHandle::CreateEmpty();
        }

        texture->Initialize( this );

        return texture;
    }

    auto VulkanDevice::RunGarbageCollection() -> void {
        // Call at the begining of each frame to free command buffers

        for (const auto& pool : m_CmdPools | std::views::values) {
            pool.As<VulkanCommandPool>()->RunGarbageCollection();
        }
    }

    auto VulkanDevice::CreateCommandList( QueueType queue ) -> CommandListHandle {
        // Find pool we can allocate command lists for the specified type of queue

        CommandListHandle resultCommandList{};

        VulkanCommandPoolHandle pool{ m_CmdPools.GetResource() };

        // If we managed to allocate a pool
        if (!pool.IsEmpty() ) {
            resultCommandList = pool->AllocateCmdList();
            if ( resultCommandList.IsEmpty() ) {
                MKT_THROW_RUNTIME_ERROR( "VulkanDevice::CreateCommandList - Failed to allocate command list." );
            }

            resultCommandList->Initialize(this);
        }

        return resultCommandList;
    }

    auto VulkanDevice::CreateBuffer( const BufferDescription& description ) -> BufferHandle {
        BufferHandle buffer{ m_Buffers.Allocate( description ).As<Buffer>() };
        if ( buffer.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreateBuffer - Failed to allocate buffer resource." );
            return BufferHandle::CreateEmpty();
        }

        buffer->Initialize( this );

        return buffer;
    }

    auto VulkanDevice::CreateFrameBuffer( const FramebufferDescription& description ) -> FramebufferHandle {
        FramebufferHandle framebuffer{ m_Framebuffers.Allocate( description ).As<Framebuffer>() };
        if ( framebuffer.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreateFrameBuffer - Failed to allocate framebuffer resource." );
            return FramebufferHandle::CreateEmpty();
        }

        framebuffer->Initialize( this );

        return framebuffer;
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

    auto VulkanDevice::SubmitCommands( CommandListHandle cmd ) -> void {
        m_PendingCmdLists.emplace_back( cmd );
    }

    auto VulkanDevice::CreateSwapchain( const VulkanSwapChainCreateInfo& createInfo ) -> SwapChainHandle {

        SwapChainHandle result{ std::move( SwapChainHandle::Create( MKT_NO_THROW_NEW VulkanSwapChain( createInfo ) ) ) };
        if ( !result.IsEmpty() ) {
            result.GetRaw()->Initialize( this );
        }

        return result;
    }

    auto VulkanDevice::CreateSwapChainTextures( const VkImageViewCreateInfo& createInfo, VkExtent2D extent ) -> TextureHandle {
        TextureHandle texture{ m_Textures.Allocate( createInfo, extent ).As<Texture>() };
        if ( texture.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreateBuffer - Failed to allocate texture resource with VkImageViewCreateInfo." );
            return TextureHandle::CreateEmpty();
        }

        texture->Initialize( this );

        return texture;
    }

    auto VulkanDevice::FlushPendingCommands( const FrameSynchronizationPrimitives& syncPrimitives ) -> void {
        VkPipelineStageFlags waitStage{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo submit{ VulkanHelpers::Initializers::SubmitInfo() };
        submit.pNext = nullptr;
        submit.pWaitDstStageMask = std::addressof( waitStage );

        // Wait-on semaphores
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = std::addressof( syncPrimitives.ImageAvailableSemaphore );

        // Completion signal semaphores
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = std::addressof( syncPrimitives.RenderFinishedSemaphore );

        // Command buffers
        std::vector<VkCommandBuffer> vkCommands{};
        for (const auto& commandBuffer : m_PendingCmdLists) {
            auto* cmd{ commandBuffer.As<VulkanCmdList>()->GetImplHandle() };

            vkCommands.emplace_back( *cmd );
        }

        submit.commandBufferCount = vkCommands.size();
        submit.pCommandBuffers = vkCommands.data();

        if ( vkQueueSubmit( m_Queues.Graphics->Queue, 1, std::addressof( submit ), syncPrimitives.RenderFence ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanContext::SubmitFrame - Error trying to submit commands." );
        }

        vkWaitForFences( m_LogicalDevice, 1, std::addressof( syncPrimitives.RenderFence ), VK_TRUE, ( std::numeric_limits<std::uint64_t>::max )() );

        //Commands have already been flushed I can get rid of them
        m_PendingCmdLists.clear();
    }

    auto VulkanDevice::PresentToSwapChain( const FrameSynchronizationPrimitives& syncPrimitives, SwapChainHandle swapchain ) -> void {
        // Presentation
        const auto result{ swapchain->Present( VulkanContext::Get()->GetCurrentImageIndex(), syncPrimitives.RenderFinishedSemaphore ) };
        if ( result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ) {
            MKT_THROW_RUNTIME_ERROR( "Error presenting to swp chain" );
            return;
        }

        if ( result != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanDevice::PresentToSwapChain - Error failed present images to swapchain." );
        }

        m_PendingCmdLists.clear();
    }

    VulkanCmdList::VulkanCmdList( const VkCommandBufferAllocateInfo& createInfo )
        : m_AllocInfo{ createInfo } {
    }

    auto VulkanCmdList::Begin() -> void {
        // Begin recording command buffer
        VkCommandBufferBeginInfo beginInfo{ VulkanHelpers::Initializers::CommandBufferBeginInfo() };

        if ( vkBeginCommandBuffer( m_CmdBuffer, std::addressof( beginInfo ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanCmdList::Close - Failed to begin recording ImGui command buffer" );
        }
    }

    auto VulkanCmdList::End() -> void {
        // End recording command buffer
        if ( vkEndCommandBuffer( m_CmdBuffer ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanCmdList::Close - Failed to record command buffer!" );
        }
    }

    auto VulkanCmdList::FillTexture( Buffer* src, Texture* dest ) -> void {
    }

    auto VulkanCmdList::CopyBuffer( Buffer* src, Buffer* dest ) -> void {
    }

    auto VulkanCmdList::CopyTexture( Texture* srcTexture, Texture* destTexture ) -> void {
        const auto src{ dynamic_cast<VulkanTexture*>( srcTexture ) };
        const auto dest{ dynamic_cast<VulkanTexture*>( destTexture ) };

        // Layout transition for transfer optimal
        src->SubmitLayoutTransition( VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_CmdBuffer );
        dest->SubmitLayoutTransition( VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, m_CmdBuffer );

        // Define copy region
        VkExtent3D extent{};
        extent.width = dest->GetWidth();
        extent.height = dest->GetHeight();
        extent.depth = 1;

        VulkanHelpers::CopyImageToImage( m_CmdBuffer, *src->GetImplHandle(), *dest->GetImplHandle(), extent );

        // Reset layout
        src->SubmitLayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_CmdBuffer );

        if (!dest->IsSwapChainImage()) {
            dest->SubmitLayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_CmdBuffer );
        } else {
            dest->SubmitLayoutTransition( VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, m_CmdBuffer );
        }

    }

    auto VulkanCmdList::WriteBuffer( Buffer* target, Byte* data, Size size ) -> void {
    }

    auto VulkanCmdList::WriteTexture( Texture* target, Byte* data, Size size ) -> void {
    }

    VulkanCmdList::~VulkanCmdList() {
        if ( m_IsAllocated ) {
            Release();
        }
    }

    auto VulkanCmdList::Allocate() -> void {
        if ( vkAllocateCommandBuffers(
                     VK_DEVICE( m_Device ),
                     std::addressof( m_AllocInfo ),
                     std::addressof( m_CmdBuffer ) ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanCmdList::Allocate - Failed to allocate command buffer" );
        }

        m_IsAllocated = true;
    }

    auto VulkanCmdList::Release() -> void {
        vkFreeCommandBuffers( VK_DEVICE( m_Device ), m_AllocInfo.commandPool, 1, std::addressof( m_CmdBuffer ) );
        m_IsAllocated = false;
    }

    VulkanCommandPool::VulkanCommandPool( QueueType queue, Size initialCmdListCount )
        : m_QueueType{ queue } {
        m_CmdLists.Init( initialCmdListCount );
    }

    auto VulkanCommandPool::AllocateCmdList() -> CommandListHandle {
        VkCommandBufferAllocateInfo allocInfo{ VulkanHelpers::Initializers::CommandBufferAllocateInfo() };
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_Pool;
        allocInfo.commandBufferCount = 1;

        Ref<VulkanCmdList> handle{ m_CmdLists.Allocate( allocInfo ) };
        if ( handle.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanCommandPool::AllocateCmdList - Failed to allocate  command list." );
        } else {
            handle.As<DeviceObject>()->Initialize( m_Device );
        }

        return handle.As<ICommandList>();
    }

    auto VulkanCommandPool::Clear() -> void {
        m_CmdLists.Clear();
    }

    VulkanCommandPool::~VulkanCommandPool() {
        if ( m_IsAllocated ) {
            Release();
        }
    }

    auto VulkanCommandPool::RunGarbageCollection() -> void {
        m_CmdLists.Clear();
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

    auto VulkanCommandPool::Allocate() -> void {
        const auto& queues{ TO_VK_DEVICE( m_Device )->GetLogicalDeviceQueues() };

        // Command pool to allocate command buffers for compute queue operations
        VkCommandPoolCreateInfo createInfo{ VulkanHelpers::Initializers::CommandPoolCreateInfo() };
        createInfo.flags = 0;
        createInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
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

}// namespace Mikoto
