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

#ifndef MIKOTO_RENDERER_RHI_GPU_DEVICE_HH
#define MIKOTO_RENDERER_RHI_GPU_DEVICE_HH

#include <EASTL/span.h>
#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Denoiser.hh>
#include <Renderer/Rhi/Fence.hh>
#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/QueryPool.hh>
#include <Renderer/Rhi/SwapChain.hh>
#include <Renderer/Rhi/ShaderTable.hh>
#include <Renderer/Rhi/CommandList.hh>
#include <Renderer/Rhi/CommandQueue.hh>
#include <Renderer/Rhi/VideoDecoder.hh>
#include <Renderer/Rhi/AccelerationStructure.hh>

namespace mikoto::platform {
    class IWindow;
}

namespace mikoto::renderer::rhi {

    /** @brief Process memory usage on the selected adapter, in bytes.
     * Local memory is VRAM on a discrete GPU; non-local memory is shared RAM.
     * Uses OS accounting where available; allocator backends may provide an estimate.
     * This is not the whole adapter's usage. OS readings include driver allocations.
     */
    struct ProcessGpuMemory {
        core::usize mLocalBytes{};
        core::usize mSharedBytes{};
        bool mAvailable{};
    };

    /**
     * Selects the API, device class and required capabilities for an RHI device.
     */
    struct GpuDeviceCreateInfo {
        GraphicsAPI mApi{ GraphicsAPI::eInvalid };
        GpuDeviceType mDeviceType{ GpuDeviceType::eInvalid };
        GpuFeatureSupportFlags mFeatureSupportFlags{ GpuFeatureSupportFlagsBits::None };

        // Borrowed window for devices that load functions through a window context.
        platform::IWindow* mWindow{};
    };

    /**
     * Factory and lifetime owner for all backend-agnostic RHI objects.
     *
     * Implementations create resources, expose queues, map CPU-visible buffers,
     * and provide device-wide synchronization and memory statistics.
     */
    class IGpuDevice {
    public:
        // CPU preparation only; the command list records initialization separately.
        MKT_NODISCARD virtual auto CreateDenoiser( const DenoiserDescription &description ) -> DenoiserHandle;

        // Nonblocking: a preparing result retains the current graph until sizing
        // is available. Native size remains the fallback for unsupported devices.
        MKT_NODISCARD virtual auto GetDenoiserInputSizes( const DenoiserDescription &description ) -> DenoiserInputSizes;

        /**
         * Creates a hardware video decoder from parsed stream configuration.
         *
         * The decoder retains the configuration supplied by the description and uses
         * its H.264 SPS/PPS to initialize the backend session. Creation does not decode
         * or submit a picture; decode commands are recorded on a video command list,
         * with queue submission and resource ownership handled by the frame graph.
         *
         * @param description Decoder creation parameters, including a parsed H.264 configuration.
         * @returns The created decoder, or an empty handle when the backend has no
         *          video decoder implementation, no decode queue, or no configuration.
         * @note Unsupported stream parameters and backend initialization failures throw exceptions.
         */
        MKT_NODISCARD virtual auto CreateVideoDecoder( const VideoDecoderDescription& description ) -> VideoDecoderHandle;

        /**
         * Initializes the selected backend device and its queues.
         */
        virtual auto Initialize() -> void = 0;


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
        * Checks whether ray tracing is requested and supported for use by the renderer.
        * Use IsFeatureSupported for hardware capability queries independent of requests.
        *
        * @returns True if ray tracing is available for this device's rendering paths, false otherwise.
        */
        MKT_NODISCARD virtual auto SupportsRayTracing() const -> bool = 0;


        /**
        * Checks whether the graphics device supports a specific set of GPU features.
        *
        * @param supportFlags The bitmask flags representing the features to check.
        * @returns True if all specified features are supported, false otherwise.
        */
        MKT_NODISCARD virtual auto IsFeatureSupported( GpuFeatureSupportFlags supportFlags ) const -> bool = 0;


        /** @brief Checks which features were requested when this device was created.
         *
         * This reports the creation request independently of hardware support or enablement.
         * @param requestFlags Feature flags to check against the device creation request.
         * @return True if every specified flag was requested; an empty mask returns true.
         */
        MKT_NODISCARD virtual auto IsFeatureRequested( GpuFeatureSupportFlags requestFlags ) const -> bool = 0;


        /**
         * Checks whether block compression formats (specifically BC6H and BC7)
         * are supported for sampled textures on this device.
         *
         * @returns True if BC6H/BC7 texture compression is supported, false otherwise.
         */
        MKT_NODISCARD virtual auto SupportsBCTextureCompression() const -> bool = 0;


        /**
         * Creates an immutable ray-tracing pipeline.
         *
         * @param description Ray-tracing-pipeline creation parameters.
         * @returns The created ray-tracing pipeline handle.
         */
        MKT_NODISCARD virtual auto CreatePipeline( const RayTracingPipelineDescription& description ) -> RayTracingPipelineHandle = 0;


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
         * @brief Creates a new Shader Binding Table (SBT) instance for ray-tracing pipelines.
         *
         * @param desc Configuration and layout criteria used to initialize the shader table.
         * @return A handle representing the newly created shader table object.
         */
        MKT_NODISCARD virtual auto CreateShaderTable( const ShaderTableDescription& desc ) -> ShaderTableHandle = 0;


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
         * Synchronously reads a rectangle from mip zero of an uncompressed, single-sample 2D color texture.
         * Submit its producing commands first and call from the submission thread. The texture needs CopySource usage.
         *
         * @param texture Source texture owned by this device.
         * @param region Rectangle in texels, with its origin at the upper left and exclusive maxima.
         * @param destination Tightly packed output rows in the texture format, top row first.
         * @returns True on success; false for unsupported or invalid reads.
         */
        MKT_NODISCARD virtual auto ReadTextureRegion(
            const ITexture* texture, const Rect& region,
            eastl::span<core::u8> destination ) -> bool = 0;


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
         * Allocates a descriptor table slot and initializes the given item in its place.
         *
         * @param descriptorTable Descriptor table to update.
         * @param item Resource assignment to write.
         * @returns The descriptor index assigned to the item.
         */
        MKT_NODISCARD virtual auto WriteDescriptorTable( DescriptorTableHandle descriptorTable, const BindingTableItem& item ) -> DescriptorTableIndex = 0;


        /**
         * Releases an allocated descriptor table slot back to the given descriptor table.
         * The caller must ensure all GPU work using the index has completed before releasing it.
         *
         * @param descriptorTable Descriptor table to update.
         * @param bindingIndex The binding index or slot within the table.
         * @param index Descriptor index to release.
         */
        virtual auto ReleaseDescriptorTableIndex( DescriptorTableHandle descriptorTable, core::u32 bindingIndex, DescriptorTableIndex index ) -> void = 0;


        /**
         * Creates a presentation swap chain.
         *
         * @param description Swap-chain creation parameters.
         * @returns The created swap chain.
         */
        MKT_NODISCARD virtual auto CreateSwapChain( const SwapChainDescription& description ) -> SwapChainHandle = 0;


        /**
         * Creates a query pool for recording hardware metrics or timestamps.
         *
         * @note Unsupported backends (including D3D11) return an empty handle.
         *
         * @param description Query-pool creation parameters.
         * @returns The created query pool handle.
         */
        MKT_NODISCARD virtual auto CreateQueryPool( const QueryPoolDescription& description ) -> QueryPoolHandle = 0;


        /**
         * Retrieves the hardware query capabilities supported by a specific queue type.
         *
         * @note Unsupported backends (including D3D11) return empty capability results.
         *
         * @param type The type of hardware query to check.
         * @param queue The execution queue type to check against.
         * @returns The query capabilities supported by the hardware and backend.
         */
        MKT_NODISCARD virtual auto GetQueryCapabilities( QueryType type, QueueType queue ) const -> QueryCapabilities = 0;


        /**
         * Returns the queue of the requested type, if the device exposes one.
         *
         * @param type Queue type to retrieve.
         * @returns The requested queue, or null when unsupported.
         */
        MKT_NODISCARD virtual auto GetQueue( QueueType type ) -> IQueue* = 0;


        /**
         * Checks whether the backend supports multisampling with color resolve
         * for the scene's HDR color and depth attachments.
         *
         * @param samples The number of multisample anti-aliasing (MSAA) samples to check.
         * @returns True if the specified sample count is supported for scene multisampling, false otherwise.
         */
        MKT_NODISCARD virtual auto SupportsSceneMultisampling( Multisampling samples ) const -> bool = 0;

        /** @brief Returns the highest RHI sample count supported by the scene attachments.
         * Tests both HDR color and depth through SupportsSceneMultisampling.
         * Returns eMsaaX1 when the backend cannot render a multisampled scene.
         */
        MKT_NODISCARD auto GetMaxSceneMultisampling() const -> Multisampling;


        /**
         * Returns allocated GPU memory in bytes.
         *
         * @returns Allocated GPU memory in bytes.
         */
        MKT_NODISCARD virtual auto GetMemoryUsage() const -> core::usize = 0;


        /**
         * Retrieves the process-local GPU memory accounting metrics.
         *
         * @note Returns unavailable or empty statistics on unsupported platforms or backends.
         *
         * @returns A structure containing the current GPU memory usage for the calling process.
         */
        MKT_NODISCARD virtual auto GetProcessMemoryUsage() const -> ProcessGpuMemory = 0;


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

#endif// MIKOTO_RENDERER_RHI_GPU_DEVICE_HH
