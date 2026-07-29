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

#ifndef MIKOTO_D3D12DEVICE_HH
#define MIKOTO_D3D12DEVICE_HH

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Memory/Allocator.hh>
#include <Memory/MemoryArena.hh>
#include <Memory/FreeListAllocator.hh>

#include <Renderer/Core/GpuDevice.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include <Renderer/D3D12/D3D12SwapChain.hh>
#include <Renderer/D3D12/D3D12MemoryAllocator.hh>

namespace mikoto::renderer::d3d12 {

    class BindingLayout final : IBindingLayout {
    public:

        MKT_NODISCARD auto IsBindless() const -> bool override;
        MKT_NODISCARD auto GetRegisterSpace() const -> u32 override;

        ~BindingLayout() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;
    };

    class BindingSet : public IBindingSet {
    public:

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;
    };

    class DescriptorTable : public IDescriptorTable {
    public:
        MKT_NODISCARD auto GetCapacity( u32 ) const -> u32 override;
    };

    class InputLayout : public IInputLayout {
    public:
        explicit InputLayout( eastl::span<const VertexAttributeDescription> desc );

        MKT_NODISCARD auto GetNumAttributes() const -> u32 override;
        MKT_NODISCARD auto GetAttributeDescription(u32 index) const -> const VertexAttributeDescription& override;

        ~InputLayout() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        eastl::fixed_hash_map<u32, VertexAttributeDescription, kMaxVertexAttributes> mAttributes{};
    };

    class PipelineLayout : public IPipelineLayout {
    public:

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;
    };

    class Queue final : public IQueue {
    public:
        explicit Queue( QueueType type, QueueOpSupportFlags flags );

        auto Wait( IFence* fence, u64 value ) -> void override;
        auto Signal( IFence* fence, u64 value ) -> void override;

        auto ExecuteCommandLists( eastl::span<CommandListHandle> commands ) -> void override;

        auto Flush() -> void;

        auto RunGarbageCollection() -> void;

        auto ExecuteCommandList( CommandListHandle cmd ) -> void;
        auto SubmitCommandList( CommandListHandle cmd ) -> u64;

        auto AllocateCmdList() -> CommandListHandle;
        auto AllocateCmdList( const CommandListParameters& params ) -> CommandListHandle;

        auto WaitIdle() const -> void;

        // Conversion operators
        operator ID3D12CommandQueue*() const; // Logical
        operator ID3D12CommandAllocator*() const; // Logical

        ~Queue() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> mQueue{};
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mAllocator{};

        // For debug
        eastl::string mSubmissionLabel{};
        Color mSubmissionLabelColor{};
    };

    class CommandList final : public ICommandList {
    public:
        explicit CommandList( QueueType type );
        explicit CommandList( QueueType type, const CommandListParameters& desc );

        auto Begin( const CommandListBeginDescription& desc ) -> void override;
        auto End() -> void override;

        auto BeginParallel() -> void override;
        auto EndParallel() -> void override;

        auto SetDebugName( eastl::string_view name) -> void override;

        // More relaxed versions of SetResourceState
        // https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12
        auto PushBarrier( const BufferBarrierDescription& barrier ) -> void override;
        auto PushBarrier( const TextureBarrierDescription& barrier ) -> void override;

        auto BeginTrackingState(IBuffer* buffer, ResourceStates stateBits) -> void override;
        auto BeginTrackingState(ITexture* buffer, ResourceStates stateBits) -> void override;

        auto SetResourceState(IBuffer* buffer, ResourceStates stateBits) -> void override;
        auto SetResourceState(ITexture* buffer, ResourceStates stateBits) -> void override;

        auto SetBarrier( const BufferBarrierDescription& barrier ) -> void override;
        auto SetBarrier( const TextureBarrierDescription& barrier ) -> void override;

        auto CommitBarriers() -> void override;

        auto SetEnableAutomaticBarriers(  bool enable  ) -> void override;

        auto SetClearColor( FramebufferHandle frameBuffer, Color color ) -> void override;
        auto SetClearColor( TextureHandle renderTargets, Color color ) -> void override;

        auto Write( IBuffer* src, ITexture* dest, u32 mipLevel ) -> void override;
        auto Write( ITexture* texture, u32 mipLevel,const void* data, size_t byteSize ) -> void override;
        auto Copy( ITexture* src, const TextureSlice& srcSlice, ITexture* dest, const TextureSlice& destSlice ) -> void override;

        auto WriteVolatile( IBuffer* target, size_t dstOffset, const void* data, size_t byteSize ) -> void override;

        auto Write( IBuffer* buffer, size_t destOffset, const void* data, size_t byteSize ) -> void override;
        auto Write( IBuffer* buffer, const void* data, size_t byteSize ) -> void override;
        auto Copy( IBuffer* src, IBuffer* dest ) -> void override;
        auto Copy( IBuffer* src, IBuffer* dest, size_t destOffset ) -> void override;

        auto Copy( IBuffer *dest, ITexture *src ) -> void override;

        auto BeginRendering( GraphicsState& state ) -> void override;
        auto EndRendering() -> void override;

        auto BindPipeline( IPipeline* pipeline ) -> void override;

        // I am not sure if I wanna have these because the viewport is set on the images we
        // render to so it makes more sense to tie them to the graphics state when we specify the render targets
        auto SetViewport( eastl::span<const Viewport> viewports ) -> void override;
        auto SetScissors( eastl::span<const Rect> scissorRects ) -> void override;
        auto SetViewportState( const ViewportState& vs ) -> void override;

        auto BindIndexBuffer( IBuffer* buffer ) -> void override;
        auto BindVertexBuffer( const VertexBufferBinding& binding ) -> void override;

        auto BindPipelineResources( const BindResourcesDescription& desc ) -> void override;

        auto Draw( const DrawArguments& args ) -> void override;
        auto BindIndirectBuffer( IBuffer* buffer ) -> void override;
        auto DrawIndexed( const DrawArguments& args ) -> void override;

        auto DrawIndirect( u32 offset, u32 drawCount ) -> void override;
        auto DrawIndexedIndirect( u32 offset, u32 drawCount ) -> void override;

        auto Dispatch( u32 groupsX, u32 groupsY, u32 groupsZ ) -> void override;

        auto SetPushConstants( IPipelineLayout* pipelineLayout, const void* data, size_t byteSize, ShaderStage visibility ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        ~CommandList() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    };

    struct GpuUploadAllocation {
        IBuffer* mBuffer{};

        // This is the mapped address we should be writing to,
        // it corresponds to the beginning of the slice or
        // range that is available within this allocation.
        // You do not need to offset or anything
        void* mMappedMemory{};

        // Size of this sub-allocation
        size_t mSize{};

        // Specifies the offset of this allocation within the large
        // buffer it was allocated from
        size_t mOffset{};

        // Metadata to track usage
        Allocation mAllocation{};
        eastl::atomic_flag mInUse{ true };
    };

    // Manages intermediate buffers that are used to upload data from CPU
    // to CPU, whenever we want to upload data from the CPU to the GPU
    // if the GPU buffer is not writeable from CPU we copy to this intermediate buffer
    // and issue a copy command. In order to implement this we can use a first fit or best fit
    // allocator approach, see the GeometryAllocator in the MeshCulling.hh file
    // we can just have a huge chunk that grows as we need??
    class GpuUploadManager final {
    public:
        explicit GpuUploadManager( IGpuDevice* device );

        auto SubAllocate( size_t byteSize ) -> GpuUploadAllocation*;
        auto ReclaimMemory() -> void;

        ~GpuUploadManager();
    private:
        struct StagingAllocation {
            BufferHandle mBuffer{};
            eastl::unique_ptr<memory::MemoryArena<IBuffer, memory::FreeListFirstFitAllocator>> mMemoryArena{};
        };

        static constexpr u32 kMaxBuffers{ 32 };
        static constexpr u32 kMaxSubAllocations{ 100 };

        auto CreateBuffer() -> StagingAllocation*;
        auto CreateSubAllocation( IBuffer* buffer ) -> GpuUploadAllocation*;

    private:
        std::mutex mMutex{};

        IGpuDevice* mDevice{};
        ankerl::unordered_dense::map<IBuffer*, eastl::unique_ptr<StagingAllocation>> mBuffers{};
        ankerl::unordered_dense::map<IBuffer*, eastl::fixed_vector<eastl::unique_ptr<GpuUploadAllocation>, kMaxSubAllocations>> mSubAllocations{};
    };

    class Device final : public IGpuDevice {
    public:
        explicit Device( const GpuDeviceCreateInfo& createInfo );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto CreateTexture( const TextureCreateDescription& description ) -> TextureHandle override;
        MKT_NODISCARD auto CreateTextureNative( ObjectType type, Object object, const TextureCreateDescription& description ) -> TextureHandle override;

        MKT_NODISCARD auto CreateFrameBuffer(const FramebufferDescription& description) -> FramebufferHandle override;

        MKT_NODISCARD auto CreateBuffer( const BufferCreateDescription& description ) -> BufferHandle override;

        MKT_NODISCARD auto CreateSampler( const SamplerCreateDescription& description ) -> SamplerHandle override;

        MKT_NODISCARD auto CreatePipeline( const ComputePipelineDescription& description ) -> PipelineHandle override;
        MKT_NODISCARD auto CreatePipeline( const GraphicsPipelineDescription& description ) -> PipelineHandle override;

        MKT_NODISCARD auto CreateAccelStructure( const AccelStructureCreateDescription& description ) -> AccelStructureHandle override;

        MKT_NODISCARD auto CreateCommandList( QueueType queue ) -> CommandListHandle override;
        MKT_NODISCARD auto CreateCommandList( const CommandListParameters& parameters ) -> CommandListHandle override;

        MKT_NODISCARD auto CreateShader( const ShaderModuleCreateDescription& desc ) -> ShaderModuleHandle override;
        MKT_NODISCARD auto CreateShader( ShaderStage type, const void* code, size_t codeSize  ) -> ShaderModuleHandle override;

        MKT_NODISCARD auto CreateInputLayout(const InputLayoutCreateDescription& desc) -> InputLayoutHandle override;

        MKT_NODISCARD auto CreateBindingLayout( const BindingLayoutDescription& desc ) -> BindingLayoutHandle override;
        MKT_NODISCARD auto CreatePipelineLayout( const PipelineLayoutCreateDescription& desc ) -> PipelineLayoutHandle override;
        MKT_NODISCARD auto CreateBindingSet( const BindingSetDescription& desc, BindingLayoutHandle layout ) -> BindingSetHandle override;

        MKT_NODISCARD auto CreateFence( u64 fenceInitialValue ) -> FenceHandle override;

        auto UnMap( IBuffer* buffer ) -> void override;
        MKT_NODISCARD auto Map(IBuffer* buffer) -> const void* override;

        // To support bindless techniques in modern graphics APIs
        MKT_NODISCARD auto CreateBindlessLayout( const BindlessLayoutDescription& desc ) -> BindingLayoutHandle override;

        MKT_NODISCARD auto CreateDescriptorTable( BindingLayoutHandle layout ) -> DescriptorTableHandle override;
        MKT_NODISCARD auto ResizeDescriptorTable( DescriptorTableHandle descriptorTable, u32 newSize, bool keepContents ) -> bool override;
        MKT_NODISCARD auto WriteDescriptorTable( DescriptorTableHandle descriptorTable, const BindingSetItem& item ) -> bool override;

        auto Wait( QueueType type, FenceHandle handle, u64 fenceValue ) -> void override;
        auto Signal( QueueType type, FenceHandle handle, u64 fenceValue ) -> void override;

        auto ExecutePendingCommands() -> void override;
        auto RunGarbageCollection() -> void override;
        auto SubmitCommands( CommandListHandle cmdList ) -> u64 override;
        auto ExecuteCommands( CommandListHandle cmdList ) -> void override;
        auto ExecuteCommands( eastl::span<CommandListHandle> cmdList ) -> void override;

        auto WaitIdle() -> void override;

        // D3D12 Specifics
        MKT_NODISCARD auto GetDevice() -> ID3D12Device*;
        MKT_NODISCARD auto GetAdapter() -> IDXGIAdapter4*;

        MKT_NODISCARD auto GetQueue( QueueType type ) -> Queue*;
        MKT_NODISCARD auto GetQueue( QueueType type ) const -> const Queue*;

        MKT_NODISCARD auto CreateSwapChain(Window* window, Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory) -> SwapChainHandle;

        ~Device() override = default;

    private:
        // [Internal usage]
        auto InitCommandQueues() -> void;
        auto InitMemoryAllocator() -> void;

    private:
        Microsoft::WRL::ComPtr<ID3D12Device2> mDevice{};
        Microsoft::WRL::ComPtr<IDXGIAdapter1> mAdapter1{};
        Microsoft::WRL::ComPtr<IDXGIAdapter4> mAdapter4{};

        DXGI_ADAPTER_DESC3 mDeviceDescription3{};

        // [Memory management]
        eastl::unique_ptr<IGpuAllocator> mGpuAllocator{};
        eastl::unique_ptr<GpuUploadManager> mUploadManager{};

        // [Command list management]
        ankerl::unordered_dense::map<QueueType, Ref<Queue>> mQueues{};

#if defined(_DEBUG)
        Microsoft::WRL::ComPtr<ID3D12Debug1> mDebugController{};
        Microsoft::WRL::ComPtr<ID3D12DebugDevice> mDebugDevice{};
#endif
    };
}

#endif

#endif// MIKOTO_D3D12DEVICE_HH
