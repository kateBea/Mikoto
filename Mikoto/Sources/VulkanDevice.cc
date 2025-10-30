//
// Created by kate on 1/26/2025.
//

#include <ankerl/unordered_dense.h>

#include <Assets/AssetsService.hh>
#include <Filesystem/FileService.hh>
#include <Logging/Logger.hh>
#include <Renderer/RenderService.hh>
#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDescriptorManager.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanRenderer.hh>
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

    static auto GetDefaultGraphicsPipelineConfigInfo() -> VulkanGraphicsPipelineDescription {
        VulkanGraphicsPipelineDescription configInfo{};

        // [Input assembly]
        configInfo.InputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        configInfo.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        //configInfo.InputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;// Every three vertices are group together into a separate triangle
        configInfo.InputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

        // [Viewport and Scissor]
        configInfo.ViewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        configInfo.ViewportInfo.viewportCount = 1;// VK_DYNAMIC_VIEWPORT_WITH_COUNT has to be set for this to be 0
        configInfo.ViewportInfo.pViewports = nullptr;
        configInfo.ViewportInfo.scissorCount = 1;// VK_DYNAMIC_SCISSOR_WITH_COUNT has to be set for this to be 0
        configInfo.ViewportInfo.pScissors = nullptr;

        constexpr float GPU_STANDARD_LINE_WIDTH{ 1.0f };
        configInfo.RasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        configInfo.RasterizationInfo.depthClampEnable = VK_FALSE;
        configInfo.RasterizationInfo.rasterizerDiscardEnable = VK_FALSE;// requires extension if enabled
        configInfo.RasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
        // The maximum line width that is supported depends on the hardware, any line thicker than 1.0f requires you to enable the wideLines GPU feature.
        configInfo.RasterizationInfo.lineWidth = configInfo.RasterizationInfo.polygonMode == VK_POLYGON_MODE_LINE ? GPU_STANDARD_LINE_WIDTH : 0.0f;
        configInfo.RasterizationInfo.cullMode = VK_CULL_MODE_NONE;
        configInfo.RasterizationInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        configInfo.RasterizationInfo.depthBiasEnable = VK_FALSE;
        configInfo.RasterizationInfo.depthBiasConstantFactor = 0.0f;
        configInfo.RasterizationInfo.depthBiasClamp = 0.0f;
        configInfo.RasterizationInfo.depthBiasSlopeFactor = 0.0f;

        configInfo.MultisampleInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        configInfo.MultisampleInfo.sampleShadingEnable = VK_FALSE;
        configInfo.MultisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        configInfo.MultisampleInfo.minSampleShading = 1.0f;         // Optional
        configInfo.MultisampleInfo.pSampleMask = nullptr;           // Optional
        configInfo.MultisampleInfo.alphaToCoverageEnable = VK_FALSE;// Optional
        configInfo.MultisampleInfo.alphaToOneEnable = VK_FALSE;     // Optional

        // Blending enabled by default
        configInfo.ColorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        configInfo.ColorBlendAttachment.blendEnable = VK_TRUE;
        configInfo.ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        configInfo.ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        configInfo.ColorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        configInfo.ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        configInfo.ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        configInfo.ColorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

        configInfo.ColorBlendInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        configInfo.ColorBlendInfo.logicOpEnable = VK_FALSE;
        configInfo.ColorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
        configInfo.ColorBlendInfo.attachmentCount = 1;
        configInfo.ColorBlendInfo.pAttachments = &configInfo.ColorBlendAttachment;
        configInfo.ColorBlendInfo.blendConstants[0] = 0.0f;
        configInfo.ColorBlendInfo.blendConstants[1] = 0.0f;
        configInfo.ColorBlendInfo.blendConstants[2] = 0.0f;
        configInfo.ColorBlendInfo.blendConstants[3] = 0.0f;

        configInfo.DepthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        configInfo.DepthStencilInfo.depthTestEnable = VK_TRUE;
        configInfo.DepthStencilInfo.depthWriteEnable = VK_TRUE;
        configInfo.DepthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        configInfo.DepthStencilInfo.depthBoundsTestEnable = VK_FALSE;
        configInfo.DepthStencilInfo.stencilTestEnable = VK_TRUE;          // Enable stencil test
        configInfo.DepthStencilInfo.back.compareOp = VK_COMPARE_OP_ALWAYS;// Always pass
        configInfo.DepthStencilInfo.back.failOp = VK_STENCIL_OP_REPLACE;
        configInfo.DepthStencilInfo.back.depthFailOp = VK_STENCIL_OP_REPLACE;
        configInfo.DepthStencilInfo.back.passOp = VK_STENCIL_OP_REPLACE;// Write stencil value
        configInfo.DepthStencilInfo.back.reference = 1;                 // Stencil value to write
        configInfo.DepthStencilInfo.back.compareMask = 0xFF;
        configInfo.DepthStencilInfo.back.writeMask = 0xFF;
        configInfo.DepthStencilInfo.front = configInfo.DepthStencilInfo.back;// Use default settings for front faces

        // VK_DYNAMIC_STATE_VERTEX_INPUT_EXT can reduce the amount of pipelines the application needs to create
        // because it allows for vertex input binding and attribute descriptions to be dynamic. This is, of course, not a
        // core feature as of Vulkan 1.3 and requires to be enabled when creating the device on which this pipeline will be created
        // Make it static because pDynamicStates does not persist the value beyond this scope

        static constexpr std::array dynamicStates{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_LINE_WIDTH, /*VK_DYNAMIC_STATE_VERTEX_INPUT_EXT*/ };
        configInfo.DynamicStateEnables = dynamicStates;
        configInfo.DynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        configInfo.DynamicStateInfo.pDynamicStates = configInfo.DynamicStateEnables.data();
        configInfo.DynamicStateInfo.dynamicStateCount = configInfo.DynamicStateEnables.size();
        configInfo.DynamicStateInfo.flags = 0;

        return configInfo;
    }

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
        m_Framebuffers.Init( 10 );
        m_Swapchains.Init( 10 );
        m_GraphicsPipelines.Init( 10 );
        m_ComputePipelines.Init( 10 );
        m_Shaders.Init( 10 );
        m_Samplers.Init( 10 );
        m_DescriptorSetLayouts.Init( 10 );

        // Pre-initialize available pools
        constexpr UInt32 poolCount{ 2 };
        m_CmdPools.Init( poolCount );
        for ( auto count{ 0 }; count < poolCount; ++count ) {
            // Temporary. On my machine, this queue is powerful xdd
            // This will depend on command recording to avoid resizing the pool often
            auto pool{ m_CmdPools.Allocate( QueueType::GRAPHICS_QUEUE, 100 ) };
            pool->Initialize( this );
        }

        InitDescriptorAllocator();

        m_IsInitialized = true;
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

        // Requested device features
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        // required for wireframe mode
        deviceFeatures.fillModeNonSolid = VK_TRUE;

        VkPhysicalDeviceVulkan13Features vulkan13Features{ VulkanHelpers::Initializers::PhysicalDeviceVulkan13Features() };

        // required for vkCmdPipelineBarrier2 used when image transitions
        vulkan13Features.synchronization2 = VK_TRUE;

#if defined( MKT_USE_VULKAN_DYNAMIC_RENDERING )
        vulkan13Features.dynamicRendering = VK_TRUE;
#endif

        VkPhysicalDeviceVulkan12Features enabled12Features{ VulkanHelpers::Initializers::PhysicalDeviceVulkan12Features() };

        // Enable bind-less
        enabled12Features.descriptorIndexing = VK_TRUE;
        enabled12Features.runtimeDescriptorArray = VK_TRUE;
        enabled12Features.descriptorBindingPartiallyBound = VK_TRUE;
        enabled12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
        enabled12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        enabled12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        enabled12Features.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
        enabled12Features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
        enabled12Features.pNext = std::addressof( vulkan13Features );

        VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{ VulkanHelpers::Initializers::PhysicalDeviceFeatures2() };
        physicalDeviceFeatures2.features = deviceFeatures;
        physicalDeviceFeatures2.pNext = std::addressof( enabled12Features );

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

        MKT_CORE_LOGGER_INFO( "VulkanDevice::Shutdown - Shutting down Vulkan Device." );

        // Wait for pending operations
        WaitQueuesIdle();

        m_PendingCmdLists.clear();

        // Clear resources (pools, etc)
        m_Textures.Shutdown();
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

    auto VulkanDevice::WaitQueuesIdle() const -> void {
        if (m_Queues.Graphics.has_value()) {
            vkQueueWaitIdle( m_Queues.Graphics->Queue );
        }

        if (m_Queues.Compute.has_value()) {
            vkQueueWaitIdle( m_Queues.Compute->Queue );
        }

        if (m_Queues.Present.has_value()) {
            vkQueueWaitIdle( m_Queues.Present->Queue );
        }
    }

    auto VulkanDevice::CreateTexture( const TextureDescription& description ) -> TextureHandle {
        TextureHandle texture{ m_Textures.Allocate( description ) };
        if ( texture.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreateTexture - Failed to allocate texture resource." );
            return TextureHandle::CreateEmpty();
        }

        texture->Initialize( this );

        // we use VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL for images to be sampled
        // we do not want to add to the descriptor depth images as we
        if ( texture->GetTextureUsage() != TextureUsage::TEXTURE_USAGE_DEPTH ) {
            // We update the set later it might be in use
            const auto renderer{ dynamic_cast<VulkanRenderer*>( RenderService::Get()->GetBackend() ) };
            renderer->RegisterTextureForRender( texture );
        }

        return texture;
    }

    auto VulkanDevice::RunGarbageCollection() -> void {
        for (auto& [frameIndex, frameFence] : m_FrameFences) {
            if (frameFence == VK_NULL_HANDLE) {
                continue;
            }

            vkWaitForFences(m_LogicalDevice, 1, &frameFence, VK_TRUE, UINT64_MAX);

            VkResult result{ vkGetFenceStatus(m_LogicalDevice, frameFence) };
            if (result == VK_SUCCESS) {
                // GPU finished work for this frame
                //vkResetFences(m_LogicalDevice, 1, &frameFence);

                // Recycle commands for this frame
                auto it{ m_PendingCmdLists.find(frameIndex) };
                if (it != m_PendingCmdLists.end()) {
                    for (auto& cmd : it->second) {
                        if (!cmd.IsEmpty()) {
                            // You might call cmd->Recycle() or return to pool
                            if (VulkanCommandPoolHandle pool = m_CmdPools.GetResource(); !pool.IsEmpty()) {
                                pool->DestroyCommandList( cmd );
                            }
                        }
                    }

                    it->second.clear();
                }
            } else if (result == VK_NOT_READY) {
                continue;
            }
            else {
                MKT_CORE_LOGGER_ERROR("VulkanDevice::RunGarbageCollection - Fence check failed (frame {}, result = {})!", frameIndex, static_cast<int>(result));
            }
        }
    }

    auto VulkanDevice::GetNativeHandle( ObjectType type ) -> Object {
        return Object( m_LogicalDevice );
    }


    auto VulkanDevice::CreateCommandList( QueueType queue ) -> CommandListHandle {
        // Find pool we can allocate command lists for the specified type of queue
        CommandListHandle resultCommandList{};

        // Get the first available pool
        if ( VulkanCommandPoolHandle pool{ m_CmdPools.GetResource() }; !pool.IsEmpty() ) {
            resultCommandList = pool->AllocateCmdList();
            if ( resultCommandList.IsEmpty() ) {
                MKT_THROW_RUNTIME_ERROR( "VulkanDevice::CreateCommandList - Failed to allocate command list." );
            }
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

    auto VulkanDevice::CreateSampler( const SamplerDescription& description ) -> SamplerHandle {
        SamplerHandle sampler{ m_Samplers.Allocate( description ).As<Sampler>() };
        if ( sampler.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreateSampler - Failed to allocate sampler resource." );
            return SamplerHandle::CreateEmpty();
        }

        sampler->Initialize( this );

        return sampler;
    }

    auto VulkanDevice::CreatePipeline( const ComputePipelineDescription& description ) -> PipelineHandle {
        PipelineHandle computePipeline{ m_ComputePipelines.Allocate( description ).As<IPipeline>() };
        if ( computePipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreatePipeline - Failed to allocate compute pipeline resource." );
            return PipelineHandle::CreateEmpty();
        }

        computePipeline->Initialize( this );

        return computePipeline;
    }

    auto VulkanDevice::CreatePipeline( const GraphicsPipelineDescription& description ) -> PipelineHandle {
        auto defaultInfo{ GetDefaultGraphicsPipelineConfigInfo() };
        defaultInfo.Layout = description.DefaultVertexLayout;
        defaultInfo.Depth = description.DepthTexture;
        defaultInfo.ColorAttachments = description.ColorAttachments;
        defaultInfo.ShaderModules = description.ShaderStages;

        PipelineHandle graphicsPipeline{ m_GraphicsPipelines.Allocate( defaultInfo ).As<IPipeline>() };
        if ( graphicsPipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::CreatePipeline - Failed to allocate graphics pipeline resource." );
            return PipelineHandle::CreateEmpty();
        }

        graphicsPipeline->Initialize( this );

        return graphicsPipeline;
    }

    auto VulkanDevice::LoadShader( const Path& path, ShaderStage stage ) -> ShaderModuleHandle {
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

    auto VulkanDevice::GetDummyTexture() -> TextureHandle {
        return AssetsService::Get()->GetDummyTexture();
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

    auto VulkanDevice::AllocateDescriptorSet( const VkDescriptorSetLayout& layout, const void* pNext ) -> VkDescriptorSet {
        VkDescriptorSet result{ *m_DescriptorAllocator.Allocate( layout, pNext ) };
        if ( result != VK_NULL_HANDLE ) {
            return result;
        }

        return VK_NULL_HANDLE;
    }

    auto VulkanDevice::AllocateDescriptorSetLayout( const VkDescriptorSetLayoutCreateInfo& layout ) -> DescriptorSetLayoutHandle {
        DescriptorSetLayoutHandle setLayout{ m_DescriptorSetLayouts.Allocate( layout ).As<DescriptorSetLayout>() };
        if ( setLayout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice::AllocateDescriptorSetLayout - Failed to allocate texture resource with VkImageViewCreateInfo." );
            return DescriptorSetLayoutHandle::CreateEmpty();
        }

        setLayout->Initialize( this );

        return setLayout;
    }

    auto VulkanDevice::SubmitCommands( CommandListHandle cmd ) -> void {
        UInt32 currentFrame{ VulkanContext::Get()->GetCurrentFrameIndex() };

        m_PendingCmdLists[currentFrame].emplace_back( cmd );
    }

    auto VulkanDevice::CreateSwapChain( const VulkanSwapChainCreateInfo& createInfo ) -> SwapChainHandle {
        SwapChainHandle result{ m_Swapchains.Allocate( createInfo ) };
        if ( !result.IsEmpty() ) {
            result.As<DeviceObject>()->Initialize( this );
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

    auto VulkanDevice::IsBindlessEnabled() const -> bool {
        return m_IsBindlessEnabled;
    }

    auto VulkanDevice::FlushPendingCommands( const FrameSynchronizationPrimitives& syncPrimitives ) -> void {
        const UInt32 currentFrame{ VulkanContext::Get()->GetCurrentFrameIndex() };

        auto& cmdList = m_PendingCmdLists[currentFrame];
        if ( cmdList.empty() ) {
            return;
        }

        std::vector<VkCommandBufferSubmitInfo> cmdInfos;
        cmdInfos.reserve( cmdList.size() );

        for ( auto& commandBuffer: cmdList ) {
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

        // Add the fence we will be waiting on later
        if (!m_FrameFences.contains( currentFrame )) {
            m_FrameFences[currentFrame] = syncPrimitives.RenderFence;
        }

        MKT_VK_CHECK( vkQueueSubmit2( m_Queues.Graphics->Queue, 1, &submitInfo, syncPrimitives.RenderFence ) );
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

        // Issue the copy command
        vkCmdCopyBufferToImage(
                m_CmdBuffer,
                *vkSrc->GetImplHandle(),// VkBuffer
                *vkDest->GetImage(),    // VkImage
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copyRegion );

        // Transition the image to SHADER_READ_ONLY for sampling
        vkDest->SubmitLayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_CmdBuffer );
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

        if ( !dest->IsSwapChainImage() ) {
            dest->SubmitLayoutTransition( VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_CmdBuffer );
        } else {
            dest->SubmitLayoutTransition( VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, m_CmdBuffer );
        }
    }

    auto VulkanCmdList::WriteBuffer( Buffer* target, Byte* data, Size size ) -> void {
        // Can be device local data
        // Here I can encapsulate all the logic about creating staging buffers etc.
    }

    auto VulkanCmdList::WriteTexture( Texture* target, Byte* data, Size size ) -> void {
        // Can be device local data
        // Here I can encapsulate all the logic about creating staging buffers, etc.
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
    auto VulkanCommandPool::DestroyCommandList( CommandListHandle cmd ) -> void {
        m_CmdLists.Release( cmd->GetHandle() );
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

    auto VulkanCommandPool::Initialize() -> void {
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
