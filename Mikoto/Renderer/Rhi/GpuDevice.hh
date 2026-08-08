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

#ifndef MIKOTO_RHI_GPU_DEVICE_HH
#define MIKOTO_RHI_GPU_DEVICE_HH

#include <EASTL/memory.h>
#include <EASTL/span.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Filesystem/Path.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Fence.hh>
#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/CommandList.hh>
#include <Renderer/Rhi/CommandQueue.hh>
#include <Renderer/Rhi/AccelerationStructure.hh>

namespace mikoto::renderer::rhi {

    struct GpuFeatureSupport {
        // Properties we want the device to support
        // used to pick the appropriate physical device
        bool mChooseDiscreteDevice{ true };
        bool mAnisotropicFiltering{ true };
        bool mHardwareWireframe{ true };
        bool mEnablePresentation{ true };
        bool mEnableRayTracingSupport{ true };

        GpuDeviceType mDeviceType{ GpuDeviceType::eDiscrete };
    };

    struct GpuDeviceCreateInfo {
        GraphicsAPI mApi{ GraphicsAPI::eVulkan };
        GpuFeatureSupport mFeaturesSupport{};
    };

    class IGpuDevice {
    public:
        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        MKT_NODISCARD virtual auto CreateTexture( const TextureCreateDescription& description ) -> TextureHandle = 0;
        MKT_NODISCARD virtual auto CreateTextureNative( ObjectType type, Object object, const TextureCreateDescription& description ) -> TextureHandle = 0;

        MKT_NODISCARD virtual auto CreateBuffer( const BufferCreateDescription& description ) -> BufferHandle = 0;

        MKT_NODISCARD virtual auto CreateSampler( const SamplerCreateDescription& description ) -> SamplerHandle = 0;

        MKT_NODISCARD virtual auto CreatePipeline( const ComputePipelineDescription& description ) -> PipelineHandle = 0;
        MKT_NODISCARD virtual auto CreatePipeline( const GraphicsPipelineDescription& description ) -> PipelineHandle = 0;

        MKT_NODISCARD virtual auto CreateAccelStructure( const AccelStructureCreateDescription& description ) -> AccelStructureHandle = 0;

        MKT_NODISCARD virtual auto CreateCommandList( QueueType queue ) -> CommandListHandle = 0;

        MKT_NODISCARD virtual auto CreateShader( const ShaderModuleCreateDescription& desc ) -> ShaderModuleHandle = 0;

        MKT_NODISCARD virtual auto CreateInputLayout(const InputLayoutCreateDescription& desc) -> InputLayoutHandle = 0;

        MKT_NODISCARD virtual auto CreateFence( core::u64 fenceInitialValue ) -> FenceHandle = 0;

        // For data read back, user will probably need to handle synchronization externally
        // or have the command lists do it. Map returns a buffer we can copy data from
        // writes need to be done via command lists.
        // These map the whole buffer, might need to look into variants to maybe map regions
        virtual auto UnMap( IBuffer* buffer ) -> void = 0;
        MKT_NODISCARD virtual auto Map(IBuffer* buffer) -> void* = 0;

        // For backends that support it this allows us to create the layout from shader reflection
        MKT_NODISCARD virtual auto CreateBindingLayout( const BindingLayoutDescription& desc ) -> BindingLayoutHandle = 0;
        MKT_NODISCARD virtual auto CreatePipelineLayout( const PipelineLayoutCreateDescription& desc ) -> PipelineLayoutHandle = 0;
        MKT_NODISCARD virtual auto CreateBindingSet( const BindingSetDescription& desc, BindingLayoutHandle layout ) -> BindingSetHandle = 0;

        // To support bindless techniques in modern graphics APIs
        MKT_NODISCARD virtual auto CreateBindlessLayout( const BindlessLayoutDescription& desc ) -> BindingLayoutHandle = 0;

        MKT_NODISCARD virtual auto CreateDescriptorTable( BindingLayoutHandle layout ) -> DescriptorTableHandle = 0;
        MKT_NODISCARD virtual auto ResizeDescriptorTable( DescriptorTableHandle descriptorTable, core::u32 newSize, bool keepContents ) -> bool = 0;
        MKT_NODISCARD virtual auto WriteDescriptorTable( DescriptorTableHandle descriptorTable, const BindingSetItem& item ) -> bool = 0;

        MKT_NODISCARD virtual auto GetQueue( QueueType type ) -> IQueue* = 0;

        virtual auto RunGarbageCollection() -> void = 0;

        virtual auto WaitIdle() -> void = 0;

        MKT_NODISCARD auto IsInitialized() const -> bool;
        MKT_NODISCARD auto IsGraphicsApi( GraphicsAPI api ) const -> bool;
        MKT_NODISCARD auto GetGraphicsApi() const -> GraphicsAPI;
        MKT_NODISCARD auto GetDeviceName() const -> eastl::string_view;

        virtual ~IGpuDevice() = default;

        MKT_NODISCARD static auto Create( const GpuDeviceCreateInfo& createInfo ) -> eastl::unique_ptr<IGpuDevice>;

    protected:
        explicit IGpuDevice( GraphicsAPI api, const GpuFeatureSupport& featuresSupport );

    protected:
        eastl::string mName{};

        GraphicsAPI mApi{};
        GpuFeatureSupport mFeaturesSupport{};

        bool mIsInitialized{ false };
    };
}// namespace mikoto::renderer

#endif//MIKOTO_RHI_GPU_DEVICE_HH