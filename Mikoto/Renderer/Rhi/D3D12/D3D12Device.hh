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

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Fence.hh>
#include <Renderer/Rhi/GpuDevice.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <directx/d3d12.h>
#include <directx/d3dx12.h>

#include <dxgi1_6.h>
#include <wrl.h>

#include <Renderer/Rhi/D3D12/D3D12Texture.hh>
#include <Renderer/Rhi/D3D12/D3D12SwapChain.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>
#include <Renderer/Rhi/D3D12/D3D12MemoryAllocator.hh>

namespace mikoto::renderer::d3d12 {

    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    class Fence final : public rhi::IFence {
    public:
        explicit Fence( core::u64 initialValue );

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto Signal( core::u64 fenceValue ) -> bool override;
        MKT_NODISCARD auto Wait( core::u64 fenceValue, core::u64 timeoutMs ) -> bool override;

        MKT_NODISCARD auto IsCompleted( core::u64 fenceValue ) const -> bool;

        MKT_NODISCARD auto GetCompletionValue() const -> core::u64 override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        MKT_NODISCARD operator HANDLE() const;
        MKT_NODISCARD operator ID3D12Fence*() const;

        ~Fence() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        HANDLE mFenceEvent{};
        UINT64 mFenceInitialValue{};
        Microsoft::WRL::ComPtr<ID3D12Fence> mFence{};
    };

    struct GpuUploadAllocation {
        rhi::IBuffer* mBuffer{};

        // This is the mapped address we should be writing to,
        // it corresponds to the beginning of the slice or
        // range that is available within this allocation.
        // You do not need to offset or anything
        void* mMappedMemory{};

        // Size of this sub-allocation
        core::size_t mSize{};

        // Specifies the offset of this allocation within the large
        // buffer it was allocated from
        core::size_t mOffset{};

        // Metadata to track usage
        memory::Allocation mAllocation{};
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
        explicit GpuUploadManager( rhi::IGpuDevice* device );

        auto SubAllocate( core::usize byteSize ) -> GpuUploadAllocation*;
        auto ReclaimMemory() -> void;

        ~GpuUploadManager();
    private:
        struct StagingAllocation {
            rhi::BufferHandle mBuffer{};
            eastl::unique_ptr<memory::MemoryArena<rhi::IBuffer, memory::FreeListFirstFitAllocator>> mMemoryArena{};
        };

        static constexpr core::u32 kMaxBuffers{ 32 };
        static constexpr core::u32 kMaxSubAllocations{ 100 };

        auto CreateBuffer() -> StagingAllocation*;
        auto CreateSubAllocation( rhi::IBuffer* buffer ) -> GpuUploadAllocation*;

    private:
        std::mutex mMutex{};

        rhi::IGpuDevice* mDevice{};
        ankerl::unordered_dense::map<rhi::IBuffer*, eastl::unique_ptr<StagingAllocation>> mBuffers{};
        ankerl::unordered_dense::map<rhi::IBuffer*, eastl::fixed_vector<eastl::unique_ptr<GpuUploadAllocation>, kMaxSubAllocations>> mSubAllocations{};
    };

    // https://github.com/microsoft/DirectXTK12/wiki/DescriptorHeap
    // https://learn.microsoft.com/en-us/windows/win32/direct3d12/hardware-support
    // https://3dgep.blogspot.com/2016/02/a-journey-through-directx-12-dynamic.html
    // https://logins.github.io/graphics/2020/07/31/DX12ResourceHandling.html
    class IDescriptorHeap {
    public:
        virtual auto AllocateDescriptors( core::u32 count ) -> DescriptorIndex = 0;
        virtual auto AllocateDescriptor() -> DescriptorIndex = 0;

        virtual auto ReleaseDescriptors( DescriptorIndex baseIndex, core::u32 count ) -> void = 0;
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
        explicit StaticDescriptorHeap( IGpuDevice* device );

        auto CopyToShaderVisibleHeap( DescriptorIndex index, core::u32 count = 1 ) -> void;

        auto AllocateDescriptor() -> DescriptorIndex override;
        auto AllocateDescriptors( core::u32 count ) -> DescriptorIndex override;

        auto ReleaseDescriptor( DescriptorIndex index ) -> void override;
        auto ReleaseDescriptors( DescriptorIndex baseIndex, core::u32 count ) -> void override;

        auto AllocateResources( D3D12_DESCRIPTOR_HEAP_TYPE heapType, core::u32 numDescriptors, bool shaderVisible ) -> HRESULT;

        MKT_NODISCARD auto GetHeap() const -> ID3D12DescriptorHeap* override;
        MKT_NODISCARD auto GetHeapType() const -> D3D12_DESCRIPTOR_HEAP_TYPE;
        MKT_NODISCARD auto GetCpuHandle( DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE override;
        MKT_NODISCARD auto GetCpuHandleShaderVisible( DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE override;
        MKT_NODISCARD auto GetGpuHandle( DescriptorIndex index ) const -> D3D12_GPU_DESCRIPTOR_HANDLE override;
        MKT_NODISCARD auto GetShaderVisibleHeap() const -> ID3D12DescriptorHeap* override;

    private:
        auto Grow( core::u32 minRequiredSize ) -> HRESULT;

    private:
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mHeap{};
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mShaderVisibleHeap{};

        D3D12_DESCRIPTOR_HEAP_TYPE mHeapType{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };

        D3D12_CPU_DESCRIPTOR_HANDLE mStartCpuHandle{};
        D3D12_CPU_DESCRIPTOR_HANDLE mStartCpuHandleShaderVisible{};
        D3D12_GPU_DESCRIPTOR_HANDLE mStartGpuHandleShaderVisible{};

        core::u32 mStride{};
        core::u32 mNumDescriptors{};

        eastl::vector<bool> mAllocatedDescriptors{};

        DescriptorIndex mSearchStart{};
        core::u32 mNumAllocatedDescriptors{};

        ID3D12Device2* mDevice{};

        std::mutex mMutex{};
    };

    class DeviceResources {
    public:
        eastl::unique_ptr<StaticDescriptorHeap> mRenderTargetViewHeap{};
        eastl::unique_ptr<StaticDescriptorHeap> mDepthStencilViewHeap{};
        eastl::unique_ptr<StaticDescriptorHeap> mShaderResourceViewHeap{};
        eastl::unique_ptr<StaticDescriptorHeap> mSamplerHeap{};
    };

    class BindingLayout final : public IBindingLayout {
    public:
        explicit BindingLayout( const BindingLayoutDescription& desc );
        explicit BindingLayout( const BindlessLayoutDescription& desc );

        MKT_NODISCARD auto GetRegisterSpace() const -> core::u32 override;

        MKT_NODISCARD auto IsBindless() const -> bool override;

        MKT_NODISCARD auto GetBindingLayoutDesc() const -> const BindingLayoutDescription&;
        MKT_NODISCARD auto GetBindlessLayoutDesc() const -> const BindlessLayoutDescription&;

        MKT_NODISCARD auto GetDescriptorRanges() const -> const eastl::vector<D3D12_DESCRIPTOR_RANGE1>&;
        MKT_NODISCARD auto GetDescriptorSamplerRanges() const -> const eastl::vector<D3D12_DESCRIPTOR_RANGE1>&;

        MKT_NODISCARD auto GetNextIndexForDescriptor(D3D12_DESCRIPTOR_RANGE_TYPE descriptor ) const -> core::u32;

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        ~BindingLayout() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        core::u32 mRegisterSpace{};
        bool mIsBindless{ false };
        BindingLayoutDescription mBindingLayoutDesc{};
        BindlessLayoutDescription mBindlessLayoutDesc{};

        // Because for D3D12 Samplers cannot be mixed
        // with other resource types in a descriptor table
        eastl::vector<D3D12_DESCRIPTOR_RANGE1> mDescriptorRanges{};
        eastl::vector<D3D12_DESCRIPTOR_RANGE1> mDescriptorRangesSamplers{};

        // Track the next available register slot for each DX12 type category
        // Default initializes all categories (SRV, UAV, CBV, Sampler) to 0
        mutable ankerl::unordered_dense::map<D3D12_DESCRIPTOR_RANGE_TYPE, u32> mNextRegisterForType{};
    };

    struct DescriptorRange {
        core::u32 mCount{ 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE mGpuHandle{};
        DescriptorIndex mBaseIndex{ d3d12::kInvalidDescriptorIndex };
    };

    class BindingSet : public IBindingSet {
    public:
        explicit BindingSet( const BindingSetDescription& desc, BindingLayoutHandle layout, DeviceResources& resources );

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        MKT_NODISCARD auto GetSrvRange() const -> const DescriptorRange&;
        MKT_NODISCARD auto GetSamplerRange() const -> const DescriptorRange&;

        ~BindingSet() override;

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        BindingLayoutHandle mBindingLayout{};
        BindingSetDescription mBindingDescription{};

        DeviceResources* mDeviceResources{};

        DescriptorRange mSrvRange{};
        DescriptorRange mSamplerRange{};
    };

    class DescriptorTable : public IDescriptorTable {
    public:
        explicit DescriptorTable();

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        ~DescriptorTable() override;

        MKT_NODISCARD auto GetCapacity( core::u32 ) const -> core::u32 override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:

    };

    class InputLayout : public IInputLayout {
    public:
        explicit InputLayout( const InputLayoutCreateDescription& desc );

        MKT_NODISCARD auto GetNumAttributes() const -> core::u32 override;
        MKT_NODISCARD auto GetAttributeDescription(core::u32 index) const -> const VertexAttributeDescription& override;

        MKT_NODISCARD auto GetInputElements() -> const D3D12_INPUT_ELEMENT_DESC*;
        MKT_NODISCARD auto GetInputElementsCount() -> UINT;

        ~InputLayout() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        eastl::vector<D3D12_INPUT_ELEMENT_DESC> mInputElems{};
        ankerl::unordered_dense::map<eastl::string, core::u32> mSemanticIndexPerName{};

        InputLayoutCreateDescription mDescription{};
        eastl::fixed_hash_map<core::u32, VertexAttributeDescription, kMaxVertexAttributes> mAttributes{};
    };

    // https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signatures
    class PipelineLayout : public IPipelineLayout {
    public:

        explicit PipelineLayout(const PipelineLayoutCreateDescription& description);

        auto SetDebugName( const eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        MKT_NODISCARD auto GetRootConstantIndex() const -> core::u32;
        MKT_NODISCARD auto GetMax32BitValuesCount() const -> core::u32;

        MKT_NODISCARD auto GetDescription() const -> const PipelineLayoutCreateDescription&;

        operator ID3D12RootSignature*() const;

        ~PipelineLayout() override;

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        core::u32 mRootConstantIndex{};
        core::u32 m32BitValueCount{ 32 };

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

        auto ExecuteCommandLists( const SubmitInfo& submitInfo ) -> void override;

        // D3D12 Specifics
        MKT_NODISCARD auto GetCurrentTimeline() -> core::u64;
        MKT_NODISCARD auto AllocateCmdList() -> CommandListHandle;

        auto WaitIdle() -> void;

        // Conversion operators
        operator ID3D12CommandQueue*() const;

        ~Queue() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        eastl::atomic<core::u64> mFenceValue{};
        rhi::FenceHandle mFence{};

        Microsoft::WRL::ComPtr<ID3D12CommandQueue> mQueue{};

        // For debug
        eastl::string mSubmissionLabel{};
        Color mSubmissionLabelColor{};
    };

    struct RecordingContext {
        core::u64 mSubmissionID{};

        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocator{};
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList7> mCommandList{};

        eastl::fixed_vector<GpuUploadAllocation*, 10> mUploadAllocations{};
    };

    // https://learn.microsoft.com/en-us/windows/win32/direct3d12/recording-command-lists-and-bundles
    class CommandList final : public ICommandList {
    public:
        explicit CommandList( IQueue* queue );

        auto Begin( const CommandListBeginDescription& desc ) -> void override;
        auto End() -> void override;

        auto SetDebugName( eastl::string_view name) -> void override;

        // More relaxed versions of SetResourceState
        // https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12
        auto RecordBarrier( const BufferBarrierDescription& desc ) -> void override;
        auto RecordBarrier( const TextureBarrierDescription& desc ) -> void override;

        auto RecordTransition(IBuffer* buffer, ResourceStates stateBits) -> void override;
        auto RecordTransition(ITexture* texture, ResourceStates stateBits) -> void override;

        auto CommitBarriers() -> void override;

        auto SetBarrier( const BufferBarrierDescription& desc ) -> void override;
        auto SetBarrier( const TextureBarrierDescription& desc ) -> void override;

        auto SetTransition(IBuffer* buffer, ResourceStates stateBits) -> void override;
        auto SetTransition(ITexture* texture, ResourceStates stateBits) -> void override;

        auto SetEnableAutomaticBarriers(  bool enable  ) -> void override;

        auto SetClearColor( rhi::TextureHandle renderTarget, Color color ) -> void override;

        auto Write( IBuffer* src, ITexture* dest ) -> void override;
        auto Write( ITexture* texture, const void* data, core::usize byteSize ) -> void override;
        auto Copy( ITexture* src, const TextureSlice& srcSlice, ITexture* dest, const TextureSlice& destSlice ) -> void override;

        auto Resolve( ITexture* src, const TextureSlice& srcSlice, ITexture* dest, const TextureSlice& destSlice ) -> void override;

        auto Write( IBuffer* buffer, core::usize destOffset, const void* data, core::usize byteSize ) -> void override;
        auto Write( IBuffer* buffer, const void* data, core::usize byteSize ) -> void override;
        auto Copy( IBuffer* src, IBuffer* dest ) -> void override;
        auto Copy( IBuffer* src, IBuffer* dest, core::usize destOffset ) -> void override;

        auto Copy( IBuffer *dest, ITexture *src ) -> void override;

        auto BeginRendering( GraphicsState& state ) -> void override;
        auto EndRendering() -> void override;

        auto BindPipeline( IPipeline* pipeline ) -> void override;

        // I am not sure if I wanna have these because the viewport is set on the images we
        // render to so it makes more sense to tie them to the graphics state when we specify the render targets
        auto SetViewport( eastl::span<const Viewport> viewports ) -> void override;
        auto SetScissors( eastl::span<const Rect> scissorRects ) -> void override;
        auto SetViewportState( const ViewportState& vs ) -> void override;

        auto SetPolygonLineWidth( core::f32 width ) -> void override;

        auto BindIndexBuffer( IBuffer* buffer ) -> void override;
        auto BindVertexBuffer( const VertexBufferBinding& binding ) -> void override;
        auto BindVertexBuffers( eastl::span<const VertexBufferBinding> binding ) -> void override;

        auto BindPipelineResources( const BindResourcesDescription& desc ) -> void override;

        auto Draw( const DrawArguments& args ) -> void override;
        auto BindIndirectBuffer( IBuffer* buffer ) -> void override;
        auto DrawIndexed( const DrawArguments& args ) -> void override;

        auto DrawIndirect( core::u32 offset, core::u32 drawCount ) -> void override;
        auto DrawIndexedIndirect( u32 offset, core::u32 drawCount ) -> void override;

        auto Dispatch( core::u32 groupsX, core::u32 groupsY, core::u32 groupsZ ) -> void override;

        auto SetPushConstants( IPipelineLayout* pipelineLayout, const void* data, core::usize byteSize, ShaderFlags visibility ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        auto BeginDebugLabel( eastl::string_view name, Color color ) -> void override;
        auto EnbDebugLabel() -> void override;

        // D3D12 Specifics
        MKT_NODISCARD auto IsInUse() const -> bool;

        auto ClearState() -> void;

        auto MarkExecuted( rhi::IQueue* queue, core::u64 submissionID) -> void;

        MKT_NODISCARD operator ID3D12GraphicsCommandList7*() const;

        ~CommandList() override;

    private:
        // [Internal usage]
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        IQueue* mQueue{};

        // We picked 5 at most, but can grow if needed
        // there are generally 3 frames in flight at most, more
        // may introduce unnecessary latency. With 4 we could be sure
        // that we will have at least one command buffer we can recycle
        // because GPU has finished executing its commands
        core::u32 mRecordingContextIndex{};
        static constexpr core::u32 kMaxRecordingContext{ 5 };
        eastl::vector<RecordingContext> mRecordingContext{};

        RecordingContext* mCurrentRecordingContext{};

        GpuUploadManager* mUploadManager{};

        bool mIsRenderScopeActive{};
        bool mEnableAutomaticBarriers{ true };

        eastl::fixed_vector<D3D12_RESOURCE_BARRIER, rhi::kMaxBarriers> mResourceBarriers{};

        eastl::fixed_vector<D3D12_BUFFER_BARRIER, rhi::kMaxBarriers> mBufferBarriers{};
        eastl::fixed_vector<D3D12_TEXTURE_BARRIER, rhi::kMaxBarriers> mTextureBarriers{};
    };

    class Device final : public IGpuDevice {
    public:
        explicit Device( const GpuDeviceCreateInfo& createInfo );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto CreateTexture( const TextureCreateDescription& description ) -> TextureHandle override;
        MKT_NODISCARD auto CreateTextureNative( ObjectType type, Object object, const TextureCreateDescription& description ) -> TextureHandle override;

        MKT_NODISCARD auto CreateBuffer( const BufferCreateDescription& description ) -> BufferHandle override;

        MKT_NODISCARD auto CreateSampler( const SamplerCreateDescription& description ) -> SamplerHandle override;

        MKT_NODISCARD auto CreatePipeline( const ComputePipelineDescription& description ) -> PipelineHandle override;
        MKT_NODISCARD auto CreatePipeline( const GraphicsPipelineDescription& description ) -> PipelineHandle override;

        MKT_NODISCARD auto CreateAccelStructure( const AccelStructureCreateDescription& description ) -> AccelStructureHandle override;

        MKT_NODISCARD auto CreateCommandList( QueueType queueType ) -> CommandListHandle override;

        MKT_NODISCARD auto CreateShader( const ShaderModuleCreateDescription& desc ) -> ShaderModuleHandle override;

        MKT_NODISCARD auto CreateInputLayout(const InputLayoutCreateDescription& desc) -> InputLayoutHandle override;

        MKT_NODISCARD auto CreateBindingLayout( const BindingLayoutDescription& desc ) -> BindingLayoutHandle override;
        MKT_NODISCARD auto CreatePipelineLayout( const PipelineLayoutCreateDescription& desc ) -> PipelineLayoutHandle override;
        MKT_NODISCARD auto CreateBindingSet( const BindingSetDescription& desc, BindingLayoutHandle layout ) -> BindingSetHandle override;

        MKT_NODISCARD auto CreateFence( u64 fenceInitialValue ) -> FenceHandle override;

        auto UnMap( IBuffer* buffer ) -> void override;
        MKT_NODISCARD auto Map(IBuffer* buffer ) -> void* override;

        // To support bindless techniques in modern graphics APIs
        MKT_NODISCARD auto CreateBindlessLayout( const BindlessLayoutDescription& desc ) -> BindingLayoutHandle override;

        MKT_NODISCARD auto CreateDescriptorTable( BindingLayoutHandle layout ) -> DescriptorTableHandle override;
        MKT_NODISCARD auto ResizeDescriptorTable( DescriptorTableHandle descriptorTable, u32 newSize, bool keepContents ) -> bool override;
        MKT_NODISCARD auto WriteDescriptorTable( DescriptorTableHandle descriptorTable, const BindingSetItem& item ) -> bool override;

        auto RunGarbageCollection() -> void override;

        auto WaitIdle() -> void override;

        MKT_NODISCARD auto GetQueue( QueueType type ) -> IQueue* override;

        MKT_NODISCARD auto GetMemoryUsage() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryTotal() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryAvailable() const -> core::usize override;

        // D3D12 Specifics
        MKT_NODISCARD auto CreateTexture( const ExternalTextureDescription& info ) -> rhi::TextureHandle;

        MKT_NODISCARD auto GetEmptyRootSignature() const -> ID3D12RootSignature*;

        auto DumpMessages() -> void;

        static auto CALLBACK DebugMessageCallback(
                D3D12_MESSAGE_CATEGORY,
                D3D12_MESSAGE_SEVERITY severity,
                D3D12_MESSAGE_ID,
                LPCSTR description,
                void* ) -> void;

        MKT_NODISCARD auto GetDevice() -> ID3D12Device2*;
        MKT_NODISCARD auto GetAdapter() -> IDXGIAdapter4*;

        MKT_NODISCARD auto GetHeapResources() -> DeviceResources*;
        MKT_NODISCARD auto GetHeapResources() const -> const DeviceResources*;

        MKT_NODISCARD auto GetAllocator() -> GpuMemoryAllocator*;
        MKT_NODISCARD auto GetUploadManager() -> GpuUploadManager*;

        MKT_NODISCARD auto CreateSwapChain(platform::Window* window, Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory) -> SwapChainHandle;

        ~Device() override = default;

    private:
        // [Internal usage]
        auto InitInfoQueue() -> void;
        auto InitCommandQueues() -> void;
        auto InitDummyResources() -> void;
        auto InitMemoryAllocator() -> void;
        auto InitDescriptorHeapManager() -> void;

    private:
        Microsoft::WRL::ComPtr<ID3D12Device2> mDevice{};
        Microsoft::WRL::ComPtr<IDXGIAdapter1> mAdapter1{};
        Microsoft::WRL::ComPtr<IDXGIAdapter4> mAdapter4{};

        DXGI_ADAPTER_DESC3 mDeviceDescription3{};

        // [Memory management]
        DeviceResources mResourceHeaps{};
        eastl::unique_ptr<memory::IGpuAllocator> mGpuAllocator{};
        eastl::unique_ptr<GpuUploadManager> mUploadManager{};

        // [Command list management]
        ankerl::unordered_dense::map<QueueType, Ref<Queue>> mQueues{};

        // Dummy resources
        Microsoft::WRL::ComPtr<ID3DBlob> mEmptyRootSignatureBlob{};
        Microsoft::WRL::ComPtr<ID3DBlob> mEmptyRootSignatureErrorMessages{};
        Microsoft::WRL::ComPtr<ID3D12RootSignature> mEmptyRootSignature{};

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
