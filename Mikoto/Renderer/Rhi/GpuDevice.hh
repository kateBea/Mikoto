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
#include <Renderer/Rhi/SwapChain.hh>
#include <Renderer/Rhi/CommandList.hh>
#include <Renderer/Rhi/CommandQueue.hh>
#include <Renderer/Rhi/AccelerationStructure.hh>

namespace mikoto::renderer::rhi {

    /**
     * Bit storage for optional device capabilities.
     */
    struct GpuFeatureSupportProperties {
        using Data = core::u64;
    };

    using GpuFeatureSupportFlags = core::Flags<GpuFeatureSupportProperties>;

    struct GpuFeatureSupportFlagsBits {
        // Device with bare minimum support
        static constexpr GpuFeatureSupportFlags None{ 0 };

        static constexpr GpuFeatureSupportFlags ChooseDiscreteDevice{ BIT_SET(0) };
        static constexpr GpuFeatureSupportFlags AnisotropicFiltering{ BIT_SET(1) };
        static constexpr GpuFeatureSupportFlags HardwareWireframe{ BIT_SET(2) };
        static constexpr GpuFeatureSupportFlags EnablePresentation{ BIT_SET(3) };
        static constexpr GpuFeatureSupportFlags EnableRayTracing{ BIT_SET(4) };
        static constexpr GpuFeatureSupportFlags EnableMeshShaders{ BIT_SET(5) };
        static constexpr GpuFeatureSupportFlags BCTextureCompression{ BIT_SET(6) };
        static constexpr GpuFeatureSupportFlags BindlessDescriptorTables{ BIT_SET(7) };
    };

    /**
     * Selects the API, device class and required capabilities for an RHI device.
     */
    struct GpuDeviceCreateInfo {
        GraphicsAPI mApi{ GraphicsAPI::eInvalid };
        GpuDeviceType mDeviceType{ GpuDeviceType::eInvalid };
        GpuFeatureSupportFlags mFeatureSupportFlags{ GpuFeatureSupportFlagsBits::None };
    };

    /**
     * Factory and lifetime owner for all backend-agnostic RHI objects.
     *
     * Implementations create resources, expose queues, map CPU-visible buffers,
     * and provide device-wide synchronization and memory statistics.
     */
    class IGpuDevice {
    public:

        /**
         * Initializes the selected backend device and its queues.
         */
        virtual auto Init() -> void = 0;

        /**
         * Releases all backend device resources.
         */
        virtual auto Shutdown() -> void = 0;

        /**
         * Creates a texture from an RHI description.
         *
         * @param description Texture creation parameters.
         * @returns The created texture.
         */
        MKT_NODISCARD virtual auto CreateTexture( const TextureCreateDescription& description ) -> TextureHandle = 0;

        /**
         * Wraps a backend-native texture object.
         *
         * @param type Type of the native object.
         * @param object Native backend object.
         * @param description Texture creation parameters.
         * @returns The created texture wrapper.
         */
        MKT_NODISCARD virtual auto CreateTextureNative( ObjectType type, Object object, const TextureCreateDescription& description ) -> TextureHandle = 0;

        /**
         * Creates a buffer from an RHI description.
         *
         * @param description Buffer creation parameters.
         * @returns The created buffer.
         */
        MKT_NODISCARD virtual auto CreateBuffer( const BufferCreateDescription& description ) -> BufferHandle = 0;

        /**
         * Creates immutable sampler state from an RHI description.
         *
         * @param description Sampler creation parameters.
         * @returns The created sampler.
         */
        MKT_NODISCARD virtual auto CreateSampler( const SamplerCreateDescription& description ) -> SamplerHandle = 0;

        /**
         * Creates an immutable compute pipeline.
         *
         * @param description Compute-pipeline creation parameters.
         * @returns The created pipeline.
         */
        MKT_NODISCARD virtual auto CreatePipeline( const ComputePipelineDescription& description ) -> PipelineHandle = 0;

        /**
         * Creates an immutable graphics pipeline.
         *
         * @param description Graphics-pipeline creation parameters.
         * @returns The created pipeline.
         */
        MKT_NODISCARD virtual auto CreatePipeline( const GraphicsPipelineDescription& description ) -> PipelineHandle = 0;

        /**
         * Creates a ray-tracing acceleration structure.
         *
         * @param description Acceleration-structure creation parameters.
         * @returns The created acceleration structure.
         */
        MKT_NODISCARD virtual auto CreateAccelStructure( const AccelStructureCreateDescription& description ) -> AccelStructureHandle = 0;

        /**
         * Creates a command list for @p queue.
         *
         * @param queue Queue type that will execute the command list.
         * @returns The created command list.
         */
        MKT_NODISCARD virtual auto CreateCommandList( QueueType queue ) -> CommandListHandle = 0;

        /**
         * Creates a command list with explicit allocation settings.
         *
         * @param desc Command-list creation parameters.
         * @returns The created command list.
         */
        MKT_NODISCARD virtual auto CreateCommandList( const CommandListCreateDescription& desc ) -> CommandListHandle = 0;

        /**
         * Creates a compiled or source shader module.
         *
         * @param desc Shader-module creation parameters.
         * @returns The created shader module.
         */
        MKT_NODISCARD virtual auto CreateShader( const ShaderModuleCreateDescription& desc ) -> ShaderModuleHandle = 0;

        /**
         * Creates a vertex input layout.
         *
         * @param desc Input-layout creation parameters.
         * @returns The created input layout.
         */
        MKT_NODISCARD virtual auto CreateInputLayout(const InputLayoutCreateDescription& desc) -> InputLayoutHandle = 0;

        /**
         * Creates a fence initialized to @p fenceInitialValue.
         *
         * @param fenceInitialValue Initial fence value.
         * @returns The created fence.
         */
        MKT_NODISCARD virtual auto CreateFence( core::u64 fenceInitialValue ) -> FenceHandle = 0;

        // For data read back, user will probably need to handle synchronization externally
        // or have the command lists do it. Map returns a buffer we can copy data from
        // writes need to be done via command lists.
        // These map the whole buffer, might need to look into variants to maybe map regions

        /**
         * Ends a CPU mapping of a buffer.
         *
         * @param buffer Mapped buffer.
         */
        virtual auto UnMap( IBuffer* buffer ) -> void = 0;

        /**
         * Maps an entire CPU-visible buffer.
         *
         * @param buffer Buffer to map.
         * @returns Pointer to the mapped memory, or null when mapping fails.
         */
        MKT_NODISCARD virtual auto Map( IBuffer* buffer ) -> void* = 0;

        /**
         * Creates a binding layout.
         *
         * @param desc Binding-layout creation parameters.
         * @returns The created binding layout.
         */
        MKT_NODISCARD virtual auto CreateBindingLayout( const BindingLayoutDescription& desc ) -> BindingLayoutHandle = 0;

        /**
         * Creates a pipeline layout from binding layouts and push constants.
         *
         * @param desc Pipeline-layout creation parameters.
         * @returns The created pipeline layout.
         */
        MKT_NODISCARD virtual auto CreatePipelineLayout( const PipelineLayoutCreateDescription& desc ) -> PipelineLayoutHandle = 0;

        /**
         * Creates resource bindings matching @p layout.
         *
         * @param desc Binding-table contents.
         * @param layout Layout used by the binding table.
         * @returns The created binding table.
         */
        MKT_NODISCARD virtual auto CreateBindingTable( const BindingTableDescription& desc, BindingLayoutHandle layout ) -> BindingTableHandle = 0;

        /**
         * Rewrites the mutable resources in a binding table.
         *
         * @param desc Updated binding-table contents.
         * @param table Binding table to update.
         */
        virtual auto UpdateBindingTable( const BindingTableDescription& desc, BindingTableHandle table ) -> void = 0;

        /**
         * Creates a descriptor-table layout.
         *
         * @param desc Descriptor-table-layout creation parameters.
         * @returns The created descriptor-table layout.
         */
        MKT_NODISCARD virtual auto CreateDescriptorTableLayout( const DescriptorTableLayoutDescription& desc ) -> BindingLayoutHandle = 0;

        /**
         * Creates a descriptor table from a layout.
         *
         * @param layout Layout used by the descriptor table.
         * @returns The created descriptor table.
         */
        MKT_NODISCARD virtual auto CreateDescriptorTable( BindingLayoutHandle layout ) -> DescriptorTableHandle = 0;

        /**
         * Resizes a descriptor table, optionally preserving its assignments.
         *
         * @param descriptorTable Descriptor table to resize.
         * @param newSize New descriptor capacity.
         * @param keepContents Whether existing assignments are preserved.
         * @returns True when the resize succeeds.
         */
        MKT_NODISCARD virtual auto ResizeDescriptorTable( DescriptorTableHandle descriptorTable, core::u32 newSize, bool keepContents ) -> bool = 0;

        /**
         * Writes one resource assignment and returns its descriptor index.
         *
         * @param descriptorTable Descriptor table to update.
         * @param item Resource assignment to write.
         * @returns The descriptor index assigned to the item.
         */
        MKT_NODISCARD virtual auto WriteDescriptorTable( DescriptorTableHandle descriptorTable, const BindingTableItem& item ) -> DescriptorTableIndex = 0;

        /**
         * Creates a presentation swap chain.
         *
         * @param description Swap-chain creation parameters.
         * @returns The created swap chain.
         */
        MKT_NODISCARD virtual auto CreateSwapChain( const SwapChainDescription& description ) -> SwapChainHandle = 0;

        /**
         * Returns the queue of the requested type, if the device exposes one.
         *
         * @param type Queue type to retrieve.
         * @returns The requested queue, or null when unsupported.
         */
        MKT_NODISCARD virtual auto GetQueue( QueueType type ) -> IQueue* = 0;

        /**
         * Returns allocated GPU memory in bytes.
         *
         * @returns Allocated GPU memory in bytes.
         */
        MKT_NODISCARD virtual auto GetMemoryUsage() const -> core::usize = 0;

        /**
         * Returns total GPU memory in bytes.
         *
         * @returns Total GPU memory in bytes.
         */
        MKT_NODISCARD virtual auto GetMemoryTotal() const -> core::usize = 0;

        /**
         * Returns currently available GPU memory in bytes.
         *
         * @returns Available GPU memory in bytes.
         */
        MKT_NODISCARD virtual auto GetMemoryAvailable() const -> core::usize = 0;

        /**
         * Releases resources deferred by the backend.
         */
        virtual auto RunGarbageCollection() -> void = 0;

        /**
         * Blocks until all submitted GPU work has finished.
         */
        virtual auto WaitIdle() -> void = 0;

        /**
         * Reports whether @ref Init has completed successfully.
         *
         * @returns True when the device has been initialized.
         */
        MKT_NODISCARD virtual auto IsInitialized() const -> bool = 0;

        /**
         * Returns whether this device uses @p api.
         *
         * @param api API to compare against.
         * @returns True when this device uses @p api.
         */
        MKT_NODISCARD virtual auto IsGraphicsApi( GraphicsAPI api ) const -> bool = 0;

        /**
         * Returns the graphics API selected for this device.
         *
         * @returns The selected graphics API.
         */
        MKT_NODISCARD virtual auto GetGraphicsApi() const -> GraphicsAPI = 0;

        /**
         * Returns the backend-reported device name.
         *
         * @returns The device name.
         */
        MKT_NODISCARD virtual auto GetDeviceName() const -> eastl::string_view = 0;

        /**
         * Returns the backend-reported vendor identifier.
         *
         * @returns The vendor identifier.
         */
        MKT_NODISCARD virtual auto GetVendorID() const -> eastl::string_view = 0;

        /**
         * Returns the backend-reported driver version.
         *
         * @returns The driver version.
         */
        MKT_NODISCARD virtual auto GetDriverVersion() const -> eastl::string_view = 0;

        /**
         * Destroys the device.
         */
        virtual ~IGpuDevice() = default;

        /**
         * Creates the platform-supported backend selected by @p createInfo.
         *
         * @param createInfo API, device type, and feature requirements.
         * @returns The created device, or null when no supported backend is available.
         */
        MKT_NODISCARD static auto Create( const GpuDeviceCreateInfo& createInfo ) -> eastl::unique_ptr<IGpuDevice>;
    };
}// namespace mikoto::renderer

#endif//MIKOTO_RHI_GPU_DEVICE_HH
