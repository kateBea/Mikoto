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

#ifndef MIKOTO_D3D11DEVICE_HH
#define MIKOTO_D3D11DEVICE_HH

#include <mutex>

#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Platform.hh>

#include <Logging/Assert.hh>

#include <Filesystem/Path.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/RenderSystem.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <d3d11.h>
#include <d3d11_3.h>

#include <wrl.h>
#include <dxgi1_3.h>
#include <d3dcommon.h>

#include <Renderer/D3D11/D3D11Buffer.hh>
#include <Renderer/D3D11/D3D11Texture.hh>
#include <Renderer/D3D11/D3D11Shader.hh>
#include <Renderer/D3D11/D3D11SwapChain.hh>
#include <Renderer/D3D11/D3D11Pipeline.hh>
#include <Renderer/D3D11/Direct3D11Libraries.hh>

namespace mikoto::renderer::d3d11 {

    class BindingLayout final : public IBindingLayout {
    public:
        explicit BindingLayout( const BindingLayoutDescription& desc );
        explicit BindingLayout( const BindlessLayoutDescription& desc );

        MKT_NODISCARD auto IsBindless() const -> bool override;
        MKT_NODISCARD auto GetRegisterSpace() const -> u32 override;

        ~BindingLayout() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        bool mIsBindless{ false };
        BindingLayoutDescription mBindingLayoutDesc{};
        BindlessLayoutDescription mBindlessLayoutDesc{};
    };

    // Variants?
    struct D3D11BindingData {
        ID3D11ShaderResourceView* srv{ nullptr };
        ID3D11SamplerState* sampler{ nullptr };
        ID3D11Buffer* constantBuffer{ nullptr };
    };

    class BindingSet : public IBindingSet {
    public:

        explicit BindingSet( const BindingSetDescription& desc, BindingLayoutHandle layout );

        auto Bind(ID3D11DeviceContext* ctx, ShaderStage stage ) const -> void;

        ~BindingSet() override;

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        BindingLayoutHandle mBindingLayout{};
        BindingSetDescription mBindingDescription{};

        eastl::vector<D3D11BindingData> mResolvedBindings{};
    };

    class PipelineLayout : public IPipelineLayout {
    public:
        explicit PipelineLayout( const PipelineLayoutCreateDescription& desc );

        MKT_NODISCARD auto GetDescription() const -> const PipelineLayoutCreateDescription&;

        ~PipelineLayout() override;

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        PipelineLayoutCreateDescription mDesc{};
    };

    class DescriptorTable : public IDescriptorTable {
    public:
        MKT_NODISCARD auto GetCapacity( u32 slot ) const -> u32 override;
    };

    // https://gamedev.stackexchange.com/questions/204429/difference-between-command-lists-and-deferred-context
    class CommandList final : public ICommandList {
    public:
        explicit CommandList( QueueType type );

        auto Begin( const CommandListBeginDescription& desc ) -> void override;
        auto End() -> void override;

        auto BeginParallel() -> void override;
        auto EndParallel() -> void override;

        // More relaxed versions of SetResourceState
        auto PushBarrier( const BufferBarrierDescription& barrier ) -> void override;
        auto PushBarrier( const TextureBarrierDescription& barrier ) -> void override;

        auto BeginTrackingState(IBuffer* buffer, ResourceStates stateBits) -> void override;
        auto BeginTrackingState(ITexture* buffer, ResourceStates stateBits) -> void override;

        auto SetResourceState(IBuffer* buffer, ResourceStates stateBits) -> void override;
        auto SetResourceState(ITexture* buffer, ResourceStates stateBits) -> void override;

        auto SetBarrier( const BufferBarrierDescription& barrier ) -> void override;
        auto SetBarrier( const TextureBarrierDescription& barrier ) -> void override;

        auto CommitBarriers() -> void override;

        auto SetEnableAutomaticBarriers(  bool enable ) -> void override;

        auto SetClearColor( FramebufferHandle frameBuffer, Color color ) -> void override;
        auto SetClearColor( TextureHandle renderTargets, Color color ) -> void override;

        auto WriteVolatile( IBuffer* target, size_t dstOffset, const void* data, size_t byteSize ) -> void override;

        auto Write( IBuffer* src, ITexture* dest, u32 mipLevel ) -> void override;
        auto Write( ITexture* target, u32 mipLevel,const void* data, size_t byteSize ) -> void override;
        auto Copy( ITexture* src, const TextureSlice& srcSlice, ITexture* dest, const TextureSlice& destSlice ) -> void override;

        auto Write( IBuffer* target, size_t destOffset, const void* data, size_t byteSize ) -> void override;
        auto Write( IBuffer* target, const void* data, size_t byteSize ) -> void override;
        auto Copy( IBuffer* src, IBuffer* dest ) -> void override;
        auto Copy( IBuffer* src, IBuffer* dest, size_t destOffset ) -> void override;

        auto Copy( IBuffer* dest, ITexture* src ) -> void override;

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

        auto SetDebugName( eastl::string_view name ) -> void override;

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;

        ~CommandList() override;

    private:
        auto ClearState() -> void;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        Microsoft::WRL::ComPtr<ID3D11CommandList> mCommandList{};

        // Each command list must be recorded by a deferred context
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> mDeviceContextDeferred{};

        bool mEnableAutomaticBarriers{ true };

        // State
        PipelineHandle mCurrentGraphicsPipeline{};
        PipelineHandle mCurrentComputePipeline{};

        FramebufferHandle mCurrentFramebuffer{};

        BufferHandle mCurrentIndirectBuffer{};
        BufferHandle mCurrentIndexBuffer{};
        IndexBufferBinding mCurrentIndexBufferBinding{};
        eastl::fixed_vector<BufferHandle, kMaxVertexAttributes> mCurrentVertexBuffers{};

        Color mCurrentClearColor{};

        bool mCurrentGraphicsStateValid{ false };
        bool mCurrentComputeStateValid{ false };
    };

    class InputLayout : public IInputLayout {
    public:
        explicit InputLayout( const InputLayoutCreateDescription& desc );

        MKT_NODISCARD auto GetNativeHandle( ObjectType ) -> Object override;
        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) const -> Object override;

        MKT_NODISCARD auto GetNumAttributes() const -> u32 override;
        MKT_NODISCARD auto GetAttributeDescription(u32 index) const -> const VertexAttributeDescription& override;

        ~InputLayout() override;

    private:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        Microsoft::WRL::ComPtr<ID3D11InputLayout> mInputLayout{};

        InputLayoutCreateDescription mDesc{};

        eastl::fixed_vector<D3D11_INPUT_ELEMENT_DESC, kMaxVertexAttributes> mInputDescriptions{};
    };

    class Device final : public GpuDevice {
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

        auto WaitIdle() -> void override;

        // Direct3D 11 specifics
        MKT_NODISCARD auto GetDevice() const -> ID3D11Device*;
        MKT_NODISCARD auto GetDevice3() const -> ID3D11Device3*;

        MKT_NODISCARD auto GetDeviceContext() -> ID3D11DeviceContext*;
        MKT_NODISCARD auto GetDeviceContext3() -> ID3D11DeviceContext3*;

        auto DumpErrorMessages() -> void;

        auto CreateSwapChain(Window* window, Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory) -> SwapChainHandle;

        ~Device() override = default;

    private:
        static constexpr u32 kMaxNonSubmittedCmds{ 100 };
        std::mutex mCmdListSubmitMutex{};
        eastl::fixed_vector<CommandListHandle, kMaxNonSubmittedCmds> mNonSubmittedCommands{};

        eastl::vector<D3D_FEATURE_LEVEL> mDeviceFeatureLevel{
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0
        };

        Microsoft::WRL::ComPtr<IDXGIAdapter> mAdapter{};

        Microsoft::WRL::ComPtr<ID3D11Device> mDevice{};
        Microsoft::WRL::ComPtr<ID3D11Device3> mDevice3{};

        std::mutex mCommandsExecuteMutex{};
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> mDeviceContext{};
        Microsoft::WRL::ComPtr<ID3D11DeviceContext3> mDeviceContext3{};
    };

#define MKT_D3D11_DEVICE_CHECK( expr, message )                                            \
    do {                                                                                   \
        HRESULT hr__{ ( expr ) };                                                          \
        checked_cast<Device*>( RenderSystem::Get()->GetGpuDevice() )                       \
                ->DumpErrorMessages();                                                     \
                                                                                           \
        if ( FAILED( hr__ ) ) {                                                            \
            MKT_ASSERT( false, string::Format( "{} (HRESULT=0x{:08X})", message, hr__ ) ); \
        }                                                                                  \
    } while ( 0 )
}// namespace mikoto::renderer::d3d11

#endif

#endif//MIKOTO_D3D11DEVICE_HH
