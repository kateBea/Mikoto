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

    using DescriptorIndex = core::u32;

    class Fence final : public IFence {
    public:
        explicit Fence( u64 initialValue );

        MKT_NODISCARD auto GetCompletionValue() const -> u64 override;

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        ~Fence() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        HANDLE mFenceEvent{};
        UINT64 mFenceValue{};
        Microsoft::WRL::ComPtr<ID3D12Fence> mFence{};
    };

    class BindingLayout final : public IBindingLayout {
    public:
        explicit BindingLayout( const BindingLayoutDescription& desc );
        explicit BindingLayout( const BindlessLayoutDescription& desc );

        MKT_NODISCARD auto GetRegisterSpace() const -> u32 override;

        MKT_NODISCARD auto IsBindless() const -> bool override;

        MKT_NODISCARD auto GetBindingLayoutDesc() const -> const BindingLayoutDescription&;
        MKT_NODISCARD auto GetBindlessLayoutDesc() const -> const BindlessLayoutDescription&;

        MKT_NODISCARD auto GetDescriptorRanges() const -> const eastl::vector<D3D12_DESCRIPTOR_RANGE1>&;
        MKT_NODISCARD auto GetDescriptorSamplerRanges() const -> const eastl::vector<D3D12_DESCRIPTOR_RANGE1>&;

        MKT_NODISCARD auto GetBindlessDescriptorRanges() const -> const eastl::vector<D3D12_DESCRIPTOR_RANGE1>&;
        MKT_NODISCARD auto GetBindlessDescriptorSamplerRanges() const -> const eastl::vector<D3D12_DESCRIPTOR_RANGE1>&;

        MKT_NODISCARD auto GetNextIndexForDescriptor(D3D12_DESCRIPTOR_RANGE_TYPE descriptor ) const -> u32;

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        ~BindingLayout() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        u32 mRegisterSpace{};
        bool mIsBindless{ false };
        BindingLayoutDescription mBindingLayoutDesc{};
        BindlessLayoutDescription mBindlessLayoutDesc{};

        // Because for D3D12 Samplers cannot be mixed
        // with other resource types in a descriptor table
        eastl::vector<D3D12_DESCRIPTOR_RANGE1> mDescriptorRanges{};
        eastl::vector<D3D12_DESCRIPTOR_RANGE1> mDescriptorRangesSamplers{};

        eastl::vector<D3D12_DESCRIPTOR_RANGE1> mBindlessDescriptorRanges{};
        eastl::vector<D3D12_DESCRIPTOR_RANGE1> mBindlessDescriptorRangesSamplers{};

        // Track the next available register slot for each DX12 type category
        // Default initializes all categories (SRV, UAV, CBV, Sampler) to 0
        mutable ankerl::unordered_dense::map<D3D12_DESCRIPTOR_RANGE_TYPE, u32> mNextRegisterForType{};
    };

    class BindingSet : public IBindingSet {
    public:
        explicit BindingSet( const BindingSetDescription& desc, BindingLayoutHandle layout );

        auto SetDebugName( eastl::string_view name ) -> void override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        ~BindingSet() override;

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        BindingLayoutHandle mBindingLayout{};
        BindingSetDescription mBindingDescription{};
    };

    class DescriptorTable : public IDescriptorTable {
    public:
        MKT_NODISCARD auto GetCapacity( u32 ) const -> u32 override;
    };

    class InputLayout : public IInputLayout {
    public:
        explicit InputLayout( const InputLayoutCreateDescription& desc );

        MKT_NODISCARD auto GetNumAttributes() const -> u32 override;
        MKT_NODISCARD auto GetAttributeDescription(u32 index) const -> const VertexAttributeDescription& override;

        MKT_NODISCARD auto GetInputElements() -> const D3D12_INPUT_ELEMENT_DESC*;
        MKT_NODISCARD auto GetInputElementsCount() -> UINT;

        ~InputLayout() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        eastl::vector<D3D12_INPUT_ELEMENT_DESC> mInputElems{};
        ankerl::unordered_dense::map<eastl::string, u32> mSemanticIndexPerName{};

        InputLayoutCreateDescription mDescription{};
        eastl::fixed_hash_map<u32, VertexAttributeDescription, kMaxVertexAttributes> mAttributes{};
    };

    class PipelineLayout : public IPipelineLayout {
    public:

        explicit PipelineLayout(const PipelineLayoutCreateDescription& description);

        auto SetDebugName( const eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        MKT_NODISCARD auto GetDescription() const -> const PipelineLayoutCreateDescription&;

        operator ID3D12RootSignature*() const;

        ~PipelineLayout() override;

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        Microsoft::WRL::ComPtr<ID3DBlob> mSignatureBlob{};
        Microsoft::WRL::ComPtr<ID3DBlob> mErrorMessages{};
        Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature{};
        PipelineLayoutCreateDescription mDescription{};
    };

    struct CommandAllocationContext {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mAllocator{};
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

        ~Queue() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        Microsoft::WRL::ComPtr<ID3D12CommandQueue> mQueue{};

        // For debug
        eastl::string mSubmissionLabel{};
        Color mSubmissionLabelColor{};
    };

    // https://learn.microsoft.com/en-us/windows/win32/direct3d12/recording-command-lists-and-bundles
    class CommandList final : public ICommandList {
    public:
        explicit CommandList( const CommandListParameters& desc );

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
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        ~CommandList() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> mCommandList{};
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocator{};
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

    class IDescriptorHeap {
    public:
        virtual auto AllocateDescriptors( u32 count ) -> DescriptorIndex = 0;
        virtual auto AllocateDescriptor() -> DescriptorIndex = 0;

        virtual auto ReleaseDescriptors( DescriptorIndex baseIndex, u32 count ) -> void = 0;
        virtual auto ReleaseDescriptor( DescriptorIndex index ) -> void = 0;

        MKT_NODISCARD virtual auto GetHeap() const -> ID3D12DescriptorHeap* = 0;
        MKT_NODISCARD virtual auto GetShaderVisibleHeap() const -> ID3D12DescriptorHeap* = 0;
        MKT_NODISCARD virtual auto GetCpuHandle( DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE = 0;
        MKT_NODISCARD virtual auto GetCpuHandleShaderVisible( DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE = 0;
        MKT_NODISCARD virtual auto GetGpuHandle( DescriptorIndex index ) const -> D3D12_GPU_DESCRIPTOR_HANDLE = 0;

        virtual ~IDescriptorHeap() = default;

        DISABLE_COPY_AND_MOVE_FOR( IDescriptorHeap );

    protected:
        IDescriptorHeap() = default;
    };

    class StaticDescriptorHeap final : public IDescriptorHeap {
    public:
        explicit StaticDescriptorHeap();

        auto CopyToShaderVisibleHeap( DescriptorIndex index, u32 count = 1 ) -> void;

        auto AllocateDescriptor() -> DescriptorIndex override;
        auto AllocateDescriptors( u32 count ) -> DescriptorIndex override;

        auto ReleaseDescriptor( DescriptorIndex index ) -> void override;
        auto ReleaseDescriptors(DescriptorIndex baseIndex, u32 count ) -> void override;

        auto AllocateResources(D3D12_DESCRIPTOR_HEAP_TYPE heapType, u32 numDescriptors, bool shaderVisible ) -> HRESULT;

        MKT_NODISCARD auto GetHeap() const -> ID3D12DescriptorHeap* override;
        MKT_NODISCARD auto GetHeapType() const -> D3D12_DESCRIPTOR_HEAP_TYPE;
        MKT_NODISCARD auto GetCpuHandle( DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE override;
        MKT_NODISCARD auto GetCpuHandleShaderVisible(DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE override;
        MKT_NODISCARD auto GetGpuHandle( DescriptorIndex index ) const -> D3D12_GPU_DESCRIPTOR_HANDLE override;
        MKT_NODISCARD auto GetShaderVisibleHeap() const -> ID3D12DescriptorHeap* override;

    private:
        auto Grow( u32 minRequiredSize ) -> HRESULT;

    private:
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap{};
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mShaderVisibleHeap{};

        D3D12_DESCRIPTOR_HEAP_TYPE mHeapType{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };

        D3D12_CPU_DESCRIPTOR_HANDLE mStartCpuHandle{};
        D3D12_CPU_DESCRIPTOR_HANDLE mStartCpuHandleShaderVisible{};
        D3D12_GPU_DESCRIPTOR_HANDLE mStartGpuHandleShaderVisible{};

        u32 mStride{};
        u32 mNumDescriptors{};

        eastl::vector<bool> mAllocatedDescriptors{};

        DescriptorIndex mSearchStart{};
        u32 mNumAllocatedDescriptors{};

        std::mutex mMutex{};
    };

    class DeviceResources {
    public:
        StaticDescriptorHeap mRenderTargetViewHeap{};
        StaticDescriptorHeap mDepthStencilViewHeap{};
        StaticDescriptorHeap mShaderResourceViewHeap{};
        StaticDescriptorHeap mSamplerHeap{};
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

        MKT_NODISCARD auto CreateCommandList( QueueType queueType ) -> CommandListHandle override;
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
        auto DumpMessages() -> void;

        static auto CALLBACK DebugMessageCallback(
                D3D12_MESSAGE_CATEGORY,
                D3D12_MESSAGE_SEVERITY severity,
                D3D12_MESSAGE_ID,
                LPCSTR description,
                void* ) -> void;

        MKT_NODISCARD auto GetDevice() -> ID3D12Device2*;
        MKT_NODISCARD auto GetAdapter() -> IDXGIAdapter4*;

        MKT_NODISCARD auto GetHeapResources() const -> const DeviceResources*;

        MKT_NODISCARD auto GetQueue( QueueType type ) -> Queue*;
        MKT_NODISCARD auto GetQueue( QueueType type ) const -> const Queue*;

        MKT_NODISCARD auto GetAllocator() -> GpuMemoryAllocator*;

        MKT_NODISCARD auto CreateSwapChain(Window* window, Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory) -> SwapChainHandle;

        ~Device() override = default;

    private:
        // [Internal usage]
        auto InitInfoQueue() -> void;
        auto InitCommandQueues() -> void;
        auto InitMemoryAllocator() -> void;

    private:
        Microsoft::WRL::ComPtr<ID3D12Device2> mDevice{};
        Microsoft::WRL::ComPtr<IDXGIAdapter1> mAdapter1{};
        Microsoft::WRL::ComPtr<IDXGIAdapter4> mAdapter4{};

        DXGI_ADAPTER_DESC3 mDeviceDescription3{};

        // [Memory management]
        DeviceResources mResourceHeaps{};
        eastl::unique_ptr<IGpuAllocator> mGpuAllocator{};
        eastl::unique_ptr<GpuUploadManager> mUploadManager{};

        // [Command list management]
        ankerl::unordered_dense::map<QueueType, Ref<Queue>> mQueues{};

#if defined(_DEBUG)
        DWORD mInfoQueueCallbackCookie{};

        Microsoft::WRL::ComPtr<ID3D12InfoQueue> mInfoQueue{};
        Microsoft::WRL::ComPtr<ID3D12InfoQueue1> mInfoQueue1{};

        Microsoft::WRL::ComPtr<ID3D12DebugDevice> mDebugDevice{};
#endif
    };
}

#endif

#endif// MIKOTO_D3D12DEVICE_HH
