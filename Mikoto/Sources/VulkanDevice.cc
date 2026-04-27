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

// Vector for very
// specific usage. See queue creation
#include <vector>
#include <ranges>

#include <volk.h>
#include <vk_mem_alloc.h>

#include <EASTL/utility.h>
#include <EASTL/vector.h>
#include <EASTL/array.h>
#include <EASTL/unordered_map.h>

#include <tracy/TracyVulkan.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Math/Math.hh>

#include <Filesystem/FileService.hh>

#include <Renderer/Core/RenderSystem.hh>

#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanPipeline.hh>
#include <Renderer/Vulkan/VulkanTexture.hh>

namespace mikoto::renderer::vulkan {

    Device::Device( const GpuDeviceCreateInfo& createInfo )
        : GpuDevice{ createInfo.mApi, createInfo.mFeaturesSupport }{
    }

    auto Device::Init() -> void {
        MKT_CORE_LOGGER_INFO( "VulkanDevice::Init - Initializing Vulkan Device." );

        // Prepare context info
        Context *ctx{ as<Context *>( RenderSystem::Get()->GetContext() ) };
        Instance& vulkanInstance{ ctx->GetInstance() };

        // Choose primary physical device
        if ( mFeaturesSupport.mEnablePresentation ) {
            mSurface = vulkanInstance.mSurface;
            mExtensions.emplace_back( VK_KHR_SWAPCHAIN_EXTENSION_NAME );
        }

        const auto pdIt{ std::ranges::find_if( vulkanInstance.mPhysicalDevices, [this](PhysicalDevice& dev) {
            return IsDeviceSuitable( dev );
        } ) };

        if (pdIt != vulkanInstance.mPhysicalDevices.end()) {
            mPhysicalDevice = MKT_ADDRESSOF( *pdIt );
            mName = mPhysicalDevice->mProperties.deviceName;
        } else {
            MKT_CORE_LOGGER_INFO( "VulkanDevice - No suitable physical device with the desired features." );
            return;
        }

        // Create the logical device
        InitLogicalDevice();

        // Create logical queues
        InitLogicalQueues();

        // Init memory allocator
        InitMemoryAllocator();

        // Init descriptor manager
        InitDescriptorAllocator();

        CreateDummyResources();

        mIsInitialized = true;
    }

    auto Device::Shutdown() -> void {
        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "VulkanDevice Shutting down Vulkan Device." );

        WaitQueuesIdle();

        DestroyDummyResources();

        // Clear descriptor manager
        mDescriptorAllocatorPool.reset();

        // Clear upload manager
        mUploadManager.reset();

        for (auto& queue : mQueues | std::ranges::views::values ) {
            queue->Shutdown();
        }

        mGpuAllocator->Shutdown();

        // Destroy the device
        vkDestroyDevice( mLogicalDevice, nullptr );

        mIsInitialized = false;
    }

    auto Device::CreateBuffer( const BufferCreateDescription &description ) -> BufferHandle {
        BufferHandle buffer{ Ref<Buffer>::Spawn(description) };

        if ( buffer.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate buffer resource." );
            return BufferHandle::CreateEmpty();
        }

        buffer->Initialize( this );

        return buffer;
    }

    auto Device::CreateTexture( const TextureCreateDescription &description ) -> TextureHandle {
        TextureHandle texture{ Ref<Texture>::Spawn(description) };

        if ( texture.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice - Failed to allocate texture" );
            return TextureHandle::CreateEmpty();
        }

        texture->Initialize( this );

        return texture;
    }

    auto Device::CreateTextureNative( ObjectType type, Object object, const TextureCreateDescription &description ) -> TextureHandle {
        return TextureHandle::CreateEmpty();
    }

    auto Device::CreateFrameBuffer( const FramebufferDescription &description ) -> FramebufferHandle {
        FramebufferHandle framebuffer{ Ref<Framebuffer>::Spawn(description) };
        if ( framebuffer.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate framebuffer resource." );
            return FramebufferHandle::CreateEmpty();
        }

        framebuffer->Initialize( this );

        return framebuffer;
    }

    auto Device::CreateSampler( const SamplerCreateDescription &description ) -> SamplerHandle {
        SamplerHandle sampler{ Ref<Sampler>::Spawn(description) };

        if ( sampler.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate sampler resource." );
            return SamplerHandle::CreateEmpty();
        }

        sampler->Initialize( this );

        return sampler;
    }

    auto Device::CreateAccelStructure( const AccelStructureCreateDescription &description ) -> AccelStructureHandle {
        return AccelStructureHandle::CreateEmpty();
    }

    auto Device::CreateShader( const ShaderModuleCreateDescription &desc ) -> ShaderModuleHandle {
        ShaderModuleHandle result{ Ref<Shader>::Spawn(desc) };

        if ( result.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to create shader." );
            return ShaderModuleHandle::CreateEmpty();
        }

        result->Initialize( this );

        return result;
    }

    auto Device::CreateShader( ShaderStage type, const void *code, size_t codeSize ) -> ShaderModuleHandle {
        return ShaderModuleHandle::CreateEmpty();
    }

    auto Device::CreateInputLayout( const InputLayoutCreateDescription& desc ) -> InputLayoutHandle {
        InputLayoutHandle layout{ Ref<InputLayout>::Spawn( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return InputLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreateBindingLayout( const BindingLayoutDescription &desc ) -> BindingLayoutHandle {
        BindingLayoutHandle layout{ Ref<BindingLayout>::Spawn( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return BindingLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreatePipelineLayout( const PipelineLayoutCreateDescription& desc ) -> PipelineLayoutHandle {
        PipelineLayoutHandle layout{ Ref<PipelineLayout>::Spawn( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate pipeline layout resource." );
            return PipelineLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreateBindingSet( const BindingSetDescription &desc, BindingLayoutHandle layout ) -> BindingSetHandle {
        BindingSetHandle set{ Ref<BindingSet>::Spawn( desc, layout ) };

        if ( set.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding set resource." );
            return BindingSetHandle::CreateEmpty();
        }

        set->Initialize( this );

        return set;
    }

    auto Device::UnMap( IBuffer* buffer ) -> void {
    }

    auto Device::Map( IBuffer* buffer ) -> const void* {
        return nullptr;
    }

    auto Device::CreateBindlessLayout( const BindlessLayoutDescription &desc ) -> BindingLayoutHandle {
        BindingLayoutHandle layout{ Ref<BindingLayout>::Spawn( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return BindingLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreateDescriptorTable( BindingLayoutHandle layout ) -> DescriptorTableHandle {
        return DescriptorTableHandle::CreateEmpty();
    }

    auto Device::ResizeDescriptorTable( DescriptorTableHandle descriptorTable, u32 newSize, bool keepContents ) -> bool {
        return false;
    }

    auto Device::WriteDescriptorTable( DescriptorTableHandle descriptorTable, const BindingSetItem &item ) -> bool {
        return false;
    }

    auto Device::Flush() -> void {
        for (auto& queue : mQueues | std::ranges::views::values ) {
            queue->Flush();
        }
    }

    auto Device::CreateTexture( const ExternalTextureDescription &info ) -> TextureHandle {
        TextureHandle texture{ Ref<Texture>::Spawn(info) };

        if ( texture.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice - Failed to allocate texture" );
            return TextureHandle::CreateEmpty();
        }

        texture->Initialize( this );

        return texture;
    }

    auto Device::CreateBinarySemaphore() -> SemaphoreHandle {
        SemaphoreHandle semaphore{ Ref<BinarySemaphore>::Spawn() };

        if ( semaphore.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice - Failed to allocate texture" );
            return SemaphoreHandle::CreateEmpty();
        }

        semaphore->Initialize( this );

        return semaphore;
    }

    auto Device::CreateTimelineSemaphore( u64 initialValue ) -> SemaphoreHandle {
        SemaphoreHandle semaphore{ Ref<TimelineSemaphore>::Spawn(initialValue) };

        if ( semaphore.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice - Failed to allocate texture" );
            return SemaphoreHandle::CreateEmpty();
        }

        semaphore->Initialize( this );

        return semaphore;
    }

    auto Device::WaitForSubmission( QueueType queueType, u64 submissionID ) -> void {
        mQueues[queueType]->WaitForSubmission( submissionID );
    }

    auto Device::AddQueueWaitFence( QueueType queueType, Fence* fence ) -> void {
        mQueues[queueType]->AddQueueWaitFence( fence );
    }

    auto Device::AddQueueSignalSemaphore( QueueType queueType, BinarySemaphore* semaphore, VkPipelineStageFlags2 stageFlags ) -> void {
        mQueues[queueType]->AddQueueSignalSemaphore( semaphore, stageFlags );
    }

    auto Device::AddQueueWaitSemaphore( QueueType queueType, BinarySemaphore* semaphore, VkPipelineStageFlags2 stageFlags ) -> void {
        mQueues[queueType]->AddQueueWaitSemaphore( semaphore, stageFlags );
    }

    auto Device::SetDebugName( VkObjectType objectType, u64 handle, eastl::string_view name ) -> void {
        VkDebugUtilsObjectNameInfoEXT nameInfo{};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = objectType;
        nameInfo.objectHandle = handle;
        nameInfo.pObjectName = name.data();

        if ( vkSetDebugUtilsObjectNameEXT ) {
            vkSetDebugUtilsObjectNameEXT( mLogicalDevice, MKT_ADDRESSOF( nameInfo ) );
        } else {
            MKT_CORE_LOGGER_WARN( "vkGetDeviceProcAddr is null, cannot set debug name." );
        }
    }

    auto Device::CreatePipeline( const ComputePipelineDescription &description ) -> PipelineHandle {
        PipelineHandle computePipeline{ Ref<ComputePipeline>::Spawn( description ) };

        if ( computePipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate compute pipeline resource." );
            return PipelineHandle::CreateEmpty();
        }

        computePipeline->Initialize( this );

        return computePipeline;
    }

    auto Device::CreatePipeline( const GraphicsPipelineDescription &description ) -> PipelineHandle {
        PipelineHandle graphicsPipeline{ Ref<GraphicsPipeline>::Spawn( description ) };

        if ( graphicsPipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate graphics pipeline resource." );
            return PipelineHandle::CreateEmpty();
        }

        graphicsPipeline->Initialize( this );

        return graphicsPipeline;
    }

    auto Device::CreateCommandList( QueueType queueType ) -> CommandListHandle {
        Queue* queue{ GetQueue( queueType ) };

        auto handle{ CommandListHandle::CreateEmpty() };
        if (queue) {
            handle = queue->AllocateCmdList();
        }

        return handle;
    }

    auto Device::CreateCommandList( const CommandListParameters &parameters ) -> CommandListHandle {
        return CommandListHandle::CreateEmpty();
    }

    auto Device::RunGarbageCollection() -> void {
        mUploadManager->ReclaimMemory();
        mDescriptorAllocatorPool->Flip();
    }

    auto Device::SubmitCommands( CommandListHandle cmd ) -> u64 {
        MKT_ASSERT( mQueues.contains( cmd->GetQueueType() ), "No queue" );
        return mQueues[cmd->GetQueueType()]->SubmitCommandList( cmd );
    }

    auto Device::ExecuteCommands( CommandListHandle cmd ) -> u64 {
        return mQueues[cmd->GetQueueType()]->ExecuteCommandList( cmd );
    }

    auto Device::WaitIdle() -> void {
        WaitQueuesIdle();
    }

    auto Device::WaitQueuesIdle() const -> void {
        for (const auto& queue : mQueues | std::ranges::views::values ) {
            queue->WaitIdle();
        }
    }

    auto Device::SubmitDeletion( eastl::function<void( GpuDevice * )> &&callback ) -> void {

    }

    auto Device::GetDummySampler() -> Sampler * {
        return checked_cast<Sampler*>( mDummySampler.GetRaw() );
    }

    auto Device::GetLayoutForEmptySet() -> VkDescriptorSetLayout {
        return mEmptyBindingLayout->GetNativeHandle( ObjectType::Vk_DescriptorSetLayout );
    }

    auto Device::GetUploadManager() -> GpuUploadManager * {
        return mUploadManager.get();
    }

    auto Device::GetDescriptorAllocator() -> DescriptorAllocatorHandle {
        return mDescriptorAllocatorPool->GetAllocator();
    }

    auto Device::GetPhysicalDevice() -> PhysicalDevice* {
        return mPhysicalDevice;
    }

    auto Device::GetAllocator() -> GpuMemoryAllocator * {
        return as<GpuMemoryAllocator*>(mGpuAllocator.get());
    }

    auto Device::GetQueue( QueueType type ) -> Queue * {
        return const_cast<Queue *>( eastl::as_const( *this ).GetQueue( type ) );
    }

    auto Device::GetQueue( QueueType type ) const -> const Queue* {
        const auto it{ mQueues.find(type) };
        return it != mQueues.end() ? it->second.get() : nullptr;
    }

    auto Device::GetActivePhysicalDeviceFeatures() const -> const VkPhysicalDeviceFeatures & {
        return mEnabledFeatures;
    }

    auto Device::GetActivePhysicalDeviceFeatures2() const -> const VkPhysicalDeviceFeatures2 & {
        return mEnabledFeatures2;
    }

    auto Device::GetActive11Features() const -> const VkPhysicalDeviceVulkan11Features & {
        return mEnabled11Features;
    }

    auto Device::GetActive12Features() const -> const VkPhysicalDeviceVulkan12Features & {
        return mEnabled12Features;
    }

    auto Device::GetActive13Features() const -> const VkPhysicalDeviceVulkan13Features & {
        return mEnabled13Features;
    }

    auto Device::CreateSwapChain( const SwapChainCreateInfo &createInfo ) -> SwapChainHandle {
        SwapChainHandle handle{ SwapChainHandle::Spawn( createInfo ) };
        if (!handle.IsEmpty()) {
            handle->Initialize(this);
        }

        return handle;
    }

    auto Device::InitLogicalDevice() -> void {
        // --- Vulkan 1.3 Features ---
        mEnabled13Features = initializers::PhysicalDeviceVulkan13Features();
        mEnabled13Features.synchronization2 = VK_TRUE;
        mEnabled13Features.dynamicRendering = VK_TRUE;

        // --- Vulkan 1.2 Features ---
        mEnabled12Features = initializers::PhysicalDeviceVulkan12Features();
        mEnabled12Features.pNext = MKT_ADDRESSOF( mEnabled13Features );

        mEnabled12Features.descriptorIndexing = VK_TRUE;
        mEnabled12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        mEnabled12Features.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
        mEnabled12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        mEnabled12Features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
        mEnabled12Features.descriptorBindingPartiallyBound = VK_TRUE;
        mEnabled12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
        mEnabled12Features.runtimeDescriptorArray = VK_TRUE;
        mEnabled12Features.scalarBlockLayout = VK_TRUE;
        mEnabled12Features.timelineSemaphore = VK_TRUE;
        mEnabled12Features.bufferDeviceAddress = VK_TRUE;

        // --- Vulkan 1.1 Features
        mEnabled11Features = initializers::PhysicalDeviceVulkan11Features();
        mEnabled11Features.pNext = MKT_ADDRESSOF( mEnabled12Features );
        mEnabled11Features.shaderDrawParameters = VK_TRUE;

        // --- Core device features ---
        mEnabledFeatures.sampleRateShading = VK_TRUE;
        mEnabledFeatures.multiDrawIndirect = VK_TRUE;
        mEnabledFeatures.drawIndirectFirstInstance = VK_TRUE;
        mEnabledFeatures.fillModeNonSolid = VK_TRUE;
        mEnabledFeatures.wideLines = VK_TRUE;
        mEnabledFeatures.samplerAnisotropy = VK_TRUE;

        // --- Final root features struct ---
        mEnabledFeatures2 = initializers::PhysicalDeviceFeatures2();
        mEnabledFeatures2.pNext = MKT_ADDRESSOF( mEnabled11Features );
        mEnabledFeatures2.features = mEnabledFeatures;

        VkDeviceCreateInfo createInfo{ initializers::DeviceCreateInfo() };
        createInfo.pNext = MKT_ADDRESSOF( mEnabledFeatures2 );

        createInfo.pEnabledFeatures = nullptr;

        // Prepare queue infos
        // All queues with same priority
        eastl::unordered_map<u32, eastl::vector<float>> mQueuePriorities{};
        eastl::vector<VkDeviceQueueCreateInfo> queueCreateInfos{ mPhysicalDevice->mQueueInfos.size() };
        for (auto& [familyIndex, queueData] : mPhysicalDevice->mQueueInfos) {
            MKT_ASSERT( familyIndex < queueCreateInfos.size(), "Queue index out of bounds." );
            queueCreateInfos[familyIndex] = initializers::DeviceQueueCreateInfo();

            queueCreateInfos[familyIndex].queueFamilyIndex = familyIndex;

            mQueuePriorities[familyIndex] = eastl::vector<float>(queueData.mProperties.queueCount, 1.0f);
            queueCreateInfos[familyIndex].queueCount = as<u32>(mQueuePriorities[familyIndex].size());
            queueCreateInfos[familyIndex].pQueuePriorities = mQueuePriorities[familyIndex].data();
        }

        createInfo.queueCreateInfoCount = as<u32>( queueCreateInfos.size() );
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        // I am only using std::vector here because of views
        // and the resulting view iterators are not compatible with eastl::vector
        auto enabledExtensions{
            mExtensions | std::views::transform( []( const eastl::string &s ) -> const char * {
                return s.c_str();
            } )
        };
        std::vector<const char*> extensionNames{ enabledExtensions.begin(), enabledExtensions.end() };

        createInfo.enabledExtensionCount = as<u32>( extensionNames.size() );
        createInfo.ppEnabledExtensionNames = extensionNames.data();

        MKT_VK_CHECK( vkCreateDevice( mPhysicalDevice->mPhysicalDevice, MKT_ADDRESSOF( createInfo ), nullptr, MKT_ADDRESSOF( mLogicalDevice ) ) );

        /**
          * [...] all device-related function calls, such as vkCmdDraw, will go through Vulkan loader dispatch code.
          * This allows you to transparently support multiple VkDevice objects in the same application, but comes at
          * a price of dispatch overhead which can be as high as 7% depending on the driver and application.
          *
          * To avoid this, For applications that use just one VkDevice object, load device-related
          * Vulkan entry-points directly from the driver with void volkLoadDevice(VkDevice device);
          * See: https://github.com/zeux/volk
          * */
        volkLoadDevice( mLogicalDevice );
    }

    auto Device::InitLogicalQueues() -> void {
        // Right now we do not have any proper filter for each type of queue
        // Just pick the first queue amongst the available queues that satisfies these operations,
        // in fact they might all end up being the same queue family
        // For every family index I keep track of the queue index
        ankerl::unordered_dense::map<u32, u32> queuesIndices{};

        // Graphics queue
        if (auto* graphicsQueueData{ mPhysicalDevice->GetQueueWithSupport( QueueOpSupportFlagsBits::kGraphics ) }) {
            u32 queueIndex{ queuesIndices[graphicsQueueData->FamilyIndex]++ };
            mQueues[QueueType::eGraphics] = eastl::make_unique<Queue>( this, QueueType::eGraphics, graphicsQueueData->FamilyIndex, queueIndex);
        }

        // Compute queue
        if (auto* computeQueueData{ mPhysicalDevice->GetQueueWithSupport( QueueOpSupportFlagsBits::kCompute ) }) {
            u32 queueIndex{ queuesIndices[computeQueueData->FamilyIndex]++ };
            mQueues[QueueType::eCompute] = eastl::make_unique<Queue>( this, QueueType::eCompute, computeQueueData->FamilyIndex, queueIndex);
        }

        // Transfer queue
        if (auto* transferQueueData{ mPhysicalDevice->GetQueueWithSupport( QueueOpSupportFlagsBits::kTransfer ) }) {
            u32 queueIndex{ queuesIndices[transferQueueData->FamilyIndex]++ };
            mQueues[QueueType::eTransfer] = eastl::make_unique<Queue>( this, QueueType::eTransfer, transferQueueData->FamilyIndex, queueIndex);
        }

        // Presentation queue
        if (mFeaturesSupport.mEnablePresentation) {
            if (auto* presentQueueData{ mPhysicalDevice->GetQueueWithSupport( QueueOpSupportFlagsBits::kPresentation ) }) {
                u32 queueIndex{ queuesIndices[presentQueueData->FamilyIndex]++ };
                mQueues[QueueType::ePresent] = eastl::make_unique<Queue>( this, QueueType::ePresent, presentQueueData->FamilyIndex, queueIndex);
            }
        }

        for (const auto& queue : mQueues | std::ranges::views::values) {
            queue->Initialize();
        }
    }

    auto Device::InitMemoryAllocator() -> void {
        mGpuAllocator = IGpuAllocator::Create( this );
        if ( !mGpuAllocator ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanDevice - Could not create GPU Allocator." );
        }

        mUploadManager = eastl::make_unique<GpuUploadManager>( this );

        mGpuAllocator->Init();
    }

    auto Device::InitDescriptorAllocator() -> void {
        Context *ctx{ as<Context *>( RenderSystem::Get()->GetContext() ) };
        mDescriptorAllocatorPool = IDescriptorAllocatorPool::Create( mLogicalDevice, ctx->GetMaxFramesInFlight() );

        constexpr float poolMultiplier{ 3.0f };

        mDescriptorAllocatorPool->SetPoolSizeMultiplier(VK_DESCRIPTOR_TYPE_SAMPLER,poolMultiplier );
        mDescriptorAllocatorPool->SetPoolSizeMultiplier(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,poolMultiplier );

        mDescriptorAllocatorPool->SetPoolSizeMultiplier(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,poolMultiplier );
        mDescriptorAllocatorPool->SetPoolSizeMultiplier(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,poolMultiplier );

        mDescriptorAllocatorPool->SetPoolSizeMultiplier(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,poolMultiplier );
        mDescriptorAllocatorPool->SetPoolSizeMultiplier(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,poolMultiplier );

        mDescriptorAllocatorPool->SetPoolSizeMultiplier(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER,poolMultiplier );
        mDescriptorAllocatorPool->SetPoolSizeMultiplier(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER,poolMultiplier );

        mDescriptorAllocatorPool->SetPoolSizeMultiplier(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,poolMultiplier );
        mDescriptorAllocatorPool->SetPoolSizeMultiplier(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,poolMultiplier );

        mDescriptorAllocatorPool->SetPoolSizeMultiplier(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,poolMultiplier );
    }

    auto Device::GetPrimaryPhysicalDevice() -> void {
    }

    auto Device::CreatePrimaryLogicalDevice() -> void {
    }

    auto Device::InitTracyContext() -> void {
    }

    auto Device::ShutdownTracyContext() -> void {
    }

    auto Device::CreateDummyResources() -> void {
        mDummySampler = CreateSampler( SamplerCreateDescription{} );
        mEmptyBindingLayout = CreateBindingLayout( BindingLayoutDescription{} );
    }

    auto Device::DestroyDummyResources() -> void {
        mDummySampler.Reset();
        mEmptyBindingLayout.Reset();
    }

    auto Device::IsDeviceSuitable( const PhysicalDevice& device ) -> bool {
        // Extensions
        if (!device.IsExtensionListAvailable( mExtensions )) {
            return false;
        }

        // Check type of GPU
        VkPhysicalDeviceType devType{ GetGpuDeviceType( mFeaturesSupport.mDeviceType ) };
        if (devType != device.mProperties.deviceType) {
            return false;
        }

        // By default, we look for a device that supports graphics and transfer and optionally presentation
        QueueOpSupportFlags opSupportFlags{
                QueueOpSupportFlagsBits::kGraphics |
                QueueOpSupportFlagsBits::kCompute |
                QueueOpSupportFlagsBits::kTransfer
        };

        // Dynamic rendering is mandatory because Mikoto targets
        // Vulkan 1.3 by default where this feature is core, this should be just a sanity check
        // because the instance is already created with this in mind, with this the MKT_USE_VULKAN_DYNAMIC_RENDERING macro is deprecated
        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{ initializers::DynamicRenderingFeature() };

        VkPhysicalDeviceFeatures2 features2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = MKT_ADDRESSOF( dynamicRenderingFeature )
        };
        vkGetPhysicalDeviceFeatures2( device.mPhysicalDevice, MKT_ADDRESSOF( features2 ) );
        if (dynamicRenderingFeature.dynamicRendering != VK_TRUE) {
            return false;
        }

        // Wireframe support
        if (mFeaturesSupport.mHardwareWireframe && !device.mFeatures.fillModeNonSolid) {
            return false;
        }

        // Improved texture quality
        if (mFeaturesSupport.mAnisotropicFiltering && !device.mFeatures.samplerAnisotropy ) {
            return false;
        }

        // Presentation support
        if (mFeaturesSupport.mEnablePresentation) {
            if (device.mFormats.empty() || device.mPresentModes.empty()) {
                return false;
            }

            opSupportFlags |= QueueOpSupportFlagsBits::kPresentation;
        }

        if (!device.HasQueueSupport( opSupportFlags )) {
            return false;
        }

        return true;
    }

    auto Device::GetDevice() -> VkDevice {
        return mLogicalDevice;
    }

    CommandList::CommandList( const VkCommandBufferAllocateInfo &createInfo, QueueType type )
        : ICommandList{ type }, mAllocInfo{ createInfo } {
    }

    auto CommandList::Begin() -> void {
        if (mIsSubmitted) {
            TryRecycle( checked_cast<Device*>( mDevice )->GetQueue( mQueueType ) );
        }

        ClearState();

        VkCommandBufferBeginInfo beginInfo{ initializers::CommandBufferBeginInfo() };
        MKT_VK_CHECK( vkBeginCommandBuffer( mCmdBuffer, MKT_ADDRESSOF( beginInfo ) ) );
    }

    auto CommandList::End() -> void {
        MKT_VK_CHECK( vkEndCommandBuffer( mCmdBuffer ) );
    }

    auto CommandList::BeginTrackingState( IBuffer *buffer, ResourceStates newState ) -> void {
        auto oldState{ buffer->GetResourceState() };

        // Before any read access you need to make sure the previous writes
        // are completed and the image is in the correct layout hence why we insert a barrier even if it has proper layout
        // if (oldState == newState)
        //     return;

        VkBufferMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;

        barrier.srcStageMask  = GetStageMask(oldState);
        barrier.srcAccessMask = GetAccessMask(oldState);

        barrier.dstStageMask  = GetStageMask(newState);
        barrier.dstAccessMask = GetAccessMask(newState);

        barrier.buffer = buffer->GetNativeHandle(ObjectType::Vk_Buffer);
        barrier.offset = 0;
        barrier.size   = VK_WHOLE_SIZE;

        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        mBufferBarriers.push_back(barrier);

        buffer->SetResourceState(newState);

    }

    auto CommandList::BeginTrackingState( ITexture *texture, ResourceStates newState ) -> void {
        auto oldState{ texture->GetResourceState() };

        // Before any read access you need to make sure the previous writes
        // are completed and the image is in the correct layout hence why we insert a barrier even if it has proper layout
        // if (oldState == newState)
        //     return;

        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

        barrier.srcStageMask  = GetStageMask(oldState);
        barrier.srcAccessMask = GetAccessMask(oldState);

        barrier.dstStageMask  = GetStageMask(newState);
        barrier.dstAccessMask = GetAccessMask(newState);

        barrier.oldLayout = GetImageLayout(oldState);
        barrier.newLayout = GetImageLayout(newState);

        barrier.image = texture->GetNativeHandle(ObjectType::Vk_Image);

        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        VkImageSubresourceRange range{};
        range.aspectMask = GetAspectMask(texture->GetFormat());
        range.baseMipLevel = 0;
        range.levelCount = texture->GetMipLevelCount();
        range.baseArrayLayer = 0;
        range.layerCount = (texture->GetDimension() == TextureDimension::eTextureCube)
            ? kMaxCubeFaces
            : 1;

        barrier.subresourceRange = range;

        mImageBarriers.push_back(barrier);

        texture->SetResourceState(newState);
    }

    auto CommandList::CommitBarriers() -> void {
        if (mBufferBarriers.empty() && mImageBarriers.empty())
            return;

        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

        depInfo.bufferMemoryBarrierCount = as<u32>(mBufferBarriers.size());
        depInfo.pBufferMemoryBarriers = mBufferBarriers.data();

        depInfo.imageMemoryBarrierCount = as<u32>(mImageBarriers.size());
        depInfo.pImageMemoryBarriers = mImageBarriers.data();

        vkCmdPipelineBarrier2(mCmdBuffer, &depInfo);

        mBufferBarriers.clear();
        mImageBarriers.clear();
    }

    auto CommandList::SetResourceState( IBuffer *buffer, ResourceStates newState ) -> void {
        BeginTrackingState(buffer, newState);
        CommitBarriers();
    }

    auto CommandList::SetResourceState( ITexture *texture, ResourceStates newState ) -> void {
        BeginTrackingState(texture, newState);
        CommitBarriers();
    }

    auto CommandList::SetEnableAutomaticBarriers(  bool enable  ) -> void {
        mEnableAutomaticBarriers = enable;
    }

    auto CommandList::SetClearColor( FramebufferHandle frameBuffer, Color color ) -> void {

    }

    auto CommandList::SetClearColor( TextureHandle renderTargets, Color color ) -> void {

    }

    auto CommandList::WriteTexture( IBuffer *src, ITexture *dest, u32 mipLevel ) -> void {
    }

    auto CommandList::WriteTexture( ITexture *texture, u32 mipLevel, const void *data, size_t byteSize ) -> void {
        auto texturePreviousState{ texture->GetResourceState() };

        if (mEnableAutomaticBarriers) {
            SetResourceState( texture, ResourceStates::eCopyDest );
        }

        GpuUploadAllocation* allocation{ mUploadManager->SubAllocate( byteSize ) };
        SetResourceState( allocation->mBuffer, ResourceStates::eCopySource );

        std::memcpy( allocation->mMappedMemory, data, byteSize );

        // Describe the region to copy
        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = allocation->mOffset;
        copyRegion.bufferRowLength = 0;  // Tightly packed
        copyRegion.bufferImageHeight = 0;// Tightly packed

        copyRegion.imageSubresource.aspectMask = GetAspectMask( texture->GetFormat() );

        copyRegion.imageSubresource.mipLevel = mipLevel;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;

        copyRegion.imageOffset = { 0, 0, 0 };
        copyRegion.imageExtent = {
            texture->GetWidth(),
            texture->GetHeight(),
            1
        };

        // Issue the copy command
        vkCmdCopyBufferToImage(
                mCmdBuffer,
                allocation->mBuffer->GetNativeHandle( ObjectType::Vk_Buffer ),
                texture->GetNativeHandle( ObjectType::Vk_Image ),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &copyRegion );

        if (mEnableAutomaticBarriers) {
            SetResourceState(texture, texturePreviousState == ResourceStates::eUnknown ? ResourceStates::eCommon : texturePreviousState);
        }

        mUploadAllocations.emplace_back( allocation );
    }

    auto CommandList::WriteBuffer( IBuffer *buffer, const void *data, size_t byteSize ) -> void {
        MKT_ASSERT(buffer, "Buffer is nullptr");
        MKT_ASSERT(data, "Data is nullptr");
        MKT_ASSERT(byteSize > 0, "Size is 0");

        // You cannot record transfer ops inside rendering
        if (mRenderPassIsActive) {
            EndRendering();
        }

        auto currentState{ buffer->GetResourceState() };
        if (mEnableAutomaticBarriers) {
            if (buffer->GetResourceState() != ResourceStates::eCopyDest) {
                SetResourceState(buffer, ResourceStates::eCopyDest);
            }
        }

        const VkBuffer vkBuffer{ buffer->GetNativeHandle(ObjectType::Vk_Buffer) };

        // ---------------------------------------------------------
        // Small data -> vkCmdUpdateBuffer (no staging needed)
        // ---------------------------------------------------------
        if (byteSize <= 65536) { // 64 KB limit
            vkCmdUpdateBuffer(
                mCmdBuffer,
                vkBuffer,
                0,
                byteSize,
                data
            );
        } else {
            // TODO: Use local linear allocator for frequently updated data
            // Instead of using the GPU default allocator I can declare a linear allocator for every command buffer
            // On first usage (lazy create) I create the buffer with a large enough size
            // every time I call End() I just reset the allocator
            GpuUploadAllocation* allocation{ mUploadManager->SubAllocate( byteSize ) };
            SetResourceState( allocation->mBuffer, ResourceStates::eCopySource );

            std::memcpy(allocation->mMappedMemory, data, byteSize);

            VkBufferCopy copy{};
            copy.srcOffset = allocation->mOffset;
            copy.dstOffset = 0; // or buffer offset if needed
            copy.size = byteSize;

            vkCmdCopyBuffer(
                mCmdBuffer,
                allocation->mBuffer->GetNativeHandle( ObjectType::Vk_Buffer ),
                buffer->GetNativeHandle( ObjectType::Vk_Buffer ),
                1,
                &copy
            );

            mUploadAllocations.emplace_back( allocation );
        }

        if (mEnableAutomaticBarriers) {
            SetResourceState(buffer, currentState == ResourceStates::eUnknown ? ResourceStates::eCommon : currentState);
        }
    }

    auto CommandList::Draw( const DrawArguments &args ) -> void {
        vkCmdDraw( mCmdBuffer, args.mVertexCount, args.mInstanceCount, args.mFirstVertex, args.mFirstInstance );
    }

    auto CommandList::DrawIndexed( const DrawArguments &args ) -> void {
        vkCmdDrawIndexed(
            mCmdBuffer,
            args.mIndexCount,
            args.mInstanceCount,
            args.mFirstIndex,
            args.mVertexOffset,
            args.mFirstInstance );
    }

    auto CommandList::DrawIndirect( u32 offset, u32 drawCount ) -> void {
    }

    auto CommandList::DrawIndexedIndirect( u32 offset, u32 drawCount ) -> void {
    }

    auto CommandList::SetPushConstants( const void *data, size_t byteSize, ShaderStage visibility ) -> void {

    }

    auto CommandList::SetDebugName( eastl::string_view name ) -> void {
        mDebugName = name;

        auto* device{ checked_cast<Device*>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_COMMAND_BUFFER, rc_cast<u64>( mCmdBuffer ), mDebugName );
    }

    auto CommandList::CopyBuffer( IBuffer *src, IBuffer *dest ) -> void {
        CopyBuffer(src, dest, 0);
    }

    auto CommandList::CopyBuffer( IBuffer *src, IBuffer *dest, size_t dstOffset ) -> void {
        MKT_ASSERT( src != nullptr, "Source buffer cannot be null" );
        MKT_ASSERT( dest != nullptr, "Destination buffer cannot be null" );

        const size_t size{ src->GetSizeBytes() };

        // The data I’m copying fits inside the destination buffer, starting at dstOffset
        MKT_ASSERT(size <= (dest->GetSizeBytes() - dstOffset), "Destination buffer is too small");

        auto srcPreviousState{ src->GetResourceState() };
        auto destPreviousState{ dest->GetResourceState() };

        if (mEnableAutomaticBarriers) {
            SetResourceState(src, ResourceStates::eCopySource);
            SetResourceState(dest, ResourceStates::eCopyDest);
        }

        VkBufferCopy region{};
        region.srcOffset = 0;
        region.dstOffset = dstOffset;
        region.size      = size;

        vkCmdCopyBuffer(
            mCmdBuffer,
            src->GetNativeHandle( ObjectType::Vk_Buffer ),
            dest->GetNativeHandle( ObjectType::Vk_Buffer ),
            1,
            &region
        );

        if (mEnableAutomaticBarriers) {
            SetResourceState(src,  srcPreviousState);
            SetResourceState(dest, destPreviousState);
        }
    }

    auto CommandList::BeginRendering( GraphicsState& state ) -> void {
        if (mEnableAutomaticBarriers) {
            for (auto& rt : state.mCurrentRenderTargets ) {
                if (rt.mRenderTarget->GetResourceState() != ResourceStates::eRenderTarget) {
                    SetResourceState( rt.mRenderTarget.GetRaw(), ResourceStates::eRenderTarget );
                }
            }

            if (state.mDepthTarget.mRenderTarget->GetResourceState() != ResourceStates::eDepthWrite) {
                SetResourceState( state.mDepthTarget.mRenderTarget.GetRaw(), ResourceStates::eDepthWrite );
            }
        }

        bool hasColorTarget{ !state.mCurrentRenderTargets.empty() };
        bool hasDepthTarget{ !state.mDepthTarget.mRenderTarget.IsEmpty() };

        MKT_ASSERT( hasColorTarget || hasDepthTarget, "Must provide either depth target or color targets" );

        std::vector<VkRenderingAttachmentInfo> colorImages{};

        for (const auto& renderTargetProps: state.mCurrentRenderTargets) {
            const Texture* texture{ checked_cast<const Texture*>(renderTargetProps.mRenderTarget.GetRaw()) };

            VkAttachmentLoadOp loadOp{ renderTargetProps.mLoadOp == LoadOp::eClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD };
            VkRenderingAttachmentInfo &colorAttachment{ colorImages.emplace_back( VkRenderingAttachmentInfo{} ) };
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = texture->GetView( 0 );
            colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            colorAttachment.loadOp = loadOp;
            colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorAttachment.clearValue.color = {
                renderTargetProps.mClearColor.mR,
                renderTargetProps.mClearColor.mG,
                renderTargetProps.mClearColor.mB,
                renderTargetProps.mClearColor.mA };
        }

        VkRenderingAttachmentInfo depthAttachment{};
        if (hasDepthTarget) {
            const Texture* texture{ checked_cast<const Texture*>(state.mDepthTarget.mRenderTarget.GetRaw()) };
            VkAttachmentLoadOp loadOp{ state.mDepthTarget.mLoadOp == LoadOp::eClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD };

            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = texture->GetNativeHandle( ObjectType::Vk_ImageView ); // Depth for now defaults to mip 0
            depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depthAttachment.loadOp = loadOp;
            depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            depthAttachment.clearValue.depthStencil = { 1.0f, 0 };
        }

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {
            { state.mRenderArea.mMinX, state.mRenderArea.mMinY },
            { (u32)state.mRenderArea.ComputeWidth(), (u32)state.mRenderArea.ComputeHeight() } };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = as<u32>( colorImages.size() );
        renderingInfo.pColorAttachments = colorImages.data();
        renderingInfo.pDepthAttachment = !hasDepthTarget ?
            nullptr : MKT_ADDRESSOF( depthAttachment );

        vkCmdBeginRendering( mCmdBuffer, std::addressof( renderingInfo ) );

        mRenderPassIsActive = true;
    }

    auto CommandList::EndRendering() -> void {
        vkCmdEndRendering( mCmdBuffer );
        mRenderPassIsActive = false;
    }

    auto CommandList::BindPipeline( IPipeline* pipeline ) -> void {
        VkPipelineBindPoint bindPoint{ VK_PIPELINE_BIND_POINT_MAX_ENUM };
        switch (pipeline->GetPipelineType()) {
            case PipelineType::eGraphics:
                bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                break;
            case PipelineType::eCompute:
                bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
                break;
            default:
                break;
        }

        vkCmdBindPipeline( mCmdBuffer, bindPoint, pipeline->GetNativeHandle( ObjectType::Vk_Pipeline ) );
    }

    auto CommandList::SetViewport( eastl::span<const Viewport> viewports ) -> void {
        eastl::fixed_vector<VkViewport , kMaxViewports> vkViewports{};
        for (const auto& viewport : viewports) {
            VkViewport value{
                .x = viewport.mMinX,
                .y = viewport.mMinY,
                .width = viewport.GetWidth(),
                .height = viewport.GetHeight(),
                .minDepth = 0.0f,
                .maxDepth = 1.0f,
            };

            if (viewport.mFlip) {
                value.x = viewport.mMinX;
                value.y = viewport.GetHeight();
                value.width = viewport.GetWidth();
                value.height = -value.y;
                value.minDepth = 0.0f;
                value.maxDepth = 1.0f;
            }

            vkViewports.emplace_back( value );
        }

        vkCmdSetViewport( mCmdBuffer, 0, as<u32>( vkViewports.size() ), vkViewports.data() );
    }

    auto CommandList::SetScissors( eastl::span<const Rect> scissorRects ) -> void {
        eastl::fixed_vector<VkRect2D, kMaxScissors> scissors{};

        for (const auto& scissor : scissorRects) {
            scissors.emplace_back( VkRect2D{
                .offset = { scissor.mMinX, scissor.mMinY },
                .extent =  { as<u32>( scissor.ComputeWidth() ), as<u32>( scissor.ComputeHeight() ) }
            } );
        }

        vkCmdSetScissor( mCmdBuffer, 0, as<u32>(scissors.size()), scissors.data() );
    }

    auto CommandList::SetViewportState( const ViewportState &vs ) -> void {
        SetViewport( vs.mViewports );
        SetScissors( vs.mScissorRects );
    }

    auto CommandList::BindIndexBuffer( IBuffer *buffer ) -> void {
        vkCmdBindIndexBuffer( mCmdBuffer, buffer->GetNativeHandle( ObjectType::Vk_Buffer ), 0, GetIndexType(buffer->GetFormat()) );
    }

    auto CommandList::BindVertexBuffer( const VertexBufferBinding& binding ) -> void {
        const std::array<VkDeviceSize, 1> offsets{ binding.mOffset };
        const std::array<VkBuffer, 1> vertexBuffers{ binding.mBuffer->GetNativeHandle( ObjectType::Vk_Buffer ) };

        vkCmdBindVertexBuffers( mCmdBuffer, binding.mSlot, 1, vertexBuffers.data(), offsets.data() );
    }

    auto CommandList::BindIndirectBuffer( IBuffer *buffer, u32 stride ) -> void {

    }

    auto CommandList::BindPipelineResources( IPipelineLayout* pipelineLayout, IBindingSet* resourceSet, u32 bindingSlot ) -> void {
        VkPipelineBindPoint bindPoint{ VK_PIPELINE_BIND_POINT_MAX_ENUM  };
        switch ( pipelineLayout->GetBindPoint() ) {
            case PipelineType::eGraphics:
                bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                break;
            case PipelineType::eCompute:
                bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
                break;
            default:
                break;
        }

        VkPipelineLayout layout{ pipelineLayout->GetNativeHandle( ObjectType::Vk_PipelineLayout ) };
        std::array<VkDescriptorSet, 1> sets{ resourceSet->GetNativeHandle( ObjectType::Vk_DescriptorSet ) };

        vkCmdBindDescriptorSets(
            mCmdBuffer,
            bindPoint,
            layout,
            bindingSlot,
            as<u32>(sets.size()),
            sets.data(),
            0,
            nullptr );
    }

    auto CommandList::CopyTexture( ITexture* src, const TextureSlice& srcSlice, ITexture* dest, const TextureSlice& destSlice ) -> void {
        MKT_ASSERT( src != nullptr, "Source texture cannot be null" );
        MKT_ASSERT( dest != nullptr, "Destination texture cannot be null" );

        const auto srcTexture{ as<Texture *>( src ) };
        const auto dstTexture{ as<Texture *>( dest ) };

        MKT_ASSERT( srcTexture != nullptr, "Source Vulkan texture cannot be null" );
        MKT_ASSERT( dstTexture != nullptr, "Destination Vulkan texture cannot be null" );

        const VkImage srcImage{
            srcTexture->GetNativeHandle( ObjectType::Vk_Image )
        };

        const VkImage dstImage{
            dstTexture->GetNativeHandle( ObjectType::Vk_Image )
        };

        MKT_ASSERT( srcImage != VK_NULL_HANDLE, "Source Vulkan image is null" );
        MKT_ASSERT( dstImage != VK_NULL_HANDLE, "Destination Vulkan image is null" );

        // Transition to transfer layouts
        auto srcPreviousState{ src->GetResourceState() };
        auto destPreviousState{ dest->GetResourceState() };

        SetResourceState(srcTexture, ResourceStates::eCopySource);
        SetResourceState(dstTexture, ResourceStates::eCopyDest);

        const bool sameWidth{ srcTexture->GetWidth() == dstTexture->GetWidth() };
        const bool sameHeight{ srcTexture->GetHeight() == dstTexture->GetHeight() };
        const bool sameSize{ sameWidth && sameHeight };
        const bool sameFormat{ srcTexture->GetFormat() == dstTexture->GetFormat() };

        // Prefer image copy when possible
        if ( sameSize && sameFormat ) {
            VkImageCopy2 copyRegion{ initializers::ImageCopy2() };

            copyRegion.srcSubresource.aspectMask = GetAspectMask( src->GetFormat() );;
            copyRegion.srcSubresource.baseArrayLayer = 0;
            copyRegion.srcSubresource.layerCount = 1;
            copyRegion.srcSubresource.mipLevel = srcSlice.mMipLevel;

            copyRegion.dstSubresource.aspectMask = GetAspectMask( dest->GetFormat() );;
            copyRegion.dstSubresource.baseArrayLayer = 0;
            copyRegion.dstSubresource.layerCount = 1;
            copyRegion.dstSubresource.mipLevel = destSlice.mMipLevel;

            copyRegion.srcOffset = { 0, 0, 0 };
            copyRegion.dstOffset = { 0, 0, 0 };

            copyRegion.extent.width = srcSlice.mWidth;
            copyRegion.extent.height = srcSlice.mHeight;
            copyRegion.extent.depth = 1;

            VkCopyImageInfo2 copyInfo{
                .sType = VK_STRUCTURE_TYPE_COPY_IMAGE_INFO_2,
                .pNext = nullptr,
                .srcImage = srcImage,
                .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .dstImage = dstImage,
                .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .regionCount = 1,
                .pRegions = &copyRegion
            };

            vkCmdCopyImage2( mCmdBuffer, MKT_ADDRESSOF( copyInfo ) );
        } else {
            VkImageBlit2 blitRegion{ initializers::ImageBlit2() };

            // Source
            blitRegion.srcOffsets[0] = { 0, 0, 0 };
            blitRegion.srcOffsets[1].x = as<i32>( srcSlice.mWidth );
            blitRegion.srcOffsets[1].y = as<i32>( srcSlice.mHeight );
            blitRegion.srcOffsets[1].z = 1;

            // Destination
            blitRegion.dstOffsets[0] = { 0, 0, 0 };
            blitRegion.dstOffsets[1].x = as<i32>( destSlice.mWidth );
            blitRegion.dstOffsets[1].y = as<i32>( destSlice.mHeight );
            blitRegion.dstOffsets[1].z = 1;

            blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.srcSubresource.baseArrayLayer = 0;
            blitRegion.srcSubresource.layerCount = 1;
            blitRegion.srcSubresource.mipLevel = srcSlice.mMipLevel;

            blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blitRegion.dstSubresource.baseArrayLayer = 0;
            blitRegion.dstSubresource.layerCount = 1;
            blitRegion.dstSubresource.mipLevel = destSlice.mMipLevel;

            VkBlitImageInfo2 blitInfo{
                .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                .pNext = nullptr,
                .srcImage = srcImage,
                .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                .dstImage = dstImage,
                .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .regionCount = 1,
                .pRegions = &blitRegion,
                .filter = VK_FILTER_NEAREST
            };

            vkCmdBlitImage2( mCmdBuffer, MKT_ADDRESSOF( blitInfo ) );
        }

        // Restore previous layouts if known
        SetResourceState(srcTexture, srcPreviousState);
        SetResourceState(dstTexture, destPreviousState == ResourceStates::eUnknown ? ResourceStates::eCommon : destPreviousState);
    }

    auto CommandList::Dispatch( u32 x, u32 y, u32 z ) -> void {
        vkCmdDispatch(mCmdBuffer, x, y, z);
    }

    auto CommandList::GetNativeHandle( ObjectType type ) -> Object {
        if ( type != ObjectType::Vk_CmdBuffer ) {
            return Object( nullptr );
        }

        return Object( mCmdBuffer );
    }

    auto CommandList::TransitionLayout( ITexture* texture, VkImageLayout newLayout ) -> void {
        VkImageLayout currentLayout{ GetImageLayout(texture->GetResourceState()) };

        VkImageSubresourceRange subresourceRange{};
        subresourceRange.aspectMask = GetAspectMask( texture->GetFormat() );
        subresourceRange.baseMipLevel = 0;
        subresourceRange.layerCount = 1;
        subresourceRange.levelCount = texture->GetMipLevelCount();

        if (texture->GetDimension() == TextureDimension::eTextureCube) {
            subresourceRange.layerCount = kMaxCubeFaces;
        }

        // Create an image barrier object
        VkImageMemoryBarrier imageMemoryBarrier{};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        imageMemoryBarrier.oldLayout = currentLayout;
        imageMemoryBarrier.newLayout = newLayout;
        imageMemoryBarrier.image = texture->GetNativeHandle( ObjectType::Vk_Image );
        imageMemoryBarrier.subresourceRange = subresourceRange;

        // Source layouts (old)
        // Source access mask controls actions that have to be finished on the old layout
        // before it will be transitioned to the new layout
        switch ( currentLayout ) {
            case VK_IMAGE_LAYOUT_UNDEFINED:
                // Image layout is undefined (or does not matter)
                // Only valid as initial layout
                // No flags required, listed only for completeness
                imageMemoryBarrier.srcAccessMask = 0;
                break;

            case VK_IMAGE_LAYOUT_PREINITIALIZED:
                // Image is preinitialized
                // Only valid as initial layout for linear images, preserves memory contents
                // Make sure host writes have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image is a color attachment
                // Make sure any writes to the color buffer have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image is a depth/stencil attachment
                // Make sure any writes to the depth/stencil buffer have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image is a transfer source
                // Make sure any reads from the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image is a transfer destination
                // Make sure any writes to the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image is read by a shader
                // Make sure any shader reads from the image have been finished
                imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default:
                // Other source layouts aren't handled (yet)
                break;
        }

        // Target layouts (new)
        // Destination access mask controls the dependency for the new image layout
        switch ( newLayout ) {
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                // Image will be used as a transfer destination
                // Make sure any writes to the image have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                // Image will be used as a transfer source
                // Make sure any reads from the image have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                break;

            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                // Image will be used as a color attachment
                // Make sure any writes to the color buffer have been finished
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                // Image layout will be used as a depth/stencil attachment
                // Make sure any writes to depth/stencil buffer have been finished
                imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
                break;

            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                // Image will be read in a shader (sampler, input attachment)
                // Make sure any writes to the image have been finished
                if ( imageMemoryBarrier.srcAccessMask == 0 ) {
                    imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
                }
                imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                break;
            default:
                // Other source layouts aren't handled (yet)
                break;
        }

        // Put barrier inside setup command buffer
        vkCmdPipelineBarrier(
                mCmdBuffer,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                0,
                0, nullptr,
                0, nullptr,
                1, &imageMemoryBarrier );

        texture->SetResourceState( vulkan::GetResourceState( newLayout ) );
    }

    auto CommandList::MarkSubmitted( u64 submissionID ) -> void {
        mSubmissionItems.emplace_back(SubmissionItem{
            submissionID,
            mCmdBuffer
        });

        mIsSubmitted = true;
    }

    CommandList::~CommandList() {
        if (mIsAllocated) {
            CommandList::Release();
        }
    }

    auto CommandList::Initialize() -> void {
        mUploadManager = as<Device*>(mDevice)->GetUploadManager();
        MKT_VK_CHECK( vkAllocateCommandBuffers(
            as<Device*>(mDevice)->GetDevice(),
            MKT_ADDRESSOF( mAllocInfo ),
            MKT_ADDRESSOF( mCmdBuffer ) ) );

        mIsAllocated = true;
    }

    auto CommandList::Release() -> void {
        ClearState();

        Queue* queue{ checked_cast<Device*>( mDevice )->GetQueue( mQueueType ) };

        for (const auto& item : mSubmissionItems ) {
            queue->PushDelete( item.mCommandBuffer, mAllocInfo.commandPool, item.mId );
        }

        mIsAllocated = false;
    }

    auto CommandList::InitializeArena() -> void {
        auto bufferDes{ BufferCreateDescription{}
            .SetByteSize( kArenaInitialSize ) // Later we can specify a size for better optimization
            .SetCpuAccessType( CpuAccessType::eWrite )
            .SetHeapType( HeapType::eUpload )
            .SetResourceType( ResourceType::eInvalid ) // Is not a shader resource
            .SetBufferUsage( BufferUsageFlagsBits::kNone )
        };
        mMemoryArena =
            eastl::make_unique<memory::MemoryArena<IBuffer, LinearAllocator>>( mDevice->CreateBuffer( bufferDes ), kArenaInitialSize );
    }

    auto CommandList::ClearState() -> void {
        for (auto& subAllocations : mUploadAllocations) {
            // Set it to false we to tell the allocator
            // this allocation can already be destroyed
            subAllocations->mInUse.clear();
        }

        mIsSubmitted = false;
        mRenderPassIsActive = false;
    }

    auto CommandList::TryRecycle( IQueue* queue ) -> void {
        auto* vkQueue{ checked_cast<Queue*>( queue ) };
        u64 completed{ vkQueue->GetCompletedValue() };

        for (auto it{ mSubmissionItems.begin() }; it != mSubmissionItems.end(); ) {
            if (completed >= it->mId) {
                // GPU finished this buffer -> safe to reuse
                vkResetCommandBuffer(it->mCommandBuffer, MKT_VK_FLAGS_NONE );
                mCmdBuffer = it->mCommandBuffer;
                it = mSubmissionItems.erase(it);
                return;
            }

            ++it;
        }

        // allocate a fresh buffer for next recording
        MKT_VK_CHECK(vkAllocateCommandBuffers(
            as<Device*>(mDevice)->GetDevice(),
            MKT_ADDRESSOF(mAllocInfo),
            MKT_ADDRESSOF(mCmdBuffer)
        ));
    }

    CommandPool::CommandPool( QueueType type, u32 queueFamilyIndex )
        : mQueueFamilyIndex{ queueFamilyIndex }, mQueueType{ type }
    {}

    auto CommandPool::GetNativeHandle( ObjectType type ) -> Object {
        if ( type != ObjectType::Vk_CmdPool ) {
            return Object( nullptr );
        }

        return Object( mPool );
    }

    auto CommandPool::AllocateCmdList() -> CommandListHandle {
        MKT_BEGIN_PROFILER_NAMED();

        VkCommandBufferAllocateInfo allocInfo{ initializers::CommandBufferAllocateInfo() };
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = mPool;
        allocInfo.commandBufferCount = 1;

        CommandListHandle handle{ Ref<CommandList>::Spawn( allocInfo, mQueueType ) };
        handle->Initialize( mDevice );

        return handle;
    }

    CommandPool::~CommandPool() {
        if ( mIsAllocated ) {
            Release();
        }
    }

    auto CommandPool::Initialize() -> void {
        // Command pool to allocate command buffers for compute queue operations
        VkCommandPoolCreateInfo createInfo{ initializers::CommandPoolCreateInfo() };
        createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        createInfo.queueFamilyIndex = mQueueFamilyIndex;

        MKT_VK_CHECK( vkCreateCommandPool( as<Device *>( mDevice )->GetDevice(), MKT_ADDRESSOF( createInfo ), nullptr, MKT_ADDRESSOF( mPool ) ) );

        mIsAllocated = true;
    }

    auto CommandPool::Release() -> void {

        vkDestroyCommandPool( as<Device *>( mDevice )->GetDevice(), mPool, nullptr );
        mIsAllocated = false;
    }

    Queue::Queue( GpuDevice *device, QueueType type, u32 queueFamilyIndex, u32 queueIndex)
        : IQueue{ type }, mDevice{ device }, mFamilyIndex{ queueFamilyIndex }, mQueueIndex{ queueIndex }
    {}

    auto Queue::Initialize() -> void {
        MKT_ASSERT(  mDevice != nullptr, "GpuDevice cannot be null" );
        auto* device{ checked_cast<Device*>(mDevice) };
        vkGetDeviceQueue( device->GetDevice(), mFamilyIndex, mQueueIndex, MKT_ADDRESSOF( mQueue ) );

        MKT_ASSERT( mQueue != VK_NULL_HANDLE, "Queue is empty" );

        mTimelineSemaphore = device->CreateTimelineSemaphore( 0 );

        TimelineSemaphore* timelineSemaphore{ checked_cast<TimelineSemaphore*>( mTimelineSemaphore.GetRaw() ) };
        timelineSemaphore->GetAndIncrement( 1 ); // vkQueueSubmit2(): pSubmits[0].pSignalSemaphoreInfos[0].semaphore signal value (0) in VkQueue 0x1dec7ccca40 must be greater than current timeline semaphore VkSemaphore 0x50000000005 value (0).
    }

    auto Queue::Shutdown() -> void {
        WaitIdle();

        // Check the ones that are done
        auto complete{ GetCompletedValue() };
        for ( auto it{mDeleteCmds.begin()}; it != mDeleteCmds.end();) {
            if (complete >= it->mSubmissionID) {
                vkFreeCommandBuffers(
                    as<Device*>(mDevice)->GetDevice(),
                    it->mPool,
                    1,
                    MKT_ADDRESSOF(it->mBuffer)
                );

                it = mDeleteCmds.erase(it); // move to next safely
            } else {
                ++it; // only increment if NOT erased
            }
        }

        mPools.clear();
        mTimelineSemaphore.Reset();
    }

    auto Queue::Flush() -> void {
        std::lock_guard lock{ mPendingSubmitMutex };

        if (mPendingSubmits.empty()) {
            return;
        }

        (void)SubmitCommands();
    }

    auto Queue::ExecuteCommandList( CommandListHandle cmd ) -> u64 {
        MKT_ASSERT(!cmd.IsEmpty(), "Command List cannot be empty");

        // Register and Flush next submissionID
        u64 id{ SubmitCommandList( cmd ) };
        Flush();

        return id;
    }

    auto Queue::SubmitCommandList( CommandListHandle cmd ) -> u64 {
        MKT_ASSERT(!cmd.IsEmpty(), "Command List cannot be empty");

        std::lock_guard lock{ mPendingSubmitMutex };

        mPendingSubmits.emplace_back(cmd);
        TimelineSemaphore* timeline{ checked_cast<TimelineSemaphore*>(mTimelineSemaphore.GetRaw()) };

        return timeline->GetCurrentID(); // ID of the batch it will belong to
    }

    auto Queue::AllocateCmdList() -> CommandListHandle {
        CommandPoolHandle pool{ AcquireThreadCmdPool() };
        return pool->AllocateCmdList();
    }

    auto Queue::GetCompletedValue() const -> u64 {
        auto* device{ checked_cast<Device*>(mDevice) };
        auto* timeline{ checked_cast<const TimelineSemaphore*>(mTimelineSemaphore.GetRaw()) };

        VkSemaphore semaphore{ timeline->GetNativeHandle(ObjectType::Vk_Semaphore) };

        u64 value{};
        MKT_VK_CHECK(vkGetSemaphoreCounterValue(device->GetDevice(), semaphore, &value));

        return value;
    }

    auto Queue::PushDelete( VkCommandBuffer cmd, VkCommandPool pool, u64 submitID ) -> void {
        std::lock_guard lock{ mDeleteCmdsMutex };
        mDeleteCmds.emplace_back( DeleteItem {
            .mSubmissionID = submitID,
            .mPool = pool,
            .mBuffer = cmd
        } );
    }

    auto Queue::WaitForSubmission( u64 submissionID ) -> void {
        auto* device{ checked_cast<Device*>(mDevice) };
        auto* timeline{
            checked_cast<TimelineSemaphore*>(mTimelineSemaphore.GetRaw())
        };

        VkSemaphore semaphore{
            timeline->GetNativeHandle(ObjectType::Vk_Semaphore)
        };

        u64 completed{};
        vkGetSemaphoreCounterValue(device->GetDevice(), timeline->GetNativeHandle( ObjectType::Vk_Semaphore ), &completed);
        MKT_ASSERT( submissionID <= timeline->GetCurrentID(), "Waiting for submission that was never submitted!");

        VkSemaphoreWaitInfo waitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &semaphore,
            .pValues = &submissionID
        };

        vkWaitSemaphores(device->GetDevice(), &waitInfo, UINT64_MAX);
    }

    auto Queue::AddQueueWaitFence( Fence* semaphore ) -> void {
        // TODO: Unused for now as i can use timeline for this too
    }

    auto Queue::AddQueueSignalSemaphore( BinarySemaphore* semaphore, VkPipelineStageFlags2 stageFlags ) -> void {
        mSignalInfos.emplace_back( VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = semaphore->GetNativeHandle( ObjectType::Vk_Semaphore ),
            .value = 0,
            .stageMask = stageFlags,
            .deviceIndex = 0 } );
    }

    auto Queue::AddQueueWaitSemaphore( BinarySemaphore* semaphore, VkPipelineStageFlags2 stageFlags ) -> void {
        mWaitInfos.emplace_back( VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = semaphore->GetNativeHandle( ObjectType::Vk_Semaphore ),
            .value = 0,
            .stageMask = stageFlags,
            .deviceIndex = 0 } );
    }

    Queue::operator u32() const {
        return mFamilyIndex;
    }

    Queue::operator VkQueue() const {
        return mQueue;
    }

    auto Queue::AcquireThreadCmdPool() -> CommandPoolHandle {
        auto id{ std::this_thread::get_id() };

        std::scoped_lock lock{ mPoolsMutex };

        const auto it{ mPools.find( id ) };
        if ( it != mPools.end() ) {
            return it->second;
        }

        auto [result, success]{
            mPools.emplace( id, CommandPoolHandle::Spawn( mType, mFamilyIndex ) )
        };

        result->second->Initialize( mDevice );

        return result->second;
    }

    auto Queue::SubmitCommands() -> u64 {
        if ( mPendingSubmits.empty() ) {
            return 0;
        }

        auto* device{ checked_cast<Device*>( mDevice ) };
        auto* timeline{ checked_cast<TimelineSemaphore*>( mTimelineSemaphore.GetRaw() ) };

        const u64 submissionID{ timeline->GetCurrentID() };

        // --- Command buffers ---
        eastl::fixed_vector<VkCommandBufferSubmitInfo, kMaxSubmits> cmdInfos{};

        for ( auto& cmd: mPendingSubmits ) {
            cmdInfos.emplace_back( VkCommandBufferSubmitInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext = nullptr,
                .commandBuffer = cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ),
                .deviceMask = 0 } );

            checked_cast<CommandList*>( cmd.GetRaw() )->MarkSubmitted( submissionID );
        }

        // --- Wait semaphores ---
        eastl::fixed_vector<VkSemaphoreSubmitInfo, 15> waitInfos{};
        for ( const auto& w: mWaitInfos ) {
            waitInfos.emplace_back( w );
        }

        // --- Signal semaphores ---
        eastl::fixed_vector<VkSemaphoreSubmitInfo, 15> signalInfos{};
        for ( const auto& s: mSignalInfos ) {
            signalInfos.emplace_back( s );
        }

        // --- Timeline signal ---
        signalInfos.emplace_back( VkSemaphoreSubmitInfo{
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .pNext = nullptr,
                .semaphore = timeline->GetNativeHandle( ObjectType::Vk_Semaphore ),
                .value = submissionID,
                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                .deviceIndex = 0 } );

        VkSubmitInfo2 submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .waitSemaphoreInfoCount = ( u32 )waitInfos.size(),
            .pWaitSemaphoreInfos = waitInfos.data(),
            .commandBufferInfoCount = ( u32 )cmdInfos.size(),
            .pCommandBufferInfos = cmdInfos.data(),
            .signalSemaphoreInfoCount = ( u32 )signalInfos.size(),
            .pSignalSemaphoreInfos = signalInfos.data()
        };

        {
            std::lock_guard lock{ mSubmissionMutex };
            MKT_VK_CHECK( vkQueueSubmit2( mQueue, 1, &submitInfo, VK_NULL_HANDLE ) );
        }

        // Clear per-submit sync
        mWaitInfos.clear();
        mSignalInfos.clear();

        mPendingSubmits.clear();

        // Check the ones that are done
        auto complete{ GetCompletedValue() };
        for ( auto it{mDeleteCmds.begin()}; it != mDeleteCmds.end();) {
            if (complete >= it->mSubmissionID) {
                vkFreeCommandBuffers(
                    as<Device*>(mDevice)->GetDevice(),
                    it->mPool,
                    1,
                    MKT_ADDRESSOF(it->mBuffer)
                );

                it = mDeleteCmds.erase(it); // move to next safely
            } else {
                ++it; // only increment if NOT erased
            }
        }

        // Advance timeline
        timeline->GetAndIncrement( 1 );

        return submissionID;
    }

    auto Queue::WaitIdle() const -> void {
        vkQueueWaitIdle( mQueue );
    }

    auto Queue::GetQueue() const -> VkQueue {
        return mQueue;
    }

    auto Queue::GetQueueIndex() const -> u32 {
        return mQueueIndex;
    }

    auto Queue::GetFamilyIndex() const -> u32 {
        return mFamilyIndex;
    }

    using namespace mikoto::core;

    bool IsMemoryError( VkResult errorResult ) {
        switch ( errorResult ) {
            case VK_ERROR_FRAGMENTED_POOL:
            case VK_ERROR_OUT_OF_POOL_MEMORY:
                return true;
            default:;
        }
        return false;
    }

    struct DescriptorAllocator {
        VkDescriptorPool mPool{};
    };

    struct PoolStorage {

        eastl::vector<DescriptorAllocator> mUsableAllocators{};
        eastl::vector<DescriptorAllocator> mFullAllocators{};
    };

    struct PoolSize {
        VkDescriptorType mType{};
        f32 mMultiplier{};
    };

    struct PoolSizes {
        eastl::vector<PoolSize> mSizes{
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1.f },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4.f },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1.f },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1.f },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1.f },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1.f },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1.f }
        };
    };


    class DescriptorAllocatorPoolImpl : public IDescriptorAllocatorPool {
    public:
        ~DescriptorAllocatorPoolImpl() override;

        auto Flip() -> void override;
        auto SetPoolSizeMultiplier( VkDescriptorType type, float multiplier ) -> void override;

        MKT_NODISCARD auto GetAllocator() -> DescriptorAllocatorHandle override;

        void ReturnAllocator( DescriptorAllocatorHandle& handle, bool bIsFull );
        VkDescriptorPool CreatePool( int count, VkDescriptorPoolCreateFlags flags );

        VkDevice mDevice{};
        PoolSizes mPoolSizes;
        i32 mFrameIndex{};
        i32 mMaxFrames{};

        std::mutex mPoolMutex;

        // zero is for static pool, next is for frame indexing
        eastl::vector<std::unique_ptr<PoolStorage>> mDescriptorPools;

        // fully cleared allocators
        std::vector<DescriptorAllocator> mClearAllocators;
    };


    auto IDescriptorAllocatorPool::Create( const VkDevice& device, i32 nFrames ) -> eastl::unique_ptr<IDescriptorAllocatorPool> {
        auto impl{ eastl::make_unique<DescriptorAllocatorPoolImpl>() };
        impl->mDevice = device;
        impl->mFrameIndex = 0;
        impl->mMaxFrames = nFrames;

        for ( i32 i{}; i < nFrames; i++ ) {
            impl->mDescriptorPools.push_back( std::make_unique<PoolStorage>() );
        }

        return impl;
    }

    DescriptorAllocatorHandle::~DescriptorAllocatorHandle() {
        auto* implPool = static_cast<DescriptorAllocatorPoolImpl*>( mOwnerPool );
        if ( implPool ) {

            implPool->ReturnAllocator( *this, false );
        }
    }

    DescriptorAllocatorHandle::DescriptorAllocatorHandle( DescriptorAllocatorHandle&& other ) noexcept {
        Return();

        mDescriptorPool = other.mDescriptorPool;
        mPoolIndex = other.mPoolIndex;
        mOwnerPool = other.mOwnerPool;

        other.mOwnerPool = nullptr;
        other.mPoolIndex = -1;
        other.mDescriptorPool = VkDescriptorPool{};
    }

    auto DescriptorAllocatorHandle::operator=( DescriptorAllocatorHandle&& other ) noexcept -> DescriptorAllocatorHandle& {
        Return();

        mDescriptorPool = other.mDescriptorPool;
        mPoolIndex = other.mPoolIndex;
        mOwnerPool = other.mOwnerPool;

        other.mOwnerPool = nullptr;
        other.mPoolIndex = -1;
        other.mDescriptorPool = VkDescriptorPool{};

        return *this;
    }

    void DescriptorAllocatorHandle::Return() {
        auto* implPool{ as<DescriptorAllocatorPoolImpl*>( mOwnerPool ) };

        if ( implPool ) {
            implPool->ReturnAllocator( *this, false );
        }

        mDescriptorPool = VkDescriptorPool{};
        mPoolIndex = -1;
        mOwnerPool = nullptr;
    }

    auto DescriptorAllocatorHandle::Allocate( const VkDescriptorSetLayout& layout, VkDescriptorSet& builtSet ) -> bool {
        auto* implPool{ as<DescriptorAllocatorPoolImpl*>( mOwnerPool ) };

        VkDescriptorSetAllocateInfo allocInfo;
        allocInfo.pNext = nullptr;
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkResult result{ vkAllocateDescriptorSets( implPool->mDevice, &allocInfo, &builtSet ) };
        if ( result != VK_SUCCESS ) {
            //we reallocate pools on memory error
            if ( IsMemoryError( result ) ) {
                //out of space need reallocate

                implPool->ReturnAllocator( *this, true );

                DescriptorAllocatorHandle newHandle = implPool->GetAllocator();

                mDescriptorPool = newHandle.mDescriptorPool;
                mPoolIndex = newHandle.mPoolIndex;

                newHandle.mDescriptorPool = VkDescriptorPool{};
                newHandle.mPoolIndex = -1;
                newHandle.mOwnerPool = nullptr;
                //could be good idea to avoid infinite loop here
                return Allocate( layout, builtSet );
            } else {
                //stuff is truly broken
                return false;
            }
        }

        return true;
    }

    auto DescriptorAllocatorPoolImpl::CreatePool( int count, VkDescriptorPoolCreateFlags flags ) -> VkDescriptorPool {
        eastl::vector<VkDescriptorPoolSize> sizes;
        sizes.reserve( mPoolSizes.mSizes.size() );
        for ( auto sz: mPoolSizes.mSizes ) {
            sizes.push_back( { sz.mType, uint32_t( sz.mMultiplier * count ) } );
        }
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = flags;
        pool_info.maxSets = count;
        pool_info.poolSizeCount = ( uint32_t )sizes.size();
        pool_info.pPoolSizes = sizes.data();

        VkDescriptorPool descriptorPool;
        vkCreateDescriptorPool( mDevice, &pool_info, nullptr, &descriptorPool );

        return descriptorPool;
    }

    DescriptorAllocatorPoolImpl::~DescriptorAllocatorPoolImpl() {
        for ( DescriptorAllocator allocator: mClearAllocators ) {
            vkDestroyDescriptorPool( mDevice, allocator.mPool, nullptr );
        }
        for ( auto&& storage: mDescriptorPools ) {
            for ( DescriptorAllocator allocator: storage->mFullAllocators ) {
                vkDestroyDescriptorPool( mDevice, allocator.mPool, nullptr );
            }
            for ( DescriptorAllocator allocator: storage->mUsableAllocators ) {
                vkDestroyDescriptorPool( mDevice, allocator.mPool, nullptr );
            }
        }
    }

    auto DescriptorAllocatorPoolImpl::Flip() -> void {
        mFrameIndex = ( mFrameIndex + 1 ) % mMaxFrames;

        for ( auto al: mDescriptorPools[mFrameIndex]->mFullAllocators ) {

            vkResetDescriptorPool( mDevice, al.mPool, VkDescriptorPoolResetFlags{ 0 } );

            mClearAllocators.push_back( al );
        }

        for ( auto al: mDescriptorPools[mFrameIndex]->mUsableAllocators ) {

            vkResetDescriptorPool( mDevice, al.mPool, VkDescriptorPoolResetFlags{ 0 } );

            mClearAllocators.push_back( al );
        }

        mDescriptorPools[mFrameIndex]->mFullAllocators.clear();
        mDescriptorPools[mFrameIndex]->mUsableAllocators.clear();
    }

    auto DescriptorAllocatorPoolImpl::SetPoolSizeMultiplier( VkDescriptorType type, float multiplier ) -> void {
        for ( auto& s: mPoolSizes.mSizes ) {
            if ( s.mType == type ) {
                s.mMultiplier = multiplier;
                return;
            }
        }

        //not found, so add it
        PoolSize newSize{};
        newSize.mType = type;
        newSize.mMultiplier = multiplier;
        mPoolSizes.mSizes.push_back( newSize );
    }

    auto DescriptorAllocatorPoolImpl::ReturnAllocator( DescriptorAllocatorHandle& handle, bool bIsFull ) -> void {
        std::lock_guard<std::mutex> lk( mPoolMutex );


        if ( bIsFull ) {
            mDescriptorPools[handle.mPoolIndex]->mFullAllocators.push_back( DescriptorAllocator{ handle.mDescriptorPool } );
        } else {
            mDescriptorPools[handle.mPoolIndex]->mUsableAllocators.push_back( DescriptorAllocator{ handle.mDescriptorPool } );
        }
    }

    auto DescriptorAllocatorPoolImpl::GetAllocator() -> DescriptorAllocatorHandle {
        std::lock_guard<std::mutex> lk( mPoolMutex );

        bool foundAllocator = false;

        i32 poolIndex = mFrameIndex;

        DescriptorAllocator allocator{};
        //try reuse an allocated pool
        if ( mClearAllocators.size() != 0 ) {
            allocator = mClearAllocators.back();
            mClearAllocators.pop_back();
            foundAllocator = true;
        } else {
            if ( mDescriptorPools[poolIndex]->mUsableAllocators.size() > 0 ) {
                allocator = mDescriptorPools[poolIndex]->mUsableAllocators.back();
                mDescriptorPools[poolIndex]->mUsableAllocators.pop_back();
                foundAllocator = 1;
            }
        }
        //need a new pool
        if ( !foundAllocator ) {
            //static pool has to be free-able
            VkDescriptorPoolCreateFlags flags = 0;
            if ( poolIndex == 0 ) {
                flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            }

            VkDescriptorPool newPool = CreatePool( 2000, flags );

            allocator.mPool = newPool;

            foundAllocator = true;
        }

        DescriptorAllocatorHandle newHandle{};
        newHandle.mOwnerPool = this;
        newHandle.mPoolIndex = as<i8>( poolIndex );
        newHandle.mDescriptorPool = allocator.mPool;

        return newHandle;
    }

    TimelineSemaphore::TimelineSemaphore( u64 initialValue )
        : mTimeline{ initialValue } {}

    auto TimelineSemaphore::SetDebugName( eastl::string_view name ) -> void {
        mDebugName = name;

        auto* device{ checked_cast<Device*>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_SEMAPHORE, rc_cast<u64>( mSemaphore ), mDebugName );
    }

    auto TimelineSemaphore::GetNativeHandle( ObjectType object ) -> Object {
        switch ( object ) {
            case ObjectType::Vk_Semaphore: return Object( mSemaphore );
            default:;
        }

        return Object( nullptr );
    }

    auto TimelineSemaphore::GetNativeHandle( ObjectType object ) const -> Object {
        switch ( object ) {
            case ObjectType::Vk_Semaphore: return Object( mSemaphore );
            default:;
        }

        return Object( nullptr );
    }

    auto TimelineSemaphore::GetCurrentID() -> u64 {
        return mTimeline.load();
    }

    TimelineSemaphore::~TimelineSemaphore() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto TimelineSemaphore::Initialize() -> void {
        auto* device{ checked_cast<Device*>( mDevice ) };

        VkSemaphoreTypeCreateInfo typeCreateInfo{ initializers::SemaphoreTypeCreateInfo() };
        typeCreateInfo.initialValue = mTimeline.load();

        VkSemaphoreCreateInfo createInfo{ initializers::SemaphoreCreateInfo() };
        createInfo.pNext = MKT_ADDRESSOF( typeCreateInfo );

        MKT_VK_CHECK( vkCreateSemaphore( device->GetDevice(), MKT_ADDRESSOF( createInfo ), nullptr, MKT_ADDRESSOF( mSemaphore ) ) );

        mIsAllocated = true;
    }

    auto TimelineSemaphore::Release() -> void {
        auto* device{ checked_cast<Device*>( mDevice ) };
        vkDestroySemaphore(  device->GetDevice(), mSemaphore, nullptr );

        mIsAllocated = false;
    }

    BinarySemaphore::BinarySemaphore() {}

    auto BinarySemaphore::SetDebugName( eastl::string_view name ) -> void {
        mDebugName = name;

        auto* device{ checked_cast<Device*>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_SEMAPHORE, rc_cast<u64>( mSemaphore ), mDebugName );
    }

    auto BinarySemaphore::GetNativeHandle( ObjectType object ) -> Object {
        switch ( object ) {
            case ObjectType::Vk_Semaphore: return Object( mSemaphore );
            default:;
        }

        return Object( nullptr );
    }

    auto BinarySemaphore::GetNativeHandle( ObjectType object ) const -> Object {
        switch ( object ) {
            case ObjectType::Vk_Semaphore: return Object( mSemaphore );
            default:;
        }

        return Object( nullptr );
    }

    BinarySemaphore::~BinarySemaphore() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto BinarySemaphore::Initialize() -> void {
        auto* device{ checked_cast<Device*>( mDevice ) };

        VkSemaphoreCreateInfo info{ initializers::SemaphoreCreateInfo() };
        MKT_VK_CHECK( vkCreateSemaphore( device->GetDevice(), MKT_ADDRESSOF( info ), nullptr, MKT_ADDRESSOF( mSemaphore ) ) );
        mIsAllocated = true;
    }
    auto BinarySemaphore::Release() -> void {
        auto* device{ checked_cast<Device*>( mDevice ) };
        vkDestroySemaphore( device->GetDevice(), mSemaphore, nullptr );
        mIsAllocated = false;
    }

    auto TimelineSemaphore::GetAndIncrement( u64 value ) -> u64 {
        mTimeline += value;
        return mTimeline;
    }

    GpuUploadManager::GpuUploadManager( GpuDevice *device )
        : mDevice{ device }
    {}

    auto GpuUploadManager::SubAllocate( size_t byteSize ) -> GpuUploadAllocation* {
        std::lock_guard lock{ mMutex };
        auto* device{ checked_cast<Device*>( mDevice ) };

        StagingAllocation* stagingAlloc{};
        eastl::optional<Allocation> subAllocProperties{};

        // Find the first allocation I can suballocate from and lock it
        if (mBuffers.empty()) {
            stagingAlloc = CreateBuffer();
            subAllocProperties = stagingAlloc->mMemoryArena->Allocate( byteSize, 1 );
            if (!subAllocProperties.has_value()) {
                MKT_ASSERT( false, "Failed to sub-allocate" );
            }
        } else {
            // Try to find any buffer that has ranges available
            for (auto& bufferInfo : mBuffers | std::views::values) {
                stagingAlloc = bufferInfo.get();
                subAllocProperties = stagingAlloc->mMemoryArena->Allocate( byteSize, 1 );

                if (subAllocProperties.has_value()) {
                    break;
                }
            }

            // If none does allocate a new staging for uploads
            if (!subAllocProperties.has_value()) {
                stagingAlloc = CreateBuffer();
                subAllocProperties = stagingAlloc->mMemoryArena->Allocate( byteSize, 1 );
            }
        }

        MKT_ASSERT( subAllocProperties.has_value() && subAllocProperties->mSize >= byteSize, "Failed to sub-allocate" );

        // Fill params
        GpuUploadAllocation* result{ CreateSubAllocation( stagingAlloc->mBuffer.GetRaw() ) };

        result->mMappedMemory = as<byte_t *>( checked_cast<Buffer *>( stagingAlloc->mBuffer.GetRaw() )->GetMappedAddress() ) + subAllocProperties->mOffset;
        result->mSize = subAllocProperties->mSize;
        result->mOffset = subAllocProperties->mOffset;
        result->mBuffer = stagingAlloc->mBuffer.GetRaw();
        result->mAllocation = *subAllocProperties;

        return result;
    }

    auto GpuUploadManager::ReclaimMemory() -> void {
        std::lock_guard lock{ mMutex };
        // Check fence, I'm not sure maybe timeline semaphores can be used instead
        for ( auto& [buffer, subAllocations]: mSubAllocations ) {
            // Is it safe to return them back here?
            for (auto it{subAllocations.begin()}; it != subAllocations.end(); ) {
                if (it->get() && !(*it)->mInUse.test()) {
                    mBuffers[buffer]->mMemoryArena->Free((*it)->mAllocation);
                    it = subAllocations.erase( it );
                } else {
                    ++it;
                }
            }
        }
    }

    GpuUploadManager::~GpuUploadManager() {
        mBuffers.clear();
        mSubAllocations.clear();
    }

    auto GpuUploadManager::CreateBuffer() -> StagingAllocation * {
        size_t initialSize{ MKT_MEGABYTES( 256 ) };

        auto bufferDes{ BufferCreateDescription{}
            .SetByteSize( initialSize )
            .SetCpuAccessType( CpuAccessType::eWrite )
            .SetHeapType( HeapType::eUpload )
            .SetResourceType( ResourceType::eInvalid ) // Is not a shader resource
            .SetBufferUsage( BufferUsageFlagsBits::kNone )
        };
        BufferHandle result{ mDevice->CreateBuffer( bufferDes ) };

        auto& newAllocation{ mBuffers[result.GetRaw()] };
        newAllocation = eastl::make_unique<StagingAllocation>();
        newAllocation->mBuffer = result;
        newAllocation->mMemoryArena = eastl::make_unique<memory::MemoryArena<IBuffer, FreeListFirstFitAllocator>>( result, initialSize );

        return newAllocation.get();
    }

    auto GpuUploadManager::CreateSubAllocation( IBuffer* buffer ) -> GpuUploadAllocation * {
        auto& result{ mSubAllocations[buffer].emplace_back( eastl::make_unique<GpuUploadAllocation>() ) };
        return result.get();
    }

    auto Fence::Create( const VkFenceCreateInfo &info, VkDevice device ) -> void {
        MKT_VK_CHECK( vkCreateFence( device, MKT_ADDRESSOF( info ), nullptr, MKT_ADDRESSOF( mFence ) ) );
    }

    auto Fence::Destroy( VkDevice device ) -> void {
        vkDestroyFence(  device, mFence, nullptr );
    }

    Fence::operator VkFence() const noexcept {
        return mFence;
    }

    auto DescriptorWriter::WriteSampler( u32 binding, VkSampler sampler ) -> DescriptorWriter& {
        VkDescriptorImageInfo& info{ mImageInfos.emplace_back(VkDescriptorImageInfo{
            .sampler = sampler,
            .imageView = VK_NULL_HANDLE,
            .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED })
        };

        VkWriteDescriptorSet write{ initializers::WriteDescriptorSet() };

        write.dstBinding = binding;
        write.dstSet = VK_NULL_HANDLE; //left empty for now until we to write it  in the updateSet()
        write.descriptorCount = 1;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        write.pImageInfo = MKT_ADDRESSOF( info );

        mWrites.push_back(write);

        return *this;
    }

    auto DescriptorWriter::WriteBuffer( u32 binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type ) -> DescriptorWriter& {
        // Descriptor types allowed for a buffer
        // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
        // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC

        // When we want to bind one or the other type into a shader, we set the correct type here.
        // Remember that it needs to match the usage when allocating the VkBuffer

        VkDescriptorBufferInfo& info{ mBufferInfos.emplace_back(VkDescriptorBufferInfo{
            .buffer = buffer,
            .offset = offset,
            .range = size , })
        };

        VkWriteDescriptorSet write{ initializers::WriteDescriptorSet() };

        write.dstBinding = binding;
        write.dstSet = VK_NULL_HANDLE; //left empty for now until we to write it in the updateSet()
        write.descriptorCount = 1;
        write.descriptorType = type;
        write.pBufferInfo = std::addressof( info );

        mWrites.push_back(write);

        return *this;
    }

    auto DescriptorWriter::WriteImage( u32 binding, VkImageView image, VkDescriptorType type, VkImageLayout layout ) -> DescriptorWriter& {
        // The layout is going to be almost always either VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        // the best layout to use for accessing textures in the shaders, or VK_IMAGE_LAYOUT_GENERAL
        // when we are using them from compute shaders and writing them.

        VkDescriptorImageInfo& info{ mImageInfos.emplace_back(VkDescriptorImageInfo{
            .imageView = image,
            .imageLayout = layout })
        };

        VkWriteDescriptorSet write{ initializers::WriteDescriptorSet() };

        write.dstBinding = binding;
        write.dstSet = VK_NULL_HANDLE; //left empty for now until we to write it  in the updateSet()
        write.descriptorCount = 1;
        write.dstArrayElement = 0;
        write.descriptorType = type;
        write.pImageInfo = MKT_ADDRESSOF( info );

        mWrites.push_back(write);

        return *this;
    }

    auto DescriptorWriter::SetVisibility( VkShaderStageFlags visibility ) -> DescriptorWriter& {
        mShaderStages |= visibility;
        return *this;
    }

    auto DescriptorWriter::Clear() -> void {
        mImageInfos.clear();
        mWrites.clear();
        mBufferInfos.clear();
    }

    auto DescriptorWriter::UpdateSet( VkDevice device, VkDescriptorSet set ) -> void {
        for (VkWriteDescriptorSet& write : mWrites) {
            write.dstSet = set;
        }

        vkUpdateDescriptorSets(device, as<u32>(mWrites.size()), mWrites.data(), 0, nullptr);
    }

    BindingLayout::BindingLayout( const BindingLayoutDescription &desc )
        : mSetIndex{ desc.mRegisterSpace }, mIsBindless{ false }, mBindingLayoutDesc{ desc } {
    }

    BindingLayout::BindingLayout( const BindlessLayoutDescription &desc )
        : mSetIndex{ desc.mRegisterSpace }, mIsBindless{ true }, mBindlessLayoutDesc{ desc } {
    }

    auto BindingLayout::GetRegisterSpace() const -> u32 {
        return mSetIndex;
    }

    auto BindingLayout::IsBindless() const -> bool {
        return mIsBindless;
    }

    auto BindingLayout::SetDebugName( eastl::string_view name ) -> void {

    }

    auto BindingLayout::GetNativeHandle( ObjectType type ) -> Object {
        switch (type) {
            case ObjectType::Vk_DescriptorSetLayout:
                return Object{ mDescriptorSetLayout };
            default:;
        }

        return Object( nullptr );
    }

    auto BindingLayout::GetNativeHandle( ObjectType type ) const -> Object {
        switch (type) {
            case ObjectType::Vk_DescriptorSetLayout:
                return Object{ mDescriptorSetLayout };
            default:;
        }

        return Object( nullptr );
    }

    BindingLayout::~BindingLayout() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto BindingLayout::Initialize() -> void {
        if (mIsBindless) {
            eastl::vector<VkDescriptorSetLayoutBinding> bindings{};
            eastl::vector<VkDescriptorBindingFlags> flags{};

            bindings.reserve(mBindlessLayoutDesc.mSlots.size());
            flags.reserve(mBindlessLayoutDesc.mSlots.size());

            for (const auto& item : mBindlessLayoutDesc.mSlots) {
                VkDescriptorSetLayoutBinding binding{};
                binding.binding = item.mSlot;
                binding.descriptorType = GetDescriptorType(item.mType, item.mDimension);
                binding.descriptorCount = item.mMaxCapacity;
                binding.stageFlags = GetShaderStageFlags(mBindlessLayoutDesc.mStageVisibility);
                binding.pImmutableSamplers = nullptr;

                bindings.emplace_back(binding);

                VkDescriptorBindingFlags bindingFlags =
                    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

                flags.emplace_back(bindingFlags);
            }

            VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = as<u32>(flags.size()),
                .pBindingFlags = flags.data()
            };

            VkDescriptorSetLayoutCreateInfo layoutInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &flagsInfo,
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                .bindingCount = as<u32>(bindings.size()),
                .pBindings = bindings.data()
            };

            MKT_VK_CHECK(vkCreateDescriptorSetLayout(
                checked_cast<Device*>(mDevice)->GetDevice(),
                &layoutInfo,
                nullptr,
                &mDescriptorSetLayout
            ));

        } else {
            eastl::vector<VkDescriptorSetLayoutBinding> bindings{};

            for (const auto& item : mBindingLayoutDesc.mBindings) {
                VkDescriptorSetLayoutBinding binding{};
                binding.binding = item.mSlot;
                binding.descriptorType = GetDescriptorType(item.mType, item.mDimension);
                binding.descriptorCount = 1;
                binding.stageFlags = GetShaderStageFlags(mBindingLayoutDesc.mStageVisibility);
                binding.pImmutableSamplers = nullptr;

                bindings.emplace_back(binding);
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = MKT_VK_FLAGS_NONE,
                .bindingCount = as<u32>(bindings.size()),
                .pBindings = bindings.data()
            };

            MKT_VK_CHECK(vkCreateDescriptorSetLayout(
                checked_cast<Device*>(mDevice)->GetDevice(),
                MKT_ADDRESSOF( layoutInfo ),
                nullptr,
                MKT_ADDRESSOF( mDescriptorSetLayout )
            ));
        }

        mIsAllocated = true;
    }

    auto BindingLayout::Release() -> void {
        vkDestroyDescriptorSetLayout( checked_cast<Device*>(mDevice)->GetDevice(), mDescriptorSetLayout, nullptr );
        mIsAllocated = false;
    }

    BindingSet::BindingSet( const BindingSetDescription &desc, BindingLayoutHandle layout )
        : mBindingLayout{ layout }, mBindingDescription{ desc }
    {}

    auto BindingSet::SetDebugName( eastl::string_view name ) -> void {
        IBindingSet::SetDebugName( name );

    }

    auto BindingSet::GetNativeHandle( ObjectType type ) -> Object {
        if ( type != ObjectType::Vk_DescriptorSet ) {
            return Object( nullptr );
        }

        return Object( mDescriptorSet );
    }

    auto BindingSet::GetNativeHandle( ObjectType type ) const -> Object {
        if ( type != ObjectType::Vk_DescriptorSet ) {
            return Object( nullptr );
        }

        return Object( mDescriptorSet );
    }

    BindingSet::~BindingSet() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto BindingSet::Initialize() -> void {
        auto* device{ checked_cast<Device*>( mDevice ) };
        auto* layout{ checked_cast<BindingLayout*>( mBindingLayout.GetRaw() ) };

        mDescriptorAllocatorHandle = device->GetDescriptorAllocator();
        if( mDescriptorAllocatorHandle.Allocate(layout->GetNativeHandle( ObjectType::Vk_DescriptorSetLayout ),mDescriptorSet) ) {
            // Update set
            DescriptorWriter writer{};
            for (const auto& item : mBindingDescription.mBindings) {
                switch (item.mType) {
                    case ResourceType::eTexture_SRV:
                        writer.WriteImage(
                            item.mSlot,
                            checked_cast<Texture*>( item.mResource )->GetNativeHandle(ObjectType::Vk_ImageView),
                            GetDescriptorType(item.mType, item.mDimension),
                            GetImageLayout( checked_cast<Texture*>( item.mResource )->GetResourceState() ) );
                        break;
                    case ResourceType::eTexture_UAV:
                        break;
                    case ResourceType::eTypedBuffer_SRV:
                        break;
                    case ResourceType::eTypedBuffer_UAV:
                        break;
                    case ResourceType::eStructuredBuffer_SRV:
                        break;
                    case ResourceType::eStructuredBuffer_UAV:
                        break;
                    case ResourceType::eRawBuffer_SRV:
                        break;
                    case ResourceType::eRawBuffer_UAV:
                        break;
                    case ResourceType::eConstantBuffer:
                        writer.WriteBuffer(
                            item.mSlot,
                            checked_cast<Buffer*>( item.mResource )->GetNativeHandle(ObjectType::Vk_Buffer),
                            checked_cast<Buffer*>( item.mResource )->GetSizeBytes(),
                            0,
                            GetDescriptorType(item.mType, item.mDimension) );
                        break;
                    case ResourceType::eSampler:
                        writer.WriteSampler(
                            item.mSlot,
                            checked_cast<Sampler*>( item.mResource )->GetNativeHandle(ObjectType::Vk_Sampler) );
                        break;
                    default:;
                }
            }

            //writer.SetVisibility()
            writer.UpdateSet( checked_cast<Device*>( mDevice )->GetDevice(), mDescriptorSet );

            mIsAllocated = true;
        }
    }

    auto BindingSet::Release() -> void {
        auto* device{ checked_cast<Device*>( mDevice ) };
        MKT_VK_CHECK( vkFreeDescriptorSets(
            device->GetDevice(),
            mDescriptorAllocatorHandle.mDescriptorPool,
            1,
            MKT_ADDRESSOF( mDescriptorSet )) );

        mIsAllocated = false;
    }

    InputLayout::InputLayout( const InputLayoutCreateDescription& desc )
        : mDesc{ desc } {
        // ----------------------------
        // Binding descriptions
        // ----------------------------
        for (const auto& bindingDesc : desc.mVertexBindingDescriptions) {
            VkVertexInputBindingDescription vkBinding{};

            vkBinding.binding = bindingDesc.mBinding;
            vkBinding.stride  = bindingDesc.mStride;

            vkBinding.inputRate =
                (bindingDesc.mRate == InputRate::ePerVertex)
                ? VK_VERTEX_INPUT_RATE_VERTEX
                : VK_VERTEX_INPUT_RATE_INSTANCE;

            mVertexBindingDescriptions.emplace_back( vkBinding );
        }

        // ----------------------------
        // Attribute descriptions
        // ----------------------------
        for (const auto& attr : desc.mVertexAttributeDescriptions) {
            VkVertexInputAttributeDescription vkAttr{};

            vkAttr.location = attr.mLocation;                 // shader location
            vkAttr.binding  = attr.mBinding;                  // which buffer
            vkAttr.format   = GetFormat(attr.mFormat);        // your conversion
            vkAttr.offset   = attr.mOffset;                   // byte offset

            mVertexAttributeDescriptions.emplace_back(vkAttr);
        }

    }
    auto InputLayout::GetVertexBindingDesc() const -> const eastl::fixed_vector<VkVertexInputBindingDescription, kMaxVertexBindings>& {
        return mVertexBindingDescriptions;
    }

    auto InputLayout::GetVertexAttributesDesc() const -> const eastl::fixed_vector<VkVertexInputAttributeDescription, kMaxVertexAttributes>& {
        return mVertexAttributeDescriptions;
    }

    auto InputLayout::GetNumAttributes() const -> u32 {
        return mVertexAttributeDescriptions.size();
    }

    auto InputLayout::GetAttributeDescription( u32 index ) const -> const VertexAttributeDescription & {
        MKT_ASSERT( index < mDesc.mVertexAttributeDescriptions.size(), "Specified index out of bounds." );
        return mDesc.mVertexAttributeDescriptions.at( index );
    }

    InputLayout::~InputLayout() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto InputLayout::Initialize() -> void {
        mIsAllocated = true;
    }

    auto InputLayout::Release() -> void {
        mIsAllocated = false;
    }

    PipelineLayout::PipelineLayout( const PipelineLayoutCreateDescription& desc )
        : mDesc{ desc }
    {}

    auto PipelineLayout::GetBindPoint() const -> PipelineType {
        return mDesc.mBindPoint;
    }

    auto PipelineLayout::GetNativeHandle( ObjectType type ) -> Object {
        switch (type) {
            case ObjectType::Vk_PipelineLayout:
                return Object( mPipelineLayout );
            default:;
        }

        return Object( nullptr );
    }

    auto PipelineLayout::GetNativeHandle( ObjectType type ) const -> Object {
        switch (type) {
            case ObjectType::Vk_PipelineLayout:
                return Object( mPipelineLayout );
            default:;
        }

        return Object( nullptr );
    }

    PipelineLayout::~PipelineLayout() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto PipelineLayout::Initialize() -> void {
        auto* device{ checked_cast<Device*>( mDevice ) };

        // IMPORTANT:
        // In Vulkan, VkPipelineLayoutCreateInfo::pSetLayouts is an array where each element
        // corresponds to a descriptor set index in order:
        //    pSetLayouts[0] -> set = 0
        //    pSetLayouts[1] -> set = 1
        //    pSetLayouts[2] -> set = 2
        // Vulkan does NOT sort or remap them automatically. If the layouts in out.setLayouts
        // are not in the same order as the shader set indices, you will get validation errors.
        // For example, if the fragment shader uses set = 1 but out.setLayouts[1] corresponds
        // to set = 2, Vulkan will complain that the descriptor is missing.
        // Here we are just filling not used slots with empty descriptor set layouts.

        // TODO: VK_EXT_Pipeline library extension
        // [11:59:23] STDERR LOG [thread 67676] Validation Error: Validation Error: [ VUID-VkPipelineLayoutCreateInfo-graphicsPipelineLibrary-06753 ] |
        // MessageID = 0x57ab6143 | vkCreatePipelineLayout(): pCreateInfo->pSetLayouts[0] is VK_NULL_HANDLE, but VK_EXT_graphics_pipeline_library is not enabled.
        // The Vulkan spec states: If graphicsPipelineLibrary is not enabled, elements of pSetLayouts must be valid VkDescriptorSetLayout objects
        // (https://www.khronos.org/registry/vulkan/specs/1.3-extensions/html/vkspec.html#VUID-VkPipelineLayoutCreateInfo-graphicsPipelineLibrary-06753)

        // Find the highest set index
        u32 maxSet{ 0 };
        for ( const auto& bindingLayout: mDesc.mBindingLayouts ) {
            maxSet = eastl::max( maxSet, bindingLayout->GetRegisterSpace() );
        }

        // Initialize everything with "holes" and fill accordingly
        VkDescriptorSetLayout emptySetLayout{ device->GetLayoutForEmptySet() };
        eastl::vector<VkDescriptorSetLayout> setLayouts( maxSet + 1, emptySetLayout );

        // Place set layouts at correct set indices
        for ( const auto& bindingLayout: mDesc.mBindingLayouts ) {
            setLayouts[bindingLayout->GetRegisterSpace()] =
                checked_cast<const BindingLayout*>( bindingLayout.GetRaw() )->GetNativeHandle( ObjectType::Vk_DescriptorSetLayout );
        }

        VkPipelineLayoutCreateInfo plInfo{ initializers::PipelineLayoutCreateInfo() };

        plInfo.setLayoutCount = as<u32>( setLayouts.size() );
        plInfo.pSetLayouts = setLayouts.data();

        // I am not sure if this is good design but for the time being push constants will always be available for all
        // pipeline layouts, although limited to amount guaranteed by vulkan across devices (128 bytes)
        VkPushConstantRange range{};
        range.offset = 0;
        range.size   = kMaxPushConstantSize;
        range.stageFlags = GetShaderStageFlags(mDesc.mPushConstantsVisibility);

        eastl::array psRanges{ range };

        plInfo.pushConstantRangeCount = as<u32>( psRanges.size() );
        plInfo.pPushConstantRanges = psRanges.data();

        MKT_VK_CHECK( vkCreatePipelineLayout( device->GetDevice(), &plInfo, nullptr, MKT_ADDRESSOF( mPipelineLayout ) ) );
        mIsAllocated = true;
    }

    auto PipelineLayout::Release() -> void {
        auto* device{ checked_cast<Device*>( mDevice ) };

        vkDestroyPipelineLayout( device->GetDevice(), mPipelineLayout, nullptr );

        mIsAllocated = false;
    }

}// namespace mikoto::renderer::vulkan
