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

#ifndef MIKOTO_RENDERER_RHI_D3D12_D3D12_DEVICE_HH
#define MIKOTO_RENDERER_RHI_D3D12_D3D12_DEVICE_HH

#include <mutex>

#include <EASTL/optional.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/shared_ptr.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>

#include <Memory/Allocator.hh>
#include <Memory/MemoryArena.hh>
#include <Memory/FreeListAllocator.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Fence.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/QueryPool.hh>
#include <Renderer/Rhi/ShaderTable.hh>
#include <Renderer/Rhi/GpuAllocator.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Platform/Windows/Windows.hh>

#include <directx/d3d12.h>
#include <directx/d3d12video.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <wrl.h>

#include <Renderer/Rhi/D3D12/D3D12Texture.hh>
#include <Renderer/Rhi/D3D12/D3D12SwapChain.hh>
#include <Renderer/Rhi/D3D12/D3D12QueryPool.hh>
#include <Renderer/Rhi/D3D12/Direct3D12Helpers.hh>
#include <Renderer/Rhi/D3D12/D3D12MemoryAllocator.hh>

#if defined( MIKOTO_ENABLE_TRACY )
namespace tracy {
    class D3D12QueueCtx;
}
#endif

namespace mikoto::renderer::d3d12 {

    enum class DeviceFeatureSupport {
        eShaderModel6_6,
        eShaderModel6_8,
    };

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
        auto Destroy() -> void override;

    private:
        HANDLE mFenceEvent{};
        UINT64 mFenceInitialValue{};
        std::timed_mutex mWaitMutex{};
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
        core::usize mSize{};

        // Specifies the offset of this allocation within the large
        // buffer it was allocated from
        core::usize mOffset{};

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

        auto SubAllocate( core::usize byteSize, core::usize alignment = 1 ) -> GpuUploadAllocation*;
        auto ReclaimMemory() -> void;

        ~GpuUploadManager();
    private:
        struct StagingAllocation {
            rhi::BufferHandle mBuffer{};
            eastl::unique_ptr<memory::MemoryArena<rhi::IBuffer, memory::FreeListFirstFitAllocator>> mMemoryArena{};
        };

        static constexpr core::u32 kMaxBuffers{ 32 };
        static constexpr core::u32 kMaxSubAllocations{ 100 };

        auto CreateBuffer( core::usize minimumSize ) -> StagingAllocation*;
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
    // https://diligentgraphics.com/diligent-engine/architecture/d3d12/managing-descriptor-heaps/
    class IDescriptorHeap : public core::INonCopyable, public core::INonMoveable {
    public:
        virtual auto AllocateDescriptors( core::u32 count ) -> DescriptorIndex = 0;
        virtual auto AllocateDescriptor() -> DescriptorIndex = 0;

        virtual auto ReleaseDescriptors( DescriptorIndex baseIndex, core::u32 count ) -> void = 0;
        virtual auto ReleaseDescriptor( DescriptorIndex index ) -> void = 0;

        MKT_NODISCARD virtual auto GetHeap() const -> ID3D12DescriptorHeap* = 0;
        MKT_NODISCARD virtual auto GetCpuHandle( DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE = 0;
        MKT_NODISCARD virtual auto GetGpuHandle( DescriptorIndex index ) const -> D3D12_GPU_DESCRIPTOR_HANDLE = 0;

        MKT_NODISCARD virtual auto GetShaderVisibleHeap() const -> ID3D12DescriptorHeap* = 0;
        MKT_NODISCARD virtual auto GetCpuHandleShaderVisible( DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE = 0;

        virtual ~IDescriptorHeap() = default;

    protected:
        IDescriptorHeap() = default;
    };

    class StaticDescriptorHeap final : public IDescriptorHeap {
    public:
        explicit StaticDescriptorHeap( renderer::rhi::IGpuDevice* device, eastl::string_view debugName = "" );

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

        mutable std::mutex mMutex{};
        eastl::vector<eastl::pair<DescriptorIndex, Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>>> mCpuPages{};

        // Debug
        eastl::string mDebugName{};
    };

    // For shader model 6_6 global resources and samplers
    class DynamicDescriptorHeap final : public IDescriptorHeap {
    public:
        explicit DynamicDescriptorHeap( renderer::rhi::IGpuDevice* device, eastl::string_view debugName = "" );

        auto AllocateDescriptor() -> DescriptorIndex override;
        auto ReleaseDescriptor( DescriptorIndex index ) -> void override;

        auto AllocateDescriptors( core::u32 count ) -> DescriptorIndex override;
        auto ReleaseDescriptors( DescriptorIndex baseIndex, core::u32 count ) -> void override;

        auto AllocateResources( D3D12_DESCRIPTOR_HEAP_TYPE heapType, core::usize capacity ) -> HRESULT;

        MKT_NODISCARD auto GetShaderVisibleHeap() const -> ID3D12DescriptorHeap* override;
        MKT_NODISCARD auto GetCpuHandleShaderVisible( DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE override;

        MKT_NODISCARD auto GetHeap() const -> ID3D12DescriptorHeap* override;
        MKT_NODISCARD auto GetHeapType() const -> D3D12_DESCRIPTOR_HEAP_TYPE;
        MKT_NODISCARD auto GetCpuHandle( DescriptorIndex index ) const -> D3D12_CPU_DESCRIPTOR_HANDLE override;
        MKT_NODISCARD auto GetGpuHandle( DescriptorIndex index ) const -> D3D12_GPU_DESCRIPTOR_HANDLE override;

        ~DynamicDescriptorHeap() override = default;

    private:
        core::usize mCapacity{};
        core::usize mDescriptorStride{};

        std::mutex mIndicesMutex{};
        eastl::vector<eastl::pair<DescriptorIndex, core::u32>> mFreeRanges{};

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> mShaderHeap{};

        D3D12_DESCRIPTOR_HEAP_TYPE mHeapType{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };

        D3D12_CPU_DESCRIPTOR_HANDLE mStartCpuHandle{};
        D3D12_GPU_DESCRIPTOR_HANDLE mStartGpuHandle{};

        ID3D12Device2* mDevice{};

        std::mutex mMutex{};

        // Debug
        eastl::string mDebugName{};
    };

    class DeviceResources {
    public:
        eastl::unique_ptr<StaticDescriptorHeap> mRenderTargetViewHeap{};
        eastl::unique_ptr<StaticDescriptorHeap> mDepthStencilViewHeap{};

        // Commented for now until I implement path for shader model pre 6.6
        //eastl::unique_ptr<StaticDescriptorHeap> mShaderResourceViewHeap{};
        //eastl::unique_ptr<StaticDescriptorHeap> mSamplerHeap{};

        // For shader model 6.6
        eastl::unique_ptr<DynamicDescriptorHeap> mSamplerDescriptorHeap{};
        eastl::unique_ptr<DynamicDescriptorHeap> mResourceDescriptorHeap{};
    };

    class BindingLayout final : public renderer::rhi::IBindingLayout {
    public:
        explicit BindingLayout( const renderer::rhi::BindingLayoutDescription& desc );
        explicit BindingLayout( const renderer::rhi::DescriptorTableLayoutDescription& desc );

        MKT_NODISCARD auto GetRegisterSpace() const -> core::u32 override;

        MKT_NODISCARD auto IsBindless() const -> bool override;

        MKT_NODISCARD auto GetBindingLayoutDesc() const -> const renderer::rhi::BindingLayoutDescription&;
        MKT_NODISCARD auto GetBindlessLayoutDesc() const -> const renderer::rhi::DescriptorTableLayoutDescription&;

        MKT_NODISCARD auto GetDescriptorRanges() const -> const eastl::vector<D3D12_DESCRIPTOR_RANGE1>&;
        MKT_NODISCARD auto GetDescriptorSamplerRanges() const -> const eastl::vector<D3D12_DESCRIPTOR_RANGE1>&;

        MKT_NODISCARD auto GetNextIndexForDescriptor(D3D12_DESCRIPTOR_RANGE_TYPE descriptor ) const -> core::u32;

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> rhi::Object override;

        ~BindingLayout() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        core::u32 mRegisterSpace{};
        bool mIsBindless{ false };
        renderer::rhi::BindingLayoutDescription mBindingLayoutDesc{};
        renderer::rhi::DescriptorTableLayoutDescription mBindlessLayoutDesc{};

        // Because for D3D12 Samplers cannot be mixed
        // with other resource types in a descriptor table
        eastl::vector<D3D12_DESCRIPTOR_RANGE1> mDescriptorRanges{};
        eastl::vector<D3D12_DESCRIPTOR_RANGE1> mDescriptorRangesSamplers{};

        // Track the next available register slot for each DX12 type category
        // Default initializes all categories (SRV, UAV, CBV, Sampler) to 0
        ankerl::unordered_dense::map<D3D12_DESCRIPTOR_RANGE_TYPE, core::u32> mNextRegisterForType{};
    };

    struct DescriptorRange {
        core::u32 mCount{ 0 };
        D3D12_GPU_DESCRIPTOR_HANDLE mGpuHandle{};
        DescriptorIndex mBaseIndex{ d3d12::kInvalidDescriptorIndex };
    };

    class BindingTable : public renderer::rhi::IBindingTable {
    public:
        explicit BindingTable( const renderer::rhi::BindingTableDescription& desc, renderer::rhi::BindingLayoutHandle layout, DeviceResources& resources );

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object override;

        MKT_NODISCARD auto GetSrvRange() const -> const DescriptorRange&;
        MKT_NODISCARD auto GetSamplerRange() const -> const DescriptorRange&;

        MKT_NODISCARD auto Snapshot() -> rhi::BindingTableHandle;
        auto Update( const rhi::BindingTableDescription& desc ) -> void;

        ~BindingTable() override;

    protected:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        rhi::BindingLayoutHandle mBindingLayout{};
        renderer::rhi::BindingTableDescription mBindingDescription{};
        eastl::vector<rhi::DeviceObjectHandle> mResources{};
        std::mutex mVersionMutex{};
        rhi::BindingTableHandle mCurrentVersion{};

        DeviceResources* mDeviceResources{};

        DescriptorRange mSrvRange{};
        DescriptorRange mSamplerRange{};
    };

    class DescriptorTable : public rhi::IDescriptorTable {
    public:
        MKT_NODISCARD auto GetBindingLayout() const -> rhi::BindingLayoutHandle override;
        auto ReleaseIndex( rhi::DescriptorTableIndex index, rhi::ResourceType type, core::u32 slot ) -> void override;
        explicit DescriptorTable( rhi::BindingLayoutHandle setLayout, DeviceResources& resources );

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object override;

        ~DescriptorTable() override;

        MKT_NODISCARD auto GetCapacity( core::u32 ) const -> core::u32 override;

        MKT_NODISCARD auto AllocateIndex( rhi::ResourceType type, core::u32 slot ) -> DescriptorIndex;
        auto UnRegisterIndex( DescriptorIndex index, rhi::ResourceType type, core::u32 slot ) -> void;

        MKT_NODISCARD auto GetLayout() const -> const BindingLayout*;

    private:
        friend class CommandList;
        friend class Device;
        mutable ankerl::unordered_dense::map<core::u64, rhi::DeviceObjectHandle> mResources{};

        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        DeviceResources* mDeviceResources{};
        rhi::BindingLayoutHandle mBindingLayout{};
        mutable std::mutex mDescriptorMutex{};
        ankerl::unordered_dense::map<core::u32, ankerl::unordered_dense::set<DescriptorIndex>> mSamplerIndices{};
        ankerl::unordered_dense::map<core::u32, ankerl::unordered_dense::set<DescriptorIndex>> mResourceIndices{};

    };

    class InputLayout : public renderer::rhi::IInputLayout {
    public:
        explicit InputLayout( const renderer::rhi::InputLayoutCreateDescription& desc );

        MKT_NODISCARD auto GetNumAttributes() const -> core::u32 override;
        MKT_NODISCARD auto GetAttributeDescription(core::u32 index) const -> const renderer::rhi::VertexAttributeDescription& override;

        MKT_NODISCARD auto GetInputElements() -> const D3D12_INPUT_ELEMENT_DESC*;
        MKT_NODISCARD auto GetInputElementsCount() -> UINT;

        ~InputLayout() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        eastl::vector<D3D12_INPUT_ELEMENT_DESC> mInputElems{};
        ankerl::unordered_dense::map<eastl::string, core::u32> mSemanticIndexPerName{};

        renderer::rhi::InputLayoutCreateDescription mDescription{};
        eastl::fixed_hash_map<core::u32, renderer::rhi::VertexAttributeDescription, renderer::rhi::kMaxVertexAttributes> mAttributes{};
    };

    // https://learn.microsoft.com/en-us/windows/win32/direct3d12/root-signatures
    class PipelineLayout : public renderer::rhi::IPipelineLayout {
    public:

        explicit PipelineLayout(const renderer::rhi::PipelineLayoutCreateDescription& description);

        auto SetDebugName( const eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( renderer::rhi::ObjectType type) -> renderer::rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object override;

        MKT_NODISCARD auto GetRootConstantIndex() const -> core::u32;
        MKT_NODISCARD auto GetMax32BitValuesCount() const -> core::u32;
        MKT_NODISCARD auto GetBindingRootIndex( core::u32 layoutIndex, bool sampler ) const -> core::u32;

        MKT_NODISCARD auto GetDescription() const -> const renderer::rhi::PipelineLayoutCreateDescription&;

        operator ID3D12RootSignature*() const;

        ~PipelineLayout() override;

    protected:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        core::u32 mRootConstantIndex{};
        core::u32 m32BitValueCount{ 32 };
        eastl::vector<core::u32> mResourceRootIndices{};
        eastl::vector<core::u32> mSamplerRootIndices{};

        Microsoft::WRL::ComPtr<ID3DBlob> mSignatureBlob{};
        Microsoft::WRL::ComPtr<ID3DBlob> mErrorMessages{};
        Microsoft::WRL::ComPtr<ID3D12RootSignature> mRootSignature{};
        renderer::rhi::PipelineLayoutCreateDescription mDescription{};
    };

    struct CommandAllocationContext {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mAllocator{};
    };

    class Queue final : public renderer::rhi::IQueue {
    public:
        auto GetType() const -> rhi::QueueType override;
        auto GetOpSupportFlags() const -> rhi::QueueOpSupportFlags override;

    private:
        rhi::QueueType mType{ rhi::QueueType::eInvalid };
        rhi::QueueOpSupportFlags mOpSupportFlags{ rhi::QueueOpSupportFlagsBits::Graphics };

    public:
        explicit Queue( renderer::rhi::QueueType type, renderer::rhi::QueueOpSupportFlags flags );

        auto ExecuteCommandLists( const renderer::rhi::SubmitInfo& submitInfo ) -> void override;

        // D3D12 Specifics
        MKT_NODISCARD auto GetCurrentTimeline() -> core::u64;
        MKT_NODISCARD auto AllocateCmdList() -> renderer::rhi::CommandListHandle;

        auto WaitIdle() -> void;
        auto WaitForSubmission( core::u64 value ) -> void;
        auto CollectGarbage() -> void;
        auto Signal(Fence* fence, core::u64 value) -> void;

        // Conversion operators
        operator ID3D12CommandQueue*() const;

        ~Queue() override;

    private:
        auto Destroy() -> void override;
        auto Initialize() -> void override;

    private:
        friend class CommandList;

#if defined( MIKOTO_ENABLE_TRACY )
        auto InitializeGpuProfiler( core::u64 connection ) -> void;
        eastl::shared_ptr<tracy::D3D12QueueCtx> mGpuProfiler{};
        eastl::optional<core::u64> mGpuProfilerConnection{};
#endif

        // Keep waits, execution, timeline assignment and signals in one order.
        std::mutex mSubmissionMutex{};
        rhi::FenceHandle mFence{};
        core::u64 mFenceValue{};
        eastl::vector<eastl::pair<core::u64, rhi::CommandListHandle>> mPendingCommands{};
        eastl::vector<eastl::pair<core::u64, rhi::FenceHandle>> mPendingFences{};

        Microsoft::WRL::ComPtr<ID3D12CommandQueue> mQueue{};

        // For debug
        eastl::string mSubmissionLabel{};
        renderer::rhi::Color mSubmissionLabelColor{};
    };

    struct RecordingContext {
        core::u64 mSubmissionID{};

        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> mCommandAllocator{};
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList7> mCommandList{};
        Microsoft::WRL::ComPtr<ID3D12VideoDecodeCommandList> mVideoCommandList{};

#if defined( MIKOTO_ENABLE_TRACY )
        eastl::shared_ptr<tracy::D3D12QueueCtx> mGpuProfiler{};
#endif

        // Resources that I'm currently using,
        // I keep a strong reference to them to avoid
        // destroying them while this command buffer is still being executed
        ankerl::unordered_dense::map<const renderer::rhi::DeviceObject*, renderer::rhi::DeviceObjectHandle> mInFlightResources{};

        // Suballocations that I'm currently using
        eastl::fixed_vector<GpuUploadAllocation*, 10> mInFlightSubAllocations{};
    };

    // https://devblogs.microsoft.com/directx/a-look-inside-d3d12-resource-state-barriers/
    // https://learn.microsoft.com/en-us/windows/win32/direct3d12/recording-command-lists-and-bundles
    class CommandList final : public rhi::ICommandList {
    private:
        auto RetainResource( const rhi::DeviceObject* resource ) -> void;

        auto ValidateQueryPool( rhi::IQueryPool* pool ) const -> QueryPool&;

    private:
        rhi::QueueType mQueueType{ rhi::QueueType::eInvalid };
        core::u32 mSelfManagedCommandListCount{ 4 };

        friend class QueryPool;
        friend class AccelerationStructure;
        friend class RayTracingPipeline;

        auto PrepareRayTracing( rhi::DeviceObject& resource ) -> ID3D12GraphicsCommandList7*;

    public:
        explicit CommandList( rhi::IQueue* queue );

        auto Open( const rhi::CommandListBeginDescription& desc ) -> void override;
        auto Close() -> void override;
        auto DecodeVideo( const rhi::VideoDecodeDescription& description ) -> void override;

        auto SetDebugName( eastl::string_view name ) -> void override;

        auto SetTransition( const rhi::TransitionDescription& description ) -> void override;
        auto SetBarrier( const rhi::BarrierDescription& description ) -> void override;

        auto AcquireResource( const rhi::BufferBarrierDescription& description, rhi::IQueue* sourceQueue ) -> void override;
        auto ReleaseResource( const rhi::BufferBarrierDescription& description, rhi::IQueue* destQueue ) -> void override;

        auto AcquireResource( const rhi::TextureBarrierDescription& description, rhi::IQueue* sourceQueue ) -> void override;
        auto ReleaseResource( const rhi::TextureBarrierDescription& description, rhi::IQueue* destQueue ) -> void override;

        auto SetEnableAutomaticBarriers( bool enable ) -> void override;

        auto SetClearColor( rhi::ITexture* renderTarget, rhi::Color color ) -> void override;

        auto Write( rhi::IBuffer* src, rhi::ITexture* dest ) -> void override;
        auto Write( rhi::ITexture* texture, const void* data, core::usize byteSize ) -> void override;
        auto Copy( rhi::ITexture* src, const rhi::TextureSlice& srcSlice, rhi::ITexture* dest, const rhi::TextureSlice& destSlice ) -> void override;

        auto Resolve( rhi::ITexture* src, const rhi::TextureSlice& srcSlice, rhi::ITexture* dest, const rhi::TextureSlice& destSlice ) -> void override;

        auto Write( rhi::IBuffer* buffer, core::usize destOffset, const void* data, core::usize byteSize ) -> void override;
        auto Write( rhi::IBuffer* buffer, const void* data, core::usize byteSize ) -> void override;
        auto Copy( rhi::IBuffer* src, rhi::IBuffer* dest ) -> void override;
        auto Copy( rhi::IBuffer* src, rhi::IBuffer* dest, core::usize destOffset ) -> void override;

        auto Copy( rhi::IBuffer* dest, rhi::ITexture* src ) -> void override;
        auto Copy( rhi::IBuffer* dest, rhi::ITexture* src, const rhi::TextureSlice& srcSlice ) -> void override;

        auto BeginRenderPass( rhi::RenderPassDescription& state ) -> void override;
        auto EndRenderPass() -> void override;

        auto BindPipeline( rhi::IPipeline* pipeline ) -> void override;

        // I am not sure if I wanna have these because the viewport is set on the images we
        // render to so it makes more sense to tie them to the graphics state when we specify the render targets
        auto SetViewport( eastl::span<const rhi::Viewport> viewports ) -> void override;
        auto SetScissors( eastl::span<const rhi::Rect> scissorRects ) -> void override;
        auto SetViewportState( const rhi::ViewportState& vs ) -> void override;

        auto SetPolygonLineWidth( core::f32 width ) -> void override;

        auto BindIndexBuffer( rhi::IBuffer* buffer ) -> void override;
        auto BindVertexBuffer( const rhi::VertexBufferBinding& binding ) -> void override;
        auto BindVertexBuffers( eastl::span<const rhi::VertexBufferBinding> binding ) -> void override;

        auto BindPipelineResources( const rhi::BindResourcesDescription& desc ) -> void override;

        auto Draw( const rhi::DrawArguments& args ) -> void override;
        auto BindIndirectBuffer( rhi::IBuffer* buffer ) -> void override;
        auto DrawIndexed( const rhi::DrawArguments& args ) -> void override;

        auto DrawIndirect( core::u32 offset, core::u32 drawCount ) -> void override;
        auto DrawIndexedIndirect( core::u32 offset, core::u32 drawCount ) -> void override;

        auto Dispatch( core::u32 groupsX, core::u32 groupsY, core::u32 groupsZ ) -> void override;

        auto DispatchRays( const rhi::RayTracingDispatchDescription& desc ) -> void override;

        auto BindShaderTable( rhi::IShaderTable* table ) -> void override;

        auto SetPushConstants( rhi::IPipelineLayout* pipelineLayout, const void* data, core::usize byteSize, rhi::ShaderFlags visibility ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> renderer::rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) const -> renderer::rhi::Object override;

        // https://renderdoc.org/docs/how/how_annotate_capture.html
        auto BeginDebugLabel( eastl::string_view name, rhi::Color color ) -> void override;
        auto EndDebugLabel() -> void override;

#if defined( MIKOTO_ENABLE_TRACY )
        auto CollectGpuProfile() -> void override;
        auto BeginGpuProfileZone( eastl::string_view name, std::source_location source ) -> void override;
        auto EndGpuProfileZone() -> void override;
#endif

        // A query pool is one submission batch. Reset only after its completion
        // fence. Begin/End must use this same recording; Resolve precedes host reads.
        auto Reset( rhi::IQueryPool* pool ) -> void override;
        auto BeginQuery( rhi::IQueryPool* pool, core::u32 index ) -> void override;
        auto EndQuery( rhi::IQueryPool* pool, core::u32 index ) -> void override;
        auto WriteTimestamp( rhi::IQueryPool* pool, core::u32 index ) -> void override;
        auto Resolve( rhi::IQueryPool* pool, core::u32 first, core::u32 count ) -> void override;

        // Initial immutable build, including any unbuilt BLAS dependencies.
        auto Build( rhi::IAccelerationStructure* structure ) -> void override;

        MKT_NODISCARD auto GetQueueType() const -> rhi::QueueType override;

        // D3D12 Specifics
        MKT_NODISCARD auto IsInUse() const -> bool;

        auto ClearState() -> void;

        auto MarkExecuted( rhi::IQueue* queue, core::u64 submissionID ) -> void;
        auto ReleaseCompletedResources() -> void;

        MKT_NODISCARD operator ID3D12GraphicsCommandList7*() const;
        MKT_NODISCARD auto GetCommandList() const -> ID3D12CommandList*;

        ~CommandList() override;

    private:
        // [Internal usage]
        auto RecordBarrier( const rhi::BufferBarrierDescription& ) -> void;
        auto RecordBarrier( const rhi::TextureBarrierDescription& ) -> void;
        auto RecordTransition( rhi::IBuffer*, rhi::ResourceStates ) -> void;
        auto RecordTransition( rhi::ITexture*, rhi::ResourceStates ) -> void;
        auto SetTransition( rhi::IBuffer*, rhi::ResourceStates ) -> void;
        auto SetTransition( rhi::ITexture*, rhi::ResourceStates ) -> void;
        auto SetBarrier( const rhi::BufferBarrierDescription& ) -> void;
        auto SetBarrier( const rhi::TextureBarrierDescription& ) -> void;
        auto CommitBarriers() -> void;


        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        rhi::IQueue* mQueue{};
        mutable std::mutex mRecordingMutex{};

#if defined( MIKOTO_ENABLE_TRACY )
        struct GpuProfileZone;
        eastl::unique_ptr<GpuProfileZone> mGpuProfileZone{};
#endif

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

        rhi::IBuffer* mIndirectBuffer{};
        // Retained by the recording's resource list; cleared on Open and non-ray binds.
        rhi::RayTracingPipeline* mBoundRayTracingPipeline{};
        rhi::IShaderTable* mBoundShaderTable{};

        rhi::PipelineType mBoundPipelineType{ rhi::PipelineType::eGraphics };
        Microsoft::WRL::ComPtr<ID3D12CommandSignature> mDrawSignature{};
        Microsoft::WRL::ComPtr<ID3D12CommandSignature> mDrawIndexedSignature{};

        bool mRecording{};
        bool mReadyForSubmission{};
        bool mIsRenderScopeActive{};
        bool mEnableAutomaticBarriers{ true };
        eastl::fixed_vector<D3D12_RESOURCE_BARRIER, rhi::kMaxBarriers> mResourceBarriers{};
        eastl::fixed_vector<D3D12_BUFFER_BARRIER, rhi::kMaxBarriers> mBufferBarriers{};
        eastl::fixed_vector<D3D12_TEXTURE_BARRIER, rhi::kMaxBarriers> mTextureBarriers{};


        // For debug
        rhi::Color mLabelColor{};
        eastl::string mRecordingScopeName{};
        eastl::string mRenderingScopeName{};
    };

    class ShaderTable final : public rhi::IShaderTable {
    public:
        explicit ShaderTable( const rhi::ShaderTableDescription& description );

        MKT_NODISCARD auto GetDesc() const -> rhi::ShaderTableDescription const& override;
        MKT_NODISCARD auto GetNumEntries() const -> core::u32 override;
        MKT_NODISCARD auto GetPipeline() const -> rhi::IPipeline* override;

        MKT_NODISCARD auto GetRayGenerationRegion() const -> const D3D12_GPU_VIRTUAL_ADDRESS_RANGE&;
        MKT_NODISCARD auto GetMissRegion() const -> const D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE&;
        MKT_NODISCARD auto GetHitGroupRegion() const -> const D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE&;
        MKT_NODISCARD auto GetCallableRegion() const -> const D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE&;

        MKT_NODISCARD auto GetBuffer() const -> const rhi::BufferHandle&;

        ~ShaderTable() override;

        DISABLE_COPY_AND_MOVE_FOR( ShaderTable );

    private:
        auto Initialize() -> void override;

        auto Destroy() -> void override;

    private:
        rhi::ShaderTableDescription mDescription{};
        rhi::RayTracingPipelineHandle mPipeline{};
        rhi::BufferHandle mSbtBuffer{};

        D3D12_GPU_VIRTUAL_ADDRESS_RANGE mRayGenRegion{};
        D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE mMissRegion{};
        D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE mHitGroupsRegion{};
        D3D12_GPU_VIRTUAL_ADDRESS_RANGE_AND_STRIDE mCallableRegion{};
    };

    // Works with shader model 6_6 which is required
    // assert if not supported, pending to implement path for
    // shader model 5.1
    class Device final : public rhi::IGpuDevice {
    public:
        explicit Device( const rhi::GpuDeviceCreateInfo& createInfo );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto CreateTexture( const rhi::TextureCreateDescription& description ) -> rhi::TextureHandle override;
        MKT_NODISCARD auto CreateTextureNative( rhi::ObjectType type, renderer::rhi::Object object, const rhi::TextureCreateDescription& description ) -> rhi::TextureHandle override;

        MKT_NODISCARD auto CreateBuffer( const rhi::BufferCreateDescription& description ) -> rhi::BufferHandle override;
        MKT_NODISCARD auto CreateVideoDecoder( const rhi::VideoDecoderDescription& description ) -> rhi::VideoDecoderHandle override;

        MKT_NODISCARD auto CreateSampler( const rhi::SamplerCreateDescription& description ) -> rhi::SamplerHandle override;

        MKT_NODISCARD auto CreatePipeline( const rhi::ComputePipelineDescription& description ) -> rhi::PipelineHandle override;
        MKT_NODISCARD auto CreatePipeline( const rhi::GraphicsPipelineDescription& description ) -> rhi::PipelineHandle override;

        MKT_NODISCARD auto SupportsRayTracing() const -> bool override;
        MKT_NODISCARD auto CreatePipeline( const rhi::RayTracingPipelineDescription& description ) -> rhi::RayTracingPipelineHandle override;

        MKT_NODISCARD auto CreateShaderTable( const rhi::ShaderTableDescription& description ) -> rhi::ShaderTableHandle override;
        MKT_NODISCARD auto CreateAccelStructure( const rhi::AccelStructureCreateDescription& description ) -> rhi::AccelStructureHandle override;

        MKT_NODISCARD auto CreateCommandList( rhi::QueueType queueType ) -> rhi::CommandListHandle override;
        MKT_NODISCARD auto CreateCommandList( const rhi::CommandListCreateDescription& desc ) -> rhi::CommandListHandle override;

        MKT_NODISCARD auto CreateShader( const rhi::ShaderModuleCreateDescription& desc ) -> rhi::ShaderModuleHandle override;

        MKT_NODISCARD auto CreateInputLayout(const rhi::InputLayoutCreateDescription& desc) -> rhi::InputLayoutHandle override;

        MKT_NODISCARD auto CreateBindingLayout( const rhi::BindingLayoutDescription& desc ) -> rhi::BindingLayoutHandle override;
        MKT_NODISCARD auto CreatePipelineLayout( const rhi::PipelineLayoutCreateDescription& desc ) -> rhi::PipelineLayoutHandle override;
        MKT_NODISCARD auto CreateBindingTable( const rhi::BindingTableDescription& desc, rhi::BindingLayoutHandle layout ) -> rhi::BindingTableHandle override;
        auto UpdateBindingTable( const rhi::BindingTableDescription& desc, rhi::BindingTableHandle table ) -> void override;

        MKT_NODISCARD auto CreateQueryPool( const rhi::QueryPoolDescription& description ) -> rhi::QueryPoolHandle override;
        MKT_NODISCARD auto GetQueryCapabilities( rhi::QueryType type, rhi::QueueType queue ) const -> rhi::QueryCapabilities override;

        MKT_NODISCARD auto CreateFence( core::u64 fenceInitialValue ) -> rhi::FenceHandle override;

        auto UnMap( rhi::IBuffer* buffer ) -> void override;
        MKT_NODISCARD auto Map(rhi::IBuffer* buffer ) -> void* override;
        MKT_NODISCARD auto ReadTextureRegion(
            const rhi::ITexture* texture, const rhi::Rect& region,
            eastl::span<core::u8> destination ) -> bool override;

        // To support bindless techniques in modern graphics APIs
        MKT_NODISCARD auto CreateDescriptorTableLayout( const rhi::DescriptorTableLayoutDescription& desc ) -> renderer::rhi::BindingLayoutHandle override;

        MKT_NODISCARD auto CreateDescriptorTable( rhi::BindingLayoutHandle layout ) -> rhi::DescriptorTableHandle override;
        MKT_NODISCARD auto ResizeDescriptorTable( rhi::DescriptorTableHandle descriptorTable, core::u32 newSize, bool keepContents ) -> bool override;
        MKT_NODISCARD auto WriteDescriptorTable( rhi::DescriptorTableHandle descriptorTable, const rhi::BindingTableItem& item ) -> rhi::DescriptorTableIndex override;
        auto ReleaseDescriptorTableIndex( rhi::DescriptorTableHandle descriptorTable, core::u32 bindingIndex, rhi::DescriptorTableIndex index ) -> void override;

        auto RunGarbageCollection() -> void override;

        auto WaitIdle() -> void override;

        MKT_NODISCARD auto CreateSwapChain( const rhi::SwapChainDescription& description ) -> rhi::SwapChainHandle override;

        MKT_NODISCARD auto SupportsSceneMultisampling( renderer::rhi::Multisampling samples ) const -> bool override;

        MKT_NODISCARD auto GetQueue( rhi::QueueType type ) -> rhi::IQueue* override;

        MKT_NODISCARD auto GetProcessMemoryUsage() const -> rhi::ProcessGpuMemory override;
        MKT_NODISCARD auto GetMemoryUsage() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryTotal() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryAvailable() const -> core::usize override;

        MKT_NODISCARD auto IsInitialized() const -> bool override;

        MKT_NODISCARD auto GetGraphicsApi() const -> renderer::rhi::GraphicsAPI override;
        MKT_NODISCARD auto SupportsBCTextureCompression() const -> bool override;
        MKT_NODISCARD auto IsGraphicsApi( renderer::rhi::GraphicsAPI api ) const -> bool override;
        MKT_NODISCARD auto IsFeatureSupported( rhi::GpuFeatureSupportFlags supportFlags ) const -> bool override;

        MKT_NODISCARD auto IsFeatureRequested( rhi::GpuFeatureSupportFlags requestFlags ) const -> bool override;

        MKT_NODISCARD auto GetVendorID() const -> eastl::string_view override;
        MKT_NODISCARD auto GetDeviceName() const -> eastl::string_view override;
        MKT_NODISCARD auto GetDriverVersion() const -> eastl::string_view override;

        // D3D12 Specifics
        MKT_NODISCARD auto CreateTexture( const ExternalTextureDescription& info ) -> rhi::TextureHandle;

        MKT_NODISCARD auto GetEmptyRootSignature() const -> ID3D12RootSignature*;

        MKT_NODISCARD auto IsSupported( DeviceFeatureSupport feature ) const -> bool;

        auto DumpMessages() -> void;

        static auto CALLBACK DebugMessageCallback(
                D3D12_MESSAGE_CATEGORY,
                D3D12_MESSAGE_SEVERITY severity,
                D3D12_MESSAGE_ID,
                LPCSTR description,
                void* ) -> void;

        MKT_NODISCARD auto GetDevice() -> ID3D12Device2*;
        MKT_NODISCARD auto GetVideoDevice() const -> ID3D12VideoDevice*;
        MKT_NODISCARD auto GetAdapter() -> IDXGIAdapter4*;

        MKT_NODISCARD auto GetHeapResources() -> DeviceResources*;
        MKT_NODISCARD auto GetHeapResources() const -> const DeviceResources*;

        MKT_NODISCARD auto GetAllocator() -> GpuMemoryAllocator*;
        MKT_NODISCARD auto GetUploadManager() -> GpuUploadManager*;

        MKT_NODISCARD auto CreateSwapChain(platform::IWindow* window, Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory) -> D3D12SwapChainHandle;

        ~Device() override = default;

    private:
        // [Internal usage]
        auto QuerySupportedFeatures() -> void;
        auto InitInfoQueue() -> void;
        auto InitCommandQueues() -> void;
        auto InitDummyResources() -> void;
        auto InitMemoryAllocator() -> void;
        auto InitDescriptorHeapManager() -> void;

    private:
        eastl::string mVendorID{};
        eastl::string mDeviceName{};
        eastl::string mDriverVersion{};

        renderer::rhi::GraphicsAPI mApi{};
        bool mIsInitialized{ false };

        // OpenGL convention
        renderer::rhi::ViewportConvention mViewportConvention{ renderer::rhi::ViewportConvention::eRightHanded_OriginBottomLeft };

        renderer::rhi::GpuDeviceType mDeviceType{ renderer::rhi::GpuDeviceType::eInvalid };

        // Requests constrain initialization; support is cached independently of those requests.
        renderer::rhi::GpuFeatureSupportFlags mRequestedFeatures{ renderer::rhi::GpuFeatureSupportFlagsBits::None };
        renderer::rhi::GpuFeatureSupportFlags mSupportedFeatures{ renderer::rhi::GpuFeatureSupportFlagsBits::None };

        Microsoft::WRL::ComPtr<ID3D12Device2> mDevice{};
        Microsoft::WRL::ComPtr<ID3D12VideoDevice> mVideoDevice{};
        Microsoft::WRL::ComPtr<IDXGIAdapter1> mAdapter1{};
        Microsoft::WRL::ComPtr<IDXGIAdapter4> mAdapter4{};

        DXGI_ADAPTER_DESC3 mDeviceDescription3{};

        // [Memory management]
        DeviceResources mResourceHeaps{};
        eastl::unique_ptr<rhi::IGpuAllocator> mGpuAllocator{};
        eastl::unique_ptr<GpuUploadManager> mUploadManager{};

        // [Command list management]
        ankerl::unordered_dense::map<renderer::rhi::QueueType, core::Ref<Queue>> mQueues{};

        ankerl::unordered_dense::map<DeviceFeatureSupport, bool> mDeviceFeaturesSupported{};

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

#endif// MIKOTO_RENDERER_RHI_D3D12_D3D12_DEVICE_HH


