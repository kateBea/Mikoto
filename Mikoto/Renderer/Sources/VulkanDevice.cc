//    Copyright 2026 ケイト
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

#include <array>
#include <bit>
#include <cstring>
#include <ranges>
#include <vector>

#include <vk_mem_alloc.h>
#include <volk.h>

#if MIKOTO_ENABLE_OIDN
#include <vk_oidn/vk_oidn.h>
#endif

#include <EASTL/algorithm.h>
#include <EASTL/array.h>
#include <EASTL/sort.h>
#include <EASTL/unordered_map.h>
#include <EASTL/unordered_set.h>
#include <EASTL/utility.h>
#include <EASTL/vector.h>

#if defined( MIKOTO_ENABLE_TRACY )
#include <tracy/TracyVulkan.hpp>
#endif

#include <Core/Core.hh>
#include <Core/Exception.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Types.hh>

#include <Filesystem/FileService.hh>
#include <Filesystem/FileSystem.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Math/Math.hh>
#include <Math/Random.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Rhi/GpuAllocator.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Rhi/Vulkan/VulkanAccelerationStructure.hh>
#include <Renderer/Rhi/Vulkan/VulkanContext.hh>
#include <Renderer/Rhi/Vulkan/VulkanDenoiser.hh>
#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanHelpers.hh>
#include <Renderer/Rhi/Vulkan/VulkanPipeline.hh>
#include <Renderer/Rhi/Vulkan/VulkanQueryPool.hh>
#include <Renderer/Rhi/Vulkan/VulkanTexture.hh>
#include <Renderer/Rhi/Vulkan/VulkanVideoDecoder.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::core;
    using namespace mikoto::memory;
    using namespace mikoto::filesystem;
    using namespace mikoto::renderer::rhi;

#if defined( MIKOTO_ENABLE_TRACY )
    struct CommandList::GpuProfileZone {
        eastl::optional<tracy::VkCtxScope> mScope{};
    };

    struct TracyCalibrationPool {
        VkDevice mDevice{};
        VkCommandPool mPool{};

        ~TracyCalibrationPool();
    };

    TracyCalibrationPool::~TracyCalibrationPool() {
        if ( mPool ) {
            vkDestroyCommandPool( mDevice, mPool, nullptr );
        }
    }
#endif

#if MIKOTO_DEBUG

    static constexpr auto RenderAreaFits( core::i32 left, core::i32 top, core::i32 right, core::i32 bottom,
        core::u32 width, core::u32 height ) -> bool {
        return left >= 0 && top >= 0 && right > left && bottom > top &&
               static_cast<core::u32>( right ) <= width && static_cast<core::u32>( bottom ) <= height;
    }

#endif

    MKT_NODISCARD static auto IsMemoryError( VkResult errorResult ) -> bool {
        MKT_BEGIN_PROFILER();

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

        auto ReturnAllocator( DescriptorAllocatorHandle &handle, bool bIsFull ) -> void;

        MKT_NODISCARD auto GetAllocator() -> DescriptorAllocatorHandle override;
        MKT_NODISCARD auto CreatePool( int count, VkDescriptorPoolCreateFlags flags ) -> VkDescriptorPool;

        VkDevice mDevice{};
        PoolSizes mPoolSizes;
        core::i32 mFrameIndex{};
        core::i32 mMaxFrames{};

        std::mutex mPoolMutex{};

        core::u32 kInitialPoolSize{ 20000 };

        // fully cleared allocators
        eastl::vector<DescriptorAllocator> mClearAllocators{};

        // zero is for static pool, next is for frame indexing
        eastl::vector<std::unique_ptr<PoolStorage>> mDescriptorPools{};
    };


    Device::Device( const renderer::rhi::GpuDeviceCreateInfo &createInfo )
        : mApi{ createInfo.mApi }, mDeviceType{ createInfo.mDeviceType }, mRequestedFeatures{ createInfo.mFeatureSupportFlags } {
    }

    auto Device::GetGraphicsApi() const -> renderer::rhi::GraphicsAPI {
        return mApi;
    }

    auto Device::IsInitialized() const -> bool {
        return mIsInitialized;
    }

    auto Device::IsGraphicsApi( renderer::rhi::GraphicsAPI api ) const -> bool {
        return mApi == api;
    }

    auto Device::IsFeatureSupported( GpuFeatureSupportFlags supportFlags ) const -> bool {
        return ( mSupportedFeatures & supportFlags ) == supportFlags;
    }

    auto Device::IsFeatureRequested( rhi::GpuFeatureSupportFlags requestFlags ) const -> bool {
        return ( mRequestedFeatures & requestFlags ) == requestFlags;
    }

    auto Device::GetDeviceName() const -> eastl::string_view {
        return mDeviceName;
    }

    auto Device::GetVendorID() const -> eastl::string_view {
        return mVendorID;
    }

    auto Device::GetDriverVersion() const -> eastl::string_view {
        return mDriverVersion;
    }

    auto Device::Initialize() -> void {
        MKT_CORE_LOGGER_INFO( "VulkanDevice::Init - Initializing Vulkan Device." );

        // Prepare context info
        Context *pContext{ core::checked_cast<Context *>( RenderSystem::Get()->GetContext() ) };
        Instance &pInstance{ pContext->GetInstance() };

        // Choose primary physical device
        if ( IsFeatureRequested( renderer::rhi::GpuFeatureSupportFlagsBits::WindowPresentation ) ) {
            mSurface = pInstance.mSurface;
            mExtensions.emplace_back( VK_KHR_SWAPCHAIN_EXTENSION_NAME );
        }

        const auto pdIt{
            std::ranges::find_if(
                pInstance.mPhysicalDevices,
                [this]( PhysicalDevice &dev ) {
                    return IsDeviceSuitable( dev );
                } )
        };

        if ( pdIt != pInstance.mPhysicalDevices.end() ) {
            // Once initialized the instance physical device list never mutates
            mPhysicalDevice = MKT_ADDRESSOF( *pdIt );
            mSupportedFeatures = mPhysicalDevice->GetSupportedFeatures();

            mDeviceName = mPhysicalDevice->mProperties.deviceName;
            mVendorID = string::Format( "{}", mPhysicalDevice->mProperties.vendorID );
            mDriverVersion = string::Format( "{}", mPhysicalDevice->mProperties.driverVersion );
        }
        else {
            MKT_CORE_LOGGER_INFO( "No suitable physical device with the desired features." );
            return;
        }

        InitLogicalDevice();
        InitLogicalQueues();
        InitMemoryAllocator();
        InitDescriptorAllocator();

        InitPipelineCache();
        InitDummyResources();

        mDlssContext.Initialize( pInstance.mInstance, mPhysicalDevice->mPhysicalDevice,
            mLogicalDevice, mDlssExtensionsReady );
        mIsInitialized = true;
    }

    auto Device::Shutdown() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "VulkanDevice Shutting down Vulkan Device." );

        WaitIdle();

        DestroyDummyResources();

        SerializePipelineCache();

        // Clear descriptor manager
        mDescriptorAllocatorPool.reset();

        // Clear upload manager
        mUploadManager.reset();

        for ( auto &queue: mQueues | std::ranges::views::values ) {
            queue.Reset();
        }

        // Command-list retirement releases per-view NGX features. Drain its SDK
        // retirement worker while the logical device and allocator still exist.
        mDlssContext.Shutdown();

        mGpuAllocator->Shutdown();

        // Destroy the device
        vkDestroyDevice( mLogicalDevice, nullptr );

        mIsInitialized = false;
    }

    auto Device::CreateBuffer( const renderer::rhi::BufferCreateDescription &description ) -> renderer::rhi::BufferHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::BufferHandle buffer{ core::Ref<Buffer>::New( description ) };

        if ( buffer.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate buffer resource." );
            return renderer::rhi::BufferHandle::CreateEmpty();
        }

        buffer->Initialize( this );

        return buffer;
    }

    auto Device::CreateBuffer( const BufferCreateDescription &description, const VkVideoProfileInfoKHR &profile ) -> BufferHandle {
        MKT_BEGIN_PROFILER();
        auto buffer{ Ref<Buffer>::New( description ) };

        if ( buffer.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate video buffer resource." );
            return BufferHandle::CreateEmpty();
        }

        buffer->Initialize( this, profile );
        return buffer;
    }

    auto Device::CreateTexture( const renderer::rhi::TextureCreateDescription &description ) -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::TextureHandle texture{ core::Ref<Texture>::New( description ) };

        if ( texture.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice - Failed to allocate texture" );
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        texture->Initialize( this );

        return texture;
    }

    auto Device::CreateTextureNative( renderer::rhi::ObjectType type, renderer::rhi::Object object, const renderer::rhi::TextureCreateDescription &description ) -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::TextureHandle::CreateEmpty();
    }

    auto Device::CreateSampler( const renderer::rhi::SamplerCreateDescription &description ) -> renderer::rhi::SamplerHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::SamplerHandle sampler{ core::Ref<Sampler>::New( description ) };

        if ( sampler.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate sampler resource." );
            return renderer::rhi::SamplerHandle::CreateEmpty();
        }

        sampler->Initialize( this );

        return sampler;
    }


    auto Device::CreateShader( const renderer::rhi::ShaderModuleCreateDescription &desc ) -> renderer::rhi::ShaderModuleHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::ShaderModuleHandle result{ core::Ref<Shader>::New( desc ) };

        if ( result.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to create shader." );
            return renderer::rhi::ShaderModuleHandle::CreateEmpty();
        }

        result->Initialize( this );

        return result;
    }


    auto Device::CreateVideoDecoder( const VideoDecoderDescription &description ) -> VideoDecoderHandle {
        if ( !GetQueue( QueueType::eVideoDecode ) || !description.mConfiguration ) {
            return {};
        }

        VideoDecoderHandle decoder{ Ref<VideoDecoder>::New( description ) };
        if ( decoder.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate video decoder." );
            return renderer::rhi::VideoDecoderHandle::CreateEmpty();
        }

        decoder->Initialize( this );
        return decoder;
    }

    auto Device::CreateInputLayout( const renderer::rhi::InputLayoutCreateDescription &desc ) -> renderer::rhi::InputLayoutHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::InputLayoutHandle layout{ core::Ref<InputLayout>::New( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return renderer::rhi::InputLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreateBindingLayout( const renderer::rhi::BindingLayoutDescription &desc ) -> renderer::rhi::BindingLayoutHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::BindingLayoutHandle layout{ core::Ref<BindingLayout>::New( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return renderer::rhi::BindingLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreatePipelineLayout( const renderer::rhi::PipelineLayoutCreateDescription &desc ) -> renderer::rhi::PipelineLayoutHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::PipelineLayoutHandle layout{ core::Ref<PipelineLayout>::New( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate pipeline layout resource." );
            return renderer::rhi::PipelineLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreateBindingTable( const renderer::rhi::BindingTableDescription &desc, renderer::rhi::BindingLayoutHandle layout ) -> renderer::rhi::BindingTableHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::BindingTableHandle set{ core::Ref<BindingTable>::New( desc, layout ) };

        if ( set.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding set resource." );
            return renderer::rhi::BindingTableHandle::CreateEmpty();
        }

        set->Initialize( this );

        return set;
    }

    auto Device::UpdateBindingTable( const rhi::BindingTableDescription &desc, rhi::BindingTableHandle table ) -> void {
    }

    auto Device::CreateSwapChain( const renderer::rhi::SwapChainDescription &description ) -> rhi::SwapChainHandle {
        MKT_BEGIN_PROFILER();
        return rhi::SwapChainHandle::CreateEmpty();
    }

    auto Device::CreateFence( core::u64 fenceInitialValue ) -> renderer::rhi::FenceHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::FenceHandle fence{ core::Ref<Fence>::New( fenceInitialValue ) };

        if ( fence.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate fence resource." );
            return renderer::rhi::FenceHandle::CreateEmpty();
        }

        fence->Initialize( this );

        return fence;
    }

    auto Device::UnMap( renderer::rhi::IBuffer *buffer ) -> void {
        MKT_BEGIN_PROFILER();
        Buffer *b{ core::checked_cast<Buffer *>( buffer ) };

        if ( b->IsMapped() ) {
            b->PersistentUnmap();
        }
    }

    auto Device::Map( renderer::rhi::IBuffer *buffer ) -> void * {
        MKT_BEGIN_PROFILER();
        Buffer *b{ core::checked_cast<Buffer *>( buffer ) };

        if ( !b->IsMapped() ) {
            b->PersistentMap();
        }

        // Callers must first wait for the GPU copy's fence. Noncoherent heaps also need invalidation.
        b->InvalidateHostReads();
        return b->GetMappedAddress();
    }

    auto Device::ReadTextureRegion( const rhi::ITexture *, const rhi::Rect &, eastl::span<core::u8> ) -> bool {
        return false;
    }

    auto Device::CreateDescriptorTableLayout( const renderer::rhi::DescriptorTableLayoutDescription &desc ) -> renderer::rhi::BindingLayoutHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::BindingLayoutHandle layout{ core::Ref<BindingLayout>::New( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return renderer::rhi::BindingLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreateDescriptorTable( renderer::rhi::BindingLayoutHandle layout ) -> renderer::rhi::DescriptorTableHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::DescriptorTableHandle table{ core::Ref<DescriptorTable>::New( layout ) };

        if ( table.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate descriptor table resource." );
            return renderer::rhi::DescriptorTableHandle::CreateEmpty();
        }

        table->Initialize( this );

        return table;
    }

    auto Device::ResizeDescriptorTable( renderer::rhi::DescriptorTableHandle descriptorTable, core::u32 newSize, bool keepContents ) -> bool {
        MKT_BEGIN_PROFILER();
        return false;
    }

    auto Device::ReleaseDescriptorTableIndex( rhi::DescriptorTableHandle descriptorTable, core::u32 bindingIndex, rhi::DescriptorTableIndex index ) -> void {
        MKT_BEGIN_PROFILER();

        DescriptorTable *table{ core::checked_cast<DescriptorTable *>( descriptorTable.GetPtr() ) };

        if ( !table ) {
            return;
        }

        table->FreeIndex( bindingIndex, index );
    }

    auto Device::WriteDescriptorTable( renderer::rhi::DescriptorTableHandle descriptorTable, const renderer::rhi::BindingTableItem &item ) -> renderer::rhi::DescriptorTableIndex {
        MKT_BEGIN_PROFILER();
        DescriptorTable *table{ core::checked_cast<DescriptorTable *>( descriptorTable.GetPtr() ) };
        renderer::rhi::DescriptorTableIndex resourceIndex{ table->AllocateNextIndex( item.mBindingIndex ) };

        DescriptorWriter writer{};

        switch ( item.mResourceType ) {
            case renderer::rhi::ResourceType::eRayTracingAccelStruct:
                writer.WriteAccelerationStructure( item.mBindingIndex,
                    core::checked_cast<AccelerationStructure *>( item.mResource )->Handle(), resourceIndex );
                break;
            case renderer::rhi::ResourceType::eTexture_SRV:
                // A descriptor declares the layout at shader access, not the
                // texture's temporary layout when the descriptor is allocated.
                // Frame-graph barriers transition it to this layout before use.
                writer.WriteImage(
                    item.mBindingIndex,
                    core::checked_cast<Texture *>( item.mResource )->GetNativeHandle( renderer::rhi::ObjectType::Vk_ImageView ),
                    GetDescriptorType( item.mResourceType ),
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, resourceIndex );
                break;
            case renderer::rhi::ResourceType::eTexture_UAV:
                // Bindless ray dispatches write their output and accumulation
                // through this slot. Use a single-mip view and GENERAL layout,
                // matching the unordered-access transition before dispatch.
                writer.WriteImage(
                    item.mBindingIndex,
                    core::checked_cast<Texture *>( item.mResource )->GetRenderView( item.mSubResourceSet.mBaseMipLevel, item.mSubResourceSet.mBaseArraySlice ),
                    GetDescriptorType( item.mResourceType ),
                    VK_IMAGE_LAYOUT_GENERAL, resourceIndex );
                break;
            case renderer::rhi::ResourceType::eTypedBuffer_SRV:
                break;
            case renderer::rhi::ResourceType::eTypedBuffer_UAV:
                break;
            case renderer::rhi::ResourceType::eConstantBuffer:
            case renderer::rhi::ResourceType::eStructuredBuffer_SRV:
            case renderer::rhi::ResourceType::eStructuredBuffer_UAV:
            case renderer::rhi::ResourceType::eRawBuffer_SRV:
            case renderer::rhi::ResourceType::eRawBuffer_UAV:
                // Slang ByteAddressBuffer and RWByteAddressBuffer both use
                // storage-buffer descriptors in Vulkan.
                writer.WriteBuffer(
                    item.mBindingIndex,
                    core::checked_cast<Buffer *>( item.mResource )->GetNativeHandle( renderer::rhi::ObjectType::Vk_Buffer ),
                    core::checked_cast<Buffer *>( item.mResource )->GetSizeBytes(),
                    0,
                    GetDescriptorType( item.mResourceType ), resourceIndex );
                break;
            case renderer::rhi::ResourceType::eSampler:
                writer.WriteSampler(
                    item.mBindingIndex,
                    core::checked_cast<Sampler *>( item.mResource )->GetNativeHandle( renderer::rhi::ObjectType::Vk_Sampler ), resourceIndex );
                break;
            default:;
        }

        VkDescriptorSet descriptorSet{ descriptorTable->GetNativeHandle( renderer::rhi::ObjectType::Vk_DescriptorSet ) };
        writer.UpdateSet( mLogicalDevice, descriptorSet );
        {
            std::lock_guard lock{ table->mDescriptorMutex };
            table->mResources[( core::u64{ item.mBindingIndex } << 32 ) | resourceIndex] = core::checked_cast<rhi::DeviceObject *>( item.mResource );
        }

        return resourceIndex;
    }

    auto Device::CreateTexture( const ExternalTextureDescription &info ) -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::TextureHandle texture{ core::Ref<Texture>::New( info ) };

        if ( texture.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice - Failed to allocate texture" );
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        texture->Initialize( this );

        return texture;
    }

    auto Device::CreateBinarySemaphore() -> BinarySemaphoreHandle {
        MKT_BEGIN_PROFILER();

        BinarySemaphoreHandle semaphore{ core::Ref<BinarySemaphore>::New() };

        if ( semaphore.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "VulkanDevice - Failed to allocate texture" );
            return BinarySemaphoreHandle::CreateEmpty();
        }

        semaphore->Initialize( this );

        return semaphore;
    }

    auto Device::SetDebugName( VkObjectType objectType, core::u64 handle, eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() ) {
            return;
        }

        VkDebugUtilsObjectNameInfoEXT nameInfo{};
        nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        nameInfo.objectType = objectType;
        nameInfo.objectHandle = handle;
        nameInfo.pObjectName = name.data();

        if ( vkSetDebugUtilsObjectNameEXT ) {
            vkSetDebugUtilsObjectNameEXT( mLogicalDevice, MKT_ADDRESSOF( nameInfo ) );
        }
        else {
            MKT_CORE_LOGGER_WARN( "vkGetDeviceProcAddr is null, cannot set debug name." );
        }
    }

    auto Device::CreateShaderTable( const rhi::ShaderTableDescription &description ) -> rhi::ShaderTableHandle {
        MKT_BEGIN_PROFILER();
        rhi::ShaderTableHandle table{ core::Ref<ShaderTable>::New( description ) };

        if ( table.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "CreateShaderTable - Failed to allocate shader table." );
            return rhi::ShaderTableHandle::CreateEmpty();
        }

        table->Initialize( this );
        return table;
    }

    auto Device::CreatePipeline( const renderer::rhi::ComputePipelineDescription &description ) -> renderer::rhi::PipelineHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::PipelineHandle computePipeline{ core::Ref<ComputePipeline>::New( description, mPipelineCache ) };

        if ( computePipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate compute pipeline resource." );
            return renderer::rhi::PipelineHandle::CreateEmpty();
        }

        computePipeline->Initialize( this );

        return computePipeline;
    }

    auto Device::CreatePipeline( const renderer::rhi::GraphicsPipelineDescription &description ) -> renderer::rhi::PipelineHandle {
        MKT_BEGIN_PROFILER();

        PipelineHandle graphicsPipeline{ core::Ref<GraphicsPipeline>::New( description, mPipelineCache ) };

        if ( graphicsPipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate graphics pipeline resource." );
            return renderer::rhi::PipelineHandle::CreateEmpty();
        }

        graphicsPipeline->Initialize( this );

        return graphicsPipeline;
    }

    auto Device::CreateCommandList( renderer::rhi::QueueType queueType ) -> renderer::rhi::CommandListHandle {
        MKT_BEGIN_PROFILER();
        Queue *queue{ core::checked_cast<Queue *>( GetQueue( queueType ) ) };
        renderer::rhi::CommandListHandle handle{ renderer::rhi::CommandListHandle::CreateEmpty() };

        if ( queue ) {
            handle = queue->AllocateCmdList( queueType );
            handle->Initialize( this );
        }

        return handle;
    }

    auto Device::SupportsBCTextureCompression() const -> bool {
        return IsFeatureSupported( GpuFeatureSupportFlagsBits::BCTextureCompression );
    }

    auto Device::SupportsRayTracing() const -> bool {
        // Renderer paths and resource factories need usable ray tracing, not just hardware support.
        return IsFeatureRequested( GpuFeatureSupportFlagsBits::HardwareRayTracing ) &&
               IsFeatureSupported( GpuFeatureSupportFlagsBits::HardwareRayTracing );
    }

    auto Device::CreateAccelStructure( const renderer::rhi::AccelStructureCreateDescription &description ) -> AccelStructureHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::AccelStructureHandle accelerationStructure{ core::Ref<AccelerationStructure>::New( description ) };

        if ( accelerationStructure.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate acceleration structure resource." );
            return AccelStructureHandle::CreateEmpty();
        }

        accelerationStructure->Initialize( this );

        return accelerationStructure;
    }

    auto Device::CreatePipeline( const renderer::rhi::RayTracingPipelineDescription &description ) -> renderer::rhi::RayTracingPipelineHandle {
        MKT_BEGIN_PROFILER();
        PipelineHandle rayTracingPipeline{ core::Ref<RayTracingPipeline>::New( description, mPipelineCache ) };

        if ( rayTracingPipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate raytracing pipeline resource." );
            return renderer::rhi::PipelineHandle::CreateEmpty();
        }

        rayTracingPipeline->Initialize( this );

        return rayTracingPipeline;
    }

    auto Device::CreateCommandList( const rhi::CommandListCreateDescription &desc ) -> rhi::CommandListHandle {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::CommandListHandle::CreateEmpty();
    }

    auto Device::GetQueryCapabilities( renderer::rhi::QueryType type, renderer::rhi::QueueType queue ) const -> renderer::rhi::QueryCapabilities {
        renderer::rhi::QueryCapabilities result{};

        if ( queue != renderer::rhi::QueueType::eGraphics && queue != renderer::rhi::QueueType::eCompute ) {
            result.mReason = "Queries are exposed on graphics and compute queues.";
            return result;
        }

        if ( !mPhysicalDevice ) {
            result.mReason = "No Vulkan physical device is available.";
            return result;
        }

        if ( type == renderer::rhi::QueryType::eTimestamp ) {
            const auto *family{ mPhysicalDevice->GetQueueWithSupport( queue == renderer::rhi::QueueType::eGraphics ? renderer::rhi::QueueOpSupportFlagsBits::Graphics : renderer::rhi::QueueOpSupportFlagsBits::Compute ) };

            if ( !family ) {
                result.mReason = "No queue family supports the requested queue type.";
                return result;
            }

            if ( !family->mProperties.timestampValidBits ) {
                result.mReason = "The requested queue family does not support timestamp queries.";
                return result;
            }

            result.mSupported = true;
            result.mTimestampValidBits = family->mProperties.timestampValidBits;
            result.mTimestampPeriodNs = mPhysicalDevice->mProperties.limits.timestampPeriod;
        }
        else if ( type == renderer::rhi::QueryType::eOcclusion ) {
            if ( queue != renderer::rhi::QueueType::eGraphics ) {
                result.mReason = "Occlusion queries require a graphics queue.";
                return result;
            }

            result.mSupported = true;
            result.mPreciseOcclusion = mEnabledFeatures.occlusionQueryPrecise;
        }
        else if ( type == renderer::rhi::QueryType::ePipelineStatistics ) {
            if ( !mEnabledFeatures.pipelineStatisticsQuery ) {
                result.mReason = "The pipelineStatisticsQuery device feature is not enabled.";
                return result;
            }

            // Capability masks use RHI statistic indices so callers do not depend on Vulkan flags.
            const auto statisticBit{ []( renderer::rhi::PipelineStatistic statistic ) -> core::u64 {
                return core::u64{ 1 } << static_cast<core::u32>( statistic );
            } };

            result.mSupported = true;
            result.mStatisticsMask = statisticBit( renderer::rhi::PipelineStatistic::eComputeInvocations );

            if ( queue == renderer::rhi::QueueType::eGraphics ) {
                result.mStatisticsMask |= statisticBit( renderer::rhi::PipelineStatistic::eInputVertices ) |
                                          statisticBit( renderer::rhi::PipelineStatistic::eInputPrimitives ) |
                                          statisticBit( renderer::rhi::PipelineStatistic::eVertexInvocations ) |
                                          statisticBit( renderer::rhi::PipelineStatistic::eClippingInvocations ) |
                                          statisticBit( renderer::rhi::PipelineStatistic::eClippingPrimitives ) |
                                          statisticBit( renderer::rhi::PipelineStatistic::ePixelInvocations );

                if ( mEnabledFeatures.geometryShader ) {
                    result.mStatisticsMask |= statisticBit( renderer::rhi::PipelineStatistic::eGeometryInvocations ) |
                                              statisticBit( renderer::rhi::PipelineStatistic::eGeometryPrimitives );
                }

                if ( mEnabledFeatures.tessellationShader ) {
                    result.mStatisticsMask |= statisticBit( renderer::rhi::PipelineStatistic::eHullPatches ) |
                                              statisticBit( renderer::rhi::PipelineStatistic::eDomainInvocations );
                }
            }
        }
        else {
            result.mReason = "Requires an optional query extension / engine integration that is not enabled.";
        }

        if ( result.mSupported ) {
            result.mReason = "";
        }

        return result;
    }

    auto Device::CreateQueryPool( const renderer::rhi::QueryPoolDescription &description ) -> renderer::rhi::QueryPoolHandle {
        if ( !GetQueryCapabilities( description.mType, description.mQueue ).mSupported ) {
            return renderer::rhi::QueryPoolHandle::CreateEmpty();
        }

        renderer::rhi::QueryPoolHandle queryPool{ core::Ref<QueryPool>::New( description ) };

        if ( queryPool.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate query pool." );
            return renderer::rhi::QueryPoolHandle::CreateEmpty();
        }

        queryPool->Initialize( this );

        return queryPool;
    }

    auto Device::RunGarbageCollection() -> void {
        MKT_BEGIN_PROFILER();

        for ( auto &queue: mQueues | std::ranges::views::values ) {
            queue->CollectGarbage();
        }

        mUploadManager->ReclaimMemory();
        mDescriptorAllocatorPool->Flip();
    }

    auto Device::WaitIdle() -> void {
        MKT_BEGIN_PROFILER();

        MKT_VK_CHECK( vkDeviceWaitIdle( mLogicalDevice ) );

        // Drop completed recording refs while allocators and descriptor pools
        // still exist, including command lists that will never be used again.
        for ( auto &queue: mQueues | std::ranges::views::values ) {
            queue->CollectGarbage();
        }
    }

    auto Device::GetDummySampler() -> Sampler * {
        MKT_BEGIN_PROFILER();
        return core::checked_cast<Sampler *>( mDummySampler.GetPtr() );
    }

    auto Device::GetDummyPipelineLayout() -> PipelineLayout * {
        MKT_BEGIN_PROFILER();
        return core::checked_cast<PipelineLayout *>( mEmptyPipelineLayout.GetPtr() );
    }

    auto Device::GetLayoutForEmptySet() -> VkDescriptorSetLayout {
        MKT_BEGIN_PROFILER();
        return mEmptyBindingLayout->GetNativeHandle( renderer::rhi::ObjectType::Vk_DescriptorSetLayout );
    }

    auto Device::GetUploadManager() -> GpuUploadManager * {
        MKT_BEGIN_PROFILER();
        return mUploadManager.get();
    }

    auto Device::GetDescriptorAllocator() -> DescriptorAllocatorHandle {
        MKT_BEGIN_PROFILER();
        return mDescriptorAllocatorPool->GetAllocator();
    }

    auto Device::GetPhysicalDevice() const -> const PhysicalDevice * {
        MKT_BEGIN_PROFILER();
        return mPhysicalDevice;
    }

    auto Device::SupportsSceneMultisampling( rhi::Multisampling samples ) const -> bool {
        VkImageFormatProperties color{};
        VkImageFormatProperties depth{};
        const auto physical{ mPhysicalDevice->mPhysicalDevice };

        if ( vkGetPhysicalDeviceImageFormatProperties( physical, VK_FORMAT_R16G16B16A16_SFLOAT,
                 VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT,
                 0, &color ) != VK_SUCCESS ||
             vkGetPhysicalDeviceImageFormatProperties( physical, VK_FORMAT_D32_SFLOAT,
                 VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 0, &depth ) != VK_SUCCESS ) {
            return false;
        }

        const auto count{ GetSampleCount( samples ) };
        return ( color.sampleCounts & depth.sampleCounts & count ) != 0;
    }

    auto Device::GetAllocator() -> GpuMemoryAllocator * {
        MKT_BEGIN_PROFILER();
        return core::checked_cast<GpuMemoryAllocator *>( mGpuAllocator.get() );
    }

    auto Device::GetQueue( renderer::rhi::QueueType type ) -> renderer::rhi::IQueue * {
        MKT_BEGIN_PROFILER();
        const auto it{ mQueues.find( type ) };
        return it != mQueues.end() ? it->second.GetPtr() : nullptr;
    }

    auto Device::GetProcessMemoryUsage() const -> rhi::ProcessGpuMemory {
        return mGpuAllocator ? mGpuAllocator->GetProcessMemoryUsage() : rhi::ProcessGpuMemory{};
    }

    auto Device::GetMemoryUsage() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mGpuAllocator->GetMemoryUsage();
    }

    auto Device::GetMemoryTotal() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mGpuAllocator->GetMemoryTotal();
    }

    auto Device::GetMemoryAvailable() const -> core::usize {
        MKT_BEGIN_PROFILER();
        return mGpuAllocator->GetMemoryAvailable();
    }

    auto Device::GetActivePhysicalDeviceFeatures() const -> const VkPhysicalDeviceFeatures & {
        MKT_BEGIN_PROFILER();
        return mEnabledFeatures;
    }

    auto Device::GetActivePhysicalDeviceFeatures2() const -> const VkPhysicalDeviceFeatures2 & {
        MKT_BEGIN_PROFILER();
        return mEnabledFeatures2;
    }

    auto Device::GetActive11Features() const -> const VkPhysicalDeviceVulkan11Features & {
        MKT_BEGIN_PROFILER();
        return mEnabled11Features;
    }

    auto Device::GetActive12Features() const -> const VkPhysicalDeviceVulkan12Features & {
        MKT_BEGIN_PROFILER();
        return mEnabled12Features;
    }

    auto Device::GetActive13Features() const -> const VkPhysicalDeviceVulkan13Features & {
        MKT_BEGIN_PROFILER();
        return mEnabled13Features;
    }

    auto Device::CreateSwapChain( const SwapChainCreateInfo &createInfo ) -> VulkanSwapChainHandle {
        MKT_BEGIN_PROFILER();
        VulkanSwapChainHandle handle{ VulkanSwapChainHandle::New( createInfo ) };

        if ( !handle.IsEmpty() ) {
            handle->Initialize( this );
        }

        return handle;
    }

    auto Device::InitLogicalDevice() -> void {
        MKT_BEGIN_PROFILER();

        // Core Vulkan 1.3 features are validated during physical-device selection.

        // Slang's SPIR-V 1.6 discard path requires shaderDemoteToHelperInvocation.
        mEnabled13Features = initializers::PhysicalDeviceVulkan13Features();
        mEnabled13Features.synchronization2 = VK_TRUE;
        mEnabled13Features.dynamicRendering = VK_TRUE;
        mEnabled13Features.shaderDemoteToHelperInvocation = VK_TRUE;

        // Enable optional extensions only when requested and supported.
        // Capability discovery still reports unrequested features in mSupportedFeatures.
        VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR tracingFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR };
        VkPhysicalDeviceMeshShaderFeaturesEXT meshFeatures{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT };

        if ( IsFeatureRequested( GpuFeatureSupportFlagsBits::HardwareRayTracing ) &&
             IsFeatureSupported( GpuFeatureSupportFlagsBits::HardwareRayTracing ) ) {
            mExtensions.emplace_back( VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME );
            mExtensions.emplace_back( VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME );

            // Required by KHR_acceleration_structure.
            // Allows work to be offloaded onto background threads and parallelized
            mExtensions.emplace_back( VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME );

            accelerationFeatures.accelerationStructure = VK_TRUE;
            accelerationFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;
            tracingFeatures.rayTracingPipeline = VK_TRUE;

            tracingFeatures.pNext = mEnabled13Features.pNext;
            accelerationFeatures.pNext = MKT_ADDRESSOF( tracingFeatures );
            mEnabled13Features.pNext = MKT_ADDRESSOF( accelerationFeatures );
        }

        if ( IsFeatureRequested( GpuFeatureSupportFlagsBits::MeshShaders ) &&
             IsFeatureSupported( GpuFeatureSupportFlagsBits::MeshShaders ) ) {
            mExtensions.emplace_back( VK_EXT_MESH_SHADER_EXTENSION_NAME );

            meshFeatures.meshShader = VK_TRUE;
            meshFeatures.taskShader = mPhysicalDevice->mMeshFeatures.taskShader;
            meshFeatures.pNext = mEnabled13Features.pNext;
            mEnabled13Features.pNext = MKT_ADDRESSOF( meshFeatures );
        }

        // --- Vulkan 1.2 Features ---
        mEnabled12Features = initializers::PhysicalDeviceVulkan12Features();
        mEnabled12Features.pNext = MKT_ADDRESSOF( mEnabled13Features );

        mEnabled12Features.descriptorIndexing = VK_TRUE;
        mEnabled12Features.bufferDeviceAddress = VK_TRUE;

        // TODO: Enable optionally via GpuFeatureSupportFlagsBits::BindlessDescriptorTables
        // just like other features
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

        // --- Vulkan 1.1 Features
        mEnabled11Features = initializers::PhysicalDeviceVulkan11Features();
        mEnabled11Features.pNext = MKT_ADDRESSOF( mEnabled12Features );
        mEnabled11Features.shaderDrawParameters = VK_TRUE;

        // Video is optional, expose its queue only when H.264
        // decode and sampling are available en encoding support is requested.
        if ( mPhysicalDevice->mFeatures11.samplerYcbcrConversion &&
             mPhysicalDevice->HasQueueSupport( rhi::QueueOpSupportFlagsBits::VideoDecode ) &&
             mPhysicalDevice->IsExtensionAvailable( VK_KHR_VIDEO_QUEUE_EXTENSION_NAME ) &&
             mPhysicalDevice->IsExtensionAvailable( VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME ) &&
             mPhysicalDevice->IsExtensionAvailable( VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME ) ) {
            mExtensions.emplace_back( VK_KHR_VIDEO_QUEUE_EXTENSION_NAME );
            mExtensions.emplace_back( VK_KHR_VIDEO_DECODE_QUEUE_EXTENSION_NAME );
            mExtensions.emplace_back( VK_KHR_VIDEO_DECODE_H264_EXTENSION_NAME );
            mEnabled11Features.samplerYcbcrConversion = VK_TRUE;
        }

        // --- Core device features ---
        mEnabledFeatures.sampleRateShading = VK_TRUE;
        mEnabledFeatures.multiDrawIndirect = VK_TRUE;
        mEnabledFeatures.drawIndirectFirstInstance = VK_TRUE;
        mEnabledFeatures.fillModeNonSolid = mPhysicalDevice->mFeatures.fillModeNonSolid;
        mEnabledFeatures.wideLines = mPhysicalDevice->mFeatures.wideLines;
        mEnabledFeatures.samplerAnisotropy = mPhysicalDevice->mFeatures.samplerAnisotropy;
        mEnabledFeatures.textureCompressionBC = mPhysicalDevice->mFeatures.textureCompressionBC;
        mEnabledFeatures.shaderInt64 = VK_TRUE;

        // Bindless Slang storage images use an unknown SPIR-V image format so one
        // descriptor array can address R16, R32 and RGBA targets. Enable the supported
        // formatless-image operations declared by those compute shaders.
        mEnabledFeatures.shaderStorageImageReadWithoutFormat = mPhysicalDevice->mFeatures.shaderStorageImageReadWithoutFormat;
        mEnabledFeatures.shaderStorageImageWriteWithoutFormat = mPhysicalDevice->mFeatures.shaderStorageImageWriteWithoutFormat;

        // Optional instrumentation must not make devices without these
        // counters fail initialization. The RHI reports the enabled subset.
        mEnabledFeatures.pipelineStatisticsQuery = mPhysicalDevice->mFeatures.pipelineStatisticsQuery;
        mEnabledFeatures.occlusionQueryPrecise = mPhysicalDevice->mFeatures.occlusionQueryPrecise;

        // --- Final root features struct ---
        mEnabledFeatures2 = initializers::PhysicalDeviceFeatures2();
        mEnabledFeatures2.pNext = MKT_ADDRESSOF( mEnabled11Features );
        mEnabledFeatures2.features = mEnabledFeatures;

        // OIDN is optional. Do not make ordinary rendering require FP16,
        // cooperative matrices, or push descriptors on devices lacking them.
        VkPhysicalDeviceCooperativeMatrixFeaturesKHR oidnCooperative{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COOPERATIVE_MATRIX_FEATURES_KHR };
#if MIKOTO_ENABLE_OIDN
        const auto &supported{ *mPhysicalDevice };
        mOidnSupported = supported.mFeatures.shaderInt16 && supported.mFeatures.shaderInt64 &&
                         supported.mFeatures.shaderStorageImageReadWithoutFormat && supported.mFeatures.shaderStorageImageWriteWithoutFormat &&
                         supported.mFeatures11.storageBuffer16BitAccess && supported.mFeatures12.shaderFloat16 &&
                         supported.mFeatures12.bufferDeviceAddress && supported.mFeatures12.vulkanMemoryModel &&
                         supported.mFeatures12.vulkanMemoryModelDeviceScope && supported.mFeatures13.maintenance4 &&
                         supported.mFeatures13.subgroupSizeControl && supported.mFeatures13.computeFullSubgroups &&
                         supported.IsExtensionAvailable( VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME );
        if ( mOidnSupported ) {
            bool cooperative{};
            if ( supported.IsExtensionAvailable( VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME ) ) {
                VkPhysicalDeviceFeatures2 query{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
                query.pNext = &oidnCooperative;
                vkGetPhysicalDeviceFeatures2( supported.mPhysicalDevice, &query );
                cooperative = oidnCooperative.cooperativeMatrix;
                oidnCooperative.cooperativeMatrixRobustBufferAccess = VK_FALSE;
            }

            std::vector<const char *> extensions{};
            vkoidn::getRequiredDeviceExtensions( Context::Get()->GetInstance().mInstance,
                supported.mPhysicalDevice, vkGetInstanceProcAddr, extensions );
            for ( const auto *extension: extensions ) {
                if ( !cooperative && std::strcmp( extension, VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME ) == 0 ) {
                    continue;
                }
                if ( eastl::none_of( mExtensions.begin(), mExtensions.end(), [extension]( const auto &name ) {
                         return name == extension;
                     } ) ) {
                    mExtensions.emplace_back( extension );
                }
            }

            if ( cooperative ) {
                oidnCooperative.pNext = mEnabled13Features.pNext;
                mEnabled13Features.pNext = &oidnCooperative;
            }
            vkoidn::enableRequiredFeatures( mEnabledFeatures2, mEnabled11Features, mEnabled12Features,
                mEnabled13Features, cooperative ? &oidnCooperative : nullptr );
            mEnabledFeatures = mEnabledFeatures2.features;
        }
#endif

        // These extensions must be enabled at device creation. Feature startup
        // itself is deferred to the device-owned NGX worker, shared by all views.
        const auto dlss{ DlssContext::GetRequiredDeviceExtensions( Context::Get()->GetInstance().mInstance,
            mPhysicalDevice->mPhysicalDevice ) };
        mDlssExtensionsReady = Context::Get()->GetInstance().mDlssExtensionsReady && dlss.mAvailable &&
            std::ranges::all_of( dlss.mNames, [this]( const auto &extension ) {
                return mPhysicalDevice->IsExtensionAvailable( extension.c_str() );
            } );
        if ( mDlssExtensionsReady ) {
            for ( const auto &extension: dlss.mNames ) {
                if ( eastl::none_of( mExtensions.begin(), mExtensions.end(), [&]( const auto &enabled ) {
                         return enabled == extension.c_str();
                     } ) ) {
                    mExtensions.emplace_back( extension.c_str() );
                }
            }
        }

        VkDeviceCreateInfo createInfo{ initializers::DeviceCreateInfo() };
        createInfo.pNext = MKT_ADDRESSOF( mEnabledFeatures2 );

        // Prepare queue infos
        // I find a queue family index that supports the operations I want to perform
        // Right we are only looking for graphics, transfer, compute and optionally present
        ankerl::unordered_dense::set<core::u32> queueFamilies{};
        queueFamilies.emplace( mPhysicalDevice->GetFamilyIndexWithSupport( renderer::rhi::QueueOpSupportFlagsBits::Graphics ) );
        queueFamilies.emplace( mPhysicalDevice->GetFamilyIndexWithSupport( renderer::rhi::QueueOpSupportFlagsBits::Compute ) );
        queueFamilies.emplace( mPhysicalDevice->GetFamilyIndexWithSupport( renderer::rhi::QueueOpSupportFlagsBits::Transfer ) );

        if ( mEnabled11Features.samplerYcbcrConversion ) {
            queueFamilies.emplace( mPhysicalDevice->GetFamilyIndexWithSupport( rhi::QueueOpSupportFlagsBits::VideoDecode ) );
        }

        if ( IsFeatureRequested( renderer::rhi::GpuFeatureSupportFlagsBits::WindowPresentation ) ) {
            queueFamilies.emplace( mPhysicalDevice->GetFamilyIndexWithSupport( renderer::rhi::QueueOpSupportFlagsBits::Presentation ) );
        }

        MKT_ASSERT( !queueFamilies.contains( PhysicalDevice::kInvalidQueueFamilyIndex ) && !queueFamilies.empty(),
            "Queue family requires at least one valid family index" );

        eastl::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};
        eastl::vector queuePriority( kMaxQueuesPerFamily, kQueueDefaultPriority );

        for ( const auto &familyIndex: queueFamilies ) {
            auto &queueCreateInfo{ queueCreateInfos.emplace_back( initializers::DeviceQueueCreateInfo() ) };

            queueCreateInfo.queueFamilyIndex = familyIndex;

            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = queuePriority.data();
        }

        createInfo.queueCreateInfoCount = core::as<core::u32>( queueCreateInfos.size() );
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        eastl::vector<const char *> extensionNames{};
        extensionNames.reserve( mExtensions.size() );

        for ( const auto &extension: mExtensions ) {
            extensionNames.emplace_back( extension.c_str() );
        }

        createInfo.enabledExtensionCount = core::as<core::u32>( extensionNames.size() );
        createInfo.ppEnabledExtensionNames = extensionNames.data();

        const VkResult result{ vkCreateDevice( mPhysicalDevice->mPhysicalDevice, MKT_ADDRESSOF( createInfo ), nullptr, MKT_ADDRESSOF( mLogicalDevice ) ) };

        // The extension structs above are local to this call, including on a creation failure.
        mEnabled13Features.pNext = nullptr;
        MKT_VK_CHECK( result );

#if defined( MIKOTO_DEBUG )
        if ( IsFeatureRequested( GpuFeatureSupportFlagsBits::MeshShaders ) &&
             !IsFeatureSupported( GpuFeatureSupportFlagsBits::MeshShaders ) ) {
            MKT_CORE_LOGGER_WARN( "Mesh shader support was requested but is not available on this device." );
        }

        if ( IsFeatureRequested( GpuFeatureSupportFlagsBits::HardwareRayTracing ) &&
             !IsFeatureSupported( GpuFeatureSupportFlagsBits::HardwareRayTracing ) ) {
            MKT_CORE_LOGGER_WARN( "Hardware ray tracing support was requested but is not available on this device." );
        }
#endif
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
        MKT_BEGIN_PROFILER();

        // Roles selecting the same family share queue zero, including transfer uploads.
        // Dedicated transfer/compute queues can be introduced through the existing queue API.
        ankerl::unordered_dense::map<renderer::rhi::QueueType, const VulkanQueueData *> queuesIndices{};
        queuesIndices[renderer::rhi::QueueType::eGraphics] = mPhysicalDevice->GetQueueWithSupport( renderer::rhi::QueueOpSupportFlagsBits::Graphics );
        queuesIndices[renderer::rhi::QueueType::eCompute] = mPhysicalDevice->GetQueueWithSupport( renderer::rhi::QueueOpSupportFlagsBits::Compute );
        queuesIndices[renderer::rhi::QueueType::eTransfer] = mPhysicalDevice->GetQueueWithSupport( renderer::rhi::QueueOpSupportFlagsBits::Transfer );

        if ( mEnabled11Features.samplerYcbcrConversion &&
             IsFeatureRequested( GpuFeatureSupportFlagsBits::VideoDecoding ) ) {
            queuesIndices[rhi::QueueType::eVideoDecode] = mPhysicalDevice->GetQueueWithSupport( rhi::QueueOpSupportFlagsBits::VideoDecode );
        }

        if ( IsFeatureRequested( renderer::rhi::GpuFeatureSupportFlagsBits::WindowPresentation ) ) {
            queuesIndices[renderer::rhi::QueueType::ePresent] = mPhysicalDevice->GetQueueWithSupport( renderer::rhi::QueueOpSupportFlagsBits::Presentation );
        }

        // Keep track of families that already have a queue
        ankerl::unordered_dense::map<core::u32, core::Ref<Queue>> queueFamilyTracking{};

        for ( constexpr core::u32 kQueueIndex{ 0 }; const auto &[queueType, queueData]: queuesIndices ) {
            const auto it{ queueFamilyTracking.find( queueData->FamilyIndex ) };

            // Video encoding and enqueuing for now
            // get their own dedicated queue rest of operations can go
            // to the "main" queue of family 0 which generally supports all
            // operations including presentation
            if ( it == queueFamilyTracking.end() ) {
                renderer::rhi::QueueHandle queue{ core::Ref<Queue>::New( queueType, queueData->mOpSupportFlags, queueData->FamilyIndex, kQueueIndex ) };
                queue->Initialize( this );

                mQueues[queueType] = queue;
                queueFamilyTracking[queueData->FamilyIndex] = queue;
            }
            else {
                mQueues[queueType] = it->second;
            }
        }
    }

    auto Device::InitMemoryAllocator() -> void {
        MKT_BEGIN_PROFILER();

        mGpuAllocator = renderer::rhi::IGpuAllocator::Create( this );

        if ( !mGpuAllocator ) {
            MKT_THROW_RUNTIME_ERROR( "VulkanDevice - Could not create GPU Allocator." );
        }

        mUploadManager = eastl::make_unique<GpuUploadManager>( this );

        mGpuAllocator->Init();
    }

    auto Device::InitPipelineCache() -> void {
        MKT_BEGIN_PROFILER();

        if ( !filesystem::FileService::HasInstance() ) {
            return;
        }

        mPipelineCachePath = filesystem::PathBuilder{}
                                 .SetPath( kPipelineCacheDirectory )
                                 .SetPath( "pipeline_pache.bin" )
                                 .Build();

        ( void )filesystem::CreateIfNotExistsDirectory( kPipelineCacheDirectory );

        filesystem::FileHandle file{ filesystem::FileService::Get()->LoadFile( mPipelineCachePath ) };

        if ( file.IsEmpty() ) {
            file = filesystem::FileService::Get()->CreateNewFile( mPipelineCachePath );
        }

        VkPipelineCacheCreateInfo cacheInfo{ initializers::PipelineCacheCreateInfo() };

        if ( !file.IsEmpty() && file->HasContents() ) {
            cacheInfo.initialDataSize = file->GetSize();
            cacheInfo.pInitialData = file->GetContentsBytes();
        }

        vkCreatePipelineCache( mLogicalDevice, MKT_ADDRESSOF( cacheInfo ), nullptr, MKT_ADDRESSOF( mPipelineCache ) );
    }

    auto Device::InitDescriptorAllocator() -> void {
        MKT_BEGIN_PROFILER();

        Context *ctx{ core::checked_cast<Context *>( RenderSystem::Get()->GetContext() ) };
        mDescriptorAllocatorPool = IDescriptorAllocatorPool::Create( mLogicalDevice, ctx->GetMaxFramesInFlight() );

        constexpr float poolMultiplier{ 3.0f };

        mDescriptorAllocatorPool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_SAMPLER, poolMultiplier );
        mDescriptorAllocatorPool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, poolMultiplier );

        mDescriptorAllocatorPool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, poolMultiplier );
        mDescriptorAllocatorPool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, poolMultiplier );

        mDescriptorAllocatorPool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, poolMultiplier );
        mDescriptorAllocatorPool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, poolMultiplier );

        mDescriptorAllocatorPool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, poolMultiplier );
        mDescriptorAllocatorPool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, poolMultiplier );

        mDescriptorAllocatorPool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, poolMultiplier );
        mDescriptorAllocatorPool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, poolMultiplier );

        mDescriptorAllocatorPool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, poolMultiplier );

        if ( SupportsRayTracing() ) {
            mDescriptorAllocatorPool->SetPoolSizeMultiplier( VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, poolMultiplier );
        }
    }

    auto Device::InitDummyResources() -> void {
        MKT_BEGIN_PROFILER();

        mDummySampler = CreateSampler( renderer::rhi::SamplerCreateDescription{} );
        mEmptyBindingLayout = CreateBindingLayout( renderer::rhi::BindingLayoutDescription{} );
        mEmptyPipelineLayout = CreatePipelineLayout( renderer::rhi::PipelineLayoutCreateDescription{} );
    }

    auto Device::DestroyDummyResources() -> void {
        MKT_BEGIN_PROFILER();

        mDummySampler.Reset();
        mEmptyBindingLayout.Reset();
        mEmptyPipelineLayout.Reset();
    }

    auto Device::SerializePipelineCache() -> void {
        MKT_BEGIN_PROFILER();

        core::usize dataSize{};
        vkGetPipelineCacheData( mLogicalDevice, mPipelineCache, MKT_ADDRESSOF( dataSize ), nullptr );

        if ( dataSize > 0 ) {
            eastl::vector<core::u8> data( dataSize );

            MKT_VK_CHECK( vkGetPipelineCacheData( mLogicalDevice, mPipelineCache, MKT_ADDRESSOF( dataSize ), data.data() ) );

            if ( filesystem::FileService::HasInstance() ) {
                filesystem::FileHandle file{ filesystem::FileService::Get()->LoadFile( mPipelineCachePath ) };
                file->Write( core::rc_cast<const char *>( data.data() ), core::as<core::usize>( data.size() ) );
            }
        }

        vkDestroyPipelineCache( mLogicalDevice, mPipelineCache, nullptr );
    }

    auto Device::IsDeviceSuitable( const PhysicalDevice &device ) -> bool {
        MKT_BEGIN_PROFILER();

        // Device class and Vulkan 1.3 are renderer requirements, independently of optional requests.
        if ( GetGpuDeviceType( mDeviceType ) != device.mProperties.deviceType ||
             device.mProperties.apiVersion < VK_API_VERSION_1_3 ) {
            return false;
        }

        const auto supported{ device.GetSupportedFeatures() };
        if ( ( supported & mRequestedFeatures ) != mRequestedFeatures ) {
            return false;
        }

        // Match the core features enabled by InitLogicalDevice. Vulkan version support alone
        // does not guarantee optional feature bits such as descriptor indexing or shaderInt64.
        // shaderInt64 is forced to be required for now because we are using device buffer address there's
        // alternative approach for devices not supporting it but this is being done for simplicy for now.
        if ( !device.mFeatures.shaderInt64 || !device.mFeatures.sampleRateShading ||
             !device.mFeatures.multiDrawIndirect || !device.mFeatures.drawIndirectFirstInstance ||
             !device.mFeatures11.shaderDrawParameters || !device.mFeatures12.bufferDeviceAddress ||
             !device.mFeatures12.scalarBlockLayout || !device.mFeatures12.timelineSemaphore ||
             !device.mFeatures13.synchronization2 || !device.mFeatures13.dynamicRendering ||
             !device.mFeatures13.shaderDemoteToHelperInvocation ||
             !( supported & GpuFeatureSupportFlagsBits::BindlessDescriptorTables ) ) {
            return false;
        }

        // Presentation queues are needed only when this logical device was requested for a window.
        QueueOpSupportFlags queueSupport{
            QueueOpSupportFlagsBits::Graphics |
            QueueOpSupportFlagsBits::Compute |
            QueueOpSupportFlagsBits::Transfer
        };

        if ( IsFeatureRequested( GpuFeatureSupportFlagsBits::WindowPresentation ) ) {
            queueSupport |= QueueOpSupportFlagsBits::Presentation;
        }

        return device.HasQueueSupport( queueSupport );
    }

    auto Device::GetDlssContext() -> DlssContext & {
        return mDlssContext;
    }

    auto Device::SupportsOidn() const -> bool {
        return mOidnSupported;
    }

    auto Device::GetEnabledExtensions() const -> eastl::span<const eastl::string> {
        return mExtensions;
    }

    auto Device::GetDevice() -> VkDevice {
        return mLogicalDevice;
    }

    CommandList::CommandList( rhi::QueueType queueType, rhi::IQueue *queue, core::u32 familyIndex )
        // See comment in AllocateCmdList() from Queue definition
        : mQueueType{ queueType }, mQueue{ queue }, mQueueFamilyIndex{ familyIndex } {
        MKT_BEGIN_PROFILER();
        mLabelColor = renderer::rhi::Color(
            ( core::f32 )math::random::GetRandomReal( 0.0f, 1.0f ),
            ( core::f32 )math::random::GetRandomReal( 0.0f, 1.0f ),
            ( core::f32 )math::random::GetRandomReal( 0.0f, 1.0f ),
            0.1f );
    }

    auto CommandList::GetStatistics() const -> const rhi::CommandListStatistics & {
        static const rhi::CommandListStatistics empty{};
        return mRecordingContext.empty() ? empty : mRecordingContext[mRecordingContextIndex].mStatistics;
    }

    auto CommandList::Open( const renderer::rhi::CommandListBeginDescription &desc ) -> void {
        std::lock_guard lock{ mRecordingMutex };
        MKT_BEGIN_PROFILER();

        if ( mRecording || mReadyForSubmission ) {
            MKT_THROW_RUNTIME_ERROR( "Vulkan recording must be ended and submitted before beginning another recording." );
        }

        mBoundRayTracingPipeline = nullptr;
        mBoundShaderTable = nullptr;

        auto *queue{ core::checked_cast<Queue *>( mQueue ) };
        core::u32 selected{ ( mRecordingContextIndex + 1 ) % static_cast<core::u32>( mRecordingContext.size() ) };

        // Cycle through the recording slots until the GPU has finished one.
        // Refresh completion on each probe; resetting an in-flight pool is invalid.
        while ( mRecordingContext[selected].mSubmissionID > queue->GetCurrentTimeline() ) {
            selected = ( selected + 1 ) % static_cast<core::u32>( mRecordingContext.size() );
        }

        mRecordingContextIndex = selected;
        mCurrentCommandBuffer = mRecordingContext[selected].mCommandBuffer;

        // Release resources only from the completed context selected above.
        ClearState();

        VkCommandBufferBeginInfo beginInfo{ initializers::CommandBufferBeginInfo() };
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        Device *device{ core::checked_cast<Device *>( mDevice ) };
        MKT_VK_CHECK( vkResetCommandPool( device->GetDevice(), mRecordingContext[mRecordingContextIndex].mCommandPool, MKT_VK_FLAGS_NONE ) );
        MKT_VK_CHECK( vkResetCommandBuffer( mCurrentCommandBuffer, MKT_VK_FLAGS_NONE ) );
        MKT_VK_CHECK( vkBeginCommandBuffer( mCurrentCommandBuffer, MKT_ADDRESSOF( beginInfo ) ) );
        mRecording = true;

        mRecordingScopeName = desc.mScopeName.empty() ? "UnnamedScope" : desc.mScopeName.c_str();

        // Must be in recording state
        // https://docs.vulkan.org/spec/latest/chapters/debugging.html
        // https://docs.vulkan.org/samples/latest/samples/extensions/debug_utils/README.html
        // https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBeginDebugUtilsLabelEXT.html
        VkDebugUtilsLabelEXT labelInfo = {};
        labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        labelInfo.pLabelName = mRecordingScopeName.c_str();
        labelInfo.color[0] = mLabelColor.mR;
        labelInfo.color[1] = mLabelColor.mG;
        labelInfo.color[2] = mLabelColor.mB;
        labelInfo.color[3] = mLabelColor.mA;

        vkCmdBeginDebugUtilsLabelEXT( mCurrentCommandBuffer, &labelInfo );
    }

    CommandList::operator VkCommandBuffer() const {
        return mCurrentCommandBuffer;
    }

    auto CommandList::Close() -> void {
        MKT_BEGIN_PROFILER();

        std::lock_guard lock{ mRecordingMutex };

        if ( !mRecording ) {
            MKT_THROW_RUNTIME_ERROR( "Vulkan command list is not recording." );
        }

        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

#if defined( MIKOTO_ENABLE_TRACY )
        EndGpuProfileZone();
#endif
        vkCmdEndDebugUtilsLabelEXT( mCurrentCommandBuffer );
        MKT_VK_CHECK( vkEndCommandBuffer( mCurrentCommandBuffer ) );

        mRecording = false;
        mReadyForSubmission = true;
        mBoundRayTracingPipeline = nullptr;
        mBoundShaderTable = nullptr;
    }


    auto CommandList::AcquireResource( const rhi::BufferBarrierDescription &description, rhi::IQueue *sourceQueue ) -> void {
        MKT_BEGIN_PROFILER();

        const auto family{ core::checked_cast<Queue *>( sourceQueue )->GetFamilyIndex() };

        if ( family == mQueueFamilyIndex ) {
            return;
        }

        if ( !mRecording || mIsRenderScopeActive || !description.mBuffer ) {
            MKT_THROW_RUNTIME_ERROR( "Ownership barriers require a resource and a recording command list outside rendering." );
        }

        RetainResource( description.mBuffer );
        CommitBarriers();

        VkBufferMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
        barrier.dstStageMask = GetStageMask( description.mStageAfter );
        barrier.dstAccessMask = GetAccessMask( description.mAccessAfter );

        // The other half has no local synchronization scope.
        barrier.srcQueueFamilyIndex = family;
        barrier.dstQueueFamilyIndex = mQueueFamilyIndex;
        barrier.buffer = description.mBuffer->GetNativeHandle( rhi::ObjectType::Vk_Buffer );
        barrier.offset = description.mRange.mByteOffset;
        barrier.size = description.mRange.mByteSize == 0 ? VK_WHOLE_SIZE : description.mRange.mByteSize;

        VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dependency.bufferMemoryBarrierCount = 1;
        dependency.pBufferMemoryBarriers = &barrier;

        ++mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mCallCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mImageCount += dependency.imageMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mBufferCount += dependency.bufferMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mMemoryCount += dependency.memoryBarrierCount;

        vkCmdPipelineBarrier2( mCurrentCommandBuffer, &dependency );
    }


    auto CommandList::ReleaseResource( const rhi::BufferBarrierDescription &description, rhi::IQueue *destQueue ) -> void {
        const auto family{ core::checked_cast<Queue *>( destQueue )->GetFamilyIndex() };

        if ( family == mQueueFamilyIndex ) {
            return;
        }

        if ( !mRecording || mIsRenderScopeActive || !description.mBuffer ) {
            MKT_THROW_RUNTIME_ERROR( "Ownership barriers require a resource and a recording command list outside rendering." );
        }

        RetainResource( description.mBuffer );
        CommitBarriers();

        VkBufferMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2 };
        barrier.srcStageMask = GetStageMask( description.mStageBefore );
        barrier.srcAccessMask = GetAccessMask( description.mAccessBefore );
        // The other half has no local synchronization scope.
        barrier.srcQueueFamilyIndex = mQueueFamilyIndex;
        barrier.dstQueueFamilyIndex = family;
        barrier.buffer = description.mBuffer->GetNativeHandle( rhi::ObjectType::Vk_Buffer );
        barrier.offset = description.mRange.mByteOffset;
        barrier.size = description.mRange.mByteSize == 0 ? VK_WHOLE_SIZE : description.mRange.mByteSize;

        VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dependency.bufferMemoryBarrierCount = 1;
        dependency.pBufferMemoryBarriers = &barrier;

        ++mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mCallCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mImageCount += dependency.imageMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mBufferCount += dependency.bufferMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mMemoryCount += dependency.memoryBarrierCount;

        vkCmdPipelineBarrier2( mCurrentCommandBuffer, &dependency );
    }


    auto CommandList::AcquireResource( const rhi::TextureBarrierDescription &description, rhi::IQueue *sourceQueue ) -> void {
        const auto family{ core::checked_cast<Queue *>( sourceQueue )->GetFamilyIndex() };

        if ( family == mQueueFamilyIndex ) {
            return;
        }

        if ( !mRecording || mIsRenderScopeActive || !description.mTexture ) {
            MKT_THROW_RUNTIME_ERROR( "Ownership barriers require a resource and a recording command list outside rendering." );
        }
        // Both halves must use explicit, identical layouts, independent of recording order.
        if ( description.mLayoutBefore == rhi::TextureLayoutBits::Unknown ||
             description.mLayoutAfter == rhi::TextureLayoutBits::Unknown ) {
            MKT_THROW_INVALID_ARGUMENT( "Texture ownership transfers require explicit before and after layouts." );
        }

        RetainResource( description.mTexture );
        CommitBarriers();

        VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        barrier.dstStageMask = GetStageMask( description.mStageAfter );
        barrier.dstAccessMask = GetAccessMask( description.mAccessAfter );
        // The other half has no local synchronization scope.
        barrier.srcQueueFamilyIndex = family;
        barrier.dstQueueFamilyIndex = mQueueFamilyIndex;
        barrier.image = description.mTexture->GetNativeHandle( rhi::ObjectType::Vk_Image );
        barrier.oldLayout = vulkan::GetImageLayout( description.mLayoutBefore );
        barrier.newLayout = vulkan::GetImageLayout( description.mLayoutAfter );
        const auto &range{ description.mSubresourceSet };
        barrier.subresourceRange = {
            .aspectMask = GetAspectMask( description.mTexture->GetFormat() ),
            .baseMipLevel = range.mBaseMipLevel,
            .levelCount = range.mNumMipLevels == rhi::TextureSubresourceSet::kAllMipLevels ? VK_REMAINING_MIP_LEVELS : range.mNumMipLevels,
            .baseArrayLayer = range.mBaseArraySlice,
            .layerCount = range.mNumArraySlices == rhi::TextureSubresourceSet::kAllArraySlices ? VK_REMAINING_ARRAY_LAYERS : range.mNumArraySlices
        };

        VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &barrier;

        ++mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mCallCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mImageCount += dependency.imageMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mBufferCount += dependency.bufferMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mMemoryCount += dependency.memoryBarrierCount;

        vkCmdPipelineBarrier2( mCurrentCommandBuffer, &dependency );
    }


    auto CommandList::ReleaseResource( const rhi::TextureBarrierDescription &description, rhi::IQueue *destQueue ) -> void {
        MKT_BEGIN_PROFILER();

        // Queue family transfer is only a thing for different queue family indices
        const auto family{ core::checked_cast<Queue *>( destQueue )->GetFamilyIndex() };
        if ( family == mQueueFamilyIndex ) {
            return;
        }

        if ( !mRecording || mIsRenderScopeActive || !description.mTexture ) {
            MKT_THROW_RUNTIME_ERROR( "Ownership barriers require a resource and a recording command list outside rendering." );
        }

        // Both halves must use explicit, identical layouts, independent of recording order.
        if ( description.mLayoutBefore == rhi::TextureLayoutBits::Unknown ||
             description.mLayoutAfter == rhi::TextureLayoutBits::Unknown ) {
            MKT_THROW_INVALID_ARGUMENT( "Texture ownership transfers require explicit before and after layouts." );
        }

        RetainResource( description.mTexture );

        CommitBarriers();

        VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
        barrier.srcStageMask = GetStageMask( description.mStageBefore );
        barrier.srcAccessMask = GetAccessMask( description.mAccessBefore );

        // The other half has no local synchronization scope.
        barrier.srcQueueFamilyIndex = mQueueFamilyIndex;
        barrier.dstQueueFamilyIndex = family;
        barrier.image = description.mTexture->GetNativeHandle( rhi::ObjectType::Vk_Image );
        barrier.oldLayout = vulkan::GetImageLayout( description.mLayoutBefore );
        barrier.newLayout = vulkan::GetImageLayout( description.mLayoutAfter );
        const auto &range{ description.mSubresourceSet };
        barrier.subresourceRange = {
            .aspectMask = GetAspectMask( description.mTexture->GetFormat() ),
            .baseMipLevel = range.mBaseMipLevel,
            .levelCount = range.mNumMipLevels == rhi::TextureSubresourceSet::kAllMipLevels ? VK_REMAINING_MIP_LEVELS : range.mNumMipLevels,
            .baseArrayLayer = range.mBaseArraySlice,
            .layerCount = range.mNumArraySlices == rhi::TextureSubresourceSet::kAllArraySlices ? VK_REMAINING_ARRAY_LAYERS : range.mNumArraySlices
        };

        VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &barrier;

        ++mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mCallCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mImageCount += dependency.imageMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mBufferCount += dependency.bufferMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mMemoryCount += dependency.memoryBarrierCount;

        vkCmdPipelineBarrier2( mCurrentCommandBuffer, &dependency );
    }


    auto CommandList::DecodeVideo( const VideoDecodeDescription &description ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mRecording || mIsRenderScopeActive || GetQueueType() != QueueType::eVideoDecode ) {
            MKT_THROW_RUNTIME_ERROR( "Video decoding requires a recording video command list outside rendering." );
        }

        auto decoderOwner{ description.mDecoder };
        auto *decoder{ core::checked_cast<VideoDecoder *>( decoderOwner.GetPtr() ) };

        if ( !decoder || !description.mPicture || !description.mOutput || !description.mReconstruction.mSurface ||
             !decoder->MatchesConfiguration( *description.mPicture ) ) {
            MKT_THROW_RUNTIME_ERROR( "Video decode operation has missing resources or changed H.264 parameters." );
        }

        const auto &bitstreamBuffer{ description.mBitstream };
        const auto &bitstreamRange{ description.mBitstreamRange };

        if ( !bitstreamBuffer || !bitstreamRange.mByteSize ||
             bitstreamRange.mByteOffset > bitstreamBuffer->GetSizeBytes() ||
             bitstreamRange.mByteSize > bitstreamBuffer->GetSizeBytes() - bitstreamRange.mByteOffset ) {
            MKT_THROW_RUNTIME_ERROR( "Video decode requires an uploaded picture range within the clip bitstream." );
        }

        RetainResource( decoder );
        RetainResource( bitstreamBuffer.GetPtr() );
        RetainResource( description.mOutput.GetPtr() );
        RetainResource( description.mReconstruction.mSurface.GetPtr() );

        // Bind active DPB slots and one free reconstruction slot to this coding scope.
        eastl::array<VkVideoReferenceSlotInfoKHR, 17> slots{};
        eastl::array<VkVideoPictureResourceInfoKHR, 17> pictures{};
        eastl::array<VkVideoDecodeH264DpbSlotInfoKHR, 17> h264Slots{};
        eastl::array<StdVideoDecodeH264ReferenceInfo, 17> references{};
        const auto count{ description.mReferences.size() };

        for ( usize index{}; index <= count; ++index ) {
            const auto &reference{ index == count ? description.mReconstruction : description.mReferences[index] };
            auto image{ reference.mSurface->GetImage() };
            RetainResource( reference.mSurface.GetPtr() );

            pictures[index] = initializers::VideoPictureResourceInfoKHR();
            pictures[index].codedExtent = decoder->GetExtent();
            pictures[index].imageViewBinding = image->GetNativeHandle( ObjectType::Vk_ImageView );
            references[index].FrameNum = reference.mFrameNumber;
            references[index].PicOrderCnt[0] = references[index].PicOrderCnt[1] = reference.mPictureOrderCount;
            references[index].flags.used_for_long_term_reference = reference.mLongTerm;
            h264Slots[index] = initializers::VideoDecodeH264DpbSlotInfoKHR();
            h264Slots[index].pStdReferenceInfo = &references[index];

            slots[index] = initializers::VideoReferenceSlotInfoKHR();
            slots[index].pNext = &h264Slots[index];
            slots[index].slotIndex = index == count ? -1 : static_cast<i32>( reference.mSlot );
            slots[index].pPictureResource = &pictures[index];
        }

        VkVideoBeginCodingInfoKHR begin{ initializers::VideoBeginCodingInfoKHR() };
        begin.videoSession = decoder->GetSession();
        begin.videoSessionParameters = decoder->GetParameters();
        begin.referenceSlotCount = static_cast<u32>( count + 1 );
        begin.pReferenceSlots = slots.data();
        vkCmdBeginVideoCodingKHR( mCurrentCommandBuffer, &begin );

        if ( description.mResetSession ) {
            VkVideoCodingControlInfoKHR reset{ initializers::VideoCodingControlInfoKHR() };
            reset.flags = VK_VIDEO_CODING_CONTROL_RESET_BIT_KHR;
            vkCmdControlVideoCodingKHR( mCurrentCommandBuffer, &reset );
        }

        const auto &slice{ description.mPicture->GetSlices().front() };

        StdVideoDecodeH264PictureInfo picture{};
        picture.seq_parameter_set_id = description.mPicture->GetSps().seq_parameter_set_id;
        picture.pic_parameter_set_id = slice.pic_parameter_set_id;
        picture.frame_num = slice.frame_num;
        picture.idr_pic_id = slice.idr_pic_id;
        picture.PicOrderCnt[0] = picture.PicOrderCnt[1] = description.mReconstruction.mPictureOrderCount;
        picture.flags.IdrPicFlag = description.mPicture->IsIdr();
        picture.flags.is_reference = description.mPicture->IsReference();
        picture.flags.is_intra = h264::is_slice_type( slice.slice_type, h264::SH_SLICE_TYPE_I );

        VkVideoDecodeH264PictureInfoKHR h264Picture{ initializers::VideoDecodeH264PictureInfoKHR() };
        h264Picture.pStdPictureInfo = &picture;
        h264Picture.sliceCount = static_cast<u32>( description.mPicture->GetSlices().size() );
        h264Picture.pSliceOffsets = description.mPicture->GetSliceOffsets().data();

        auto output{ description.mOutput->GetImage() };

        VkVideoDecodeInfoKHR decode{ initializers::VideoDecodeInfoKHR() };
        decode.pNext = &h264Picture;
        decode.srcBuffer = bitstreamBuffer->GetNativeHandle( ObjectType::Vk_Buffer );
        decode.srcBufferOffset = bitstreamRange.mByteOffset;
        decode.srcBufferRange = bitstreamRange.mByteSize;
        decode.dstPictureResource = initializers::VideoPictureResourceInfoKHR();
        decode.dstPictureResource.codedExtent = decoder->GetExtent();
        decode.dstPictureResource.imageViewBinding = output->GetNativeHandle( ObjectType::Vk_ImageView );

        if ( decoder->UsesCoincidentOutput() ) {
            decode.dstPictureResource = pictures[count];
        }

        decode.referenceSlotCount = static_cast<u32>( count );
        decode.pReferenceSlots = count ? slots.data() : nullptr;

        auto setup{ slots[count] };
        setup.slotIndex = static_cast<i32>( description.mReconstruction.mSlot );
        decode.pSetupReferenceSlot = &setup;
        vkCmdDecodeVideoKHR( mCurrentCommandBuffer, &decode );

        auto end{ initializers::VideoEndCodingInfoKHR() };
        vkCmdEndVideoCodingKHR( mCurrentCommandBuffer, &end );
    }

    auto CommandList::CommitBarriers() -> void {
        MKT_BEGIN_PROFILER();

        if ( mBufferBarriers.empty() && mImageBarriers.empty() ) {
            return;
        }

        if ( mIsRenderScopeActive ) {
            MKT_ASSERT( false, "Pipeline barriers must be committed before beginning dynamic rendering." );
            return;
        }

        VkDependencyInfo depInfo{};

        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

        depInfo.imageMemoryBarrierCount = core::as<core::u32>( mImageBarriers.size() );
        depInfo.pImageMemoryBarriers = mImageBarriers.data();

        depInfo.bufferMemoryBarrierCount = core::as<core::u32>( mBufferBarriers.size() );
        depInfo.pBufferMemoryBarriers = mBufferBarriers.data();

        ++mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mCallCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mImageCount += depInfo.imageMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mBufferCount += depInfo.bufferMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mMemoryCount += depInfo.memoryBarrierCount;
        vkCmdPipelineBarrier2( mCurrentCommandBuffer, &depInfo );

        mBufferBarriers.clear();
        mImageBarriers.clear();
    }

    auto CommandList::SetTransition( renderer::rhi::IBuffer *buffer, renderer::rhi::ResourceStates newState ) -> void {
        MKT_BEGIN_PROFILER();
        // https://docs.vulkan.org/guide/latest/synchronization.html#synchronization
        // https://docs.vulkan.org/samples/latest/samples/performance/pipeline_barriers/README.html

        // https://www.rastergrid.com/blog/gpu-tech/2026/03/vulkan-memory-barriers-and-image-layouts-explained/
        // https://docs.vulkan.org/samples/latest/samples/performance/pipeline_barriers/README.html
        // https://gpuopen.com/learn/vulkan-barriers-explained/
        // https://cpp-rendering.io/barriers-vulkan-not-difficult/

        RetainResource( buffer );

        const ResourceStates oldState{ buffer->GetResourceState() };

        // If the dynamicRenderingLocalRead feature is not enabled, and vkCmdPipelineBarrier2 is called within a
        // render pass instance started with vkCmdBeginRendering, there must be no buffer or image memory barriers specified by this command
        // https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdPipelineBarrier2.html
        if ( mIsRenderScopeActive ) {
            MKT_ASSERT( false, "Buffer transitions must be recorded before beginning dynamic rendering." );
            return;
        }

        VkBufferMemoryBarrier2 barrier{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
            .srcStageMask = GetStageMask( oldState ),
            .srcAccessMask = GetAccessMask( oldState ),
            .dstStageMask = GetStageMask( newState ),
            .dstAccessMask = GetAccessMask( newState ),
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .buffer = buffer->GetNativeHandle( renderer::rhi::ObjectType::Vk_Buffer ),
            .offset = 0,
            .size = VK_WHOLE_SIZE
        };

        VkDependencyInfo dependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .bufferMemoryBarrierCount = 1,
            .pBufferMemoryBarriers = MKT_ADDRESSOF( barrier )
        };

        ++mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mCallCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mImageCount += dependencyInfo.imageMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mBufferCount += dependencyInfo.bufferMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mMemoryCount += dependencyInfo.memoryBarrierCount;
        vkCmdPipelineBarrier2( mCurrentCommandBuffer, MKT_ADDRESSOF( dependencyInfo ) );

        if ( newState != renderer::rhi::ResourceStates::eUnknown ) {
            buffer->SetResourceState( newState );
        }
    }

    auto CommandList::SetTransition( renderer::rhi::ITexture *texture, renderer::rhi::ResourceStates newState ) -> void {
        MKT_BEGIN_PROFILER();

        RetainResource( texture );

        const renderer::rhi::ResourceStates oldState{ texture->GetResourceState() };

        if ( mIsRenderScopeActive ) {
            MKT_ASSERT( false, "Texture transitions must be recorded before beginning dynamic rendering." );
            return;
        }

        VkImageMemoryBarrier2 barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = GetStageMask( oldState ),
            .srcAccessMask = GetAccessMask( oldState ),
            .dstStageMask = GetStageMask( newState ),
            .dstAccessMask = GetAccessMask( newState ),
            .oldLayout = GetImageLayout( oldState ),
            .newLayout = GetImageLayout( newState ),
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = texture->GetNativeHandle( renderer::rhi::ObjectType::Vk_Image ),
            .subresourceRange = {
                .aspectMask = GetAspectMask( texture->GetFormat() ),
                .baseMipLevel = 0,
                .levelCount = texture->GetMipLevelCount(),
                .baseArrayLayer = 0,
                .layerCount = texture->GetDimension() == renderer::rhi::TextureDimension::eTextureCube ? renderer::rhi::kMaxCubeFaces : 1 }
        };

        VkDependencyInfo dependencyInfo{
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = MKT_ADDRESSOF( barrier )
        };

        ++mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mCallCount;

        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mImageCount += dependencyInfo.imageMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mBufferCount += dependencyInfo.bufferMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mMemoryCount += dependencyInfo.memoryBarrierCount;

        vkCmdPipelineBarrier2( mCurrentCommandBuffer, MKT_ADDRESSOF( dependencyInfo ) );

        if ( newState != renderer::rhi::ResourceStates::eUnknown ) {
            texture->SetResourceState( newState );
        }
    }

    auto CommandList::SetEnableAutomaticBarriers( bool enable ) -> void {
        MKT_BEGIN_PROFILER();
        mEnableAutomaticBarriers = enable;
    }

    auto CommandList::BindShaderTable( rhi::IShaderTable *table ) -> void {
        MKT_BEGIN_PROFILER();
        RetainResource( table );

        mBoundShaderTable = table;
    }

    auto CommandList::DispatchRays( const rhi::RayTracingDispatchDescription &desc ) -> void {
        MKT_BEGIN_PROFILER();

        RayTracingPipeline *pPipeline{ core::checked_cast<RayTracingPipeline *>( mBoundRayTracingPipeline ) };
        ShaderTable *pShaderTable{ core::checked_cast<ShaderTable *>( mBoundShaderTable ) };

        RetainResource( pPipeline );
        RetainResource( pShaderTable->GetBuffer().GetPtr() );

        // Vulkan expects regions in ray-generation, miss, hit-group and callable order.
        eastl::fixed_vector<VkStridedDeviceAddressRegionKHR, 4, false> regions{};
        regions.push_back( pShaderTable->GetRegion( ShaderTable::AddressRegion::eRayGeneration ) );
        regions.push_back( pShaderTable->GetRegion( ShaderTable::AddressRegion::eRayMiss ) );
        regions.push_back( pShaderTable->GetRegion( ShaderTable::AddressRegion::eHitGroup ) );
        regions.push_back( pShaderTable->GetRegion( ShaderTable::AddressRegion::eCallable ) );

        ++mRecordingContext[mRecordingContextIndex].mStatistics.mDispatchCallCount;
        vkCmdTraceRaysKHR( mCurrentCommandBuffer,
            &regions[0],
            &regions[1],
            &regions[2],
            &regions[3],
            desc.mWidth, desc.mHeight, desc.mDepth );
    }

    auto CommandList::SetTransition( const renderer::rhi::TransitionDescription &description ) -> void {
        MKT_BEGIN_PROFILER();

        for ( const auto &transition: description.mBuffers ) {
            SetTransition( transition.mBuffer, transition.mState );
        }

        for ( const auto &transition: description.mTextures ) {
            SetTransition( transition.mTexture, transition.mState );
        }
    }

    // https://docs.vulkan.org/guide/latest/synchronization_examples.html
    auto Device::GetDenoiserInputSizes( const DenoiserDescription &description ) -> DenoiserInputSizes {
        if ( description.mType != ImageDenoiserType::eDlssRayReconstruction ) {
            return IGpuDevice::GetDenoiserInputSizes( description );
        }
        const auto sizes{ GetDlssContext().GetInputSizes( { description.mOutputSize.x, description.mOutputSize.y } ) };
        return {
            .mOptimal = { sizes.mOptimal.width, sizes.mOptimal.height },
            .mMinimum = { sizes.mMinimum.width, sizes.mMinimum.height },
            .mMaximum = { sizes.mMaximum.width, sizes.mMaximum.height },
            .mSupport = sizes.mSupport == DlssSupport::ePreparing ? DenoiserSupport::ePreparing :
                sizes.mSupport == DlssSupport::eAvailable ? DenoiserSupport::eAvailable : DenoiserSupport::eUnavailable,
        };
    }

    auto Device::CreateDenoiser( const DenoiserDescription &description ) -> DenoiserHandle {
        auto denoiser{ core::Ref<Denoiser>::New( description ) };
        denoiser->DeviceObject::Initialize( this );
        return denoiser;
    }

    auto CommandList::InitializeDenoiser( IDenoiser *denoiser ) -> bool {
        RetainResource( denoiser );
        return core::checked_cast<Denoiser *>( denoiser )->RecordInitialization( mCurrentCommandBuffer );
    }

    auto CommandList::DispatchDenoiser( IDenoiser *denoiser, const DenoiserDispatch &dispatch ) -> DenoiserResult {
        RetainResource( denoiser );
        const eastl::array<ITexture *, 7> textures{
            dispatch.mColor,
            dispatch.mDiffuseAlbedo,
            dispatch.mSpecularAlbedo,
            dispatch.mNormalRoughness,
            dispatch.mDepth,
            dispatch.mMotion,
            dispatch.mOutput,
        };
        for ( auto *texture: textures ) {
            if ( texture ) {
                RetainResource( texture );
            }
        }
        const auto result{ core::checked_cast<Denoiser *>( denoiser )->RecordEvaluation( mCurrentCommandBuffer, dispatch ) };
        if ( result == DenoiserResult::eSuccess ) {
            ++mRecordingContext[mRecordingContextIndex].mStatistics.mDispatchCallCount;
        }
        return result;
    }

    auto CommandList::SetBarrier( const renderer::rhi::BarrierDescription &description ) -> void {
        MKT_BEGIN_PROFILER();

        // Enhanced barriers honor the caller's explicit synchronization scopes.
        // TODO: Keep reusable barrier vectors on CommandList and clear them here.
        // Retaining capacity avoids repeated heap allocations when local vectors
        // overflow, while allowing barrier batches to grow beyond the inline limit.
        eastl::fixed_vector<VkMemoryBarrier2, 4> memory{};
        for ( const auto &barrier: description.mMemory ) {
            memory.push_back( VkMemoryBarrier2{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = GetStageMask( barrier.mStageBefore ),
                .srcAccessMask = GetAccessMask( barrier.mAccessBefore ),
                .dstStageMask = GetStageMask( barrier.mStageAfter ),
                .dstAccessMask = GetAccessMask( barrier.mAccessAfter ),
            } );
        }
        eastl::fixed_vector<VkImageMemoryBarrier2, 30> images{};
        eastl::fixed_vector<VkBufferMemoryBarrier2, 30> buffers{};

        for ( const renderer::rhi::BufferBarrierDescription &barrierDescription: description.mBuffers ) {
            renderer::rhi::IBuffer *buffer{ barrierDescription.mBuffer };

            if ( !buffer ) {
                continue;
            }

            RetainResource( buffer );

            buffers.push_back(
                VkBufferMemoryBarrier2{
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
                    .pNext = nullptr,
                    .srcStageMask = GetStageMask( barrierDescription.mStageBefore ),
                    .srcAccessMask = GetAccessMask( barrierDescription.mAccessBefore ),
                    .dstStageMask = GetStageMask( barrierDescription.mStageAfter ),
                    .dstAccessMask = GetAccessMask( barrierDescription.mAccessAfter ),
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .buffer = buffer->GetNativeHandle( renderer::rhi::ObjectType::Vk_Buffer ),
                    .offset = barrierDescription.mRange.mByteOffset,
                    .size = barrierDescription.mRange.mByteSize == 0 ? VK_WHOLE_SIZE : barrierDescription.mRange.mByteSize } );
        }

        for ( const renderer::rhi::TextureBarrierDescription &barrierDescription: description.mTextures ) {
            Texture *texture{ core::checked_cast<Texture *>( barrierDescription.mTexture ) };

            if ( !texture ) {
                continue;
            }

            RetainResource( texture );

            VkImageMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };

            barrier.srcStageMask = GetStageMask( barrierDescription.mStageBefore );
            barrier.srcAccessMask = GetAccessMask( barrierDescription.mAccessBefore );

            barrier.dstStageMask = GetStageMask( barrierDescription.mStageAfter );
            barrier.dstAccessMask = GetAccessMask( barrierDescription.mAccessAfter );

            const VkImageLayout trackedLayout{ GetImageLayout( texture->GetResourceState() ) };

            barrier.oldLayout = vulkan::GetImageLayout( barrierDescription.mLayoutBefore );
            barrier.newLayout = vulkan::GetImageLayout( barrierDescription.mLayoutAfter );

            // Keep old layouts if nothing changes or wrong layout is specified
            if ( barrier.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ) {
                barrier.oldLayout = trackedLayout;
            }

            if ( barrier.newLayout == VK_IMAGE_LAYOUT_UNDEFINED ) {
                barrier.oldLayout;
            }

            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = *texture;

            if ( barrier.oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR ) {
                barrier.srcAccessMask = VK_ACCESS_2_NONE;
            }

            if ( barrier.newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR ) {
                barrier.dstAccessMask = VK_ACCESS_2_NONE;
            }

            const auto &subResources{ barrierDescription.mSubresourceSet };
            barrier.subresourceRange = {
                .aspectMask = GetAspectMask( texture->GetFormat() ),
                .baseMipLevel = subResources.mBaseMipLevel,
                .levelCount = subResources.mNumMipLevels == renderer::rhi::TextureSubresourceSet::kAllMipLevels ? VK_REMAINING_MIP_LEVELS : subResources.mNumMipLevels,
                .baseArrayLayer = subResources.mBaseArraySlice,
                .layerCount = subResources.mNumArraySlices == renderer::rhi::TextureSubresourceSet::kAllArraySlices ? VK_REMAINING_ARRAY_LAYERS : subResources.mNumArraySlices
            };

            images.push_back( barrier );
            texture->SetResourceState( vulkan::GetResourceState( barrier.newLayout ) );
        }

        VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };

        dependency.memoryBarrierCount = core::as<core::u32>( memory.size() );
        dependency.pMemoryBarriers = memory.data();
        dependency.bufferMemoryBarrierCount = core::as<core::u32>( buffers.size() );
        dependency.pBufferMemoryBarriers = buffers.data();
        dependency.imageMemoryBarrierCount = core::as<core::u32>( images.size() );
        dependency.pImageMemoryBarriers = images.data();

        if ( dependency.memoryBarrierCount || dependency.bufferMemoryBarrierCount || dependency.imageMemoryBarrierCount ) {
            ++mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mCallCount;
            mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mImageCount += dependency.imageMemoryBarrierCount;
            mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mBufferCount += dependency.bufferMemoryBarrierCount;
            mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mMemoryCount += dependency.memoryBarrierCount;
            vkCmdPipelineBarrier2( mCurrentCommandBuffer, &dependency );
        }
    }


    auto CommandList::SetClearColor( renderer::rhi::ITexture *renderTarget, renderer::rhi::Color color ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !renderTarget ) {
            return;
        }


        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

        RetainResource( renderTarget );

        // Image needs to be VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        if ( mEnableAutomaticBarriers ) {
            SetTransition( renderTarget, renderer::rhi::ResourceStates::eCopyDest );
        }

        VkClearColorValue clearColor{ { color.mR, color.mG, color.mB, color.mA } };

        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel = 0;

        // Clear all mip levels starting from baseMipLevel
        range.levelCount = VK_REMAINING_MIP_LEVELS;
        range.baseArrayLayer = 0;

        range.levelCount = VK_REMAINING_MIP_LEVELS;
        range.baseArrayLayer = 0;

        // Clear all layers starting from baseArrayLayer
        range.layerCount = VK_REMAINING_ARRAY_LAYERS;

        vkCmdClearColorImage( mCurrentCommandBuffer,
            renderTarget->GetNativeHandle( renderer::rhi::ObjectType::Vk_Image ),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range );
    }

    auto CommandList::Write( renderer::rhi::IBuffer *src, renderer::rhi::ITexture *dest ) -> void {
        MKT_BEGIN_PROFILER();

        RetainResource( src );
        RetainResource( dest );

        // Describe the region to copy
        VkBufferImageCopy copyRegion{};
        copyRegion.bufferOffset = 0;
        copyRegion.bufferRowLength = 0;  // Tightly packed
        copyRegion.bufferImageHeight = 0;// Tightly packed

        copyRegion.imageSubresource.aspectMask = GetAspectMask( dest->GetFormat() );

        copyRegion.imageSubresource.mipLevel = 0;
        copyRegion.imageSubresource.baseArrayLayer = 0;
        copyRegion.imageSubresource.layerCount = 1;

        copyRegion.imageOffset = { 0, 0, 0 };
        copyRegion.imageExtent = {
            dest->GetWidth(),
            dest->GetHeight(),
            dest->GetDepth()
        };

        // If called on the owner thread we use the primary
        // otherwise we use a secondary
        vkCmdCopyBufferToImage(
            mCurrentCommandBuffer,
            src->GetNativeHandle( renderer::rhi::ObjectType::Vk_Buffer ),
            dest->GetNativeHandle( renderer::rhi::ObjectType::Vk_Image ),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion );
    }

    auto CommandList::Write( renderer::rhi::ITexture *texture, const void *data, core::usize byteSize ) -> void {
        MKT_BEGIN_PROFILER();

        RetainResource( texture );

        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

        if ( mEnableAutomaticBarriers ) {
            SetTransition( texture, renderer::rhi::ResourceStates::eCopyDest );
        }

        // Each subresource gets an aligned upload range, including small mips.
        for ( const auto &region: rhi::GetTextureUploadRegions( *texture, byteSize ) ) {
            auto *allocation{ mUploadManager->SubAllocate( region.mSize, eastl::max<core::usize>( 4, rhi::GetFormatInfo( texture->GetFormat() ).mBytesPerBlock ) ) };
            SetTransition( allocation->mBuffer, rhi::ResourceStates::eCopySource );
            std::memcpy( allocation->mMappedMemory, static_cast<const core::u8 *>( data ) + region.mOffset, region.mSize );
            VkBufferImageCopy copy{};
            copy.bufferOffset = allocation->mOffset;
            copy.imageSubresource = { GetAspectMask( texture->GetFormat() ), region.mMip, region.mLayer, 1 };
            copy.imageExtent = { region.mWidth, region.mHeight, region.mDepth };
            vkCmdCopyBufferToImage( mCurrentCommandBuffer,
                allocation->mBuffer->GetNativeHandle( rhi::ObjectType::Vk_Buffer ),
                texture->GetNativeHandle( rhi::ObjectType::Vk_Image ), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy );
            mRecordingContext[mRecordingContextIndex].mInFlightSubAllocations.emplace_back( allocation );
        }
    }

    auto CommandList::Write( renderer::rhi::IBuffer *buffer, core::usize destOffset, const void *data, core::usize byteSize ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !buffer || buffer->GetOwningDevice() != mDevice || destOffset > buffer->GetSizeBytes() ||
             byteSize > buffer->GetSizeBytes() - destOffset || ( byteSize && !data ) ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid Vulkan buffer write destination or range." );
        }

        if ( byteSize == 0 ) {
            return;
        }

        // Upload heaps need a host write and flush, with no GPU recording or submission.
        if ( buffer->GetHeapType() == rhi::HeapType::eUpload ) {
            auto *destination{ core::checked_cast<Buffer *>( buffer ) };
            auto *mapped{ static_cast<core::u8 *>( destination->GetMappedAddress() ) };

            if ( !mapped ) {
                MKT_THROW_RUNTIME_ERROR( "Vulkan upload buffer is not mapped." );
            }

            std::memcpy( mapped + destOffset, data, byteSize );
            destination->FlushHostWrites( destOffset, byteSize );
            return;
        }

        if ( buffer->GetHeapType() != rhi::HeapType::eDeviceLocal ) {
            MKT_THROW_RUNTIME_ERROR( "Vulkan readback buffers cannot receive CPU uploads." );
        }

        RetainResource( buffer );

        // You cannot record transfer ops inside rendering
        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

        if ( mEnableAutomaticBarriers ) {
            SetTransition( buffer, renderer::rhi::ResourceStates::eCopyDest );
        }

        VkBuffer vkBuffer{ buffer->GetNativeHandle( renderer::rhi::ObjectType::Vk_Buffer ) };

        if ( byteSize <= 65536 ) {// 64 KB limit
            // I do not think I can simply update more bytes here what if I end up overwriting something
            // the caller does not expect, user must ensure buffer is multiple of 4??
            // I will leave it like this for now.
            vkCmdUpdateBuffer(
                mCurrentCommandBuffer,
                vkBuffer,
                destOffset,
                byteSize,
                data );
        }
        else {
            // Instead of using the GPU default allocator I can declare a linear allocator for every command buffer
            // On first usage (lazy create) I create the buffer with a large enough size
            // every time I call End() I just reset the allocator?
            GpuUploadAllocation *allocation{ mUploadManager->SubAllocate( byteSize ) };

            SetTransition( allocation->mBuffer, renderer::rhi::ResourceStates::eCopySource );

            std::memcpy( allocation->mMappedMemory, data, byteSize );

            VkBufferCopy copy{};
            copy.srcOffset = allocation->mOffset;
            copy.dstOffset = destOffset;
            copy.size = byteSize;

            vkCmdCopyBuffer(
                mCurrentCommandBuffer,
                allocation->mBuffer->GetNativeHandle( renderer::rhi::ObjectType::Vk_Buffer ),
                buffer->GetNativeHandle( renderer::rhi::ObjectType::Vk_Buffer ),
                1,
                &copy );

            mRecordingContext[mRecordingContextIndex].mInFlightSubAllocations.emplace_back( allocation );
        }
    }

    auto CommandList::Write( renderer::rhi::IBuffer *buffer, const void *data, core::usize byteSize ) -> void {
        MKT_BEGIN_PROFILER();
        Write( buffer, 0, data, byteSize );
    }

    auto CommandList::Draw( const renderer::rhi::DrawArguments &args ) -> void {
        MKT_BEGIN_PROFILER();
        ++mRecordingContext[mRecordingContextIndex].mStatistics.mDrawCallCount;
        vkCmdDraw( mCurrentCommandBuffer, args.mVertexCount, args.mInstanceCount, args.mFirstVertex, args.mFirstInstance );
    }

    auto CommandList::DrawIndexed( const renderer::rhi::DrawArguments &args ) -> void {
        MKT_BEGIN_PROFILER();
        ++mRecordingContext[mRecordingContextIndex].mStatistics.mDrawCallCount;
        vkCmdDrawIndexed(
            mCurrentCommandBuffer,
            args.mIndexCount,
            args.mInstanceCount,
            args.mFirstIndex,
            args.mVertexOffset,
            args.mFirstInstance );
    }

    auto CommandList::DrawIndirect( core::u32 offset, core::u32 drawCount ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIndirectBuffer ) {
            return;
        }

        Buffer *pIndirectBuffer{ core::checked_cast<Buffer *>( mIndirectBuffer ) };
        ++mRecordingContext[mRecordingContextIndex].mStatistics.mDrawCallCount;
        vkCmdDrawIndirect( mCurrentCommandBuffer,
            *pIndirectBuffer, offset, drawCount, MKT_SIZEOF( VkDrawIndirectCommand ) );
    }

    auto CommandList::DrawIndexedIndirect( core::u32 offset, core::u32 drawCount ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIndirectBuffer ) {
            return;
        }

        Buffer *pIndirectBuffer{ core::checked_cast<Buffer *>( mIndirectBuffer ) };
        ++mRecordingContext[mRecordingContextIndex].mStatistics.mDrawCallCount;
        vkCmdDrawIndexedIndirect( mCurrentCommandBuffer,
            *pIndirectBuffer, offset, drawCount, MKT_SIZEOF( VkDrawIndexedIndirectCommand ) );
    }

    auto CommandList::SetPushConstants( renderer::rhi::IPipelineLayout *pipelineLayout, const void *data, core::usize byteSize, renderer::rhi::ShaderFlags visibility ) -> void {
        MKT_BEGIN_PROFILER();

        RetainResource( pipelineLayout );

        if ( !data || byteSize == 0 || !pipelineLayout ) {
            return;
        }

        VkShaderStageFlags pcShaderStages{ GetShaderStageFlags( visibility ) };

        vkCmdPushConstants(
            mCurrentCommandBuffer,
            pipelineLayout->GetNativeHandle( renderer::rhi::ObjectType::Vk_PipelineLayout ),
            pcShaderStages,
            0,
            core::as<core::u32>( byteSize ),
            data );
    }

    auto CommandList::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() || !mDevice ) {
            return;
        }

        mDebugName = name;

        auto *device{ core::checked_cast<Device *>( mDevice ) };

        for ( const auto &ctx: mRecordingContext ) {
            device->SetDebugName( VK_OBJECT_TYPE_COMMAND_BUFFER, core::rc_cast<core::u64>( ctx.mCommandBuffer ), mDebugName );

            const eastl::string commandPoolTag{ string::Format( "CommandPool for command list: {}", mDebugName ) };
            device->SetDebugName( VK_OBJECT_TYPE_COMMAND_POOL, core::rc_cast<core::u64>( ctx.mCommandPool ), commandPoolTag );
        }
    }

    auto CommandList::SetBarrier( const renderer::rhi::BufferBarrierDescription &barrier ) -> void {
    }

    auto CommandList::SetBarrier( const renderer::rhi::TextureBarrierDescription &barrier ) -> void {
    }

    auto CommandList::Copy( renderer::rhi::IBuffer *src, renderer::rhi::IBuffer *dest ) -> void {
        MKT_BEGIN_PROFILER();
        Copy( src, dest, 0 );
    }

    auto CommandList::GetQueueType() const -> rhi::QueueType {
        return mQueueType;
    }

    auto CommandList::Build( rhi::IAccelerationStructure *resource ) -> void {
        MKT_BEGIN_PROFILER();

        AccelerationStructure *pAccelStructure{ core::checked_cast<AccelerationStructure *>( resource ) };

        if ( !pAccelStructure ) {
            MKT_THROW_RUNTIME_ERROR( "Acceleration structure belongs to a different backend or is null." );
        }

        RetainResource( resource );

        if ( pAccelStructure->GetType() == renderer::rhi::AccelStructureType::eTopLevel ) {
            for ( auto &instance: pAccelStructure->mDescription.mInstances ) {
                AccelerationStructure *bottom{ core::checked_cast<AccelerationStructure *>( instance.mBottomLevel.GetPtr() ) };

                if ( !bottom->IsBuilt() ) {
                    Build( bottom );
                }
            }
        }
        else {
            SetTransition( { pAccelStructure->mDescription.mVertices.GetPtr(), renderer::rhi::ResourceStates::eAccelStructBuildInput } );
            SetTransition( { pAccelStructure->mDescription.mIndices.GetPtr(), renderer::rhi::ResourceStates::eAccelStructBuildInput } );
        }

        CommitBarriers();

        // Build scratch is temporary. The recording retains it until queue completion,
        // so cached BLASes do not keep build scratch resident after the build finishes.
        // https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdBuildAccelerationStructuresKHR.html
        auto build{ pAccelStructure->mBuild };

        if ( pAccelStructure->mScratchSize ) {
            auto scratch{ mDevice->CreateBuffer( rhi::BufferCreateDescription{}
                .SetName( "Acceleration Structure Scratch" )
                .SetByteSize( pAccelStructure->mScratchSize + pAccelStructure->mScratchAlignment - 1 )
                .SetResourceType( rhi::ResourceType::eRawBuffer_UAV )
                .SetBufferUsage( rhi::BufferUsageFlagsBits::AccelStructureScratch ) ) };

            RetainResource( scratch.GetPtr() );
            build.scratchData.deviceAddress = memory::AlignUp( scratch->GetGpuDeviceAddress(), pAccelStructure->mScratchAlignment );
        }

        // Publish uploaded geometry and completed BLAS before consuming them.
        VkMemoryBarrier2 barrier{ VK_STRUCTURE_TYPE_MEMORY_BARRIER_2 };
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR | VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
        barrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR | VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR | VK_ACCESS_2_SHADER_READ_BIT;

        VkDependencyInfo dependency{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
        dependency.memoryBarrierCount = 1;
        dependency.pMemoryBarriers = &barrier;
        ++mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mCallCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mImageCount += dependency.imageMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mBufferCount += dependency.bufferMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mMemoryCount += dependency.memoryBarrierCount;
        vkCmdPipelineBarrier2( mCurrentCommandBuffer, &dependency );

        const VkAccelerationStructureBuildRangeInfoKHR *range{ &pAccelStructure->mRange };
        vkCmdBuildAccelerationStructuresKHR( mCurrentCommandBuffer, 1, &build, &range );

        // A following TLAS build or ray dispatch must observe these AS writes.
        ++mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mCallCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mImageCount += dependency.imageMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mBufferCount += dependency.bufferMemoryBarrierCount;
        mRecordingContext[mRecordingContextIndex].mStatistics.mBarriers.mMemoryCount += dependency.memoryBarrierCount;

        vkCmdPipelineBarrier2( mCurrentCommandBuffer, &dependency );

        pAccelStructure->mBuilt = true;
    }

    auto CommandList::Copy( renderer::rhi::IBuffer *src, renderer::rhi::IBuffer *dest, core::usize dstOffset ) -> void {
        MKT_BEGIN_PROFILER();

        RetainResource( src );
        RetainResource( dest );

        MKT_ASSERT( src != nullptr, "Source buffer cannot be null" );
        MKT_ASSERT( dest != nullptr, "Destination buffer cannot be null" );

        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

        const core::usize size{ src->GetSizeBytes() };

        // The data I’m copying fits inside the destination buffer, starting at dstOffset
        MKT_ASSERT( size <= ( dest->GetSizeBytes() - dstOffset ), "Destination buffer is too small" );

        if ( mEnableAutomaticBarriers ) {
            SetTransition( src, renderer::rhi::ResourceStates::eCopySource );
            SetTransition( dest, renderer::rhi::ResourceStates::eCopyDest );
        }

        VkBufferCopy region{};
        region.srcOffset = 0;
        region.dstOffset = dstOffset;
        region.size = size;

        vkCmdCopyBuffer(
            mCurrentCommandBuffer,
            src->GetNativeHandle( renderer::rhi::ObjectType::Vk_Buffer ),
            dest->GetNativeHandle( renderer::rhi::ObjectType::Vk_Buffer ),
            1,
            &region );
    }

    auto CommandList::Copy( renderer::rhi::IBuffer *dest, renderer::rhi::ITexture *src ) -> void {
        MKT_BEGIN_PROFILER();

        RetainResource( src );
        RetainResource( dest );

        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

        if ( mEnableAutomaticBarriers ) {
            SetTransition( src, renderer::rhi::ResourceStates::eCopySource );
            SetTransition( dest, renderer::rhi::ResourceStates::eCopyDest );
        }

        VkBufferImageCopy region{
            // Start of data in the buffer (in bytes).
            // Must be a multiple of the pixel size
            .bufferOffset = 0,

            // Number of pixels in a row for buffer layout.
            // Set to 0 if data is tightly packed with no extra padding.
            .bufferRowLength = 0,

            // Number of rows in the buffer for 3D layout padding.
            // Set to 0 if data is tightly packed with no extra padding.
            .bufferImageHeight = 0,

            // Target texture subresource options (mip levels, array layers).
            .imageSubresource = {
                .aspectMask = core::checked_cast<Texture *>( src )->GetAspectMask(),// Target is color data
                .mipLevel = 0,                                                      // Base mipmap level
                .baseArrayLayer = 0,                                                // Target the first layer
                .layerCount = 1                                                     // Copying exactly 1 layer
            },

            // Destination starting coordinates (x, y, z) within the image.
            .imageOffset = { 0, 0, 0 },

            // Size of the pixel region to copy.
            .imageExtent = {
                .width = src->GetWidth(),  // Width of the region in pixels
                .height = src->GetHeight(),// Height of the region in pixels
                .depth = src->GetDepth()   // Full voxel depth for volume readback
            }
        };

        eastl::array regions{ region };

        VkImage image{ src->GetNativeHandle( renderer::rhi::ObjectType::Vk_Image ) };
        VkBuffer buffer{ dest->GetNativeHandle( renderer::rhi::ObjectType::Vk_Buffer ) };

        vkCmdCopyImageToBuffer( mCurrentCommandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, core::as<core::u32>( regions.size() ), regions.data() );
    }

    auto CommandList::Copy( rhi::IBuffer *dest, rhi::ITexture *src, const renderer::rhi::TextureSlice &srcSlice ) -> void {
        MKT_BEGIN_PROFILER();

        RetainResource( src );
        RetainResource( dest );

        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

        if ( mEnableAutomaticBarriers ) {
            SetTransition( src, renderer::rhi::ResourceStates::eCopySource );
            SetTransition( dest, renderer::rhi::ResourceStates::eCopyDest );
        }

        VkBufferImageCopy region{
            // Start of data in the buffer (in bytes).
            // Must be a multiple of the pixel size
            .bufferOffset = 0,

            // Number of pixels in a row for buffer layout.
            // Set to 0 if data is tightly packed with no extra padding.
            .bufferRowLength = 0,

            // Number of rows in the buffer for 3D layout padding.
            // Set to 0 if data is tightly packed with no extra padding.
            .bufferImageHeight = 0,

            // Target texture subresource options (mip levels, array layers).
            .imageSubresource = {
                .aspectMask = core::checked_cast<Texture *>( src )->GetAspectMask(),
                .mipLevel = srcSlice.mMipLevel,
                .baseArrayLayer = srcSlice.mArrayLayer,
                .layerCount = 1 },

            // Destination starting coordinates (x, y, z) within the image.
            .imageOffset = { ( core::i32 )srcSlice.x, ( core::i32 )srcSlice.y, ( core::i32 )srcSlice.z },

            // Size of the pixel region to copy.
            .imageExtent = { .width = srcSlice.mWidth, .height = srcSlice.mHeight, .depth = srcSlice.mDepth }
        };

        eastl::array regions{ region };

        VkImage image{ src->GetNativeHandle( renderer::rhi::ObjectType::Vk_Image ) };
        VkBuffer buffer{ dest->GetNativeHandle( renderer::rhi::ObjectType::Vk_Buffer ) };

        vkCmdCopyImageToBuffer( mCurrentCommandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, core::as<core::u32>( regions.size() ), regions.data() );
    }

    auto CommandList::BeginRenderPass( renderer::rhi::RenderPassDescription &state ) -> void {
        MKT_BEGIN_PROFILER();

        RetainResource( state.mDepthTarget.mRenderTarget.GetPtr() );

        for ( const auto &target: state.mCurrentRenderTargets ) {
            RetainResource( target.mRenderTarget.GetPtr() );
        }

        bool hasColorTarget{ !state.mCurrentRenderTargets.empty() };
        bool hasDepthTarget{ !state.mDepthTarget.mRenderTarget.IsEmpty() };

        MKT_ASSERT( hasColorTarget || hasDepthTarget, "Must provide either depth target or color target(s)" );

#if MIKOTO_DEBUG
        // Validate against every attached image, not against the requested tile's size.
        // Never move a bad rectangle to the origin: doing so clears unrelated atlas pages.
        const auto validateArea = [&]( const renderer::rhi::ITexture &texture, core::u32 mip ) {
            MKT_ASSERT( mip < texture.GetMipLevelCount() && mip < 32, "Render-target mip level out of bounds" );

            const core::u32 width{ eastl::max( 1u, texture.GetWidth() >> mip ) };
            const core::u32 height{ eastl::max( 1u, texture.GetHeight() >> mip ) };

            const auto &area{ state.mRenderArea };

            MKT_ASSERT( RenderAreaFits( area.mMinX, area.mMinY, area.mMaxX, area.mMaxY, width, height ),
                string::Format( "Render area [{}, {}) x [{}, {}) exceeds attachment mip {} dimensions {} x {} or is empty/negative.",
                    area.mMinX, area.mMaxX, area.mMinY, area.mMaxY, mip, width, height ) );
        };

        for ( const auto &target: state.mCurrentRenderTargets ) {
            validateArea( *target.mRenderTarget, target.mSubresourceSet.mBaseMipLevel );
        }

        if ( hasDepthTarget ) {
            // The depth attachment below currently binds mip zero.
            validateArea( *state.mDepthTarget.mRenderTarget, 0 );
        }
#endif

        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

        if ( mEnableAutomaticBarriers ) {
            for ( auto &rt: state.mCurrentRenderTargets ) {
                SetTransition( rt.mRenderTarget.GetPtr(), renderer::rhi::ResourceStates::eRenderTarget );
            }

            if ( !state.mDepthTarget.mRenderTarget.IsEmpty() ) {
                if ( state.mDepthTarget.mLoadOp == renderer::rhi::LoadOp::eClear ) {
                    SetTransition( state.mDepthTarget.mRenderTarget.GetPtr(), renderer::rhi::ResourceStates::eDepthWrite );
                }
                else {
                    SetTransition( state.mDepthTarget.mRenderTarget.GetPtr(), renderer::rhi::ResourceStates::eDepthRead );
                }
            }
        }

        mRenderingScopeName = state.mName;

        if ( !mRenderingScopeName.empty() ) {
            VkDebugUtilsLabelEXT labelInfo{};
            labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            labelInfo.pLabelName = mRenderingScopeName.c_str();
            labelInfo.color[0] = 0.0f;
            labelInfo.color[1] = 0.0f;
            labelInfo.color[2] = 0.0f;
            labelInfo.color[3] = 0.0f;

            vkCmdBeginDebugUtilsLabelEXT( mCurrentCommandBuffer, &labelInfo );
        }

        eastl::fixed_vector<VkRenderingAttachmentInfo, renderer::rhi::kMaxRenderTargets> colorImages{};

        for ( const auto &renderTargetProps: state.mCurrentRenderTargets ) {
            const Texture *texture{ core::checked_cast<const Texture *>( renderTargetProps.mRenderTarget.GetPtr() ) };

            VkAttachmentLoadOp loadOp{ renderTargetProps.mLoadOp == renderer::rhi::LoadOp::eClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD };
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
                renderTargetProps.mClearColor.mA
            };
        }

        VkRenderingAttachmentInfo depthAttachment{};

        if ( hasDepthTarget ) {
            const Texture *texture{ core::checked_cast<const Texture *>( state.mDepthTarget.mRenderTarget.GetPtr() ) };
            VkAttachmentLoadOp loadOp{ state.mDepthTarget.mLoadOp == renderer::rhi::LoadOp::eClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD };

            depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depthAttachment.imageView = texture->GetNativeHandle( renderer::rhi::ObjectType::Vk_ImageView );// Depth for now defaults to mip 0
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
            { ( core::u32 )state.mRenderArea.ComputeWidth(), ( core::u32 )state.mRenderArea.ComputeHeight() }
        };
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = core::as<core::u32>( colorImages.size() );
        renderingInfo.pColorAttachments = colorImages.data();
        renderingInfo.pDepthAttachment = !hasDepthTarget ? nullptr : MKT_ADDRESSOF( depthAttachment );

        vkCmdBeginRendering( mCurrentCommandBuffer, std::addressof( renderingInfo ) );

        mIsRenderScopeActive = true;
    }

    auto CommandList::EndRenderPass() -> void {
        MKT_BEGIN_PROFILER();

        vkCmdEndRendering( mCurrentCommandBuffer );

        if ( !mRenderingScopeName.empty() ) {
            vkCmdEndDebugUtilsLabelEXT( mCurrentCommandBuffer );
        }

        mIsRenderScopeActive = false;
    }

    auto CommandList::BindPipeline( renderer::rhi::IPipeline *pipeline ) -> void {
        MKT_BEGIN_PROFILER();

        RetainResource( pipeline );

        mBoundRayTracingPipeline = pipeline->GetPipelineType() == rhi::PipelineType::eRayTracing
                                       ? core::checked_cast<RayTracingPipeline *>( pipeline )
                                       : nullptr;

        VkPipelineBindPoint bindPoint{ VK_PIPELINE_BIND_POINT_MAX_ENUM };

        switch ( pipeline->GetPipelineType() ) {
            case renderer::rhi::PipelineType::eGraphics:
                bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                break;
            case renderer::rhi::PipelineType::eCompute:
                bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
                break;
            case rhi::PipelineType::eRayTracing:
                bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
                break;
            default:
                break;
        }

        ++mRecordingContext[mRecordingContextIndex].mStatistics.mPipelineBindCount;
        vkCmdBindPipeline( mCurrentCommandBuffer, bindPoint, pipeline->GetNativeHandle( renderer::rhi::ObjectType::Vk_Pipeline ) );
    }

    auto CommandList::SetViewport( eastl::span<const renderer::rhi::Viewport> viewports ) -> void {
        MKT_BEGIN_PROFILER();

        eastl::fixed_vector<VkViewport, renderer::rhi::kMaxViewports> vkViewports{};

        for ( const auto &viewport: viewports ) {
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
            if ( !viewport.mFlip ) {
                value.x = viewport.mMinX;
                value.y = viewport.mMaxY;
                value.width = viewport.GetWidth();
                value.height = -viewport.GetHeight();
                value.minDepth = viewport.mMinZ;
                value.maxDepth = viewport.mMaxZ;
            }

            vkViewports.emplace_back( value );
        }

        vkCmdSetViewport( mCurrentCommandBuffer, 0, core::as<core::u32>( vkViewports.size() ), vkViewports.data() );
    }

    auto CommandList::SetScissors( eastl::span<const renderer::rhi::Rect> scissorRects ) -> void {
        MKT_BEGIN_PROFILER();
        eastl::fixed_vector<VkRect2D, renderer::rhi::kMaxScissors> scissors{};

        for ( const auto &scissor: scissorRects ) {
            scissors.emplace_back( VkRect2D{
                .offset = { scissor.mMinX, scissor.mMinY },
                .extent = { core::as<core::u32>( scissor.ComputeWidth() ), core::as<core::u32>( scissor.ComputeHeight() ) } } );
        }

        vkCmdSetScissor( mCurrentCommandBuffer, 0, core::as<core::u32>( scissors.size() ), scissors.data() );
    }

    auto CommandList::SetViewportState( const renderer::rhi::ViewportState &vs ) -> void {
        MKT_BEGIN_PROFILER();
        SetViewport( vs.mViewports );
        SetScissors( vs.mScissorRects );
    }

    auto CommandList::SetPolygonLineWidth( core::f32 width ) -> void {
        MKT_BEGIN_PROFILER();
        Device *device{ core::checked_cast<Device *>( mDevice ) };
        const PhysicalDevice *pPhysicalDevice{ device->GetPhysicalDevice() };

        // Does physical device support wide lines
        if ( pPhysicalDevice->mFeatures.wideLines == VK_FALSE ) {
            return;
        }

        // Is wide lines enabled?
        if ( device->GetActivePhysicalDeviceFeatures2().features.wideLines == VK_FALSE ) {
            return;
        }

        core::f32 minLineWidth{ pPhysicalDevice->mProperties.limits.lineWidthRange[0] };
        core::f32 maxLineWidth{ pPhysicalDevice->mProperties.limits.lineWidthRange[1] };

        if ( !math::IsBetween( width, minLineWidth, maxLineWidth ) ) {
            MKT_CORE_LOGGER_ERROR( "Trying to use polygon line width '{}' out of device limits [{}, {}]", width, minLineWidth, maxLineWidth );
            width = rhi::kDefaultPolygonLineWidth;
        }

        vkCmdSetLineWidth( mCurrentCommandBuffer, width );
    }

    auto CommandList::BindIndexBuffer( renderer::rhi::IBuffer *buffer ) -> void {
        MKT_BEGIN_PROFILER();
        RetainResource( buffer );

        if ( mEnableAutomaticBarriers && buffer->GetResourceState() != rhi::ResourceStates::eIndexBuffer ) {
            SetTransition( buffer, rhi::ResourceStates::eIndexBuffer );
        }

        vkCmdBindIndexBuffer( mCurrentCommandBuffer, buffer->GetNativeHandle( renderer::rhi::ObjectType::Vk_Buffer ), 0, GetIndexType( buffer->GetFormat() ) );
    }

    auto CommandList::BindIndirectBuffer( renderer::rhi::IBuffer *buffer ) -> void {
        MKT_BEGIN_PROFILER();
        RetainResource( buffer );

        if ( mEnableAutomaticBarriers && buffer->GetResourceState() != rhi::ResourceStates::eIndirectArgument ) {
            SetTransition( buffer, rhi::ResourceStates::eIndirectArgument );
        }

        mIndirectBuffer = buffer;
    }

    auto CommandList::BindVertexBuffer( const renderer::rhi::VertexBufferBinding &binding ) -> void {
        MKT_BEGIN_PROFILER();
        eastl::array bindings{ binding };
        BindVertexBuffers( bindings );
    }

    auto CommandList::BindVertexBuffers( eastl::span<const renderer::rhi::VertexBufferBinding> bindings ) -> void {
        MKT_BEGIN_PROFILER();
        eastl::fixed_vector<VkDeviceSize, renderer::rhi::kMaxVertexBuffers> offsets{};
        eastl::fixed_vector<VkBuffer, renderer::rhi::kMaxVertexBuffers> buffers{};

        core::u32 firstBinding{};

        for ( const auto &binding: bindings ) {
            RetainResource( binding.mBuffer );
            offsets.emplace_back( binding.mOffset );

            if ( mEnableAutomaticBarriers && binding.mBuffer->GetResourceState() != rhi::ResourceStates::eVertexBuffer ) {
                SetTransition( binding.mBuffer, rhi::ResourceStates::eVertexBuffer );
            }

            firstBinding = eastl::min( binding.mSlot, firstBinding );

            VkBuffer buffer{ binding.mBuffer->GetNativeHandle( renderer::rhi::ObjectType::Vk_Buffer ) };
            buffers.emplace_back( buffer );
        }

        vkCmdBindVertexBuffers( mCurrentCommandBuffer, firstBinding, core::as<core::u32>( bindings.size() ), buffers.data(), offsets.data() );
    }

    auto CommandList::BindPipelineResources( const renderer::rhi::BindResourcesDescription &desc ) -> void {
        MKT_BEGIN_PROFILER();

        RetainResource( desc.mPipelineLayout );

        for ( const auto &set: desc.mResourceSets ) {
            RetainResource( set.second );

            if ( const auto *table{ dynamic_cast<const DescriptorTable *>( set.second ) } ) {
                std::lock_guard lock{ table->mDescriptorMutex };

                for ( const auto &[index, resource]: table->mResources ) {
                    RetainResource( resource.GetPtr() );
                }
            }
        }

        VkPipelineBindPoint bindPoint{ VK_PIPELINE_BIND_POINT_MAX_ENUM };

        switch ( desc.mBindPoint ) {
            case renderer::rhi::PipelineType::eGraphics:
                bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                break;
            case renderer::rhi::PipelineType::eCompute:
                bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
                break;
            case rhi::PipelineType::eRayTracing:
                bindPoint = VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR;
                break;
            default:
                break;
        }

        MKT_ASSERT( bindPoint != VK_PIPELINE_BIND_POINT_MAX_ENUM, "Invalid pipeline bind point" );

        if ( desc.mPushConstantSize != 0 ) {
            SetPushConstants( desc.mPipelineLayout, desc.mPushConstants.data(), desc.mPushConstantSize, desc.mPushConstantVisibility );
        }

        VkPipelineLayout layout{ desc.mPipelineLayout->GetNativeHandle( renderer::rhi::ObjectType::Vk_PipelineLayout ) };

        for ( const auto &resourceSet: desc.mResourceSets ) {
            // Get the interface because descriptor tables can also be IBindingSet
            const renderer::rhi::IBindingTable *set{ core::checked_cast<const renderer::rhi::IBindingTable *>( resourceSet.second ) };

            std::array<VkDescriptorSet, 1> sets{ set->GetNativeHandle( renderer::rhi::ObjectType::Vk_DescriptorSet ) };

            vkCmdBindDescriptorSets(
                mCurrentCommandBuffer,
                bindPoint,
                layout,
                resourceSet.first,
                core::as<core::u32>( sets.size() ),
                sets.data(),
                0,
                nullptr );
        }
    }

    auto CommandList::Copy( renderer::rhi::ITexture *src, const renderer::rhi::TextureSlice &srcSlice, renderer::rhi::ITexture *dest, const renderer::rhi::TextureSlice &destSlice ) -> void {
        MKT_BEGIN_PROFILER();

        RetainResource( src );
        RetainResource( dest );

        const auto srcTexture{ core::checked_cast<Texture *>( src ) };
        const auto dstTexture{ core::checked_cast<Texture *>( dest ) };

        MKT_ASSERT( srcTexture != nullptr, "Source Vulkan texture cannot be null" );
        MKT_ASSERT( dstTexture != nullptr, "Destination Vulkan texture cannot be null" );

        VkImage srcImage{ *srcTexture };
        VkImage dstImage{ *dstTexture };

        MKT_ASSERT( srcImage != VK_NULL_HANDLE, "Source Vulkan image is null" );
        MKT_ASSERT( dstImage != VK_NULL_HANDLE, "Destination Vulkan image is null" );

        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

        if ( mEnableAutomaticBarriers ) {
            SetTransition( srcTexture, renderer::rhi::ResourceStates::eCopySource );
            SetTransition( dstTexture, renderer::rhi::ResourceStates::eCopyDest );
        }

        // Compare the regions, not base texture sizes: copying a temporary
        // face into a smaller cubemap mip is still an exact image copy.
        const bool sameWidth{ srcSlice.mWidth == destSlice.mWidth };
        const bool sameHeight{ srcSlice.mHeight == destSlice.mHeight };
        const bool sameSize{ sameWidth && sameHeight && srcSlice.mDepth == destSlice.mDepth };
        const bool sameFormat{ srcTexture->GetFormat() == dstTexture->GetFormat() };

        // Prefer image copy when possible
        if ( sameSize && sameFormat ) {
            VkImageCopy2 copyRegion{ initializers::ImageCopy2() };

            // Source
            renderer::rhi::TextureSubresourceSet srcSubresourceSet{ srcSlice.mMipLevel, 1, srcSlice.mArrayLayer, 1 };
            copyRegion.srcSubresource.aspectMask = GetAspectMask( src->GetFormat() );
            copyRegion.srcSubresource.baseArrayLayer = srcSubresourceSet.mBaseArraySlice;
            copyRegion.srcSubresource.layerCount = srcSubresourceSet.mNumArraySlices;
            copyRegion.srcSubresource.mipLevel = srcSubresourceSet.mBaseMipLevel;
            copyRegion.srcOffset = { ( core::i32 )srcSlice.x, ( core::i32 )srcSlice.y, ( core::i32 )srcSlice.z };

            // Destination
            renderer::rhi::TextureSubresourceSet destSubresourceSet{ destSlice.mMipLevel, 1, destSlice.mArrayLayer, 1 };
            copyRegion.dstSubresource.aspectMask = GetAspectMask( dest->GetFormat() );
            copyRegion.dstSubresource.baseArrayLayer = destSubresourceSet.mBaseArraySlice;
            copyRegion.dstSubresource.layerCount = destSubresourceSet.mNumArraySlices;
            copyRegion.dstSubresource.mipLevel = destSubresourceSet.mBaseMipLevel;
            copyRegion.dstOffset = { ( core::i32 )destSlice.x, ( core::i32 )destSlice.y, ( core::i32 )destSlice.z };

            copyRegion.extent.width = srcSlice.mWidth;
            copyRegion.extent.height = srcSlice.mHeight;
            copyRegion.extent.depth = srcSlice.mDepth;

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
            vkCmdCopyImage2( mCurrentCommandBuffer, MKT_ADDRESSOF( copyInfo ) );
        }
        else {
            VkImageBlit2 blitRegion{ initializers::ImageBlit2() };

            // Source
            blitRegion.srcOffsets[0] = { 0, 0, 0 };
            blitRegion.srcOffsets[1].x = core::as<core::i32>( srcSlice.mWidth );
            blitRegion.srcOffsets[1].y = core::as<core::i32>( srcSlice.mHeight );
            blitRegion.srcOffsets[1].z = 1;

            // Destination
            blitRegion.dstOffsets[0] = { 0, 0, 0 };
            blitRegion.dstOffsets[1].x = core::as<core::i32>( destSlice.mWidth );
            blitRegion.dstOffsets[1].y = core::as<core::i32>( destSlice.mHeight );
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
            vkCmdBlitImage2( mCurrentCommandBuffer, MKT_ADDRESSOF( blitInfo ) );
        }
    }

    auto CommandList::Resolve( renderer::rhi::ITexture *src, const renderer::rhi::TextureSlice &srcSlice, renderer::rhi::ITexture *dest, const renderer::rhi::TextureSlice &destSlice ) -> void {
        MKT_BEGIN_PROFILER();

        RetainResource( src );
        RetainResource( dest );
        // To resolve a multisample color image to a non-multisample color image.
        const auto srcTexture{ core::checked_cast<Texture *>( src ) };
        const auto dstTexture{ core::checked_cast<Texture *>( dest ) };

        MKT_ASSERT( srcTexture != nullptr, "Source Vulkan texture cannot be null" );
        MKT_ASSERT( dstTexture != nullptr, "Destination Vulkan texture cannot be null" );

        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

        VkImage srcImage{ *srcTexture };
        VkImage dstImage{ *dstTexture };

        MKT_ASSERT( srcImage != VK_NULL_HANDLE, "Source Vulkan image is null" );
        MKT_ASSERT( dstImage != VK_NULL_HANDLE, "Destination Vulkan image is null" );

        if ( mEnableAutomaticBarriers ) {
            SetTransition( srcTexture, renderer::rhi::ResourceStates::eCopySource );
            SetTransition( dstTexture, renderer::rhi::ResourceStates::eCopyDest );
        }

        VkImageResolve2 imageResolve{};
        imageResolve.sType = VK_STRUCTURE_TYPE_IMAGE_RESOLVE_2;

        // Source
        renderer::rhi::TextureSubresourceSet srcSubresourceSet{ srcSlice.mMipLevel, 1, srcSlice.mArrayLayer, 1 };
        imageResolve.srcSubresource.aspectMask = GetAspectMask( src->GetFormat() );
        imageResolve.srcSubresource.baseArrayLayer = srcSubresourceSet.mBaseArraySlice;
        imageResolve.srcSubresource.layerCount = srcSubresourceSet.mNumArraySlices;
        imageResolve.srcSubresource.mipLevel = srcSubresourceSet.mBaseMipLevel;
        imageResolve.srcOffset = { ( core::i32 )srcSlice.x, ( core::i32 )srcSlice.y, ( core::i32 )srcSlice.z };

        // Destination
        renderer::rhi::TextureSubresourceSet destSubresourceSet{ destSlice.mMipLevel, 1, destSlice.mArrayLayer, 1 };
        imageResolve.dstSubresource.aspectMask = GetAspectMask( dest->GetFormat() );
        imageResolve.dstSubresource.baseArrayLayer = destSubresourceSet.mBaseArraySlice;
        imageResolve.dstSubresource.layerCount = destSubresourceSet.mNumArraySlices;
        imageResolve.dstSubresource.mipLevel = destSubresourceSet.mBaseMipLevel;
        imageResolve.dstOffset = { ( core::i32 )destSlice.x, ( core::i32 )destSlice.y, ( core::i32 )destSlice.z };

        imageResolve.extent = { core::as<core::u32>( src->GetWidth() ), core::as<core::u32>( src->GetHeight() ), 1 };

        VkResolveImageInfo2 info{};
        info.sType = VK_STRUCTURE_TYPE_RESOLVE_IMAGE_INFO_2;
        info.srcImage = srcImage;
        info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        info.dstImage = dstImage;
        info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        info.regionCount = 1;
        info.pRegions = &imageResolve;

        vkCmdResolveImage2( mCurrentCommandBuffer, &info );
    }

    auto CommandList::Dispatch( core::u32 x, core::u32 y, core::u32 z ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsRenderScopeActive ) {
            EndRenderPass();
        }

        ++mRecordingContext[mRecordingContextIndex].mStatistics.mDispatchCallCount;
        vkCmdDispatch( mCurrentCommandBuffer, x, y, z );
    }

    auto CommandList::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( type != renderer::rhi::ObjectType::Vk_CmdBuffer ) {
            return renderer::rhi::Object( nullptr );
        }

        return renderer::rhi::Object( mCurrentCommandBuffer );
    }

    auto CommandList::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( type != renderer::rhi::ObjectType::Vk_CmdBuffer ) {
            return renderer::rhi::Object( nullptr );
        }

        return renderer::rhi::Object( mCurrentCommandBuffer );
    }

    auto CommandList::Reset( rhi::IQueryPool *queryPool ) -> void {
        auto &pool{ ValidateQueryPool( queryPool ) };
        auto &commands{ *this };

        if ( pool.mActive != ~0u || static_cast<CommandList &>( commands ).mIsRenderScopeActive ) {
            MKT_THROW_RUNTIME_ERROR( "Query reset requires no active query or render pass." );
        }

        vkCmdResetQueryPool( ( RetainResource( &pool ), mCurrentCommandBuffer ), pool.mPool, 0, pool.mDescription.mCount );
        eastl::fill( pool.mStates.begin(), pool.mStates.end(), QueryPool::SlotState::eReady );
    }

    auto CommandList::BeginQuery( rhi::IQueryPool *queryPool, core::u32 index ) -> void {
        auto &pool{ ValidateQueryPool( queryPool ) };
        auto &commands{ *this };

        pool.ValidateRange( index, 1 );

        if ( pool.mDescription.mType == renderer::rhi::QueryType::eTimestamp || pool.mStates[index] != QueryPool::SlotState::eReady || pool.mActive != ~0u ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid Vulkan query begin." );
        }

        vkCmdBeginQuery( ( RetainResource( &pool ), mCurrentCommandBuffer ), pool.mPool, index, pool.mDescription.mPreciseOcclusion ? VK_QUERY_CONTROL_PRECISE_BIT : 0 );
        pool.mStates[index] = QueryPool::SlotState::eActive;
        pool.mActive = index;
        pool.mActiveCommands = &commands;
    }

    auto CommandList::EndQuery( rhi::IQueryPool *queryPool, core::u32 index ) -> void {
        auto &pool{ ValidateQueryPool( queryPool ) };
        auto &commands{ *this };

        pool.ValidateRange( index, 1 );

        if ( pool.mStates[index] != QueryPool::SlotState::eActive || pool.mActive != index || pool.mActiveCommands != &commands ) {
            MKT_THROW_RUNTIME_ERROR( "Vulkan query end has no matching begin." );
        }

        vkCmdEndQuery( ( RetainResource( &pool ), mCurrentCommandBuffer ), pool.mPool, index );
        pool.mStates[index] = QueryPool::SlotState::eEnded;
        pool.mActive = ~0u;
        pool.mActiveCommands = nullptr;
    }

    auto CommandList::WriteTimestamp( rhi::IQueryPool *queryPool, core::u32 index ) -> void {
        MKT_BEGIN_PROFILER();

        auto &pool{ ValidateQueryPool( queryPool ) };

        RetainResource( &pool );

        pool.ValidateRange( index, 1 );

        if ( pool.mDescription.mType != renderer::rhi::QueryType::eTimestamp || pool.mStates[index] != QueryPool::SlotState::eReady ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid Vulkan timestamp query." );
        }

        vkCmdWriteTimestamp2( mCurrentCommandBuffer, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, pool.mPool, index );
        pool.mStates[index] = QueryPool::SlotState::eEnded;
    }

    auto CommandList::Resolve( rhi::IQueryPool *queryPool, core::u32 first, core::u32 count ) -> void {
        auto &pool{ ValidateQueryPool( queryPool ) };
        RetainResource( &pool );

        pool.ValidateRange( first, count );

        for ( core::u32 index{ first }; index < first + count; ++index ) {
            if ( pool.mStates[index] != QueryPool::SlotState::eEnded ) {
                MKT_THROW_RUNTIME_ERROR( "Cannot resolve an unwritten Vulkan query." );
            }
        }

        eastl::fill( pool.mStates.begin() + first, pool.mStates.begin() + first + count, QueryPool::SlotState::eResolved );

        // Vulkan exposes host reads directly; the common interface still
        // requires Resolve so the D3D12 readback path has the same lifecycle.
    }

    auto CommandList::ValidateQueryPool( rhi::IQueryPool *queryPool ) const -> QueryPool & {
        auto *nativePool{ dynamic_cast<QueryPool *>( queryPool ) };

        if ( !nativePool ) {
            MKT_THROW_RUNTIME_ERROR( "Query pool belongs to a different backend or is null." );
        }

        auto &pool{ *nativePool };

        if ( !pool.IsReady() || this->mDevice != pool.mDevice || !this->mRecording ) {
            MKT_THROW_RUNTIME_ERROR( "Queries require an initialized pool and a recording command list from the same Vulkan device." );
        }

        if ( GetQueueType() != pool.mDescription.mQueue ) {
            MKT_THROW_RUNTIME_ERROR( "Query pool and command list use different queue types." );
        }

        return pool;
    }

#if defined( MIKOTO_ENABLE_TRACY )
    auto CommandList::CollectGpuProfile() -> void {
        if ( mQueueType != QueueType::eGraphics || !TracyIsConnected ) {
            return;
        }

#if defined( TRACY_ON_DEMAND )
        const core::u64 connection{ tracy::GetProfiler().ConnectionId() };
#else
        constexpr core::u64 connection{};
#endif
        auto &queue{ *core::checked_cast<Queue *>( mQueue ) };
        if ( queue.mGpuProfilerConnection != connection ) {
            queue.InitializeGpuProfiler( connection );
        }

        auto &profiler{ mRecordingContext[mRecordingContextIndex].mGpuProfiler };
        profiler = queue.mGpuProfiler;
        if ( profiler ) {
            // Record query resets in the same queue order as the profiled passes.
            TracyVkCollect( profiler.get(), mCurrentCommandBuffer );
        }
    }

    auto CommandList::BeginGpuProfileZone( eastl::string_view name, std::source_location source ) -> void {
        auto *profiler{ mRecordingContext[mRecordingContextIndex].mGpuProfiler.get() };
        if ( !profiler || !TracyIsConnected ) {
            return;
        }

        if ( !mGpuProfileZone ) {
            mGpuProfileZone = eastl::make_unique<GpuProfileZone>();
        }

        // Use Tracy's transient zone so dynamically named passes are copied into the trace.
        // https://github.com/wolfpld/tracy/blob/v0.14.1/public/tracy/TracyVulkan.hpp
        const eastl::string_view file{ source.file_name() };
        const eastl::string_view function{ source.function_name() };
        mGpuProfileZone->mScope.emplace( profiler, source.line(), file.data(), file.size(),
            function.data(), function.size(), name.data(), name.size(), mCurrentCommandBuffer, true );
    }

    auto CommandList::EndGpuProfileZone() -> void {
        if ( mGpuProfileZone ) {
            mGpuProfileZone->mScope.reset();
        }
    }
#endif

    auto CommandList::BeginDebugLabel( eastl::string_view name, renderer::rhi::Color color ) -> void {
        MKT_BEGIN_PROFILER();
        VkDebugUtilsLabelEXT labelInfo{};
        labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        labelInfo.pLabelName = name.data();
        labelInfo.color[0] = color.mR;
        labelInfo.color[1] = color.mG;
        labelInfo.color[2] = color.mB;
        labelInfo.color[3] = color.mA;

        vkCmdBeginDebugUtilsLabelEXT( mCurrentCommandBuffer, &labelInfo );
    }

    auto CommandList::EndDebugLabel() -> void {
        MKT_BEGIN_PROFILER();
        vkCmdEndDebugUtilsLabelEXT( mCurrentCommandBuffer );
    }


    auto CommandList::MarkExecuted( rhi::IQueue *queue, core::u64 submissionID ) -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mRecordingMutex };
        mRecordingContext[mRecordingContextIndex].mSubmissionID = submissionID;
        mReadyForSubmission = false;
    }

    auto CommandList::ValidateSubmission( const rhi::IQueue *queue ) const -> void {
        std::lock_guard lock{ mRecordingMutex };

        if ( queue != mQueue || mRecording || !mReadyForSubmission ) {
            MKT_THROW_RUNTIME_ERROR( "Vulkan command list must be closed and submitted once to its owning queue." );
        }
    }

    // TODO: Objects are created with a track state flag we can use that to determine
    // if this method needs to be called, if the client is already ensuring the resource will stay alive for the entire
    // duration of the CommandList execution than this responsibility can be offloaded to the client so we don't need to extend its lifetime.
    auto CommandList::RetainResource( const renderer::rhi::DeviceObject *resource ) -> void {
        if ( !resource ) {
            //MKT_CORE_LOGGER_WARN( "Trying to keep alive a null device object." );
            return;
        }

        if ( !mRecording ) {
            MKT_THROW_RUNTIME_ERROR( "Resources can only be retained while recording Vulkan commands." );
        }

        if ( resource == this ) {
            MKT_THROW_RUNTIME_ERROR( "A command list cannot retain itself." );
        }

        // Barriers and binds often mention the same object many times.
        // One reference per recording avoids allocation/refcount churn.
        mRecordingContext[mRecordingContextIndex].mInFlightResources.try_emplace(
            resource, const_cast<renderer::rhi::DeviceObject *>( resource ) );
    }

    auto CommandList::ReleaseCompletedResources() -> void {
        std::lock_guard lock{ mRecordingMutex };
        const auto completed{ core::checked_cast<Queue *>( mQueue )->GetCurrentTimeline() };

        for ( auto &context: mRecordingContext ) {
            // A recycled slot can already hold a new recording that has not
            // been submitted. Its old timeline value must not release it.
            if ( &context == &mRecordingContext[mRecordingContextIndex] && ( mRecording || mReadyForSubmission ) ) {
                continue;
            }

            if ( context.mSubmissionID <= completed ) {
                ClearState( context );
            }
        }
    }

    CommandList::~CommandList() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            CommandList::Destroy();
        }
    }

    auto CommandList::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        mUploadManager = core::checked_cast<Device *>( mDevice )->GetUploadManager();

        Device *pDevice{ core::checked_cast<Device *>( mDevice ) };
        mRecordingContext.resize( kMaxRecordingContext );

        for ( auto &item: mRecordingContext ) {
            VkCommandPoolCreateInfo createInfo{ initializers::CommandPoolCreateInfo() };
            createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            createInfo.queueFamilyIndex = mQueueFamilyIndex;

            MKT_VK_CHECK( vkCreateCommandPool(
                pDevice->GetDevice(),
                MKT_ADDRESSOF( createInfo ), nullptr,
                MKT_ADDRESSOF( item.mCommandPool ) ) );

            VkCommandBufferAllocateInfo allocInfo{ initializers::CommandBufferAllocateInfo() };
            allocInfo.commandPool = item.mCommandPool;
            allocInfo.commandBufferCount = 1;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

            MKT_VK_CHECK( vkAllocateCommandBuffers( pDevice->GetDevice(), MKT_ADDRESSOF( allocInfo ), MKT_ADDRESSOF( item.mCommandBuffer ) ) );
        }

        mIsAllocated = true;
    }

    auto CommandList::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        Device *pDevice{ core::checked_cast<Device *>( mDevice ) };

#if defined( MIKOTO_ENABLE_TRACY )
        EndGpuProfileZone();
#endif
        for ( auto &item: mRecordingContext ) {
            vkFreeCommandBuffers( pDevice->GetDevice(),
                item.mCommandPool, 1,
                MKT_ADDRESSOF( item.mCommandBuffer ) );

            vkDestroyCommandPool( pDevice->GetDevice(), item.mCommandPool, nullptr );

            for ( auto &subAllocations: item.mInFlightSubAllocations ) {
                // Set it to false we to tell the allocator
                // this allocation can already be destroyed
                subAllocations->mInUse.clear();
            }

            item.mInFlightResources.clear();
            item.mInFlightSubAllocations.clear();
        }

        mRecordingContext.clear();

        mIsAllocated = false;
    }

    auto CommandList::ClearState() -> void {
        mIndirectBuffer = nullptr;
        mRecordingContext[mRecordingContextIndex].mStatistics = {};

        ClearState( mRecordingContext[mRecordingContextIndex] );
    }

    auto CommandList::ClearState( RecordingContext &context ) -> void {
        MKT_BEGIN_PROFILER();

        for ( auto *allocation: context.mInFlightSubAllocations ) {
            allocation->mInUse.clear();
        }

        context.mInFlightSubAllocations.clear();
        context.mInFlightResources.clear();

#if defined( MIKOTO_ENABLE_TRACY )
        context.mGpuProfiler.reset();
#endif
    }

    auto SubmitSemaphoresInfo::AddCommandList( renderer::rhi::CommandListHandle cmd ) -> SubmitSemaphoresInfo & {
        mCommands.emplace_back( cmd );
        return *this;
    }

    auto SubmitSemaphoresInfo::AddWaitFence( renderer::rhi::FenceHandle fence, core::u64 value, VkPipelineStageFlags2 pipelineStage ) -> SubmitSemaphoresInfo & {
        mSyncObjects.emplace_back( fence.GetPtr() );
        mWaitSemaphores.emplace_back( VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = fence->GetNativeHandle( renderer::rhi::ObjectType::Vk_Semaphore ),
            .value = value,
            .stageMask = pipelineStage,
            .deviceIndex = 0 } );
        return *this;
    }

    auto SubmitSemaphoresInfo::AddSignalFence( renderer::rhi::FenceHandle fence, core::u64 value, VkPipelineStageFlags2 pipelineStage ) -> SubmitSemaphoresInfo & {
        mSyncObjects.emplace_back( fence.GetPtr() );
        mSignalSemaphores.emplace_back( VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = fence->GetNativeHandle( renderer::rhi::ObjectType::Vk_Semaphore ),
            .value = value,
            .stageMask = pipelineStage,
            .deviceIndex = 0 } );
        return *this;
    }

    auto SubmitSemaphoresInfo::AddWaitSemaphore( BinarySemaphoreHandle semaphore, VkPipelineStageFlags2 pipelineStage ) -> SubmitSemaphoresInfo & {
        mSyncObjects.emplace_back( semaphore.GetPtr() );
        mWaitSemaphores.emplace_back( VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = *semaphore,
            .value = 0,
            .stageMask = pipelineStage,
            .deviceIndex = 0 } );
        return *this;
    }

    auto SubmitSemaphoresInfo::AddSignalSemaphore( BinarySemaphoreHandle semaphore, VkPipelineStageFlags2 pipelineStage ) -> SubmitSemaphoresInfo & {
        mSyncObjects.emplace_back( semaphore.GetPtr() );
        mSignalSemaphores.emplace_back( VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = *semaphore,
            .value = 0,
            .stageMask = pipelineStage,
            .deviceIndex = 0 } );
        return *this;
    }

    Queue::Queue( renderer::rhi::QueueType type, renderer::rhi::QueueOpSupportFlags opFlags, core::u32 queueFamilyIndex, core::u32 queueIndex )
        : mType{ type }, mOpSupportFlags{ opFlags }, mFamilyIndex{ queueFamilyIndex }, mQueueIndex{ queueIndex } {
        switch ( type ) {
            case renderer::rhi::QueueType::eTransfer:
                mSubmissionLabelColor = renderer::rhi::Color( 0.2f, 0.6f, 1.0f, 0.5f );
                break;
            case renderer::rhi::QueueType::eGraphics:
                mSubmissionLabelColor = renderer::rhi::Color( 0.2f, 0.8f, 0.2f, 0.5f );
                break;
            case renderer::rhi::QueueType::eCompute:
                mSubmissionLabelColor = renderer::rhi::Color( 0.7f, 0.3f, 0.9f, 0.5f );
                break;
            case renderer::rhi::QueueType::ePresent:
                mSubmissionLabelColor = renderer::rhi::Color( 1.0f, 0.8f, 0.0f, 0.5f );
                break;
            default:;
        }
    }

    auto Queue::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        Device *device{ core::checked_cast<Device *>( mDevice ) };

        vkGetDeviceQueue( device->GetDevice(), mFamilyIndex, mQueueIndex, MKT_ADDRESSOF( mQueue ) );

        MKT_ASSERT( mQueue != VK_NULL_HANDLE, "Queue is empty" );

        mTimelineSemaphore = device->CreateFence( mTimelineValue++ );
        mTimelineSemaphore->SetDebugName( string::Format( "Queue [{}] Timeline", core::rc_cast<core::u64>( mQueue ) ) );

        mIsAllocated = true;
    }

#if defined( MIKOTO_ENABLE_TRACY )
    auto Queue::InitializeGpuProfiler( core::u64 connection ) -> void {
        std::lock_guard lock{ mSubmissionMutex };
        // The new capture has no zone records for queries issued to the old connection.
        // Recording slots retain the previous query pool through GPU completion.
        mGpuProfiler.reset();
        mGpuProfilerConnection = connection;

        if ( tracy::GetGpuCtxCounter().load() > UINT8_MAX ) {
            MKT_CORE_LOGGER_WARN( "Tracy has exhausted its GPU context IDs. Restart Mikoto to capture more GPU sessions." );
            return;
        }

        auto *device{ core::checked_cast<Device *>( mDevice ) };
        if ( !device->GetQueryCapabilities( QueryType::eTimestamp, QueueType::eGraphics ).mSupported ) {
            return;
        }

        // The upstream context submits and re-records a command buffer to calibrate its clock.
        TracyCalibrationPool calibration{ .mDevice = device->GetDevice() };
        auto poolInfo{ initializers::CommandPoolCreateInfo() };
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = mFamilyIndex;

        if ( vkCreateCommandPool( calibration.mDevice, &poolInfo, nullptr, &calibration.mPool ) != VK_SUCCESS ) {
            MKT_CORE_LOGGER_WARN( "Could not create a Vulkan command pool for Tracy GPU profiling." );
            return;
        }

        auto allocateInfo{ initializers::CommandBufferAllocateInfo() };
        allocateInfo.commandPool = calibration.mPool;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;
        VkCommandBuffer commands{};

        if ( vkAllocateCommandBuffers( calibration.mDevice, &allocateInfo, &commands ) != VK_SUCCESS ) {
            MKT_CORE_LOGGER_WARN( "Could not allocate a Vulkan command buffer for Tracy GPU profiling." );
            return;
        }

        constexpr char kName[]{ "Vulkan Graphics" };
        auto *profiler{ tracy::CreateVkContext( device->GetPhysicalDevice()->mPhysicalDevice, calibration.mDevice, mQueue, commands, nullptr, nullptr ) };
        mGpuProfiler.reset( profiler, []( tracy::VkCtx *context ) {
            TracyVkDestroy( context );
        } );
        TracyVkContextName( profiler, kName, sizeof( kName ) - 1 );
    }
#endif

    Queue::operator VkQueue() const {
        MKT_BEGIN_PROFILER();
        return mQueue;
    }

    auto Queue::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        WaitIdle();

#if defined( MIKOTO_ENABLE_TRACY )
        mGpuProfiler.reset();
#endif
        mTimelineSemaphore.Reset();

        mIsAllocated = false;
    }

    auto Queue::Present( const VkPresentInfoKHR &info ) -> VkResult {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mSubmissionMutex };
        return vkQueuePresentKHR( mQueue, MKT_ADDRESSOF( info ) );
    }

    auto Queue::ExecuteCommandLists( const renderer::rhi::SubmitInfo &submitInfo ) -> void {
        MKT_BEGIN_PROFILER();

        std::lock_guard lock{ mSubmissionMutex };

        eastl::fixed_vector<VkCommandBufferSubmitInfo, 10> submissions{};

        for ( const auto &commandHandle: submitInfo.mCommands ) {
            const CommandList *cmd{ core::checked_cast<const CommandList *>( commandHandle.GetPtr() ) };

            cmd->ValidateSubmission( this );

            VkCommandBuffer native{ cmd->GetNativeHandle( renderer::rhi::ObjectType::Vk_CmdBuffer ) };

            if ( eastl::any_of( submissions.begin(), submissions.end(), [native]( const auto &entry ) {
                     return entry.commandBuffer == native;
                 } ) ) {
                MKT_THROW_RUNTIME_ERROR( "Vulkan submission contains the same recording twice." );
            }

            VkCommandBufferSubmitInfo commandSubmitInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = cmd->GetNativeHandle( renderer::rhi::ObjectType::Vk_CmdBuffer ),
                .deviceMask = 0
            };

            submissions.emplace_back( commandSubmitInfo );
        }

        // Signal timelines
        eastl::fixed_vector<VkSemaphoreSubmitInfo, 5> signalInfos{};

        // Queue timeline
        signalInfos.emplace_back( VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = mTimelineSemaphore->GetNativeHandle( renderer::rhi::ObjectType::Vk_Semaphore ),
            .value = mTimelineValue,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .deviceIndex = 0 } );

        // Caller signals
        if ( !submitInfo.mSignals.empty() ) {
            for ( const auto &[signalValue, signalFence]: submitInfo.mSignals ) {
                const Fence *fence{ core::checked_cast<const Fence *>( signalFence.GetPtr() ) };
                signalInfos.emplace_back( VkSemaphoreSubmitInfo{
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .pNext = nullptr,
                    .semaphore = *fence,
                    .value = signalValue,
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .deviceIndex = 0 } );
            }
        }

        // Wait timelines
        eastl::fixed_vector<VkSemaphoreSubmitInfo, 5> waitInfos{};

        // Caller waits
        if ( !submitInfo.mWaits.empty() ) {
            for ( const auto &[waitValue, waitFence]: submitInfo.mWaits ) {
                const Fence *fence{ core::checked_cast<const Fence *>( waitFence.GetPtr() ) };
                waitInfos.emplace_back( VkSemaphoreSubmitInfo{
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                    .pNext = nullptr,
                    .semaphore = *fence,
                    .value = waitValue,
                    .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                    .deviceIndex = 0 } );
            }
        }

        VkSubmitInfo2 submitInfo2{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .pNext = nullptr,
            .flags = 0,
            .waitSemaphoreInfoCount = ( core::u32 )waitInfos.size(),
            .pWaitSemaphoreInfos = waitInfos.data(),
            .commandBufferInfoCount = ( core::u32 )submissions.size(),
            .pCommandBufferInfos = submissions.data(),
            .signalSemaphoreInfoCount = ( core::u32 )signalInfos.size(),
            .pSignalSemaphoreInfos = signalInfos.data()
        };

        {
            mSubmissionLabel = string::Format( "Queue {} SubmissionID: {}", GetQueueName( mType ).data(), mTimelineValue.load() ).c_str();
            VkDebugUtilsLabelEXT labelInfo = {};
            labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            labelInfo.pLabelName = mSubmissionLabel.c_str();
            labelInfo.color[0] = mSubmissionLabelColor.mR;
            labelInfo.color[1] = mSubmissionLabelColor.mG;
            labelInfo.color[2] = mSubmissionLabelColor.mB;
            labelInfo.color[3] = mSubmissionLabelColor.mA;

            vkQueueBeginDebugUtilsLabelEXT( mQueue, &labelInfo );
            MKT_VK_CHECK( vkQueueSubmit2( mQueue, 1, &submitInfo2, VK_NULL_HANDLE ) );

            // Failed submissions never own a command list or its staging allocations.
            for ( auto command: submitInfo.mCommands ) {
                core::checked_cast<CommandList *>( command.GetPtr() )->MarkExecuted( this, mTimelineValue );
            }

            vkQueueEndDebugUtilsLabelEXT( mQueue );

            ++mTimelineValue;
        }
    }

    auto Queue::GetType() const -> rhi::QueueType {
        return mType;
    }

    auto Queue::GetOpSupportFlags() const -> rhi::QueueOpSupportFlags {
        return mOpSupportFlags;
    }

    auto DescriptorTable::GetBindingLayout() const -> rhi::BindingLayoutHandle {
        return mBindingLayout;
    }

    auto Queue::WaitCompletionValue( core::u64 value ) -> void {
        MKT_BEGIN_PROFILER();

        Device *device{ core::checked_cast<Device *>( mDevice ) };
        VkSemaphore semaphore{ mTimelineSemaphore->GetNativeHandle( renderer::rhi::ObjectType::Vk_Semaphore ) };

        VkSemaphoreWaitInfo waitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &semaphore,
            .pValues = &value
        };

        MKT_VK_CHECK( vkWaitSemaphores( device->GetDevice(), &waitInfo, UINT64_MAX ) );
    }

    auto Queue::ExecuteCommandLists( SubmitSemaphoresInfo &&submitInfo ) -> core::u64 {
        MKT_BEGIN_PROFILER();

        // A frame without an acquired image still needs its completion signal.
        if ( submitInfo.mCommands.empty() && submitInfo.mWaitSemaphores.empty() && submitInfo.mSignalSemaphores.empty() ) {
            return 0;
        }

        std::lock_guard lock{ mSubmissionMutex };

        const core::u64 submissionID{ mTimelineValue };
        Fence *timeline{ core::checked_cast<Fence *>( mTimelineSemaphore.GetPtr() ) };

        eastl::fixed_vector<VkCommandBufferSubmitInfo, kMaxSubmits> cmdInfos{};

        for ( auto &commandHandle: submitInfo.mCommands ) {
            CommandList *pCommandList{ core::checked_cast<CommandList *>( commandHandle.GetPtr() ) };
            VkCommandBuffer native{ *pCommandList };

            if ( eastl::any_of( cmdInfos.begin(), cmdInfos.end(), [native]( const auto &entry ) {
                     return entry.commandBuffer == native;
                 } ) ) {
                MKT_THROW_RUNTIME_ERROR( "Vulkan submission contains the same recording twice." );
            }

            cmdInfos.emplace_back( VkCommandBufferSubmitInfo{
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .pNext = nullptr,
                .commandBuffer = pCommandList->GetNativeHandle( renderer::rhi::ObjectType::Vk_CmdBuffer ),
                .deviceMask = 0 } );

            pCommandList->ValidateSubmission( this );
        }

        auto waitInfos{ eastl::move( submitInfo.mWaitSemaphores ) };
        auto signalInfos{ eastl::move( submitInfo.mSignalSemaphores ) };

        // --- Timeline signal ---
        signalInfos.emplace_back( VkSemaphoreSubmitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .pNext = nullptr,
            .semaphore = timeline->GetNativeHandle( renderer::rhi::ObjectType::Vk_Semaphore ),
            .value = submissionID,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
            .deviceIndex = 0 } );

        {
            mSubmissionLabel = string::Format( "Queue {} SubmissionID: {}", GetQueueName( mType ).data(), submissionID ).c_str();
            VkDebugUtilsLabelEXT labelInfo = {};
            labelInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
            labelInfo.pLabelName = mSubmissionLabel.c_str();
            labelInfo.color[0] = mSubmissionLabelColor.mR;
            labelInfo.color[1] = mSubmissionLabelColor.mG;
            labelInfo.color[2] = mSubmissionLabelColor.mB;
            labelInfo.color[3] = mSubmissionLabelColor.mA;

            vkQueueBeginDebugUtilsLabelEXT( mQueue, &labelInfo );

            VkSubmitInfo2 queueSubmitInfo{
                .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                .pNext = nullptr,
                .flags = 0,
                .waitSemaphoreInfoCount = ( core::u32 )waitInfos.size(),
                .pWaitSemaphoreInfos = waitInfos.data(),
                .commandBufferInfoCount = ( core::u32 )cmdInfos.size(),
                .pCommandBufferInfos = cmdInfos.data(),
                .signalSemaphoreInfoCount = ( core::u32 )signalInfos.size(),
                .pSignalSemaphoreInfos = signalInfos.data()
            };
            MKT_VK_CHECK( vkQueueSubmit2( mQueue, 1, &queueSubmitInfo, VK_NULL_HANDLE ) );

            for ( auto command: submitInfo.mCommands ) {
                core::checked_cast<CommandList *>( command.GetPtr() )->MarkExecuted( this, submissionID );
            }

            vkQueueEndDebugUtilsLabelEXT( mQueue );

            ++mTimelineValue;
        }

        return submissionID;
    }

    auto Queue::AllocateCmdList( renderer::rhi::QueueType type ) -> renderer::rhi::CommandListHandle {
        renderer::rhi::CommandListHandle result{ core::Ref<CommandList>::New( type, this, mFamilyIndex ) };
        return result;
    }

    Queue::operator core::u32() const {
        return mFamilyIndex;
    }


    Queue::~Queue() {
        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto Queue::WaitIdle() -> void {
        {
            std::lock_guard lock{ mSubmissionMutex };
            MKT_VK_CHECK( vkQueueWaitIdle( mQueue ) );
        }

        CollectGarbage();
    }

    auto Queue::CollectGarbage() -> void {
        // Empty
    }

    auto Queue::GetCurrentTimeline() -> core::u64 {
        return mTimelineSemaphore->GetCompletionValue();
    }

    auto Queue::GetQueue() const -> VkQueue {
        return mQueue;
    }

    auto Queue::GetQueueIndex() const -> core::u32 {
        return mQueueIndex;
    }

    auto Queue::GetFamilyIndex() const -> core::u32 {
        return mFamilyIndex;
    }

    auto IDescriptorAllocatorPool::Create( const VkDevice &device, core::i32 nFrames ) -> eastl::unique_ptr<IDescriptorAllocatorPool> {
        auto impl{ eastl::make_unique<DescriptorAllocatorPoolImpl>() };
        impl->mDevice = device;
        impl->mFrameIndex = 0;
        impl->mMaxFrames = nFrames;

        for ( core::i32 i{}; i < nFrames; i++ ) {
            impl->mDescriptorPools.push_back( std::make_unique<PoolStorage>() );
        }

        return impl;
    }

    DescriptorAllocatorHandle::~DescriptorAllocatorHandle() {
        auto *implPool{ as<DescriptorAllocatorPoolImpl *>( mOwnerPool ) };

        if ( implPool ) {
            implPool->ReturnAllocator( *this, false );
        }
    }

    DescriptorAllocatorHandle::DescriptorAllocatorHandle( DescriptorAllocatorHandle &&other ) noexcept {
        MKT_BEGIN_PROFILER();

        Return();

        mDescriptorPool = other.mDescriptorPool;
        mPoolIndex = other.mPoolIndex;
        mOwnerPool = other.mOwnerPool;

        other.mOwnerPool = nullptr;
        other.mPoolIndex = -1;
        other.mDescriptorPool = VkDescriptorPool{};
    }

    auto DescriptorAllocatorHandle::operator=( DescriptorAllocatorHandle &&other ) noexcept -> DescriptorAllocatorHandle & {
        MKT_BEGIN_PROFILER();

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
        MKT_BEGIN_PROFILER();
        auto *implPool{ core::as<DescriptorAllocatorPoolImpl *>( mOwnerPool ) };

        if ( implPool ) {
            implPool->ReturnAllocator( *this, false );
        }

        mDescriptorPool = VkDescriptorPool{};
        mPoolIndex = -1;
        mOwnerPool = nullptr;
    }

    auto DescriptorAllocatorHandle::Allocate( const VkDescriptorSetLayout &layout, VkDescriptorSet &builtSet ) -> bool {
        MKT_BEGIN_PROFILER();
        auto *implPool{ core::as<DescriptorAllocatorPoolImpl *>( mOwnerPool ) };

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
            }
            else {
                //stuff is truly broken
                return false;
            }
        }

        return true;
    }

    auto DescriptorAllocatorPoolImpl::CreatePool( int count, VkDescriptorPoolCreateFlags flags ) -> VkDescriptorPool {
        MKT_BEGIN_PROFILER();
        eastl::vector<VkDescriptorPoolSize> sizes;
        sizes.reserve( mPoolSizes.mSizes.size() );

        for ( auto sz: mPoolSizes.mSizes ) {
            sizes.push_back( { sz.mType, ( u32 )( sz.mMultiplier * count ) } );
        }

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = flags;
        pool_info.maxSets = count;
        pool_info.poolSizeCount = ( u32 )sizes.size();
        pool_info.pPoolSizes = sizes.data();

        VkDescriptorPool descriptorPool;
        vkCreateDescriptorPool( mDevice, &pool_info, nullptr, &descriptorPool );

        return descriptorPool;
    }

    DescriptorAllocatorPoolImpl::~DescriptorAllocatorPoolImpl() {
        MKT_BEGIN_PROFILER();

        for ( DescriptorAllocator allocator: mClearAllocators ) {
            vkDestroyDescriptorPool( mDevice, allocator.mPool, nullptr );
        }

        for ( auto &&storage: mDescriptorPools ) {
            for ( DescriptorAllocator allocator: storage->mFullAllocators ) {
                vkDestroyDescriptorPool( mDevice, allocator.mPool, nullptr );
            }

            for ( DescriptorAllocator allocator: storage->mUsableAllocators ) {
                vkDestroyDescriptorPool( mDevice, allocator.mPool, nullptr );
            }
        }
    }

    auto DescriptorAllocatorPoolImpl::Flip() -> void {
        MKT_BEGIN_PROFILER();
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
        MKT_BEGIN_PROFILER();

        for ( auto &s: mPoolSizes.mSizes ) {
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

    auto DescriptorAllocatorPoolImpl::ReturnAllocator( DescriptorAllocatorHandle &handle, bool bIsFull ) -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard<std::mutex> lk( mPoolMutex );

        if ( bIsFull ) {
            mDescriptorPools[handle.mPoolIndex]->mFullAllocators.push_back( DescriptorAllocator{ handle.mDescriptorPool } );
        }
        else {
            mDescriptorPools[handle.mPoolIndex]->mUsableAllocators.push_back( DescriptorAllocator{ handle.mDescriptorPool } );
        }
    }

    auto DescriptorAllocatorPoolImpl::GetAllocator() -> DescriptorAllocatorHandle {
        MKT_BEGIN_PROFILER();
        std::lock_guard<std::mutex> lk( mPoolMutex );

        bool foundAllocator = false;

        core::i32 poolIndex = mFrameIndex;

        DescriptorAllocator allocator{};
        //try reuse an allocated pool
        if ( mClearAllocators.size() != 0 ) {
            allocator = mClearAllocators.back();
            mClearAllocators.pop_back();
            foundAllocator = true;
        }
        else {
            if ( mDescriptorPools[poolIndex]->mUsableAllocators.size() > 0 ) {
                allocator = mDescriptorPools[poolIndex]->mUsableAllocators.back();
                mDescriptorPools[poolIndex]->mUsableAllocators.pop_back();
                foundAllocator = true;
            }
        }
        //need a new pool
        if ( !foundAllocator ) {
            // Tables retain their allocator until destruction and free their set
            // individually, including tables created during a renderer rebuild.
            VkDescriptorPoolCreateFlags flags{ VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT |
                                               VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT };

            VkDescriptorPool newPool = CreatePool( ( core::i32 )kInitialPoolSize, flags );

            allocator.mPool = newPool;

            foundAllocator = true;
        }

        DescriptorAllocatorHandle newHandle{};
        newHandle.mOwnerPool = this;
        newHandle.mPoolIndex = core::as<core::i8>( poolIndex );
        newHandle.mDescriptorPool = allocator.mPool;

        return newHandle;
    }

    auto BinarySemaphore::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() || !mDevice ) {
            return;
        }

        mDebugName = name;

        auto *device{ core::checked_cast<Device *>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_SEMAPHORE, core::rc_cast<core::u64>( mSemaphore ), mDebugName );
    }

    auto BinarySemaphore::GetNativeHandle( renderer::rhi::ObjectType object ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( object ) {
            case renderer::rhi::ObjectType::Vk_Semaphore:
                return renderer::rhi::Object( mSemaphore );
            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto BinarySemaphore::GetNativeHandle( renderer::rhi::ObjectType object ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( object ) {
            case renderer::rhi::ObjectType::Vk_Semaphore:
                return renderer::rhi::Object( mSemaphore );
            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    BinarySemaphore::operator VkSemaphore() const {
        MKT_BEGIN_PROFILER();
        return mSemaphore;
    }

    BinarySemaphore::~BinarySemaphore() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto BinarySemaphore::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        auto *device{ core::checked_cast<Device *>( mDevice ) };

        VkSemaphoreCreateInfo info{ initializers::SemaphoreCreateInfo() };
        MKT_VK_CHECK( vkCreateSemaphore( device->GetDevice(), MKT_ADDRESSOF( info ), nullptr, MKT_ADDRESSOF( mSemaphore ) ) );
        mIsAllocated = true;
    }

    auto BinarySemaphore::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        auto *device{ core::checked_cast<Device *>( mDevice ) };
        vkDestroySemaphore( device->GetDevice(), mSemaphore, nullptr );
        mIsAllocated = false;
    }

    GpuUploadManager::GpuUploadManager( renderer::rhi::IGpuDevice *device )
        : mDevice{ device } {
    }

    auto GpuUploadManager::SubAllocate( core::usize byteSize, core::usize alignment ) -> GpuUploadAllocation * {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mMutex };

        StagingAllocation *stagingAlloc{};
        eastl::optional<memory::Allocation> subAllocProperties{};

        // Find the first allocation I can suballocate from and lock it
        if ( mBuffers.empty() ) {
            stagingAlloc = CreateBuffer( eastl::max<core::usize>( MKT_MEGABYTES( 64 ), byteSize + alignment ) );
            subAllocProperties = stagingAlloc->mMemoryArena->Allocate( byteSize, alignment );

            if ( !subAllocProperties.has_value() ) {
                MKT_ASSERT( false, "Failed to sub-allocate" );
            }
        }
        else {
            // Try to find any buffer that has ranges available
            for ( auto &bufferInfo: mBuffers | std::views::values ) {
                stagingAlloc = bufferInfo.get();
                subAllocProperties = stagingAlloc->mMemoryArena->Allocate( byteSize, alignment );

                if ( subAllocProperties.has_value() ) {
                    break;
                }
            }

            // If none does allocate a new staging for uploads
            if ( !subAllocProperties.has_value() ) {
                stagingAlloc = CreateBuffer( eastl::max<core::usize>( MKT_MEGABYTES( 64 ), byteSize + alignment ) );
                subAllocProperties = stagingAlloc->mMemoryArena->Allocate( byteSize, alignment );
            }
        }

        MKT_ASSERT( subAllocProperties.has_value() && subAllocProperties->mSize >= byteSize, "Failed to sub-allocate" );

        // Fill params
        GpuUploadAllocation *result{ CreateSubAllocation( stagingAlloc->mBuffer.GetPtr() ) };

        result->mMappedMemory = core::as<core::ubyte *>( core::checked_cast<Buffer *>( stagingAlloc->mBuffer.GetPtr() )->GetMappedAddress() ) + subAllocProperties->mOffset;
        result->mSize = subAllocProperties->mSize;
        result->mOffset = subAllocProperties->mOffset;
        result->mBuffer = stagingAlloc->mBuffer.GetPtr();
        result->mAllocation = *subAllocProperties;

        return result;
    }

    auto GpuUploadManager::ReclaimMemory() -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mMutex };

        // Check fence, I'm not sure maybe timeline semaphores can be used instead
        auto it{ mSubAllocations.begin() };

        for ( ; it != mSubAllocations.end(); ) {
            auto &[buffer, subAllocations]{ *it };
            // Is it safe to return them back here?
            for ( auto itSub{ subAllocations.begin() }; itSub != subAllocations.end(); ) {
                if ( itSub->get() && !( *itSub )->mInUse.test() ) {
                    mBuffers[buffer]->mMemoryArena->Free( ( *itSub )->mAllocation );
                    itSub = subAllocations.erase( itSub );
                }
                else {
                    ++itSub;
                }
            }

            if ( subAllocations.empty() ) {
                subAllocations.clear();
                mBuffers.erase( it->first );
                it = mSubAllocations.erase( it );
            }
            else {
                ++it;
            }
        }
    }

    GpuUploadManager::~GpuUploadManager() {
        MKT_BEGIN_PROFILER();
        mBuffers.clear();
        mSubAllocations.clear();
    }

    auto GpuUploadManager::CreateBuffer( core::usize minimumSize ) -> StagingAllocation * {
        MKT_BEGIN_PROFILER();
        auto bufferDes{ renderer::rhi::BufferCreateDescription{}
            .SetByteSize( minimumSize )
            .SetCpuAccessType( renderer::rhi::AccessType::eWrite )
            .SetHeapType( renderer::rhi::HeapType::eUpload )
            .SetResourceType( renderer::rhi::ResourceType::eInvalid )// Is not a shader resource
            .SetBufferUsage( renderer::rhi::BufferUsageFlagsBits::CopySource ) };
        renderer::rhi::BufferHandle result{ mDevice->CreateBuffer( bufferDes ) };

        result->SetDebugName( string::Format( "Device UploadBuffer Size: {}", minimumSize ) );

        auto &newAllocation{ mBuffers[result.GetPtr()] };
        newAllocation = eastl::make_unique<StagingAllocation>();
        newAllocation->mBuffer = result;
        newAllocation->mMemoryArena = eastl::make_unique<memory::MemoryArena<renderer::rhi::IBuffer, memory::FreeListFirstFitAllocator>>( result, minimumSize );

        return newAllocation.get();
    }

    auto GpuUploadManager::CreateSubAllocation( renderer::rhi::IBuffer *buffer ) -> GpuUploadAllocation * {
        MKT_BEGIN_PROFILER();
        auto &result{ mSubAllocations[buffer].emplace_back( eastl::make_unique<GpuUploadAllocation>() ) };
        return result.get();
    }

    auto Fence::GetCompletionValue() const -> core::u64 {
        MKT_BEGIN_PROFILER();
        Device *device{ core::checked_cast<Device *>( mDevice ) };

        MKT_VK_CHECK( vkGetSemaphoreCounterValue( device->GetDevice(), mSemaphore, MKT_ADDRESSOF( mTimeline ) ) );

        return mTimeline;
    }

    auto Fence::Signal( core::u64 fenceValue ) -> bool {
        MKT_BEGIN_PROFILER();
        Device *device{ core::checked_cast<Device *>( mDevice ) };

        VkSemaphoreSignalInfo signalInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
            .semaphore = mSemaphore,
            .value = fenceValue
        };

        return vkSignalSemaphore( device->GetDevice(), &signalInfo ) == VK_SUCCESS;
    }

    auto Fence::Wait( core::u64 fenceValue, core::u64 timeoutMs ) -> bool {
        MKT_BEGIN_PROFILER();
        Device *device{ core::checked_cast<Device *>( mDevice ) };

        VkSemaphoreWaitInfo waitInfo{
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
            .semaphoreCount = 1,
            .pSemaphores = &mSemaphore,
            .pValues = &fenceValue
        };

        // Timeout is in nanoseconds
        // https://docs.vulkan.org/refpages/latest/refpages/source/vkWaitSemaphores.html
        constexpr core::u64 nanosecondsPerMillisecond{ 1000000 };
        const core::u64 timeout{ timeoutMs > UINT64_MAX / nanosecondsPerMillisecond ? UINT64_MAX : timeoutMs * nanosecondsPerMillisecond };
        const auto result{ vkWaitSemaphores( device->GetDevice(), &waitInfo, timeout ) };

        if ( result == VK_TIMEOUT ) {
            return false;
        }
        // Device loss is an error, not a timeout to retry forever.
        MKT_VK_CHECK( result );
        return true;
    }

    Fence::Fence( core::u64 initialValue )
        : mTimeline{ initialValue } {
    }

    auto Fence::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() || !mDevice ) {
            return;
        }

        mDebugName = name;

        auto *device{ core::checked_cast<Device *>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_SEMAPHORE, core::rc_cast<core::u64>( mSemaphore ), mDebugName );
    }

    auto Fence::GetNativeHandle( renderer::rhi::ObjectType object ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( object ) {
            case renderer::rhi::ObjectType::Vk_Semaphore:
                return renderer::rhi::Object( mSemaphore );
            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto Fence::GetNativeHandle( renderer::rhi::ObjectType object ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( object ) {
            case renderer::rhi::ObjectType::Vk_Semaphore:
                return renderer::rhi::Object( mSemaphore );
            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    Fence::operator VkSemaphore() const {
        MKT_BEGIN_PROFILER();
        return mSemaphore;
    }

    Fence::~Fence() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto Fence::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        auto *device{ core::checked_cast<Device *>( mDevice ) };

        VkSemaphoreTypeCreateInfo typeCreateInfo{ initializers::SemaphoreTypeCreateInfo() };
        typeCreateInfo.initialValue = mTimeline;

        VkSemaphoreCreateInfo createInfo{ initializers::SemaphoreCreateInfo() };
        createInfo.pNext = MKT_ADDRESSOF( typeCreateInfo );

        MKT_VK_CHECK( vkCreateSemaphore( device->GetDevice(), MKT_ADDRESSOF( createInfo ), nullptr, MKT_ADDRESSOF( mSemaphore ) ) );

        mIsAllocated = true;
    }

    auto Fence::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        auto *device{ core::checked_cast<Device *>( mDevice ) };
        vkDestroySemaphore( device->GetDevice(), mSemaphore, nullptr );

        mIsAllocated = false;
    }

    auto DescriptorWriter::WriteAccelerationStructure( core::u32 binding, VkAccelerationStructureKHR structure, core::u32 arrayIndex ) -> DescriptorWriter & {
        MKT_BEGIN_PROFILER();

        // The pNext payload and its handle must outlive this method, just like image and buffer infos.
        auto &handle{ mAccelerationStructures.emplace_back( structure ) };
        auto &info{ mAccelerationInfos.emplace_back( VkWriteDescriptorSetAccelerationStructureKHR{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures = MKT_ADDRESSOF( handle ) } ) };

        VkWriteDescriptorSet write{ initializers::WriteDescriptorSet() };
        write.pNext = MKT_ADDRESSOF( info );
        write.dstBinding = binding;
        write.dstArrayElement = arrayIndex;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        mWrites.push_back( write );

        return *this;
    }

    auto DescriptorWriter::WriteSampler( core::u32 binding, VkSampler sampler, core::u32 arrayIndex ) -> DescriptorWriter & {
        MKT_BEGIN_PROFILER();
        VkDescriptorImageInfo &info{ mImageInfos.emplace_back( VkDescriptorImageInfo{
            .sampler = sampler,
            .imageView = VK_NULL_HANDLE,
            .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED } ) };

        VkWriteDescriptorSet write{ initializers::WriteDescriptorSet() };

        write.dstBinding = binding;
        write.dstSet = VK_NULL_HANDLE;//left empty for now until we to write it  in the updateSet()
        write.descriptorCount = 1;
        write.dstArrayElement = arrayIndex;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        write.pImageInfo = MKT_ADDRESSOF( info );

        mWrites.push_back( write );

        return *this;
    }

    auto DescriptorWriter::WriteBuffer( core::u32 binding, VkBuffer buffer, core::usize size, core::usize offset, VkDescriptorType type, core::u32 arrayIndex ) -> DescriptorWriter & {
        MKT_BEGIN_PROFILER();
        // Descriptor types allowed for a buffer
        // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
        // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
        // VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
        // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC

        // When we want to bind one or the other type into a shader, we set the correct type here.
        // Remember that it needs to match the usage when allocating the VkBuffer

        VkDescriptorBufferInfo &info{ mBufferInfos.emplace_back( VkDescriptorBufferInfo{
            .buffer = buffer,
            .offset = offset,
            .range = size,
        } ) };

        VkWriteDescriptorSet write{ initializers::WriteDescriptorSet() };

        write.dstBinding = binding;
        write.dstSet = VK_NULL_HANDLE;//left empty for now until we to write it in the updateSet()
        write.descriptorCount = 1;
        write.descriptorType = type;
        write.dstArrayElement = arrayIndex;
        write.pBufferInfo = std::addressof( info );

        mWrites.push_back( write );

        return *this;
    }

    auto DescriptorWriter::WriteImage( core::u32 binding, VkImageView image, VkDescriptorType type, VkImageLayout layout, core::u32 arrayIndex ) -> DescriptorWriter & {
        MKT_BEGIN_PROFILER();
        // The layout is going to be almost always either VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        // the best layout to use for accessing textures in the shaders, or VK_IMAGE_LAYOUT_GENERAL
        // when we are using them from compute shaders and writing them.

        VkDescriptorImageInfo &info{ mImageInfos.emplace_back( VkDescriptorImageInfo{
            .imageView = image,
            .imageLayout = layout } ) };

        VkWriteDescriptorSet write{ initializers::WriteDescriptorSet() };

        write.dstBinding = binding;
        write.dstSet = VK_NULL_HANDLE;//left empty for now until we to write it  in the updateSet()
        write.descriptorCount = 1;
        write.dstArrayElement = arrayIndex;
        write.descriptorType = type;
        write.pImageInfo = MKT_ADDRESSOF( info );

        mWrites.push_back( write );

        return *this;
    }

    auto DescriptorWriter::SetVisibility( VkShaderStageFlags visibility ) -> DescriptorWriter & {
        MKT_BEGIN_PROFILER();
        mShaderStages |= visibility;
        return *this;
    }

    auto DescriptorWriter::Clear() -> void {
        MKT_BEGIN_PROFILER();
        mImageInfos.clear();
        mWrites.clear();
        mBufferInfos.clear();
        mAccelerationInfos.clear();
        mAccelerationStructures.clear();
    }

    auto DescriptorWriter::UpdateSet( VkDevice device, VkDescriptorSet set ) -> void {
        MKT_BEGIN_PROFILER();

        for ( VkWriteDescriptorSet &write: mWrites ) {
            write.dstSet = set;
        }

        vkUpdateDescriptorSets( device, core::as<core::u32>( mWrites.size() ), mWrites.data(), 0, nullptr );
    }

    BindingLayout::BindingLayout( const renderer::rhi::BindingLayoutDescription &desc )
        : mSetIndex{ desc.mRegisterSpace }, mIsBindless{ false }, mBindingLayoutDesc{ desc } {
    }

    BindingLayout::BindingLayout( const renderer::rhi::DescriptorTableLayoutDescription &desc )
        : mSetIndex{ desc.mRegisterSpace }, mIsBindless{ true }, mBindlessLayoutDesc{ desc } {
    }

    auto BindingLayout::GetRegisterSpace() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mSetIndex;
    }

    auto BindingLayout::IsBindless() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsBindless;
    }

    auto BindingLayout::GetBindlessLayoutDesc() const -> const renderer::rhi::DescriptorTableLayoutDescription & {
        MKT_BEGIN_PROFILER();
        return mBindlessLayoutDesc;
    }

    auto BindingLayout::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() || !mDevice ) {
            return;
        }

        mDebugName = name;

        auto *device{ core::checked_cast<Device *>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, core::rc_cast<core::u64>( mDescriptorSetLayout ), mDebugName );
    }

    auto BindingLayout::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::Vk_DescriptorSetLayout:
                return renderer::rhi::Object{ mDescriptorSetLayout };
            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto BindingLayout::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::Vk_DescriptorSetLayout:
                return renderer::rhi::Object{ mDescriptorSetLayout };
            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    BindingLayout::operator VkDescriptorSetLayout() const {
        MKT_BEGIN_PROFILER();
        return mDescriptorSetLayout;
    }

    BindingLayout::~BindingLayout() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto BindingLayout::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsBindless ) {
            eastl::vector<VkDescriptorBindingFlags> flags{};
            eastl::vector<VkDescriptorSetLayoutBinding> bindings{};

            bindings.reserve( mBindlessLayoutDesc.mSlots.size() );
            flags.reserve( mBindlessLayoutDesc.mSlots.size() );

            for ( auto &item: mBindlessLayoutDesc.mSlots ) {
                if ( item.mResourceType == renderer::rhi::ResourceType::eRayTracingAccelStruct ) {
                    auto *device{ core::checked_cast<Device *>( mDevice ) };

                    if ( !device->SupportsRayTracing() ) {
                        MKT_THROW_RUNTIME_ERROR( "Bindless TLAS descriptors are not supported by this device." );
                    }

                    VkPhysicalDeviceAccelerationStructurePropertiesKHR properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR };
                    VkPhysicalDeviceProperties2 physical{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &properties };
                    vkGetPhysicalDeviceProperties2( device->GetPhysicalDevice()->mPhysicalDevice, &physical );
                    item.mMaxCapacity = eastl::min( item.mMaxCapacity,
                        eastl::min( properties.maxDescriptorSetUpdateAfterBindAccelerationStructures,
                            properties.maxPerStageDescriptorUpdateAfterBindAccelerationStructures ) );

                    if ( item.mMaxCapacity == 0 ) {
                        MKT_THROW_RUNTIME_ERROR( "The device has no bindless TLAS descriptor capacity." );
                    }
                }

                VkDescriptorSetLayoutBinding binding{};
                binding.binding = item.mSlot;
                binding.descriptorType = GetDescriptorType( item.mResourceType );
                binding.descriptorCount = item.mMaxCapacity;
                binding.stageFlags = GetShaderStageFlags( mBindlessLayoutDesc.mStageVisibility );
                binding.pImmutableSamplers = nullptr;

                bindings.emplace_back( binding );

                VkDescriptorBindingFlags bindingFlags =
                    VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                    VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

                flags.emplace_back( bindingFlags );
            }

            // I think Vulkan expects them in order even if you specify
            // bindings[0] for binding at binding 0, etc...
            eastl::sort( bindings.begin(), bindings.end(), []( const auto &a, const auto &b ) {
                return a.binding < b.binding;
            } );

            VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                .bindingCount = core::as<core::u32>( flags.size() ),
                .pBindingFlags = flags.data()
            };

            VkDescriptorSetLayoutCreateInfo layoutInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = &flagsInfo,
                .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                .bindingCount = core::as<core::u32>( bindings.size() ),
                .pBindings = bindings.data()
            };

            MKT_VK_CHECK( vkCreateDescriptorSetLayout(
                core::checked_cast<Device *>( mDevice )->GetDevice(),
                &layoutInfo,
                nullptr,
                &mDescriptorSetLayout ) );
        }
        else {
            eastl::vector<VkDescriptorSetLayoutBinding> bindings{};

            for ( const auto &item: mBindingLayoutDesc.mBindings ) {
                VkDescriptorSetLayoutBinding binding{};
                binding.binding = item.mSlot;
                binding.descriptorType = GetDescriptorType( item.mType );
                binding.descriptorCount = 1;
                binding.stageFlags = GetShaderStageFlags( mBindingLayoutDesc.mStageVisibility );
                binding.pImmutableSamplers = nullptr;


                bindings.emplace_back( binding );
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .pNext = nullptr,
                .flags = MKT_VK_FLAGS_NONE,
                .bindingCount = core::as<core::u32>( bindings.size() ),
                .pBindings = bindings.data()
            };

            MKT_VK_CHECK( vkCreateDescriptorSetLayout(
                core::checked_cast<Device *>( mDevice )->GetDevice(),
                MKT_ADDRESSOF( layoutInfo ),
                nullptr,
                MKT_ADDRESSOF( mDescriptorSetLayout ) ) );
        }

        mIsAllocated = true;
    }

    auto BindingLayout::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        vkDestroyDescriptorSetLayout( core::checked_cast<Device *>( mDevice )->GetDevice(), mDescriptorSetLayout, nullptr );
        mIsAllocated = false;
    }

    BindingTable::BindingTable( const renderer::rhi::BindingTableDescription &desc, renderer::rhi::BindingLayoutHandle layout )
        : mBindingLayout{ layout }, mBindingDescription{ desc } {
        // Ordinary binding sets are immutable: their descriptor contents must
        // outlive every recording that retains the set.
        for ( const auto &item: desc.mBindings ) {
            if ( item.mResource ) {
                mResources.emplace_back( core::checked_cast<renderer::rhi::DeviceObject *>( item.mResource ) );
            }
        }
    }

    auto BindingTable::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() || !mDevice ) {
            return;
        }

        mDebugName = name;

        auto *device{ core::checked_cast<Device *>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_DESCRIPTOR_SET, core::rc_cast<core::u64>( mDescriptorSet ), mDebugName );
    }

    auto BindingTable::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( type != renderer::rhi::ObjectType::Vk_DescriptorSet ) {
            return renderer::rhi::Object( nullptr );
        }

        return renderer::rhi::Object( mDescriptorSet );
    }

    auto BindingTable::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( type != renderer::rhi::ObjectType::Vk_DescriptorSet ) {
            return renderer::rhi::Object( nullptr );
        }

        return renderer::rhi::Object( mDescriptorSet );
    }

    BindingTable::~BindingTable() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto BindingTable::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        auto *device{ core::checked_cast<Device *>( mDevice ) };
        auto *layout{ core::checked_cast<BindingLayout *>( mBindingLayout.GetPtr() ) };

        mDescriptorAllocatorHandle = device->GetDescriptorAllocator();

        if ( mDescriptorAllocatorHandle.Allocate( layout->GetNativeHandle( renderer::rhi::ObjectType::Vk_DescriptorSetLayout ), mDescriptorSet ) ) {
            DescriptorWriter writer{};

            for ( const auto &item: mBindingDescription.mBindings ) {
                switch ( item.mResourceType ) {
                    case renderer::rhi::ResourceType::eTexture_SRV:
                        writer.WriteImage(
                            item.mBindingIndex,
                            core::checked_cast<Texture *>( item.mResource )->GetNativeHandle( renderer::rhi::ObjectType::Vk_ImageView ),
                            GetDescriptorType( item.mResourceType ),
                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL );
                        break;
                    case renderer::rhi::ResourceType::eTexture_UAV:
                        writer.WriteImage(
                            item.mBindingIndex,
                            core::checked_cast<Texture *>( item.mResource )->GetRenderView( item.mSubResourceSet.mBaseMipLevel, item.mSubResourceSet.mBaseArraySlice ),
                            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                            VK_IMAGE_LAYOUT_GENERAL );
                        break;
                    case renderer::rhi::ResourceType::eTypedBuffer_SRV:
                    case renderer::rhi::ResourceType::eTypedBuffer_UAV:
                    case renderer::rhi::ResourceType::eStructuredBuffer_SRV:
                    case renderer::rhi::ResourceType::eStructuredBuffer_UAV:
                    case renderer::rhi::ResourceType::eRawBuffer_SRV:
                    case renderer::rhi::ResourceType::eRawBuffer_UAV:
                    case renderer::rhi::ResourceType::eConstantBuffer:
                        writer.WriteBuffer(
                            item.mBindingIndex,
                            core::checked_cast<Buffer *>( item.mResource )->GetNativeHandle( renderer::rhi::ObjectType::Vk_Buffer ),
                            core::checked_cast<Buffer *>( item.mResource )->GetSizeBytes(),
                            0,
                            GetDescriptorType( item.mResourceType ) );
                        break;
                    case renderer::rhi::ResourceType::eSampler:
                        writer.WriteSampler(
                            item.mBindingIndex,
                            core::checked_cast<Sampler *>( item.mResource )->GetNativeHandle( renderer::rhi::ObjectType::Vk_Sampler ) );
                        break;
                    default:;
                }
            }

            writer.UpdateSet( device->GetDevice(), mDescriptorSet );

            mIsAllocated = true;
        }
    }

    auto BindingTable::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        auto *device{ core::checked_cast<Device *>( mDevice ) };
        MKT_VK_CHECK( vkFreeDescriptorSets( device->GetDevice(), mDescriptorAllocatorHandle.mDescriptorPool, 1, MKT_ADDRESSOF( mDescriptorSet ) ) );

        mIsAllocated = false;
    }

    DescriptorTable::DescriptorTable( renderer::rhi::BindingLayoutHandle setLayout )
        : mBindingLayout{ eastl::move( setLayout ) } {
    }

    auto DescriptorTable::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() ) {
            return;
        }

        mDebugName = name;

        auto *device{ core::checked_cast<Device *>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_DESCRIPTOR_SET, core::rc_cast<core::u64>( mDescriptorSet ), mDebugName );
    }

    auto DescriptorTable::GetCapacity( core::u32 slot ) const -> core::u32 {
        MKT_BEGIN_PROFILER();
        const auto it{ mSlotSize.find( slot ) };

        if ( it != mSlotSize.end() ) {
            return static_cast<core::u32>( it->second );
        }

        return 0;
    }

    DescriptorTable::~DescriptorTable() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto DescriptorTable::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( type != renderer::rhi::ObjectType::Vk_DescriptorSet ) {
            return renderer::rhi::Object( nullptr );
        }

        return renderer::rhi::Object( mDescriptorSet );
    }

    auto DescriptorTable::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( type != renderer::rhi::ObjectType::Vk_DescriptorSet ) {
            return renderer::rhi::Object( nullptr );
        }

        return renderer::rhi::Object( mDescriptorSet );
    }

    auto DescriptorTable::FreeIndex( core::u32 slot, rhi::DescriptorTableIndex index ) const -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mDescriptorMutex };
        mResources.erase( ( core::u64{ slot } << 32 ) | index );

        auto it{ mDescriptorTableItemIndices.find( slot ) };

        if ( it != mDescriptorTableItemIndices.end() ) {
            it->second.emplace( index );
        }
    }

    auto DescriptorTable::AllocateNextIndex( core::u32 slot ) const -> rhi::DescriptorTableIndex {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mDescriptorMutex };

        auto it{ mDescriptorTableItemIndices.find( slot ) };

        if ( it != mDescriptorTableItemIndices.end() ) {
            // Exhaustion is a runtime condition in Release too. Dereferencing
            // an empty free-index set corrupts its dense-map bookkeeping.
            if ( it->second.empty() ) {
                MKT_THROW_RUNTIME_ERROR( string::Format( "Vulkan bindless descriptor slot {} capacity exhausted.", slot ).c_str() );
            }

            const renderer::rhi::DescriptorTableIndex result{ *it->second.begin() };
            it->second.erase( it->second.begin() );

            return result;
        }

        MKT_THROW_RUNTIME_ERROR( "Vulkan bindless descriptor slot does not exist." );
    }

    auto DescriptorTable::ReleaseIndex( renderer::rhi::DescriptorTableIndex index, renderer::rhi::ResourceType type, core::u32 slot ) -> void {
        std::lock_guard lock{ mDescriptorMutex };
        const auto &layout{ static_cast<BindingLayout &>( *mBindingLayout ).GetBindlessLayoutDesc() };

        for ( const auto &item: layout.mSlots ) {
            if ( item.mSlot == slot ) {
                if ( item.mResourceType != type || index >= item.mMaxCapacity ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid bindless descriptor release." );
                }

                if ( !mDescriptorTableItemIndices.at( slot ).emplace( index ).second ) {
                    MKT_THROW_RUNTIME_ERROR( "Bindless descriptor was released twice." );
                }

                mResources.erase( ( core::u64{ slot } << 32 ) | index );
                return;
            }
        }

        MKT_THROW_RUNTIME_ERROR( "Vulkan bindless descriptor slot does not exist." );
    }

    auto DescriptorTable::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        auto *device{ core::checked_cast<Device *>( mDevice ) };
        auto *layout{ core::checked_cast<BindingLayout *>( mBindingLayout.GetPtr() ) };

        mDescriptorAllocatorHandle = device->GetDescriptorAllocator();
        ( void )mDescriptorAllocatorHandle.Allocate( layout->GetNativeHandle( renderer::rhi::ObjectType::Vk_DescriptorSetLayout ), mDescriptorSet );

        for ( const auto &item: layout->GetBindlessLayoutDesc().mSlots ) {
            mSlotSize[item.mSlot] = item.mMaxCapacity;

            for ( renderer::rhi::DescriptorTableIndex index{}; index < item.mMaxCapacity; ++index ) {
                mDescriptorTableItemIndices[item.mSlot].emplace( index );
            }
        }

        mIsAllocated = mDescriptorSet != VK_NULL_HANDLE;
    }

    auto DescriptorTable::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        auto *device{ core::checked_cast<Device *>( mDevice ) };
        MKT_VK_CHECK( vkFreeDescriptorSets(
            device->GetDevice(),
            mDescriptorAllocatorHandle.mDescriptorPool,
            1,
            MKT_ADDRESSOF( mDescriptorSet ) ) );

        mIsAllocated = false;
    }

    InputLayout::InputLayout( const renderer::rhi::InputLayoutCreateDescription &desc )
        : mDesc{ desc } {
        MKT_BEGIN_PROFILER();

        for ( const auto &bindingDesc: desc.mVertexBindingDescriptions ) {
            VkVertexInputBindingDescription vkBinding{};

            vkBinding.binding = bindingDesc.mBinding;
            vkBinding.stride = bindingDesc.mStride;

            vkBinding.inputRate =
                ( bindingDesc.mRate == renderer::rhi::InputRate::ePerVertex )
                    ? VK_VERTEX_INPUT_RATE_VERTEX
                    : VK_VERTEX_INPUT_RATE_INSTANCE;

            mVertexBindingDescriptions.emplace_back( vkBinding );
        }

        for ( const auto &attr: desc.mVertexAttributeDescriptions ) {
            VkVertexInputAttributeDescription vkAttr{};

            vkAttr.location = attr.mLocation;
            vkAttr.binding = attr.mBinding;
            vkAttr.format = GetFormat( attr.mFormat );
            vkAttr.offset = attr.mOffset;

            mVertexAttributeDescriptions.emplace_back( vkAttr );
        }
    }

    auto InputLayout::GetVertexBindingDesc() const -> const eastl::fixed_vector<VkVertexInputBindingDescription, renderer::rhi::kMaxVertexBindings> & {
        MKT_BEGIN_PROFILER();
        return mVertexBindingDescriptions;
    }

    auto InputLayout::GetVertexAttributesDesc() const -> const eastl::fixed_vector<VkVertexInputAttributeDescription, renderer::rhi::kMaxVertexAttributes> & {
        MKT_BEGIN_PROFILER();
        return mVertexAttributeDescriptions;
    }

    auto InputLayout::GetNumAttributes() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mVertexAttributeDescriptions.size();
    }

    auto InputLayout::GetAttributeDescription( core::u32 index ) const -> const renderer::rhi::VertexAttributeDescription & {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( index < mDesc.mVertexAttributeDescriptions.size(), "Specified index out of bounds." );
        return mDesc.mVertexAttributeDescriptions.at( index );
    }

    InputLayout::~InputLayout() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto InputLayout::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = true;
    }

    auto InputLayout::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    PipelineLayout::PipelineLayout( const renderer::rhi::PipelineLayoutCreateDescription &desc )
        : mDescription{ desc } {
    }

    auto PipelineLayout::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( name.empty() || !mDevice ) {
            return;
        }

        mDebugName = name;

        auto *device{ core::checked_cast<Device *>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_PIPELINE_LAYOUT, core::rc_cast<core::u64>( mPipelineLayout ), mDebugName );
    }

    auto PipelineLayout::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::Vk_PipelineLayout:
                return renderer::rhi::Object( mPipelineLayout );
            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    auto PipelineLayout::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::Vk_PipelineLayout:
                return renderer::rhi::Object( mPipelineLayout );
            default:;
        }

        return renderer::rhi::Object( nullptr );
    }

    PipelineLayout::operator VkPipelineLayout() const {
        MKT_BEGIN_PROFILER();
        return mPipelineLayout;
    }

    PipelineLayout::~PipelineLayout() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto PipelineLayout::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        auto *device{ core::checked_cast<Device *>( mDevice ) };

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
        core::u32 maxSet{ 0 };

        for ( const auto &bindingLayout: mDescription.mBindingLayouts ) {
            maxSet = eastl::max( maxSet, bindingLayout->GetRegisterSpace() );
        }

        // Initialize everything with "holes" and fill accordingly
        VkDescriptorSetLayout emptySetLayout{ device->GetLayoutForEmptySet() };
        eastl::vector<VkDescriptorSetLayout> setLayouts( maxSet + 1, emptySetLayout );

        // Place set layouts at correct set indices
        for ( const auto &bindingLayout: mDescription.mBindingLayouts ) {
            setLayouts[bindingLayout->GetRegisterSpace()] = *core::checked_cast<const BindingLayout *>( bindingLayout.GetPtr() );
        }

        VkPipelineLayoutCreateInfo plInfo{ initializers::PipelineLayoutCreateInfo() };

        plInfo.setLayoutCount = core::as<core::u32>( setLayouts.size() );
        plInfo.pSetLayouts = setLayouts.data();

        // I am not sure if this is good design but for the time being push constants will always be available for all
        // pipeline layouts, although limited to amount guaranteed by vulkan across devices (128 bytes)
        VkPushConstantRange range{};
        range.offset = 0;
        range.size = math::Min( renderer::rhi::kMaxPushConstantSize, mDescription.mPushConstantsSize );// Get the min, kMaxPushConstantSize is guaranteed max across devices
        range.stageFlags = GetShaderStageFlags( mDescription.mPushConstantsVisibility );

        // size validations offset and size need to be multiple of 4
        // https://docs.vulkan.org/refpages/latest/refpages/source/VkPushConstantRange.html
        core::usize size{ rhi::NextMultiple( range.size, 4 ) };
#if MIKOTO_DEBUG
        if ( size != mDescription.mPushConstantsSize ) {
            MKT_CORE_LOGGER_WARN( "Size for push constant range is not multiple of 4, changing size to {}", size );
        }
#endif
        range.size = size;
        mDescription.mPushConstantsSize = size;

        eastl::array psRanges{ range };

        if ( range.size != 0 ) {
            plInfo.pushConstantRangeCount = core::as<core::u32>( psRanges.size() );
            plInfo.pPushConstantRanges = psRanges.data();
        }

        MKT_VK_CHECK( vkCreatePipelineLayout( device->GetDevice(), &plInfo, nullptr, MKT_ADDRESSOF( mPipelineLayout ) ) );
        mIsAllocated = true;
    }

    auto PipelineLayout::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        auto *device{ core::checked_cast<Device *>( mDevice ) };

        vkDestroyPipelineLayout( device->GetDevice(), mPipelineLayout, nullptr );

        mIsAllocated = false;
    }

    ShaderTable::ShaderTable( const rhi::ShaderTableDescription &description )
        : mDescription{ description } {
        mPipeline = rhi::RayTracingPipelineHandle{ core::checked_cast<rhi::RayTracingPipeline *>( description.mPipeline ) };
    }

    ShaderTable::~ShaderTable() {
        Destroy();
    }

    auto ShaderTable::GetDesc() const -> const rhi::ShaderTableDescription & {
        return mDescription;
    }

    auto ShaderTable::GetPipeline() const -> rhi::IPipeline * {
        return mDescription.mPipeline;
    }

    auto ShaderTable::GetNumEntries() const -> core::u32 {
        return 1u + mDescription.mMissRecordCount + mDescription.mHitGroupRecordCount;
    }

    auto ShaderTable::GetRegion( AddressRegion region ) const -> const VkStridedDeviceAddressRegionKHR & {
        switch ( region ) {
            case AddressRegion::eRayGeneration:
                return mRayGenRegion;
            case AddressRegion::eRayMiss:
                return mMissRegion;
            case AddressRegion::eHitGroup:
                return mHitGroupsRegion;
            case AddressRegion::eCallable:
                return mCallableRegion;
        }

        MKT_THROW_RUNTIME_ERROR( "Unknown shader table region." );
    }

    auto ShaderTable::GetBuffer() const -> const rhi::BufferHandle & {
        return mSbtBuffer;
    }

    auto ShaderTable::Destroy() -> void {
        mSbtBuffer.Reset();
        mRayGenRegion = {};
        mMissRegion = {};
        mHitGroupsRegion = {};
        mCallableRegion = {};
        mIsAllocated = false;
    }

    auto ShaderTable::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        Device *device{ core::checked_cast<Device *>( mDevice ) };

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR properties{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR };

        // Query the record layout limits and the three groups defined by the pipeline.
        VkPhysicalDeviceProperties2 physical{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2 };
        physical.pNext = &properties;
        vkGetPhysicalDeviceProperties2( device->GetPhysicalDevice()->mPhysicalDevice, &physical );

        constexpr core::u32 groupCount{ 3 };
        eastl::vector<core::u8> shaderHandles{};
        shaderHandles.resize( properties.shaderGroupHandleSize * groupCount );

        VkPipeline pipeline{ mPipeline->GetNativeHandle( rhi::ObjectType::Vk_Pipeline ) };
        MKT_VK_CHECK( vkGetRayTracingShaderGroupHandlesKHR( device->GetDevice(), pipeline, 0, groupCount, shaderHandles.size(), shaderHandles.data() ) );

        // Record strides align handles; each region has a separate base alignment.
        const VkDeviceSize handleSize{ properties.shaderGroupHandleSize };
        const VkDeviceSize handleAlignment{ properties.shaderGroupHandleAlignment };
        const VkDeviceSize baseAlignment{ properties.shaderGroupBaseAlignment };

        const VkDeviceSize stride{ memory::AlignUp( handleSize, handleAlignment ) };
        const VkDeviceSize rayGenSize{ stride };
        const VkDeviceSize missSize{ stride * mDescription.mMissRecordCount };
        const VkDeviceSize hitSize{ stride * mDescription.mHitGroupRecordCount };

        const VkDeviceSize rayGenOffset{ 0 };
        const VkDeviceSize missOffset{ memory::AlignUp( rayGenSize, baseAlignment ) };
        const VkDeviceSize hitOffset{ memory::AlignUp( missOffset + missSize, baseAlignment ) };
        const VkDeviceSize callableOffset{ memory::AlignUp( hitOffset + hitSize, baseAlignment ) };
        const VkDeviceSize bufferSize{ callableOffset };

        // Align the actual device address too: a suballocation's start need not
        // satisfy shaderGroupBaseAlignment. Publish new storage only after upload.
        auto buffer{ mDevice->CreateBuffer( rhi::BufferCreateDescription{}
            .SetName( "ShaderTable" )
            .SetByteSize( bufferSize + baseAlignment - 1 )
            .SetHeapType( rhi::HeapType::eUpload )
            .SetResourceType( rhi::ResourceType::eRawBuffer_SRV )
            .SetBufferUsage( rhi::BufferUsageFlagsBits::ShaderBindingTable ) ) };

        const VkDeviceAddress address{ memory::AlignUp( buffer->GetGpuDeviceAddress(), baseAlignment ) };
        eastl::vector<core::u8> records( core::as<core::usize>( bufferSize ), 0 );

        // Group handles are tightly packed; destination records include alignment padding.
        std::memcpy( records.data() + rayGenOffset, shaderHandles.data(), handleSize );

        for ( core::u32 index{}; index < mDescription.mMissRecordCount; ++index ) {
            std::memcpy( records.data() + missOffset + index * stride, shaderHandles.data() + handleSize, handleSize );
        }

        for ( core::u32 index{}; index < mDescription.mHitGroupRecordCount; ++index ) {
            std::memcpy( records.data() + hitOffset + index * stride, shaderHandles.data() + 2 * handleSize, handleSize );
        }

        // Upload heaps are written immediately through the command-list interface.
        core::u64 fenceValue{ 0 };
        renderer::rhi::FenceHandle fence{ mDevice->CreateFence( fenceValue++ ) };
        CommandListHandle commands{ mDevice->CreateCommandList( rhi::QueueType::eTransfer ) };

        commands->Open( {} );
        commands->Write( buffer.GetPtr(), core::as<core::usize>( address - buffer->GetGpuDeviceAddress() ), records.data(), records.size() );
        commands->Close();

        // Signal fenceValue on one fence on completion of these
        // commands, then we wait for that completion this blocks the caller
        // but client should ideally offload this task to worker threads
        const auto submitInfo{ renderer::rhi::SubmitInfo{}
                .AddSignal( fence, fenceValue )
                .AddCommandList( commands ) };
        mDevice->GetQueue( renderer::rhi::QueueType::eTransfer )->ExecuteCommandLists( submitInfo );
        ( void )fence->Wait( fenceValue, eastl::numeric_limits<core::u64>::max() );

        mSbtBuffer = eastl::move( buffer );
        mRayGenRegion = { address + rayGenOffset, stride, rayGenSize };
        mMissRegion = mDescription.mMissRecordCount ? VkStridedDeviceAddressRegionKHR{ address + missOffset, stride, missSize } : VkStridedDeviceAddressRegionKHR{};

        mHitGroupsRegion = mDescription.mHitGroupRecordCount ? VkStridedDeviceAddressRegionKHR{ address + hitOffset, stride, hitSize } : VkStridedDeviceAddressRegionKHR{};

        mCallableRegion = {};

        mIsAllocated = true;
    }
}// namespace mikoto::renderer::vulkan
