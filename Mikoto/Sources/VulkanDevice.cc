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

#include <ranges>
#include <vector>

#include <EASTL/utility.h>
#include <EASTL/vector.h>
#include <EASTL/sort.h>
#include <EASTL/array.h>
#include <EASTL/unordered_map.h>
#include <EASTL/unordered_set.h>
#include <EASTL/algorithm.h>

#include <volk.h>
#include <vk_mem_alloc.h>

#include <tracy/TracyVulkan.hpp>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/Platform.hh>
#include <Core/Types.hh>

#include <Filesystem/FileService.hh>
#include <Filesystem/FileSystem.hh>

#include <Threading/ThreadUtility.hh>

#include <Logging/Logger.hh>

#include <Math/Math.hh>
#include <Math/Random.hh>

#include <Renderer/Core/Rhi.hh>
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
        Instance& instance{ ctx->GetInstance() };

        // Choose primary physical device
        if ( mFeaturesSupport.mEnablePresentation ) {
            mSurface = instance.mSurface;
            mExtensions.emplace_back( VK_KHR_SWAPCHAIN_EXTENSION_NAME );
        }

        const auto pdIt{ std::ranges::find_if( instance.mPhysicalDevices, [this](PhysicalDevice& dev) {
            return IsDeviceSuitable( dev );
        } ) };

        if (pdIt != instance.mPhysicalDevices.end()) {
            mPhysicalDevice = MKT_ADDRESSOF( *pdIt );
            mName = mPhysicalDevice->mProperties.deviceName;
        } else {
            MKT_CORE_LOGGER_INFO( "VulkanDevice - No suitable physical device with the desired features." );
            return;
        }

        InitLogicalDevice();
        InitLogicalQueues();
        InitMemoryAllocator();
        InitDescriptorAllocator();

        InitPipelineCache();
        InitTracyContext();
        InitDummyResources();

        mIsInitialized = true;
    }

    auto Device::Shutdown() -> void {
        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "VulkanDevice Shutting down Vulkan Device." );

        WaitIdle();

        ShutdownTracyContext();

        DestroyDummyResources();

        SerializePipelineCache();

        // Clear descriptor manager
        mDescriptorAllocatorPool.reset();

        // Clear upload manager
        mUploadManager.reset();

        for (auto& queue : mQueues | std::ranges::views::values ) {
            queue.Reset();
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

    auto Device::CreateFence( u64 fenceInitialValue ) -> FenceHandle {
        FenceHandle fence{ Ref<Fence>::Spawn( fenceInitialValue ) };

        if ( fence.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate fence resource." );
            return FenceHandle::CreateEmpty();
        }

        fence->Initialize( this );

        return fence;
    }

    auto Device::UnMap( IBuffer* buffer ) -> void {
    }

    auto Device::Map( IBuffer* buffer ) -> const void* {
        Buffer* b{ checked_cast<Buffer*>( buffer ) };
        if (!b->IsMapped()) {
            b->PersistentMap();
        }

        return b->GetMappedAddress();
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
        DescriptorTableHandle table{ Ref<DescriptorTable>::Spawn( layout ) };

        if ( table.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate descriptor table resource." );
            return DescriptorTableHandle::CreateEmpty();
        }

        table->Initialize( this );

        return table;
    }

    auto Device::ResizeDescriptorTable( DescriptorTableHandle descriptorTable, u32 newSize, bool keepContents ) -> bool {
        return false;
    }

    auto Device::WriteDescriptorTable( DescriptorTableHandle descriptorTable, const BindingSetItem& item ) -> bool {
        DescriptorTable* table{ checked_cast<DescriptorTable*>( descriptorTable.GetRaw() ) };

        i32 resourceSlot{ table->GetResourceSlot( item.mType ) };

        DescriptorWriter writer{};

        switch ( item.mType ) {
            case ResourceType::eTexture_SRV:
                writer.WriteImage(
                        resourceSlot,
                        checked_cast<Texture*>( item.mResource )->GetNativeHandle( ObjectType::Vk_ImageView ),
                        GetDescriptorType( item.mType ),
                        GetImageLayout( checked_cast<Texture*>( item.mResource )->GetResourceState() ), item.mSlot );
                break;
            case ResourceType::eTexture_UAV:
                break;
            case ResourceType::eTypedBuffer_SRV:
                break;
            case ResourceType::eTypedBuffer_UAV:
                break;
            case ResourceType::eStructuredBuffer_SRV:
                writer.WriteBuffer(
                    resourceSlot,
                    checked_cast<Buffer*>( item.mResource )->GetNativeHandle( ObjectType::Vk_Buffer ),
                    checked_cast<Buffer*>( item.mResource )->GetSizeBytes(),
                    0,
                    GetDescriptorType( item.mType ), item.mSlot );
                break;
            case ResourceType::eStructuredBuffer_UAV:
                writer.WriteBuffer(
                    resourceSlot,
                    checked_cast<Buffer*>( item.mResource )->GetNativeHandle( ObjectType::Vk_Buffer ),
                    checked_cast<Buffer*>( item.mResource )->GetSizeBytes(),
                    0,
                    GetDescriptorType( item.mType ), item.mSlot );
                break;
            case ResourceType::eRawBuffer_SRV:
                break;
            case ResourceType::eRawBuffer_UAV:
                break;
            case ResourceType::eConstantBuffer:
                writer.WriteBuffer(
                    resourceSlot,
                    checked_cast<Buffer*>( item.mResource )->GetNativeHandle( ObjectType::Vk_Buffer ),
                    checked_cast<Buffer*>( item.mResource )->GetSizeBytes(),
                    0,
                    GetDescriptorType( item.mType ), item.mSlot );
                break;
            case ResourceType::eSampler:
                writer.WriteSampler(
                        resourceSlot,
                        checked_cast<Sampler*>( item.mResource )->GetNativeHandle( ObjectType::Vk_Sampler ), item.mSlot );
                break;
            default:;
        }

        VkDescriptorSet descriptorSet{ descriptorTable->GetNativeHandle( ObjectType::Vk_DescriptorSet ) };
        writer.UpdateSet( mLogicalDevice, descriptorSet );

        return true;
    }

    auto Device::Wait( QueueType type, FenceHandle handle, u64 fenceValue ) -> void {
        VkSemaphore semaphore{ handle->GetNativeHandle(ObjectType::Vk_Semaphore) };

        VkSemaphoreWaitInfo waitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &semaphore,
            .pValues = &fenceValue
        };

        vkWaitSemaphores(mLogicalDevice, &waitInfo, UINT64_MAX);
    }

    auto Device::Signal( QueueType type, FenceHandle handle, u64 fenceValue ) -> void {
        Fence* fence{ checked_cast<Fence*>( handle.GetRaw() ) };
        mQueues[type]->AddQueueSignalSemaphore( fence, fenceValue, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT );
    }

    auto Device::ExecutePendingCommands() -> void {
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

    auto Device::AddQueueWaitFence( QueueType queueType, FencePlain* fence ) -> void {
        mQueues[queueType]->AddQueueWaitFence( fence );
    }

    auto Device::AddQueueSignalSemaphore( QueueType queueType, BinarySemaphore* semaphore, VkPipelineStageFlags2 stageFlags ) -> void {
        mQueues[queueType]->AddQueueSignalSemaphore( semaphore, stageFlags );
    }

    auto Device::AddQueueWaitSemaphore( QueueType queueType, BinarySemaphore* semaphore, VkPipelineStageFlags2 stageFlags ) -> void {
        mQueues[queueType]->AddQueueWaitSemaphore( semaphore, stageFlags );
    }

    auto Device::SetDebugName( VkObjectType objectType, u64 handle, eastl::string_view name ) -> void {
        if (name.empty()) {
            return;
        }

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
        PipelineHandle computePipeline{ Ref<ComputePipeline>::Spawn( description, mPipelineCache ) };

        if ( computePipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate compute pipeline resource." );
            return PipelineHandle::CreateEmpty();
        }

        computePipeline->Initialize( this );

        return computePipeline;
    }

    auto Device::CreatePipeline( const GraphicsPipelineDescription &description ) -> PipelineHandle {
        PipelineHandle graphicsPipeline{ Ref<GraphicsPipeline>::Spawn( description, mPipelineCache ) };

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
        Queue* queue{ GetQueue( parameters.mQueueType ) };

        auto handle{ CommandListHandle::CreateEmpty() };
        if (queue) {
            handle = queue->AllocateCmdList( parameters );
        }

        return handle;
    }

    auto Device::RunGarbageCollection() -> void {
        mUploadManager->ReclaimMemory();
        mDescriptorAllocatorPool->Flip();

        for (auto& queue : mQueues | std::ranges::views::values ) {
            queue->RunGarbageCollection();
        }
    }

    auto Device::SubmitCommands( CommandListHandle cmd ) -> u64 {
        MKT_ASSERT( mQueues.contains( cmd->GetQueueType() ), "No queue" );
        return mQueues[cmd->GetQueueType()]->SubmitCommandList( cmd );
    }

    auto Device::ExecuteCommands( CommandListHandle cmd ) -> void {
        mQueues[cmd->GetQueueType()]->ExecuteCommandList( cmd );
    }
    
    auto Device::ExecuteCommands( eastl::span<CommandListHandle> cmdList ) -> void {

    }

    auto Device::WaitIdle() -> void {
        vkDeviceWaitIdle( mLogicalDevice );
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
        return it != mQueues.end() ? it->second.GetRaw() : nullptr;
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
        mEnabled12Features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;

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

        // Prepare queue infos
        // I find a queue family index that supports the operations I want to perform
        // Right we are only looking for graphics, transfer, compute and optionally present
        eastl::unordered_set<u32> queueFamilies{};
        queueFamilies.emplace( mPhysicalDevice->GetFamilyIndexWithSupport( QueueOpSupportFlagsBits::kGraphics ) );
        queueFamilies.emplace( mPhysicalDevice->GetFamilyIndexWithSupport( QueueOpSupportFlagsBits::kCompute ) );
        queueFamilies.emplace( mPhysicalDevice->GetFamilyIndexWithSupport( QueueOpSupportFlagsBits::kTransfer ) );

        if (mFeaturesSupport.mEnablePresentation) {
            queueFamilies.emplace( mPhysicalDevice->GetFamilyIndexWithSupport( QueueOpSupportFlagsBits::kPresentation ) );
        }

        MKT_ASSERT( !queueFamilies.contains( PhysicalDevice::kInvalidQueueFamilyIndex ) && !queueFamilies.empty(),
            "Queue family requires at least one valid family index");

        eastl::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
        eastl::vector queuePriority( kMaxQueuesPerFamily, kQueueDefaultPriority );

        for (const auto& familyIndex : queueFamilies) {
            auto& queueCreateInfo{ queueCreateInfos.emplace_back( initializers::DeviceQueueCreateInfo() ) };

            queueCreateInfo.queueFamilyIndex = familyIndex;

            queueCreateInfo.queueCount = kMaxQueuesPerFamily;
            queueCreateInfo.pQueuePriorities = queuePriority.data();
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
        // Right now I do not have any proper filter for each type of queue
        // Just pick the first queue amongst the available queues that satisfies the operations I need.
        // I am not sure if I will benefit from multiple VkQueue's, stick to single queue for now
        // as it also simplifies synchronization, work from different queues needs to be synchronized via
        // semaphores even if they belong to same family.

        ankerl::unordered_dense::map<QueueType, const VulkanQueueData*> queuesIndices{};
        queuesIndices[QueueType::eGraphics] = mPhysicalDevice->GetQueueWithSupport( QueueOpSupportFlagsBits::kGraphics );
        queuesIndices[QueueType::eCompute] = mPhysicalDevice->GetQueueWithSupport( QueueOpSupportFlagsBits::kCompute );
        queuesIndices[QueueType::eTransfer] = mPhysicalDevice->GetQueueWithSupport( QueueOpSupportFlagsBits::kTransfer );

        if (mFeaturesSupport.mEnablePresentation) {
            queuesIndices[QueueType::ePresent] = mPhysicalDevice->GetQueueWithSupport( QueueOpSupportFlagsBits::kPresentation );
        }

        // Keep track of families that already have a queue
        ankerl::unordered_dense::map<u32, Ref<Queue>> queueFamilyTracking{};

        for (constexpr u32 kQueueIndex{ 0 }; const auto& [queueType, queueData] : queuesIndices) {
            const auto it{ queueFamilyTracking.find( queueData->FamilyIndex ) };
            if (it == queueFamilyTracking.end()) {
                QueueHandle queue{ Ref<Queue>::Spawn( queueType, queueData->mOpSupportFlags, queueData->FamilyIndex, kQueueIndex ) };
                queue->Initialize( this );

                mQueues[queueType] = queue;
                queueFamilyTracking[queueData->FamilyIndex] = queue;
            } else {
                mQueues[queueType] = it->second;
            }
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

    auto Device::InitPipelineCache() -> void {
        if (!FileService::GetPtr()) {
            return;
        }

        mPipelineCachePath = PathBuilder{}
            .SetPath( kPipelineCacheDirectory )
            .SetPath( "pipeline_pache.bin" )
            .Build();

        (void)CreateIfNotExistsDirectory( kPipelineCacheDirectory );

        FileHandle file{ FileService::Get()->LoadFile( mPipelineCachePath ) };
        if (file.IsEmpty()) {
            file = FileService::Get()->CreateNewFile( mPipelineCachePath );
        }

        VkPipelineCacheCreateInfo cacheInfo{ initializers::PipelineCacheCreateInfo() };

        if (!file.IsEmpty() && file->HasContents()) {
            cacheInfo.initialDataSize = file->GetSize();
            cacheInfo.pInitialData = file->GetContentsBytes();
        }

        vkCreatePipelineCache( mLogicalDevice , MKT_ADDRESSOF( cacheInfo ), nullptr, MKT_ADDRESSOF( mPipelineCache ));
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

    auto Device::InitTracyContext() -> void {

    }

    auto Device::ShutdownTracyContext() -> void {

    }

    auto Device::InitDummyResources() -> void {
        mDummySampler = CreateSampler( SamplerCreateDescription{} );
        mEmptyBindingLayout = CreateBindingLayout( BindingLayoutDescription{} );
    }

    auto Device::DestroyDummyResources() -> void {
        mDummySampler.Reset();
        mEmptyBindingLayout.Reset();
    }

    auto Device::SerializePipelineCache() -> void {
        size_t dataSize{};
        vkGetPipelineCacheData(mLogicalDevice, mPipelineCache, MKT_ADDRESSOF( dataSize ), nullptr);

        if (dataSize > 0) {
            eastl::vector<u8> data(dataSize);

            MKT_VK_CHECK( vkGetPipelineCacheData(mLogicalDevice, mPipelineCache, MKT_ADDRESSOF( dataSize ), data.data()) );

            if (FileService::GetPtr()) {
                FileHandle file{ FileService::Get()->LoadFile( mPipelineCachePath ) };
                file->Write( rc_cast<const char*>(data.data()), as<size_t>(data.size()) );
            }
        }

        vkDestroyPipelineCache(mLogicalDevice, mPipelineCache, nullptr);
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

        // By default, we look for a device that supports
        // compute, graphics, transfer and optionally presentation
        QueueOpSupportFlags opSupportFlags{
            QueueOpSupportFlagsBits::kGraphics |
            QueueOpSupportFlagsBits::kCompute |
            QueueOpSupportFlagsBits::kTransfer };

        // Dynamic rendering is mandatory because Mikoto targets
        // Vulkan 1.3 by default where this feature is core, this should be just a sanity check
        // because the instance is already created with this in mind, with this the MKT_USE_VULKAN_DYNAMIC_RENDERING macro is deprecated
        VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{ initializers::DynamicRenderingFeature() };

        VkPhysicalDeviceFeatures2 features2{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = MKT_ADDRESSOF( dynamicRenderingFeature ) };
        vkGetPhysicalDeviceFeatures2( device.mPhysicalDevice, MKT_ADDRESSOF( features2 ) );
        if (dynamicRenderingFeature.dynamicRendering != VK_TRUE) {
            return false;
        }

        // Wireframe support if requested
        if (mFeaturesSupport.mHardwareWireframe && !device.mFeatures.fillModeNonSolid) {
            return false;
        }

        // Improved texture quality if requested
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

    CommandList::CommandList( const VkCommandBufferAllocateInfo &createInfo, QueueType type, CommandPool* pool )
        : ICommandList{ type }, mCommandPool{ pool }, mAllocInfo{ createInfo }, mMaxThreadConcurrency{ 0 } {
        mLabelColor = Color(
            (f32)math::random::GetRandomReal( 0.0f, 1.0f ),
            (f32)math::random::GetRandomReal( 0.0f, 1.0f ),
            (f32)math::random::GetRandomReal( 0.0f, 1.0f ),
            0.1f );
    }

    CommandList::CommandList( const VkCommandBufferAllocateInfo& createInfo, const CommandListParameters& desc, CommandPool* pool )
        : ICommandList{ desc.mQueueType }, mCommandPool{ pool }, mAllocInfo{ createInfo }, mMaxThreadConcurrency{ desc.mMaxThreadConcurrency } {
        mLabelColor = Color(
            (f32)math::random::GetRandomReal( 0.0f, 1.0f ),
            (f32)math::random::GetRandomReal( 0.0f, 1.0f ),
            (f32)math::random::GetRandomReal( 0.0f, 1.0f ),
            0.1f );
    }

    auto CommandList::Begin( const CommandListBeginDescription& desc ) -> void {
        mHostThread = std::this_thread::get_id();

        ClearState();

        if (mMaxThreadConcurrency != 0) {
            // TODO: instead of doing this allocate secondary command buffer from the queue kind of like you are doing with AllocateCommandList but instead we get just the VkCommandHandle, every worker thread to its pool
            // This should happen when we call BeginParallel the thread creates its context and its command buffer

            // Access to pool must be synchronized, only one thread can allocate at a time
            // we allocate as many as concurrency value specified. Worker thread will only have to grab from this
            // pool of already allocated secondary command buffers

            // Insert as many as we need to reach mMaxThreadConcurrency
            size_t missingCmdsCount{ mMaxThreadConcurrency - mSecondaryCommandBuffersPool.size() };
            for (size_t i{}; i < missingCmdsCount; ++i) {
                VkCommandBufferAllocateInfo allocInfo{ initializers::CommandBufferAllocateInfo() };
                allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                allocInfo.commandPool = mAllocInfo.commandPool;
                allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
                allocInfo.commandBufferCount = 1;

                VkCommandBuffer cmd{};
                MKT_VK_CHECK( vkAllocateCommandBuffers(checked_cast<Device*>(mDevice)->GetDevice(), &allocInfo, &cmd) );

                mSecondaryCommandBuffersPool.emplace_back(cmd);
            }
        }

        auto* ctx{ GetThreadContext( false ) };

        // Store the primary command buffer for the duration of the record
        // frame (a record frame is set of commands between Begin() and End())
        // Most of operations can be called from multiple threads on the same command buffer
        // every thread gets its context, but Begin() and End() must be called from same thread
        mPrimaryCommandBuffer = ctx->mCommandBuffer;

        VkCommandBufferBeginInfo beginInfo{ initializers::CommandBufferBeginInfo() };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        MKT_VK_CHECK( vkBeginCommandBuffer( mPrimaryCommandBuffer, MKT_ADDRESSOF( beginInfo ) ) );

        // Must be in recording state
        // https://docs.vulkan.org/spec/latest/chapters/debugging.html
        // https://docs.vulkan.org/samples/latest/samples/extensions/debug_utils/README.html
        // https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBeginDebugUtilsLabelEXT.html
        VkDebugUtilsLabelEXT labelInfo = {};
        labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        labelInfo.pLabelName = desc.mScopeName.empty() ? "UnnamedScope" : desc.mScopeName.c_str();
        labelInfo.color[0] = mLabelColor.mR;
        labelInfo.color[1] = mLabelColor.mG;
        labelInfo.color[2] = mLabelColor.mB;
        labelInfo.color[3] = mLabelColor.mA;

        vkCmdBeginDebugUtilsLabelEXT( mPrimaryCommandBuffer, &labelInfo);
    }

    auto CommandList::End() -> void {
        // The thread that enters here is the one that called Begin()
        MKT_ASSERT( mHostThread == std::this_thread::get_id(), "Error simultaneous use of command buffer" );

        eastl::fixed_vector<VkCommandBuffer, kMaxCmdConcurrency> secondaryCmdBuffers{};
        for (const auto& [threadID, ctx] : mThreadContexts ) {
            if (threadID != mHostThread) {
                secondaryCmdBuffers.emplace_back( ctx->mCommandBuffer );
            }
        }

        if (!secondaryCmdBuffers.empty()) {
            vkCmdExecuteCommands(
                mPrimaryCommandBuffer,
                as<u32>( secondaryCmdBuffers.size() ),
                secondaryCmdBuffers.data() );
        }

        vkCmdEndDebugUtilsLabelEXT( mPrimaryCommandBuffer );
        MKT_VK_CHECK( vkEndCommandBuffer( mPrimaryCommandBuffer ) );
    }

    auto CommandList::BeginParallel() -> void {
        MKT_ASSERT( mMaxThreadConcurrency != 0, "Command buffer does not allow parallel execution" );

        auto* ctx{ GetThreadContext() };

        VkCommandBufferInheritanceInfo inheritanceInfo{};
        inheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
        inheritanceInfo.subpass = 0;
        inheritanceInfo.renderPass = VK_NULL_HANDLE;
        inheritanceInfo.framebuffer = VK_NULL_HANDLE;

        VkCommandBufferBeginInfo beginInfo{ initializers::CommandBufferBeginInfo() };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        beginInfo.pInheritanceInfo = MKT_ADDRESSOF( inheritanceInfo );

        vkBeginCommandBuffer(ctx->mCommandBuffer, &beginInfo);
    }

    auto CommandList::EndParallel() -> void {
        auto* ctx{ GetThreadContext() };
        MKT_ASSERT( ctx, "Forgot to call BeginParallel(...)" );

        MKT_VK_CHECK( vkEndCommandBuffer( ctx->mCommandBuffer ) );
    }

    auto CommandList::BeginTrackingState( IBuffer *buffer, ResourceStates newState ) -> void {
        // https://docs.vulkan.org/guide/latest/synchronization.html#synchronization
        // https://docs.vulkan.org/samples/latest/samples/performance/pipeline_barriers/README.html
        auto oldState{ buffer->GetResourceState() };

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

        mBufferBarriers.push_back( barrier );

        buffer->SetResourceState(newState);
    }

    auto CommandList::BeginTrackingState( ITexture *texture, ResourceStates newState ) -> void {
        // https://docs.vulkan.org/samples/latest/samples/performance/pipeline_barriers/README.html
        // https://gpuopen.com/learn/vulkan-barriers-explained/
        // https://cpp-rendering.io/barriers-vulkan-not-difficult/
        auto oldState{ texture->GetResourceState() };

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

        mImageBarriers.push_back( barrier );

        texture->SetResourceState(newState);
    }

    auto CommandList::CommitBarriers() -> void {
        if (mBufferBarriers.empty() && mImageBarriers.empty())
            return;

        VkDependencyInfo depInfo{};

        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

        depInfo.imageMemoryBarrierCount = as<u32>(mImageBarriers.size());
        depInfo.pImageMemoryBarriers = mImageBarriers.data();

        depInfo.bufferMemoryBarrierCount = as<u32>(mBufferBarriers.size());
        depInfo.pBufferMemoryBarriers = mBufferBarriers.data();

        auto* ctx{ GetThreadContext() };
        vkCmdPipelineBarrier2( ctx->mCommandBuffer, &depInfo);

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
        for (auto image : frameBuffer->GetColorAttachments()) {
            if (mEnableAutomaticBarriers) {
                SetResourceState( image.GetRaw(), ResourceStates::eCopyDest );
            }

            VkClearColorValue clearColor{{color.mR, color.mB, color.mB, color.mA}};
            VkImageSubresourceRange range{};
            range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            range.baseMipLevel = 0;
            range.levelCount = 1;
            range.baseArrayLayer = 0;
            range.layerCount = 1;

            // Image needs to be VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL

            auto* ctx{ GetThreadContext() };
            vkCmdClearColorImage(ctx->mCommandBuffer, image->GetNativeHandle( ObjectType::Vk_Image ), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
        }
    }

    auto CommandList::SetClearColor( TextureHandle image, Color color ) -> void {
        if (mEnableAutomaticBarriers) {
            SetResourceState( image.GetRaw(), ResourceStates::eCopyDest );
        }

        VkClearColorValue clearColor{{color.mR, color.mB, color.mB, color.mA}};
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;
        range.levelCount = 1;
        range.baseArrayLayer = 0;
        range.layerCount = 1;

        // Image needs to be VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL

        auto* ctx{ GetThreadContext() };
        vkCmdClearColorImage(ctx->mCommandBuffer, image->GetNativeHandle( ObjectType::Vk_Image ), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
    }

    auto CommandList::Write( IBuffer *src, ITexture *dest, u32 mipLevel ) -> void {

    }

    auto CommandList::Write( ITexture *texture, u32 mipLevel, const void *data, size_t byteSize ) -> void {
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

        auto* ctx{ GetThreadContext() };

        // If called on the owner thread we use the primary
        // otherwise we use a secondary
        vkCmdCopyBufferToImage(
            ctx->mCommandBuffer,
            allocation->mBuffer->GetNativeHandle( ObjectType::Vk_Buffer ),
            texture->GetNativeHandle( ObjectType::Vk_Image ),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion );

        std::lock_guard lock{ mUploadAllocationsMutex };
        mUploadAllocations.emplace_back( allocation );
    }

    auto CommandList::Write( IBuffer* buffer, size_t destOffset, const void* data, size_t byteSize ) -> void {
        MKT_ASSERT(buffer, "Buffer is nullptr");
        MKT_ASSERT(data, "Data is nullptr");
        MKT_ASSERT(byteSize > 0, "Size is 0");

        auto* ctx{ GetThreadContext() };

        // You cannot record transfer ops inside rendering
        if (ctx->mIsRenderScopeActive) {
            EndRendering();
        }

        if (mEnableAutomaticBarriers) {
            SetResourceState(buffer, ResourceStates::eCopyDest);
        }

        VkBuffer vkBuffer{ buffer->GetNativeHandle(ObjectType::Vk_Buffer) };

        if (byteSize <= 65536) { // 64 KB limit
            // I do not think I can simply update more bytes here what if I end up overwriting something
            // the called does not expect user must then ensure buffer is multiple of 4
            vkCmdUpdateBuffer(
                ctx->mCommandBuffer,
                vkBuffer,
                destOffset,
                byteSize,
                data
            );
        } else {
            // Instead of using the GPU default allocator I can declare a linear allocator for every command buffer
            // On first usage (lazy create) I create the buffer with a large enough size
            // every time I call End() I just reset the allocator
            GpuUploadAllocation* allocation{ mUploadManager->SubAllocate( byteSize ) };

            // TODO: Implement a set resource state internal version that specifies ranges
            // to be protected, SetResourceState() by default protects the whole range of the buffer
            SetResourceState( allocation->mBuffer, ResourceStates::eCopySource );

            std::memcpy(allocation->mMappedMemory, data, byteSize);

            VkBufferCopy copy{};
            copy.srcOffset = allocation->mOffset;
            copy.dstOffset = destOffset;
            copy.size = byteSize;

            vkCmdCopyBuffer(
                ctx->mCommandBuffer,
                allocation->mBuffer->GetNativeHandle( ObjectType::Vk_Buffer ),
                buffer->GetNativeHandle( ObjectType::Vk_Buffer ),
                1,
                &copy
            );

            std::lock_guard lock{ mUploadAllocationsMutex };
            mUploadAllocations.emplace_back( allocation );
        }
    }

    auto CommandList::Write( IBuffer *buffer, const void *data, size_t byteSize ) -> void {
        Write( buffer, 0, data, byteSize );
    }

    auto CommandList::Draw( const DrawArguments &args ) -> void {
        auto* ctx{ GetThreadContext() };
        vkCmdDraw( ctx->mCommandBuffer, args.mVertexCount, args.mInstanceCount, args.mFirstVertex, args.mFirstInstance );
    }

    auto CommandList::DrawIndexed( const DrawArguments &args ) -> void {
        auto* ctx{ GetThreadContext() };

        vkCmdDrawIndexed(
            ctx->mCommandBuffer,
            args.mIndexCount,
            args.mInstanceCount,
            args.mFirstIndex,
            args.mVertexOffset,
            args.mFirstInstance );
    }

    auto CommandList::DrawIndirect( u32 offset, u32 drawCount ) -> void {
        auto* ctx{ GetThreadContext() };
        vkCmdDrawIndirect( ctx->mCommandBuffer,
            ctx->mIndirectBuffer->GetNativeHandle( ObjectType::Vk_Buffer ), offset, drawCount, MKT_SIZEOF( VkDrawIndirectCommand ) );
    }

    auto CommandList::DrawIndexedIndirect( u32 offset, u32 drawCount ) -> void {
        auto* ctx{ GetThreadContext() };
        vkCmdDrawIndexedIndirect( ctx->mCommandBuffer,
            ctx->mIndirectBuffer->GetNativeHandle( ObjectType::Vk_Buffer ), offset, drawCount, MKT_SIZEOF( VkDrawIndirectCommand ) );
    }

    auto CommandList::SetPushConstants( IPipelineLayout* pipelineLayout, const void* data, size_t byteSize, ShaderStage visibility ) -> void {
        if (!data || byteSize == 0 || !pipelineLayout) {
            return;
        }

        VkShaderStageFlags pcShaderStages{ GetShaderStageFlags( visibility ) };

        auto* ctx{ GetThreadContext() };

        vkCmdPushConstants(
            ctx->mCommandBuffer,
            pipelineLayout->GetNativeHandle( ObjectType::Vk_PipelineLayout ),
            pcShaderStages,
            0,
            as<u32>(byteSize),
            data);
    }

    auto CommandList::SetDebugName( eastl::string_view name ) -> void {
        mDebugName = name;
    }

    auto CommandList::PushBarrier( const BufferBarrierDescription& barrier ) -> void {

    }

    auto CommandList::PushBarrier( const TextureBarrierDescription& barrier ) -> void {

    }

    auto CommandList::SetBarrier( const BufferBarrierDescription& barrier ) -> void {

    }

    auto CommandList::SetBarrier( const TextureBarrierDescription& barrier ) -> void {

    }

    auto CommandList::Copy( IBuffer *src, IBuffer *dest ) -> void {
        Copy(src, dest, 0);
    }

    auto CommandList::Copy( IBuffer *src, IBuffer *dest, size_t dstOffset ) -> void {
        MKT_ASSERT( src != nullptr, "Source buffer cannot be null" );
        MKT_ASSERT( dest != nullptr, "Destination buffer cannot be null" );

        const size_t size{ src->GetSizeBytes() };

        // The data I’m copying fits inside the destination buffer, starting at dstOffset
        MKT_ASSERT(size <= (dest->GetSizeBytes() - dstOffset), "Destination buffer is too small");

        if (mEnableAutomaticBarriers) {
            BeginTrackingState(src, ResourceStates::eCopySource);
            BeginTrackingState(dest, ResourceStates::eCopyDest);

            CommitBarriers();
        }

        VkBufferCopy region{};
        region.srcOffset = 0;
        region.dstOffset = dstOffset;
        region.size      = size;

        auto* ctx{ GetThreadContext() };

        vkCmdCopyBuffer(
            ctx->mCommandBuffer,
            src->GetNativeHandle( ObjectType::Vk_Buffer ),
            dest->GetNativeHandle( ObjectType::Vk_Buffer ),
            1,
            &region
        );
    }

    auto CommandList::Copy( IBuffer* dest, ITexture* src ) -> void {
        if (mEnableAutomaticBarriers) {
            BeginTrackingState( src, ResourceStates::eCopySource  );
            BeginTrackingState( dest, ResourceStates::eCopyDest  );
            CommitBarriers();
        }

        VkBufferImageCopy region{
            // Start of data in the buffer (in bytes).
            // Must be a multiple of the pixel size (e.g., 4 bytes for R32_UINT).
            .bufferOffset = 0,

            // Number of pixels in a row for buffer layout.
            // Set to 0 if data is tightly packed with no extra padding.
            .bufferRowLength = 0,

            // Number of rows in the buffer for 3D layout padding.
            // Set to 0 if data is tightly packed with no extra padding.
            .bufferImageHeight = 0,

            // Target texture subresource options (mip levels, array layers).
            .imageSubresource = {
                .aspectMask = checked_cast<Texture*>( src )->GetAspectMask(), // Target is color data
                .mipLevel = 0,                           // Base mipmap level
                .baseArrayLayer = 0,                     // Target the first layer
                .layerCount = 1                          // Copying exactly 1 layer
            },

            // Destination starting coordinates (x, y, z) within the image.
            .imageOffset = {0, 0, 0},

            // Size of the pixel region to copy.
            .imageExtent = {
                .width =
                src->GetWidth(),            // Width of the region in pixels
                .height = src->GetHeight(), // Height of the region in pixels
                .depth = 1                  // Depth is 1 for standard 2D textures
            }
        };

        eastl::array regions{ region };

        auto* ctx{ GetThreadContext() };

        VkImage image{ src->GetNativeHandle( ObjectType::Vk_Image ) };
        VkBuffer buffer{ dest->GetNativeHandle( ObjectType::Vk_Buffer ) };

        vkCmdCopyImageToBuffer( ctx->mCommandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, as<u32>(regions.size()), regions.data());
    }

    auto CommandList::BeginRendering( GraphicsState& state ) -> void {
        bool hasColorTarget{ !state.mCurrentRenderTargets.empty() };
        bool hasDepthTarget{ !state.mDepthTarget.mRenderTarget.IsEmpty() };

        MKT_ASSERT( hasColorTarget || hasDepthTarget, "Must provide either depth target or color target(s)" );

        if (mEnableAutomaticBarriers) {
            for (auto& rt : state.mCurrentRenderTargets ) {
                if (rt.mRenderTarget->GetResourceState() != ResourceStates::eRenderTarget) {
                    BeginTrackingState( rt.mRenderTarget.GetRaw(), ResourceStates::eRenderTarget );
                }
            }

            if (!state.mDepthTarget.mRenderTarget.IsEmpty() && state.mDepthTarget.mRenderTarget->GetResourceState() != ResourceStates::eDepthWrite) {
                BeginTrackingState( state.mDepthTarget.mRenderTarget.GetRaw(), ResourceStates::eDepthWrite );
            }

            CommitBarriers();
        }

        eastl::fixed_vector<VkRenderingAttachmentInfo, kMaxRenderTargets> colorImages{};

        for (const auto& renderTargetProps: state.mCurrentRenderTargets) {
            const Texture* texture{ checked_cast<const Texture*>(renderTargetProps.mRenderTarget.GetRaw()) };

            VkAttachmentLoadOp loadOp{ renderTargetProps.mLoadOp == LoadOp::eClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD };
            VkRenderingAttachmentInfo &colorAttachment{ colorImages.emplace_back( VkRenderingAttachmentInfo{} ) };
            colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            colorAttachment.imageView = texture->GetRenderView( renderTargetProps.mSubresourceSet.mBaseMipLevel, renderTargetProps.mSubresourceSet.mBaseArraySlice );
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
        renderingInfo.pNext = nullptr;
        renderingInfo.renderArea = {
            { state.mRenderArea.mMinX, state.mRenderArea.mMinY },
            { (u32)state.mRenderArea.ComputeWidth(), (u32)state.mRenderArea.ComputeHeight() } };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = as<u32>( colorImages.size() );
        renderingInfo.pColorAttachments = colorImages.data();
        renderingInfo.pDepthAttachment = !hasDepthTarget ?
            nullptr : MKT_ADDRESSOF( depthAttachment );

        auto* ctx{ GetThreadContext() };
        vkCmdBeginRendering( ctx->mCommandBuffer, std::addressof( renderingInfo ) );

        ctx->mIsRenderScopeActive = true;
    }

    auto CommandList::EndRendering() -> void {
        auto* ctx{ GetThreadContext() };
        vkCmdEndRendering( ctx->mCommandBuffer );

        ctx->mIsRenderScopeActive = false;
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

        auto* ctx{ GetThreadContext() };
        vkCmdBindPipeline( ctx->mCommandBuffer, bindPoint, pipeline->GetNativeHandle( ObjectType::Vk_Pipeline ) );
    }

    auto CommandList::SetViewport( eastl::span<const Viewport> viewports ) -> void {
        eastl::fixed_vector<VkViewport , kMaxViewports> vkViewports{};
        for (const auto& viewport : viewports) {
            VkViewport value{
                .x = viewport.mMinX,
                .y = viewport.mMinY,
                .width = viewport.GetWidth(),
                .height = viewport.GetHeight(),
                .minDepth = viewport.mMinZ,
                .maxDepth = viewport.mMaxZ,
            };

            // Vulkan is flipped by default
            // Rest of API supported by my RHI work just fine with OpenGL
            // If I do not do this models appear upside down
            // https://www.saschawillems.de/blog/2019/03/29/flipping-the-vulkan-viewport/
            if (!viewport.mFlip) {
                value.x = viewport.mMinX;
                value.y = viewport.GetHeight();
                value.width = viewport.GetWidth();
                value.height = -value.y;
                value.minDepth = viewport.mMinZ;
                value.maxDepth = viewport.mMaxZ;
            }

            vkViewports.emplace_back( value );
        }

        auto* ctx{ GetThreadContext() };

        vkCmdSetViewport( ctx->mCommandBuffer, 0, as<u32>( vkViewports.size() ), vkViewports.data() );
    }

    auto CommandList::SetScissors( eastl::span<const Rect> scissorRects ) -> void {
        eastl::fixed_vector<VkRect2D, kMaxScissors> scissors{};

        for (const auto& scissor : scissorRects) {
            scissors.emplace_back( VkRect2D{
                .offset = { scissor.mMinX, scissor.mMinY },
                .extent =  { as<u32>( scissor.ComputeWidth() ), as<u32>( scissor.ComputeHeight() ) }
            } );
        }

        auto* ctx{ GetThreadContext() };
        vkCmdSetScissor( ctx->mCommandBuffer, 0, as<u32>(scissors.size()), scissors.data() );
    }

    auto CommandList::SetViewportState( const ViewportState &vs ) -> void {
        SetViewport( vs.mViewports );
        SetScissors( vs.mScissorRects );
    }

    auto CommandList::BindIndexBuffer( IBuffer *buffer ) -> void {
        auto* ctx{ GetThreadContext() };
        vkCmdBindIndexBuffer( ctx->mCommandBuffer, buffer->GetNativeHandle( ObjectType::Vk_Buffer ), 0, GetIndexType(buffer->GetFormat()) );
    }

    auto CommandList::BindIndirectBuffer( IBuffer* buffer ) -> void {
        auto* ctx{ GetThreadContext() };
        ctx->mIndirectBuffer = buffer;
    }

    auto CommandList::BindVertexBuffer( const VertexBufferBinding& binding ) -> void {
        const std::array<VkDeviceSize, 1> offsets{ binding.mOffset };
        const std::array<VkBuffer, 1> vertexBuffers{ binding.mBuffer->GetNativeHandle( ObjectType::Vk_Buffer ) };

        auto* ctx{ GetThreadContext() };

        vkCmdBindVertexBuffers( ctx->mCommandBuffer, binding.mSlot, 1, vertexBuffers.data(), offsets.data() );
    }

    auto CommandList::BindPipelineResources( const BindResourcesDescription& desc ) -> void {
        VkPipelineBindPoint bindPoint{ VK_PIPELINE_BIND_POINT_MAX_ENUM  };
        switch ( desc.mBindPoint ) {
            case PipelineType::eGraphics:
                bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                break;
            case PipelineType::eCompute:
                bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
                break;
            default:
                break;
        }

        MKT_ASSERT( bindPoint != VK_PIPELINE_BIND_POINT_MAX_ENUM, "Invalid pipeline bind point" );

        auto* ctx{ GetThreadContext() };

        if (desc.mPushConstantSize != 0) {
            VkShaderStageFlags pcShaderStages{ GetShaderStageFlags( desc.mPushConstantVisibility ) };

            vkCmdPushConstants(
                 ctx->mCommandBuffer,
                desc.mPipelineLayout->GetNativeHandle( ObjectType::Vk_PipelineLayout ),
                pcShaderStages,
                0,
                as<u32>(desc.mPushConstants.size()),
                desc.mPushConstants.data());
        }

        VkPipelineLayout layout{ desc.mPipelineLayout->GetNativeHandle( ObjectType::Vk_PipelineLayout ) };

        for (const auto& resourceSet : desc.mResourceSets) {
            // Get the interface because descriptor tables can also be IBindingSet
            const IBindingSet* set{ checked_cast<const IBindingSet*>( resourceSet.second.mResourceSet ) };

            std::array<VkDescriptorSet, 1> sets{ set->GetNativeHandle( ObjectType::Vk_DescriptorSet ) };

            vkCmdBindDescriptorSets(
                ctx->mCommandBuffer,
                bindPoint,
                layout,
                resourceSet.first,
                as<u32>(sets.size()),
                sets.data(),
                0,
                nullptr );
        }
    }

    auto CommandList::Copy( ITexture* src, const TextureSlice& srcSlice, ITexture* dest, const TextureSlice& destSlice ) -> void {
        const auto srcTexture{ checked_cast<Texture *>( src ) };
        const auto dstTexture{ checked_cast<Texture *>( dest ) };

        MKT_ASSERT( srcTexture != nullptr, "Source Vulkan texture cannot be null" );
        MKT_ASSERT( dstTexture != nullptr, "Destination Vulkan texture cannot be null" );

        const VkImage srcImage{ srcTexture->GetNativeHandle( ObjectType::Vk_Image ) };
        const VkImage dstImage{ dstTexture->GetNativeHandle( ObjectType::Vk_Image ) };

        MKT_ASSERT( srcImage != VK_NULL_HANDLE, "Source Vulkan image is null" );
        MKT_ASSERT( dstImage != VK_NULL_HANDLE, "Destination Vulkan image is null" );

        if (mEnableAutomaticBarriers) {
            BeginTrackingState(srcTexture, ResourceStates::eCopySource);
            BeginTrackingState(dstTexture, ResourceStates::eCopyDest);
            CommitBarriers();
        }

        const bool sameWidth{ srcTexture->GetWidth() == dstTexture->GetWidth() };
        const bool sameHeight{ srcTexture->GetHeight() == dstTexture->GetHeight() };
        const bool sameSize{ sameWidth && sameHeight };
        const bool sameFormat{ srcTexture->GetFormat() == dstTexture->GetFormat() };

        auto* ctx{ GetThreadContext() };

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

            // Requires vulkan 1.3
            vkCmdCopyImage2( ctx->mCommandBuffer, MKT_ADDRESSOF( copyInfo ) );
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

            vkCmdBlitImage2( ctx->mCommandBuffer, MKT_ADDRESSOF( blitInfo ) );
        }
    }

    auto CommandList::WriteVolatile( IBuffer* target, size_t dstOffset, const void* data, size_t byteSize ) -> void {
        if (mArenasAllocators.empty()) {
            InitializeArenaAllocators();
        }

        auto* ctx{ GetThreadContext() };

        // You cannot record transfer operations inside rendering
        if (ctx->mIsRenderScopeActive) {
            EndRendering();
        }

        if (mEnableAutomaticBarriers) {
            SetResourceState( target, ResourceStates::eCopyDest );
        }

        auto& allocator{ mArenasAllocators[mCurrentVolatileVersion] };
        auto allocation{ allocator->Allocate( byteSize, 1 ) };

        Buffer* staging{ checked_cast<Buffer*>( mArenasAllocators[mCurrentVolatileVersion]->GetBuffer().GetRaw() ) };

        SetResourceState( staging, ResourceStates::eCopySource );

        std::memcpy( as<byte_t*>(staging->GetMappedAddress()) + allocation->mOffset, data, byteSize );

        VkBufferCopy copy{};
        copy.srcOffset = allocation->mOffset;
        copy.dstOffset = dstOffset;
        copy.size = byteSize;

        vkCmdCopyBuffer(
            ctx->mCommandBuffer,
            staging->GetNativeHandle( ObjectType::Vk_Buffer ),
            target->GetNativeHandle( ObjectType::Vk_Buffer ),
            1,
            &copy
        );
    }

    auto CommandList::Dispatch( u32 x, u32 y, u32 z ) -> void {
        auto* ctx{ GetThreadContext() };
        vkCmdDispatch(ctx->mCommandBuffer, x, y, z);
    }

    auto CommandList::GetNativeHandle( ObjectType type ) -> Object {
        if ( type != ObjectType::Vk_CmdBuffer ) {
            return Object( nullptr );
        }

        return Object( mPrimaryCommandBuffer );
    }

    auto CommandList::GetNativeHandle( ObjectType type ) const -> Object {
        if ( type != ObjectType::Vk_CmdBuffer ) {
            return Object( nullptr );
        }

        return Object( mPrimaryCommandBuffer );
    }

    auto CommandList::GetPool() const -> const CommandPool* {
        return mCommandPool;
    }

    CommandList::~CommandList() {
        if (mIsAllocated) {
            CommandList::Release();
        }
    }

    auto CommandList::Initialize() -> void {
        mUploadManager = checked_cast<Device*>(mDevice)->GetUploadManager();

        mIsAllocated = true;
    }

    auto CommandList::Release() -> void {
        ClearState();

        Queue* queue{ checked_cast<Device*>( mDevice )->GetQueue( mQueueType ) };
        for (const auto& item : mThreadContexts | std::views::values  ) {
            queue->PushDelete( item->mCommandBuffer, mAllocInfo.commandPool, item->mSubmissionId );
        }

        mIsAllocated = false;
    }

    auto CommandList::InitializeArenaAllocators() -> void {
        for (u32 count{}; count < kMaxVolatileBufferVersions; count++) {
            auto bufferDes{ BufferCreateDescription{}
                .SetByteSize( kArenaInitialSize ) // Later we can specify a size for better optimization
                .SetCpuAccessType( CpuAccessType::eWrite )
                .SetHeapType( HeapType::eUpload )
                .SetResourceType( ResourceType::eInvalid ) // Is not a shader resource
                .SetBufferUsage( BufferUsageFlagsBits::kNone )
            };
            mArenasAllocators[count] =
                eastl::make_unique<memory::MemoryArena<IBuffer, LinearAllocator>>( mDevice->CreateBuffer( bufferDes ), kArenaInitialSize );
        }
    }

    auto CommandList::ClearState() -> void {
        for (auto& subAllocations : mUploadAllocations) {
            // Set it to false we to tell the allocator
            // this allocation can already be destroyed
            subAllocations->mInUse.clear();
        }

        // Reset it so we can use it next time when it is ready
        // we are not checking if it is ready just optimistic assume it is because of the versioning
        const auto it{ mArenasAllocators.find( mCurrentVolatileVersion ) };
        if (it != mArenasAllocators.end()) {
            it->second->Reset();
        }

        mCurrentVolatileVersion = mCurrentVolatileVersion % kMaxVolatileBufferVersions;

        mThreadContexts.clear();
    }

    auto CommandList::TryRecycle( IQueue* queue ) -> void {


    }

    auto CommandList::GetThreadContext( bool isSecondary ) -> CommandThreadContext* {
        // https://docs.vulkan.org/guide/latest/threading.html
        // https://docs.vulkan.org/spec/latest/chapters/fundamentals.html#fundamentals-threadingbehavior
        auto id{ std::this_thread::get_id() };

        std::scoped_lock lock{ mSecondaryCmdsAllocMutex };

        if (!mThreadContexts.contains( id )) {
            auto result{ mThreadContexts.emplace( id, eastl::make_unique<CommandThreadContext>() ) };

            if (isSecondary) {
                result.first->second->mCommandBuffer = mSecondaryCommandBuffersPool.back();
                mSecondaryCommandBuffersPool.pop_back();
            } else {
                MKT_VK_CHECK( vkAllocateCommandBuffers(
                   as<Device*>(mDevice)->GetDevice(),
                   MKT_ADDRESSOF( mAllocInfo ),
                   MKT_ADDRESSOF( result.first->second->mCommandBuffer ) ) );
            }

            auto* device{ checked_cast<Device*>( mDevice ) };
            device->SetDebugName( VK_OBJECT_TYPE_COMMAND_BUFFER, rc_cast<u64>( result.first->second->mCommandBuffer ), mDebugName );
        }

        return mThreadContexts[id].get();
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

    auto CommandPool::GetNativeHandle( ObjectType type ) const -> Object {
        if ( type != ObjectType::Vk_CmdPool ) {
            return Object( nullptr );
        }

        return Object( mPool );
    }

    auto CommandPool::SetDebugName( eastl::string_view name ) -> void {
        mDebugName = name;

        auto* device{ checked_cast<Device*>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_COMMAND_POOL, rc_cast<u64>( mPool ), mDebugName );
    }

    auto CommandPool::AllocateCmdList() -> CommandListHandle {
        MKT_BEGIN_PROFILER_NAMED();

        VkCommandBufferAllocateInfo allocInfo{ initializers::CommandBufferAllocateInfo() };
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = mPool;
        allocInfo.commandBufferCount = 1;

        CommandListHandle handle{ Ref<CommandList>::Spawn( allocInfo, mQueueType, this ) };
        handle->Initialize( mDevice );

        return handle;
    }

    auto CommandPool::AllocateCmdList( const CommandListParameters& params ) -> CommandListHandle {
        MKT_BEGIN_PROFILER_NAMED();

        VkCommandBufferAllocateInfo allocInfo{ initializers::CommandBufferAllocateInfo() };
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = mPool;
        allocInfo.commandBufferCount = 1;

        CommandListHandle handle{ Ref<CommandList>::Spawn( allocInfo, params, this ) };
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

    Queue::Queue( QueueType type, QueueOpSupportFlags opFlags, u32 queueFamilyIndex, u32 queueIndex)
        : IQueue{ type, opFlags }, mFamilyIndex{ queueFamilyIndex }, mQueueIndex{ queueIndex } {
        switch ( type ) {
            case QueueType::eTransfer:
                mSubmissionLabelColor = Color( 0.2f, 0.6f, 1.0f, 0.5f );
                break;
            case QueueType::eGraphics:
                mSubmissionLabelColor = Color( 0.2f, 0.8f, 0.2f, 0.5f );
                break;
            case QueueType::eCompute:
                mSubmissionLabelColor = Color( 0.7f, 0.3f, 0.9f, 0.5f );
                break;
            case QueueType::ePresent:
                mSubmissionLabelColor = Color( 1.0f, 0.8f, 0.0f, 0.5f );
                break;
            default:;
        }
    }

    auto Queue::Wait( IFence* fence, u64 value ) -> void {

    }

    auto Queue::Signal( IFence* fence, u64 value ) -> void {

    }

    auto Queue::ExecuteCommandLists( eastl::span<CommandListHandle> commands ) -> void {

    }

    auto Queue::Initialize() -> void {
        MKT_ASSERT(  mDevice != nullptr, "GpuDevice cannot be null" );
        auto* device{ checked_cast<Device*>(mDevice) };
        vkGetDeviceQueue( device->GetDevice(), mFamilyIndex, mQueueIndex, MKT_ADDRESSOF( mQueue ) );

        MKT_ASSERT( mQueue != VK_NULL_HANDLE, "Queue is empty" );

        mTimelineSemaphore = device->CreateTimelineSemaphore( 0 );

        TimelineSemaphore* timelineSemaphore{ checked_cast<TimelineSemaphore*>( mTimelineSemaphore.GetRaw() ) };

        // vkQueueSubmit2(): pSubmits[0].pSignalSemaphoreInfos[0].semaphore signal
        // value (0) in VkQueue 0x1dec7ccca40 must be greater than current
        // timeline semaphore VkSemaphore 0x50000000005 value (0).
        timelineSemaphore->GetAndIncrement( 1 );

        mIsAllocated = true;
    }

    auto Queue::Release() -> void {
        WaitIdle();

        RunGarbageCollection();

        mPools.clear();
        mTimelineSemaphore.Reset();

        mIsAllocated = false;
    }

    auto Queue::Flush() -> void {
        std::lock_guard lock{ mPendingSubmitMutex };

        if (mPendingSubmits.empty()) {
            return;
        }

        (void)SubmitCommands();
    }

    auto Queue::Present( const VkPresentInfoKHR& info ) -> VkResult {
        std::lock_guard lock{ mSubmissionMutex };
        return vkQueuePresentKHR( mQueue, MKT_ADDRESSOF( info ) );
    }

    auto Queue::RunGarbageCollection() -> void {
        eastl::fixed_vector<DeleteItem, 100> deleteCmds{};
        {
            std::lock_guard lock{ mDeleteCmdsMutex };
            deleteCmds.swap( mDeleteCmds );
        }

        auto complete{ GetCompletedValue() };
        auto* device{ as<Device*>(mDevice) };

        for ( auto it{ deleteCmds.begin() }; it != deleteCmds.end(); ) {
            if ( it->mFence != VK_NULL_HANDLE ) {
                VkResult status{ vkGetFenceStatus( device->GetDevice(), it->mFence ) };
                if ( status == VK_SUCCESS ) {
                    it->mFence.Destroy( device->GetDevice() );
                    it = deleteCmds.erase( it );// move to next safely
                }
            } else if ( complete >= it->mSubmissionID ) {
                vkFreeCommandBuffers(
                        device->GetDevice(),
                        it->mPool,
                        1,
                        MKT_ADDRESSOF( it->mBuffer ) );

                it = deleteCmds.erase( it );// move to next safely
            } else {
                ++it;// only increment if NOT erased
            }
        }

        {
            // push remaining stuff back. Manually push back because there might
            // be commands in mDeleteCmdsMutex from other threads. Because we do not
            // if these commands are done and can be freed
            std::lock_guard lock{ mDeleteCmdsMutex };
            for (const auto& cmd: deleteCmds ) {
                mDeleteCmds.emplace_back( cmd );
            }
        }
    }

    auto Queue::ExecuteCommandList( CommandListHandle cmd ) -> void {
        MKT_ASSERT(!cmd.IsEmpty(), "Command List cannot be empty");

        std::vector<VkCommandBufferSubmitInfo> cmds{ VkCommandBufferSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ),
            .deviceMask = 0
        }};

        VkSubmitInfo2 submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .commandBufferInfoCount = as<u32>( cmds.size() ),
            .pCommandBufferInfos = cmds.data(),
        };

        {
            std::lock_guard lock{ mSubmissionMutex };

            auto* device{ checked_cast<Device*>(mDevice) };

            // TODO: use a single timeline instead of this of immediate submissions
            FencePlain fence{};
            VkFenceCreateInfo fenceInfo{ initializers::FenceCreateInfo( MKT_VK_FLAGS_NONE ) };
            fence.Create( fenceInfo, device->GetDevice() );

            MKT_VK_CHECK( vkQueueSubmit2( mQueue, 1, &submitInfo, fence.mFence ) );

            CommandList* cmdList{  checked_cast<CommandList*>( cmd.GetRaw() ) };
            PushDelete( cmdList->GetNativeHandle( ObjectType::Vk_CmdBuffer ),
                cmdList->GetPool()->GetNativeHandle( ObjectType::Vk_CmdPool ), fence );
        }
    }

    auto Queue::SubmitCommandList( CommandListHandle cmd ) -> u64 {
        MKT_ASSERT(!cmd.IsEmpty(), "Command List cannot be empty");

        std::lock_guard lock{ mPendingSubmitMutex };

        VkCommandBuffer commandBuffer{ cmd->GetNativeHandle( ObjectType::Vk_CmdBuffer ) };
        mPendingSubmits.emplace_back( commandBuffer );

        CommandList* cmdList{  checked_cast<CommandList*>( cmd.GetRaw() ) };

        TimelineSemaphore* timeline{ checked_cast<TimelineSemaphore*>(mTimelineSemaphore.GetRaw()) };
        u64 currentTimeline{ timeline->GetCurrentID() };

        PushDelete( commandBuffer, cmdList->GetPool()->GetNativeHandle( ObjectType::Vk_CmdPool ), currentTimeline );

        return currentTimeline; // ID of the batch it will belong to
    }

    auto Queue::AllocateCmdList() -> CommandListHandle {
        CommandPoolHandle pool{ AcquireThreadCmdPool() };
        return pool->AllocateCmdList();
    }

    auto Queue::AllocateCmdList( const CommandListParameters& params ) -> CommandListHandle {
        CommandPoolHandle pool{ AcquireThreadCmdPool() };
        return pool->AllocateCmdList( params );
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

    auto Queue::PushDelete( VkCommandBuffer cmd, VkCommandPool pool, const FencePlain& fence ) -> void {
        std::lock_guard lock{ mDeleteCmdsMutex };
        mDeleteCmds.emplace_back( DeleteItem {
            .mFence = fence,
            .mPool = pool,
            .mBuffer = cmd
        } );
    }

    auto Queue::AddQueueWaitFence( FencePlain* semaphore ) -> void {
        // TODO: Unused for now as i can use timeline for this too
    }

    auto Queue::AddQueueSignalSemaphore( Fence* semaphore, u64 value, VkPipelineStageFlags2 stageFlags ) -> void {
        mSignalInfos.emplace_back( VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = semaphore->GetNativeHandle( ObjectType::Vk_Semaphore ),
            .value = value,
            .stageMask = stageFlags,
            .deviceIndex = 0 } );
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

    Queue::~Queue() {
        if (mIsAllocated) {
            Release();
        }
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
        result->second->SetDebugName( string::Format( "CommandPool QueueType {}, ThreadID: {}", GetQueueName( mType ), threading::GetHashedID(id) ) );

        return result->second;
    }

    auto Queue::SubmitCommands() -> u64 {
        if ( mPendingSubmits.empty() ) {
            return 0;
        }

        auto* timeline{ checked_cast<TimelineSemaphore*>( mTimelineSemaphore.GetRaw() ) };

        const u64 submissionID{ timeline->GetCurrentID() };

        // --- Command buffers ---
        eastl::fixed_vector<VkCommandBufferSubmitInfo, kMaxSubmits> cmdInfos{};

        for ( auto& cmd: mPendingSubmits ) {
            cmdInfos.emplace_back( VkCommandBufferSubmitInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext = nullptr,
                .commandBuffer = cmd,
                .deviceMask = 0 } );
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
            mSubmissionLabel = string::Format( "Queue {} SubmissionID: {}", GetQueueName(mType).data(), submissionID ).c_str();
            VkDebugUtilsLabelEXT labelInfo = {};
            labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            labelInfo.pLabelName = mSubmissionLabel.c_str();
            labelInfo.color[0] = mSubmissionLabelColor.mR; // Optional: set a custom color (RGBA)
            labelInfo.color[1] = mSubmissionLabelColor.mG;
            labelInfo.color[2] = mSubmissionLabelColor.mB;
            labelInfo.color[3] = mSubmissionLabelColor.mA;

            std::lock_guard lock{ mSubmissionMutex };

            vkQueueBeginDebugUtilsLabelEXT(mQueue, &labelInfo);
            MKT_VK_CHECK( vkQueueSubmit2( mQueue, 1, &submitInfo, VK_NULL_HANDLE ) );

            vkQueueEndDebugUtilsLabelEXT(mQueue);
        }

        mWaitInfos.clear();
        mSignalInfos.clear();

        mPendingSubmits.clear();

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

    MKT_NODISCARD auto IsMemoryError( VkResult errorResult ) -> bool {
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

        u32 kInitialPoolSize{ 20000 };

        std::mutex mPoolMutex{};

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

    auto DescriptorAllocatorHandle::Return() -> void {
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
                foundAllocator = true;
            }
        }
        //need a new pool
        if ( !foundAllocator ) {
            //static pool has to be free-able
            VkDescriptorPoolCreateFlags flags{ VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT };
            if ( poolIndex == 0 ) {
                flags |= VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            }

            VkDescriptorPool newPool = CreatePool( (i32)kInitialPoolSize, flags );

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
        return mTimeline.fetch_add( value );
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
        size_t initialSize{ MKT_MEGABYTES( 512 ) };

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

    auto FencePlain::Create( const VkFenceCreateInfo &info, VkDevice device ) -> void {
        MKT_VK_CHECK( vkCreateFence( device, MKT_ADDRESSOF( info ), nullptr, MKT_ADDRESSOF( mFence ) ) );
    }

    auto FencePlain::Destroy( VkDevice device ) -> void {
        vkDestroyFence(  device, mFence, nullptr );
    }

    FencePlain::operator VkFence() const noexcept {
        return mFence;
    }

    Fence::Fence( u64 initialValue )
        : mTimeline{ initialValue } {}

    auto Fence::GetCompletionValue() const -> u64 {
        auto* device{ checked_cast<Device*>(mDevice) };

        u64 value{};
        MKT_VK_CHECK(vkGetSemaphoreCounterValue(device->GetDevice(), mSemaphore, MKT_ADDRESSOF( value )));

        return value;
    }

    auto Fence::SetDebugName( eastl::string_view name ) -> void {
        mDebugName = name;

        auto* device{ checked_cast<Device*>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_SEMAPHORE, rc_cast<u64>( mSemaphore ), mDebugName );
    }

    auto Fence::GetNativeHandle( ObjectType object ) -> Object {
        switch ( object ) {
            case ObjectType::Vk_Semaphore: return Object( mSemaphore );
            default:;
        }

        return Object( nullptr );
    }

    auto Fence::GetNativeHandle( ObjectType object ) const -> Object {
        switch ( object ) {
            case ObjectType::Vk_Semaphore: return Object( mSemaphore );
            default:;
        }

        return Object( nullptr );
    }

    auto Fence::GetCurrentID() const -> u64 {
        return mTimeline.load();
    }

    auto Fence::GetAndIncrement( u64 value ) -> u64 {
        return mTimeline.fetch_add( value );
    }

    Fence::~Fence() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto Fence::Initialize() -> void {
        auto* device{ checked_cast<Device*>( mDevice ) };

        VkSemaphoreTypeCreateInfo typeCreateInfo{ initializers::SemaphoreTypeCreateInfo() };
        typeCreateInfo.initialValue = mTimeline.load();

        VkSemaphoreCreateInfo createInfo{ initializers::SemaphoreCreateInfo() };
        createInfo.pNext = MKT_ADDRESSOF( typeCreateInfo );

        MKT_VK_CHECK( vkCreateSemaphore( device->GetDevice(), MKT_ADDRESSOF( createInfo ), nullptr, MKT_ADDRESSOF( mSemaphore ) ) );

        mIsAllocated = true;
    }

    auto Fence::Release() -> void {
        auto* device{ checked_cast<Device*>( mDevice ) };
        vkDestroySemaphore(  device->GetDevice(), mSemaphore, nullptr );

        mIsAllocated = false;
    }

    auto DescriptorWriter::WriteSampler( u32 binding, VkSampler sampler, u32 arrayIndex ) -> DescriptorWriter& {
        VkDescriptorImageInfo& info{ mImageInfos.emplace_back(VkDescriptorImageInfo{
            .sampler = sampler,
            .imageView = VK_NULL_HANDLE,
            .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED })
        };

        VkWriteDescriptorSet write{ initializers::WriteDescriptorSet() };

        write.dstBinding = binding;
        write.dstSet = VK_NULL_HANDLE; //left empty for now until we to write it  in the updateSet()
        write.descriptorCount = 1;
        write.dstArrayElement = arrayIndex;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        write.pImageInfo = MKT_ADDRESSOF( info );

        mWrites.push_back(write);

        return *this;
    }

    auto DescriptorWriter::WriteBuffer( u32 binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type, u32 arrayIndex ) -> DescriptorWriter& {
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
        write.dstArrayElement = arrayIndex;
        write.pBufferInfo = std::addressof( info );

        mWrites.push_back(write);

        return *this;
    }

    auto DescriptorWriter::WriteImage( u32 binding, VkImageView image, VkDescriptorType type, VkImageLayout layout, u32 arrayIndex ) -> DescriptorWriter& {
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
        write.dstArrayElement = arrayIndex;
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

    auto BindingLayout::GetBindlessLayoutDesc() const -> const BindlessLayoutDescription& {
        return mBindlessLayoutDesc;
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
                binding.descriptorType = GetDescriptorType(item.mType);
                binding.descriptorCount = item.mMaxCapacity;
                binding.stageFlags = GetShaderStageFlags(mBindlessLayoutDesc.mStageVisibility);
                binding.pImmutableSamplers = nullptr;

                bindings.emplace_back(binding);

                VkDescriptorBindingFlags bindingFlags =
                    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

                flags.emplace_back(bindingFlags);
            }

            // I think Vulkan expects them in order even if you specify
            // bindings[0] for binding at binding 0, etc...
            eastl::sort(bindings.begin(), bindings.end(), [](const auto& a, const auto& b) {
                return a.binding < b.binding;
            } );

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
                binding.descriptorType = GetDescriptorType(item.mType);
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
                            GetDescriptorType(item.mType),
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
                            GetDescriptorType(item.mType) );
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

    DescriptorTable::DescriptorTable( BindingLayoutHandle setLayout )
        : mBindingLayout{ eastl::move( setLayout ) }
    {}

    auto DescriptorTable::SetDebugName( eastl::string_view name ) -> void {
        auto* device{ checked_cast<Device*>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_DESCRIPTOR_SET, rc_cast<u64>( mDescriptorSet ), mDebugName );
    }

    auto DescriptorTable::GetCapacity( u32 slot ) const -> u32 {
        const BindingLayout* layout{ checked_cast<const BindingLayout*>( mBindingLayout.GetRaw() ) };
        const auto& blDesc{ layout->GetBindlessLayoutDesc() };

        const auto it{ eastl::find_if( blDesc.mSlots.begin(), blDesc.mSlots.end(),
            [slot](const BindlessLayoutItem& item) {
                return item.mSlot == slot;
            }) };

        return it != blDesc.mSlots.end() ? it->mMaxCapacity : 0;
    }

    auto DescriptorTable::GetResourceSlot( ResourceType type ) const -> i32 {
        const auto it{ mSlotResourceType.find( type ) };
        if (it != mSlotResourceType.end() ) {
            return it->second;
        }

        return -1;
    }

    DescriptorTable::~DescriptorTable() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto DescriptorTable::GetNativeHandle( ObjectType type ) -> Object {
        if ( type != ObjectType::Vk_DescriptorSet ) {
            return Object( nullptr );
        }

        return Object( mDescriptorSet );
    }

    auto DescriptorTable::GetNativeHandle( ObjectType type ) const -> Object {
        if ( type != ObjectType::Vk_DescriptorSet ) {
            return Object( nullptr );
        }

        return Object( mDescriptorSet );
    }

    auto DescriptorTable::Initialize() -> void {
        auto* device{ checked_cast<Device*>( mDevice ) };
        auto* layout{ checked_cast<BindingLayout*>( mBindingLayout.GetRaw() ) };

        mDescriptorAllocatorHandle = device->GetDescriptorAllocator();
        if( mDescriptorAllocatorHandle.Allocate(layout->GetNativeHandle( ObjectType::Vk_DescriptorSetLayout ),mDescriptorSet) ) {
            for (const auto& item : layout->GetBindlessLayoutDesc().mSlots) {
                // This resource type is at binding mSlot
                // Allows for stuff like the following (same set different bindings)
                // vk::binding(0, 1) Texture2D textures[];
                // vk::binding(1, 1) SamplerState samplers[];
                mSlotResourceType[item.mType] = item.mSlot;
            }

            mIsAllocated = true;
        }
    }

    auto DescriptorTable::Release() -> void {
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
