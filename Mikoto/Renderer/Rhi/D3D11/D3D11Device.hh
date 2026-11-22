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

#ifndef MIKOTO_RENDERER_RHI_D3D11_D3D11_DEVICE_HH
#define MIKOTO_RENDERER_RHI_D3D11_D3D11_DEVICE_HH

#include <mutex>

#include <EASTL/fixed_vector.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Logging/Assert.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Buffer.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/Pipeline.hh>
#include <Renderer/Rhi/GpuDevice.hh>
#include <Renderer/Rhi/CommandList.hh>
#include <Renderer/Core/RenderSystem.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Platform/Windows/Windows.hh>

#include <d3d11.h>
#include <d3d11_3.h>
#include <d3d11_4.h>
#include <d3dcommon.h>
#include <wrl.h>

#include <Renderer/Rhi/D3D11/D3D11Buffer.hh>
#include <Renderer/Rhi/D3D11/D3D11Shader.hh>
#include <Renderer/Rhi/D3D11/D3D11SwapChain.hh>

namespace mikoto::renderer::d3d11 {

    // D3D11 does not need these as it handles synchronization
    // itself internally, this is added for the sake of the RHI
    class Fence final : public rhi::IFence {
    public:
        explicit Fence() = default;

        MKT_NODISCARD auto GetCompletionValue() const -> core::u64 override;

        MKT_NODISCARD auto Signal( core::u64 fenceValue ) -> bool override;
        MKT_NODISCARD auto Wait( core::u64 fenceValue, core::u64 timeoutMs ) -> bool override;

        ~Fence() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;
    };

    class BindingLayout final : public renderer::rhi::IBindingLayout {
    public:
        explicit BindingLayout( const renderer::rhi::BindingLayoutDescription& desc );
        explicit BindingLayout( const renderer::rhi::DescriptorTableLayoutDescription& desc );

        MKT_NODISCARD auto IsBindless() const -> bool override;
        MKT_NODISCARD auto GetRegisterSpace() const -> core::u32 override;

        ~BindingLayout() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        bool mIsBindless{ false };
        renderer::rhi::BindingLayoutDescription mBindingLayoutDesc{};
        renderer::rhi::DescriptorTableLayoutDescription mBindlessLayoutDesc{};
    };

    // Variants?
    struct D3D11BindingData {
        ID3D11ShaderResourceView* mSrv{ nullptr };
        ID3D11SamplerState* mSampler{ nullptr };
        ID3D11Buffer* mConstantBuffer{ nullptr };
    };

    class BindingTable : public renderer::rhi::IBindingTable {
    public:

        explicit BindingTable( const renderer::rhi::BindingTableDescription& desc, renderer::rhi::BindingLayoutHandle layout );

        auto Bind(ID3D11DeviceContext* ctx, rhi::ShaderFlags stage ) const -> void;

        ~BindingTable() override;

    protected:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        renderer::rhi::BindingLayoutHandle mBindingLayout{};
        renderer::rhi::BindingTableDescription mBindingDescription{};

        eastl::vector<D3D11BindingData> mResolvedBindings{};
    };

    class PipelineLayout : public renderer::rhi::IPipelineLayout {
    public:
        explicit PipelineLayout( const renderer::rhi::PipelineLayoutCreateDescription& desc );

        MKT_NODISCARD auto GetDescription() const -> const renderer::rhi::PipelineLayoutCreateDescription&;

        ~PipelineLayout() override;

    protected:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        renderer::rhi::PipelineLayoutCreateDescription mDesc{};
    };

    class DescriptorTable : public renderer::rhi::IDescriptorTable {
    public:
        MKT_NODISCARD auto GetCapacity( core::u32 slot ) const -> core::u32 override;
    };

    // D3D11 does not really expose queues
    // for the sake of the RHI I run commands through it
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

        auto CollectGarbage( bool wait = false ) -> void;
        ~Queue() override;

    private:
        auto Destroy() -> void override;
        auto Initialize() -> void override;

    private:
        std::mutex mSubmitMutex{};
        struct PendingResources {
            Microsoft::WRL::ComPtr<ID3D11Query> mCompletion{};
        };
        eastl::vector<PendingResources> mPendingResources{};

        ID3D11DeviceContext* mDeviceContext{};
        ID3D11DeviceContext3* mDeviceContext3{};
    };

    // https://gamedev.stackexchange.com/questions/204429/difference-between-command-lists-and-deferred-context
    class CommandList final : public rhi::ICommandList {
    public:
        // A query pool is one submission batch. Reset only after its completion
        // fence. Begin/End must use this same recording; Resolve precedes host reads.
        auto Reset( rhi::IQueryPool* pool ) -> void override;
        auto BeginQuery( rhi::IQueryPool* pool, core::u32 index ) -> void override;
        auto EndQuery( rhi::IQueryPool* pool, core::u32 index ) -> void override;
        auto WriteTimestamp( rhi::IQueryPool* pool, core::u32 index ) -> void override;
        auto Resolve( rhi::IQueryPool* pool, core::u32 first, core::u32 count ) -> void override;
        // Initial immutable build, including any unbuilt BLAS dependencies.
        auto Build( rhi::IAccelerationStructure* structure ) -> void override;

        auto GetQueueType() const -> rhi::QueueType override;

    private:

        rhi::QueueType mQueueType{ rhi::QueueType::eInvalid };
        core::u32 mSelfManagedCommandListCount{ 4 };

    public:
        explicit CommandList( rhi::QueueType type );

        MKT_NODISCARD auto GetStatistics() const -> const rhi::CommandListStatistics& override;

        auto DecodeVideo( const rhi::VideoDecodeDescription& description ) -> void override;

        auto Open( const rhi::CommandListBeginDescription& desc ) -> void override;
        auto Close() -> void override;
        auto ValidateSubmission() const -> void;
        auto MarkSubmitted() -> void;

        auto SetTransition( const rhi::TransitionDescription& description ) -> void override;
        auto SetBarrier( const rhi::BarrierDescription& description ) -> void override;

        auto AcquireResource( const rhi::BufferBarrierDescription& description, rhi::IQueue* sourceQueue ) -> void override;
        auto ReleaseResource( const rhi::BufferBarrierDescription& description, rhi::IQueue* destQueue ) -> void override;

        auto AcquireResource( const rhi::TextureBarrierDescription& description, rhi::IQueue* sourceQueue ) -> void override;
        auto ReleaseResource( const rhi::TextureBarrierDescription& description, rhi::IQueue* destQueue ) -> void override;

        auto SetEnableAutomaticBarriers( bool enable ) -> void override;

        auto SetClearColor( rhi::ITexture* renderTarget, rhi::Color color ) -> void override;

        auto Write( rhi::IBuffer* src, rhi::ITexture* dest ) -> void override;
        auto Write( rhi::ITexture* target, const void* data, core::usize byteSize ) -> void override;
        auto Copy( rhi::ITexture* src, const rhi::TextureSlice& srcSlice, rhi::ITexture* dest, const rhi::TextureSlice& destSlice ) -> void override;

        auto Resolve( rhi::ITexture* src, const rhi::TextureSlice& srcSlice, rhi::ITexture* dest, const rhi::TextureSlice& destSlice ) -> void override;

        auto Write( rhi::IBuffer* target, core::usize destOffset, const void* data, core::usize byteSize ) -> void override;
        auto Write( rhi::IBuffer* target, const void* data, core::usize byteSize ) -> void override;
        auto Copy( rhi::IBuffer* src, rhi::IBuffer* dest ) -> void override;
        auto Copy( rhi::IBuffer* src, rhi::IBuffer* dest, core::usize destOffset ) -> void override;

        auto Copy( rhi::IBuffer* dest, rhi::ITexture* src ) -> void override;
        auto Copy( rhi::IBuffer* dest, rhi::ITexture* src, const renderer::rhi::TextureSlice& srcSlice ) -> void override;

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
        auto BindVertexBuffers( eastl::span<const rhi::VertexBufferBinding> bindings ) -> void override;

        auto BindPipelineResources( const rhi::BindResourcesDescription& desc ) -> void override;

        auto Draw( const rhi::DrawArguments& args ) -> void override;
        auto BindIndirectBuffer( rhi::IBuffer* buffer ) -> void override;
        auto DrawIndexed( const rhi::DrawArguments& args ) -> void override;

        auto DrawIndirect( core::u32 offset, core::u32 drawCount ) -> void override;
        auto DrawIndexedIndirect( core::u32 offset, core::u32 drawCount ) -> void override;

        auto Dispatch( core::u32 groupsX, core::u32 groupsY, core::u32 groupsZ ) -> void override;

        auto DispatchRays( const rhi::RayTracingDispatchDescription& desc ) -> void override;

        auto BindShaderTable( rhi::IShaderTable* table ) -> void override;

        auto SetPushConstants( rhi::IPipelineLayout* pipelineLayout, const void* data, core::usize byteSize, renderer::rhi::ShaderFlags visibility ) -> void override;

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( rhi::ObjectType type ) -> rhi::Object override;

        auto BeginDebugLabel( eastl::string_view name, rhi::Color color ) -> void override;
        auto EndDebugLabel() -> void override;

        ~CommandList() override;

    private:

    private:
        auto RecordBarrier( const rhi::BufferBarrierDescription& ) -> void;
        auto RecordBarrier( const rhi::TextureBarrierDescription& ) -> void;
        auto RecordTransition( rhi::IBuffer*, rhi::ResourceStates ) -> void;
        auto RecordTransition( rhi::ITexture*, rhi::ResourceStates ) -> void;
        auto SetTransition( rhi::IBuffer*, rhi::ResourceStates ) -> void;
        auto SetTransition( rhi::ITexture*, rhi::ResourceStates ) -> void;
        auto SetBarrier( const rhi::BufferBarrierDescription& ) -> void;
        auto SetBarrier( const rhi::TextureBarrierDescription& ) -> void;
        auto CommitBarriers() -> void;
        auto ClearState() -> void;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        Microsoft::WRL::ComPtr<ID3D11CommandList> mCommandList{};

        // Each command list must be recorded by a deferred context
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> mDeviceContextDeferred{};

        rhi::CommandListStatistics mStatistics{};

        bool mRecording{};
        bool mExecutable{};
        bool mEnableAutomaticBarriers{ true };

        // State
        rhi::PipelineHandle mCurrentGraphicsPipeline{};
        rhi::PipelineHandle mCurrentComputePipeline{};

        rhi::IBuffer* mCurrentIndirectBuffer{};
        rhi::IBuffer* mCurrentIndexBuffer{};
        rhi::IndexBufferBinding mCurrentIndexBufferBinding{};
        eastl::fixed_vector<renderer::rhi::IBuffer*, renderer::rhi::kMaxVertexAttributes> mCurrentVertexBuffers{};

        rhi::Color mCurrentClearColor{};

        bool mCurrentGraphicsStateValid{ false };
        bool mCurrentComputeStateValid{ false };
    };

    class InputLayout : public renderer::rhi::IInputLayout {
    public:
        explicit InputLayout( const renderer::rhi::InputLayoutCreateDescription& desc );

        MKT_NODISCARD auto GetNativeHandle( renderer::rhi::ObjectType ) -> renderer::rhi::Object override;
        MKT_NODISCARD auto GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object override;

        MKT_NODISCARD auto GetNumAttributes() const -> core::u32 override;
        MKT_NODISCARD auto GetAttributeDescription(core::u32 index) const -> const renderer::rhi::VertexAttributeDescription& override;

        ~InputLayout() override;

    private:
        auto Initialize() -> void override;
        auto Destroy() -> void override;

    private:
        Microsoft::WRL::ComPtr<ID3D11InputLayout> mInputLayout{};

        renderer::rhi::InputLayoutCreateDescription mDesc{};

        eastl::fixed_vector<D3D11_INPUT_ELEMENT_DESC, renderer::rhi::kMaxVertexAttributes> mInputDescriptions{};
    };

    class Device final : public renderer::rhi::IGpuDevice {
    public:
        explicit Device( const renderer::rhi::GpuDeviceCreateInfo& createInfo );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto CreateTexture( const renderer::rhi::TextureCreateDescription& description ) -> renderer::rhi::TextureHandle override;
        MKT_NODISCARD auto CreateTextureNative( renderer::rhi::ObjectType type, renderer::rhi::Object object, const renderer::rhi::TextureCreateDescription& description ) -> renderer::rhi::TextureHandle override;

        MKT_NODISCARD auto CreateBuffer( const renderer::rhi::BufferCreateDescription& description ) -> renderer::rhi::BufferHandle override;

        MKT_NODISCARD auto CreateSampler( const renderer::rhi::SamplerCreateDescription& description ) -> renderer::rhi::SamplerHandle override;

        MKT_NODISCARD auto CreatePipeline( const renderer::rhi::ComputePipelineDescription& description ) -> renderer::rhi::PipelineHandle override;
        MKT_NODISCARD auto CreatePipeline( const renderer::rhi::GraphicsPipelineDescription& description ) -> renderer::rhi::PipelineHandle override;

        MKT_NODISCARD auto SupportsRayTracing() const -> bool override;
        MKT_NODISCARD auto CreatePipeline( const rhi::RayTracingPipelineDescription& ) -> rhi::RayTracingPipelineHandle override;

        MKT_NODISCARD auto CreateShaderTable( const rhi::ShaderTableDescription& description ) -> rhi::ShaderTableHandle override;
        MKT_NODISCARD auto CreateAccelStructure( const renderer::rhi::AccelStructureCreateDescription& description ) -> renderer::rhi::AccelStructureHandle override;

        MKT_NODISCARD auto CreateCommandList( rhi::QueueType queue ) -> renderer::rhi::CommandListHandle override;
        MKT_NODISCARD auto CreateCommandList( const rhi::CommandListCreateDescription& desc ) -> rhi::CommandListHandle override;

        MKT_NODISCARD auto CreateShader( const renderer::rhi::ShaderModuleCreateDescription& desc ) -> renderer::rhi::ShaderModuleHandle override;

        MKT_NODISCARD auto CreateInputLayout(const renderer::rhi::InputLayoutCreateDescription& desc) -> renderer::rhi::InputLayoutHandle override;

        MKT_NODISCARD auto CreateBindingLayout( const renderer::rhi::BindingLayoutDescription& desc ) -> renderer::rhi::BindingLayoutHandle override;
        MKT_NODISCARD auto CreatePipelineLayout( const renderer::rhi::PipelineLayoutCreateDescription& desc ) -> renderer::rhi::PipelineLayoutHandle override;
        MKT_NODISCARD auto CreateBindingTable( const renderer::rhi::BindingTableDescription& desc, renderer::rhi::BindingLayoutHandle layout ) -> renderer::rhi::BindingTableHandle override;
        auto UpdateBindingTable( const rhi::BindingTableDescription& desc, rhi::BindingTableHandle table ) -> void override;

        MKT_NODISCARD auto CreateFence( core::u64 fenceInitialValue ) -> renderer::rhi::FenceHandle override;

        auto UnMap( renderer::rhi::IBuffer* buffer ) -> void override;
        MKT_NODISCARD auto Map(renderer::rhi::IBuffer* buffer ) -> void* override;
        MKT_NODISCARD auto ReadTextureRegion(
            const rhi::ITexture* texture, const rhi::Rect& region,
            eastl::span<core::u8> destination ) -> bool override;

        // To support bindless techniques in modern graphics APIs
        MKT_NODISCARD auto CreateDescriptorTableLayout( const renderer::rhi::DescriptorTableLayoutDescription& desc ) -> renderer::rhi::BindingLayoutHandle override;

        MKT_NODISCARD auto CreateDescriptorTable( renderer::rhi::BindingLayoutHandle layout ) -> renderer::rhi::DescriptorTableHandle override;
        MKT_NODISCARD auto ResizeDescriptorTable( renderer::rhi::DescriptorTableHandle descriptorTable, core::u32 newSize, bool keepContents ) -> bool override;
        MKT_NODISCARD auto WriteDescriptorTable( renderer::rhi::DescriptorTableHandle descriptorTable, const renderer::rhi::BindingTableItem& item ) -> rhi::DescriptorTableIndex override;
        auto ReleaseDescriptorTableIndex( rhi::DescriptorTableHandle descriptorTable, core::u32 bindingIndex, rhi::DescriptorTableIndex index ) -> void override;

        MKT_NODISCARD auto CreateSwapChain( const renderer::rhi::SwapChainDescription& description ) -> rhi::SwapChainHandle override;

        MKT_NODISCARD auto CreateQueryPool( const renderer::rhi::QueryPoolDescription& description ) -> renderer::rhi::QueryPoolHandle override;
        MKT_NODISCARD auto GetQueryCapabilities( renderer::rhi::QueryType type, renderer::rhi::QueueType queue ) const -> renderer::rhi::QueryCapabilities override;

        MKT_NODISCARD auto GetQueue( renderer::rhi::QueueType type ) -> renderer::rhi::IQueue* override;

        MKT_NODISCARD auto GetProcessMemoryUsage() const -> rhi::ProcessGpuMemory override;
        MKT_NODISCARD auto GetMemoryUsage() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryTotal() const -> core::usize override;
        MKT_NODISCARD auto GetMemoryAvailable() const -> core::usize override;

        MKT_NODISCARD auto IsFeatureSupported( rhi::GpuFeatureSupportFlags supportFlags ) const -> bool override;

        MKT_NODISCARD auto IsFeatureRequested( rhi::GpuFeatureSupportFlags requestFlags ) const -> bool override;

        MKT_NODISCARD auto SupportsSceneMultisampling( renderer::rhi::Multisampling samples ) const -> bool override;
        MKT_NODISCARD auto SupportsBCTextureCompression() const -> bool override;

        auto RunGarbageCollection() -> void override;

        auto WaitIdle() -> void override;

        // Direct3D 11 specifics
        MKT_NODISCARD auto GetDevice() const -> ID3D11Device*;
        MKT_NODISCARD auto GetDevice3() const -> ID3D11Device3*;

        MKT_NODISCARD auto GetDeviceContext() -> ID3D11DeviceContext*;
        MKT_NODISCARD auto GetDeviceContext3() -> ID3D11DeviceContext3*;

        MKT_NODISCARD auto IsInitialized() const -> bool override;

        MKT_NODISCARD auto GetGraphicsApi() const -> renderer::rhi::GraphicsAPI override;
        MKT_NODISCARD auto IsGraphicsApi( renderer::rhi::GraphicsAPI api ) const -> bool override;

        MKT_NODISCARD auto GetVendorID() const -> eastl::string_view override;
        MKT_NODISCARD auto GetDeviceName() const -> eastl::string_view override;
        MKT_NODISCARD auto GetDriverVersion() const -> eastl::string_view override;

        auto DumpErrorMessages() -> void;

        auto CreateSwapChain(platform::IWindow* window, Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory) -> D3D11SwapChainHandle;

        ~Device() override = default;

    private:
        eastl::string mVendorID{};
        eastl::string mDeviceName{};
        eastl::string mDriverVersion{};

        renderer::rhi::GraphicsAPI mApi{};
        bool mIsInitialized{ false };

        // OpenGL convention
        renderer::rhi::ViewportConvention mViewportConvention{ renderer::rhi::ViewportConvention::eRightHanded_OriginBottomLeft };

        renderer::rhi::GpuDeviceType mDeviceType{ renderer::rhi::GpuDeviceType::eInvalid };
        renderer::rhi::GpuFeatureSupportFlags mFeatureSupportFlags{ renderer::rhi::GpuFeatureSupportFlagsBits::None };

        eastl::fixed_vector<D3D_FEATURE_LEVEL, 5> mDeviceFeatureLevel{
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0
        };

        Microsoft::WRL::ComPtr<IDXGIDevice> mDxgiDevice{};
        Microsoft::WRL::ComPtr<IDXGIAdapter> mAdapter{};
        Microsoft::WRL::ComPtr<IDXGIAdapter3> mAdapter3{};

        Microsoft::WRL::ComPtr<ID3D11Device> mDevice{};
        Microsoft::WRL::ComPtr<ID3D11Device3> mDevice3{};

        std::mutex mCommandsExecuteMutex{};
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> mDeviceContext{};
        Microsoft::WRL::ComPtr<ID3D11DeviceContext3> mDeviceContext3{};

        rhi::QueueHandle mQueue{};
    };

#define MKT_D3D11_DEVICE_CHECK( expr, message )                                            \
    do {                                                                                   \
        HRESULT hr__{ ( expr ) };                                                          \
        core::checked_cast<Device*>( RenderSystem::Get()->GetGpuDevice() )                       \
                ->DumpErrorMessages();                                                     \
                                                                                           \
        if ( FAILED( hr__ ) ) {                                                            \
            MKT_ASSERT( false, string::Format( "{} (HRESULT=0x{:08X})", message, hr__ ) ); \
        }                                                                                  \
    } while ( 0 )
}// namespace mikoto::renderer::d3d11

#endif

#endif// MIKOTO_RENDERER_RHI_D3D11_D3D11_DEVICE_HH



