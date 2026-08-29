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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Exception.hh>
#include <Core/Platform.hh>

#include <Memory/Allocator.hh>

#include <Filesystem/Path.hh>

#include <Logging/Logger.hh>

#include <Renderer/Core/RenderSystem.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Renderer/Rhi/D3D11/D3D11Buffer.hh>
#include <Renderer/Rhi/D3D11/D3D11Texture.hh>
#include <Renderer/Rhi/D3D11/D3D11Context.hh>
#include <Renderer/Rhi/D3D11/D3D11Device.hh>
#include <Renderer/Rhi/D3D11/D3D11Pipeline.hh>
#include <Renderer/Rhi/D3D11/Direct3D11Helpers.hh>

#if !defined(NDEBUG)
#include <dxgidebug.h>
#pragma comment(lib, "dxguid.lib")
#endif

#include <Platform/PlatformWin32.hh>

namespace mikoto::renderer::d3d11 {

    auto Fence::GetCompletionValue() const -> core::u64 {
        return 0;
    }

    auto Fence::Signal( core::u64 fenceValue ) -> bool {
        return true;
    }

    auto Fence::Wait( core::u64 fenceValue, core::u64 timeoutMs ) -> bool {
        return true;
    }

    Fence::~Fence() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto Fence::Initialize() -> void {
        mIsAllocated = true;
    }

    auto Fence::Release() -> void {
        mIsAllocated = false;
    }

    BindingLayout::BindingLayout( const BindingLayoutDescription &desc )
        : mBindingLayoutDesc{ desc }
    {}

    BindingLayout::BindingLayout( const BindlessLayoutDescription &desc )
        : mBindlessLayoutDesc{ desc }
    {}

    auto BindingLayout::IsBindless() const -> bool {
        return mIsBindless;
    }

    auto BindingLayout::GetRegisterSpace() const -> u32 {
        return 0; //TODO
    }

    BindingLayout::~BindingLayout() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto BindingLayout::Initialize() -> void {
        mIsAllocated = true;
    }

    auto BindingLayout::Release() -> void {
        mIsAllocated = false;
    }

    BindingSet::BindingSet( const BindingSetDescription &desc, BindingLayoutHandle layout )
        : mBindingLayout{ std::move( layout ) }, mBindingDescription{ desc }
    {}

    BindingSet::~BindingSet() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto BindingSet::Initialize() -> void {
        mResolvedBindings.clear();
        mResolvedBindings.resize(mBindingDescription.mBindings.size());

        for ( size_t i{}; i < mBindingDescription.mBindings.size(); ++i ) {
            const auto &item = mBindingDescription.mBindings[i];
            auto &out = mResolvedBindings[i];
            switch ( item.mType ) {
                case ResourceType::eTexture_SRV:
                    out.srv = checked_cast<Texture*>( item.mResource )->GetNativeHandle( ObjectType::D3D11_SRV );
                    break;

                case ResourceType::eConstantBuffer:
                    out.constantBuffer = checked_cast<Buffer *>( item.mResource )->GetNativeHandle( ObjectType::D3D11_Buffer );
                    break;

                case ResourceType::eSampler:
                    out.sampler = checked_cast<Sampler*>( item.mResource )->GetNativeHandle( ObjectType::D3D11_Sampler );
                    break;

                default:
                    break;
            }
        }

        mIsAllocated = true;
    }

    auto BindingSet::Bind( ID3D11DeviceContext *ctx, ShaderFlags visibility ) const -> void {

        // Slots start from 0 for each type of register and increment in order
        u32 constantBuffrSlotIndex{ 0 };
        u32 samplerSlotIndex{ 0 };
        u32 textureSlotIndex{ 0 };

        for ( size_t i{ 0 }; i < mBindingDescription.mBindings.size(); ++i ) {
            const auto &item{ mBindingDescription.mBindings[i] };
            const auto &data{ mResolvedBindings[i] };

            // Resolve slot
            UINT slot{};
            switch ( item.mType ) {
                case ResourceType::eTexture_SRV:
                    slot = textureSlotIndex++;
                    break;
                case ResourceType::eConstantBuffer:
                    slot = constantBuffrSlotIndex++;
                    break;
                case ResourceType::eSampler:
                    slot = samplerSlotIndex++;
                    break;
                default:;
            }

            auto bindSRV = [&]( auto fn ) {
                if ( data.srv ) fn( slot, 1, &data.srv );
            };

            auto bindCB = [&]( auto fn ) {
                if ( data.constantBuffer ) fn( slot, 1, &data.constantBuffer );
            };

            auto bindSampler = [&]( auto fn ) {
                if ( data.sampler ) fn( slot, 1, &data.sampler );
            };

            switch ( item.mType ) {
                case ResourceType::eTexture_SRV:
                    if ( visibility & ShaderFlagsBits::kVertex )
                        bindSRV( [&]( auto... args ) { ctx->VSSetShaderResources( args... ); } );

                    if ( visibility & ShaderFlagsBits::kPixel )
                        bindSRV( [&]( auto... args ) { ctx->PSSetShaderResources( args... ); } );

                    if ( visibility & ShaderFlagsBits::kGeometry )
                        bindSRV( [&]( auto... args ) { ctx->GSSetShaderResources( args... ); } );

                    if ( visibility & ShaderFlagsBits::kHull )
                        bindSRV( [&]( auto... args ) { ctx->HSSetShaderResources( args... ); } );

                    if ( visibility & ShaderFlagsBits::kDomain )
                        bindSRV( [&]( auto... args ) { ctx->DSSetShaderResources( args... ); } );

                    if ( visibility & ShaderFlagsBits::kCompute )
                        bindSRV( [&]( auto... args ) { ctx->CSSetShaderResources( args... ); } );

                    break;

                case ResourceType::eConstantBuffer:
                    if ( visibility & ShaderFlagsBits::kVertex )
                        bindCB( [&]( auto... args ) { ctx->VSSetConstantBuffers( args... ); } );

                    if ( visibility & ShaderFlagsBits::kPixel )
                        bindCB( [&]( auto... args ) { ctx->PSSetConstantBuffers( args... ); } );

                    if ( visibility & ShaderFlagsBits::kGeometry )
                        bindCB( [&]( auto... args ) { ctx->GSSetConstantBuffers( args... ); } );

                    if ( visibility & ShaderFlagsBits::kHull )
                        bindCB( [&]( auto... args ) { ctx->HSSetConstantBuffers( args... ); } );

                    if ( visibility & ShaderFlagsBits::kDomain )
                        bindCB( [&]( auto... args ) { ctx->DSSetConstantBuffers( args... ); } );

                    if ( visibility & ShaderFlagsBits::kCompute )
                        bindCB( [&]( auto... args ) { ctx->CSSetConstantBuffers( args... ); } );

                    break;

                case ResourceType::eSampler:
                    if ( visibility & ShaderFlagsBits::kVertex )
                        bindSampler( [&]( auto... args ) { ctx->VSSetSamplers( args... ); } );

                    if ( visibility & ShaderFlagsBits::kPixel )
                        bindSampler( [&]( auto... args ) { ctx->PSSetSamplers( args... ); } );

                    if ( visibility & ShaderFlagsBits::kGeometry )
                        bindSampler( [&]( auto... args ) { ctx->GSSetSamplers( args... ); } );

                    if ( visibility & ShaderFlagsBits::kHull )
                        bindSampler( [&]( auto... args ) { ctx->HSSetSamplers( args... ); } );

                    if ( visibility & ShaderFlagsBits::kDomain )
                        bindSampler( [&]( auto... args ) { ctx->DSSetSamplers( args... ); } );

                    if ( visibility & ShaderFlagsBits::kCompute )
                        bindSampler( [&]( auto... args ) { ctx->CSSetSamplers( args... ); } );

                    break;

                default:
                    break;
            }
        }
    }

    auto BindingSet::Release() -> void {
        mIsAllocated = false;
    }

    PipelineLayout::PipelineLayout( const PipelineLayoutCreateDescription& desc )
        : mDesc{ desc }
    {}

    auto PipelineLayout::GetDescription() const -> const PipelineLayoutCreateDescription & {
        return mDesc;
    }

    PipelineLayout::~PipelineLayout() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto PipelineLayout::Initialize() -> void {
        mIsAllocated = true;
    }

    auto PipelineLayout::Release() -> void {
        mIsAllocated = false;
    }

    Queue::Queue( QueueType type, QueueOpSupportFlags flags )
        : IQueue{ type, flags }
    {
    }

    auto Queue::ExecuteCommandLists( const SubmitInfo& submitInfo  ) -> void {
        std::lock_guard lock{ mSubmitMutex };

        for (auto& cmdHandle : submitInfo.mCommands ) {
            ID3D11CommandList* native{ cmdHandle->GetNativeHandle(ObjectType::D3D11_CommandList) };
            MKT_ASSERT(native, "Command list is null");

            mDeviceContext->ExecuteCommandList(native, TRUE);
        }
    }

    Queue::~Queue() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto Queue::Release() -> void {
        mIsAllocated = false;
    }

    auto Queue::Initialize() -> void {
        Device* device{ checked_cast<Device*>( mDevice ) };
        mDeviceContext = device->GetDeviceContext();
        mDeviceContext3 = device->GetDeviceContext3();

        mIsAllocated = true;
    }

    CommandList::CommandList( QueueType type )
        : ICommandList{ type }
    {}

    auto CommandList::Begin( const CommandListBeginDescription &desc ) -> void {
        // Reset state before recording new commands
        ClearState();
    }

    auto CommandList::End() -> void {
        MKT_ASSERT(mDeviceContextDeferred, "Deferred context is null");
        mCommandList.Reset(); // release previous one

        if (FAILED(mDeviceContextDeferred->FinishCommandList(FALSE, &mCommandList))) {
            MKT_ASSERT( false, "Failed to finish command list" );
        }
    }

    auto CommandList::RecordBarrier( const BufferBarrierDescription &barrier ) -> void {

    }

    auto CommandList::RecordBarrier( const TextureBarrierDescription &barrier ) -> void {

    }

    auto CommandList::SetBarrier( const BufferBarrierDescription &barrier ) -> void {

    }

    auto CommandList::SetBarrier( const TextureBarrierDescription &barrier ) -> void {

    }

    auto CommandList::RecordTransition( IBuffer *buffer, ResourceStates stateBits ) -> void {
    }

    auto CommandList::RecordTransition( ITexture *texture, ResourceStates stateBits ) -> void {
    }

    auto CommandList::SetTransition( IBuffer *buffer, ResourceStates stateBits ) -> void {
    }

    auto CommandList::SetTransition( ITexture *texture, ResourceStates stateBits ) -> void {
        checked_cast<Texture*>( texture )->EnableUsage( stateBits );
    }

    auto CommandList::CommitBarriers() -> void {
    }

    auto CommandList::SetEnableAutomaticBarriers( bool enable ) -> void {
        mEnableAutomaticBarriers = enable;
    }

    auto CommandList::SetClearColor( TextureHandle renderTargets, Color color ) -> void {
    }

    auto CommandList::ClearState() -> void {
        mDeviceContextDeferred->ClearState();

        mCurrentGraphicsStateValid = false;
        mCurrentComputeStateValid = false;

        mCurrentGraphicsPipeline = nullptr;
        mCurrentVertexBuffers.resize(0);
        mCurrentIndexBuffer = nullptr;
        mCurrentComputePipeline = nullptr;
        mCurrentIndirectBuffer = nullptr;
        mCurrentClearColor = Color{};
    }

    auto CommandList::Write( IBuffer *src, ITexture *dest, u32 mipLevel ) -> void {

    }

    auto CommandList::Write( ITexture *target, u32 mipLevel, const void *data, size_t byteSize ) -> void {

    }

    auto CommandList::Copy( ITexture *src, const TextureSlice &srcSlice, ITexture *dest, const TextureSlice &destSlice ) -> void {

    }

    auto CommandList::Resolve( ITexture *src, const TextureSlice &srcSlice, ITexture *dest, const TextureSlice &destSlice ) -> void {

    }

    auto CommandList::Write( IBuffer *target, size_t destOffset, const void *data, size_t byteSize ) -> void {

    }

    auto CommandList::Write( IBuffer *target, const void *data, size_t byteSize ) -> void {
        D3D11_MAPPED_SUBRESOURCE mappedResource{};
        ID3D11Buffer *buffer{ target->GetNativeHandle( ObjectType::D3D11_Buffer ) };

        if (FAILED( mDeviceContextDeferred->Map(buffer, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &mappedResource))) {
            MKT_ASSERT( false, "Failed to map buffer" );
        } else {
            std::memcpy(mappedResource.pData, data, byteSize);
            mDeviceContextDeferred->Unmap(buffer, 0);
        }
    }

    auto CommandList::Copy( IBuffer *src, IBuffer *dest ) -> void {

    }

    auto CommandList::Copy( IBuffer *src, IBuffer *dest, size_t destOffset ) -> void {

    }

    auto CommandList::Copy( IBuffer *dest, ITexture *src ) -> void {

    }

    auto CommandList::BeginRendering( GraphicsState &state ) -> void {
        // Render targets
        eastl::fixed_vector<ID3D11RenderTargetView*, kMaxRenderTargets> renderTargetViews{};
        for ( const auto &v: state.mCurrentRenderTargets ) {
            auto& rtv{ renderTargetViews.emplace_back( as<ID3D11RenderTargetView *>( v.mRenderTarget->GetNativeHandle( ObjectType::D3D11_RTV ) ) ) };

            auto& rtvClear{ v.mRenderTarget };
            const eastl::array clearColor{ v.mClearColor.mR, v.mClearColor.mG, v.mClearColor.mB, v.mClearColor.mA };
            mDeviceContextDeferred->ClearRenderTargetView(rtv, clearColor.data());
        }

        ID3D11DepthStencilView* dsv{ nullptr };
        if (!state.mDepthTarget.mRenderTarget.IsEmpty()) {
            dsv = state.mDepthTarget.mRenderTarget->GetNativeHandle( ObjectType::D3D11_DSV );
            mDeviceContextDeferred->ClearDepthStencilView(dsv, D3D11_CLEAR_FLAG::D3D11_CLEAR_DEPTH, 1.0f, 0);
        }

        mDeviceContextDeferred->OMSetRenderTargets(
            as<UINT>( renderTargetViews.size() ),
            renderTargetViews.data(),
            dsv
        );
    }

    auto CommandList::EndRendering() -> void {
        // Unbind all render targets + depth
        mDeviceContextDeferred->OMSetRenderTargets(0, nullptr, nullptr);
    }

    auto CommandList::BindPipeline( IPipeline* pipeline ) -> void {
        if (pipeline->GetPipelineType() != PipelineType::eGraphics) {
            return;
        }

        GraphicsPipeline* d3d11Pipeline{ checked_cast<GraphicsPipeline*>( pipeline ) };
        const auto& description{ d3d11Pipeline->GetDescription() };

        // Buffers and input layout
        if (!description.mInputLayout.IsEmpty()) {
            ID3D11InputLayout* inputLayout{ description.mInputLayout->GetNativeHandle( ObjectType::D3D11_InputLayout ) };
            mDeviceContextDeferred->IASetInputLayout(inputLayout);
        }

        // Primitive topology
        mDeviceContextDeferred->IASetPrimitiveTopology( GetPrimitiveTopology( description.mPrimitiveTopology ));

        // Pipeline states state
        ID3D11BlendState* bs{ d3d11Pipeline->GetNativeHandle( ObjectType::D3D11_BlendState ) };
        ID3D11RasterizerState* rs{ d3d11Pipeline->GetNativeHandle( ObjectType::D3D11_RasterizerState ) };
        ID3D11DepthStencilState* dss{ d3d11Pipeline->GetNativeHandle( ObjectType::D3D11_DepthStencilState ) };

        mDeviceContextDeferred->RSSetState(rs);
        mDeviceContextDeferred->OMSetBlendState(bs, nullptr, 0xFFFFFFFF);
        mDeviceContextDeferred->OMSetDepthStencilState(dss, 0);

        // Shaders
        MKT_ASSERT( description.mShaders.contains(ShaderType::eVertex), "No vertex shader found" );

        auto vertexShaderHandle{ description.mShaders.at(ShaderType::eVertex) };
        auto pixelShaderHandle{ description.mShaders.at(ShaderType::ePixel) };

        ID3D11VertexShader *vertexShader{ vertexShaderHandle->GetNativeHandle( ObjectType::D3D11_Shader ) };
        ID3D11PixelShader *pixelShader{ pixelShaderHandle->GetNativeHandle( ObjectType::D3D11_Shader ) };

        mDeviceContextDeferred->VSSetShader(
            vertexShader,
            nullptr,
            0);

        mDeviceContextDeferred->PSSetShader(
            pixelShader,
            nullptr,
            0);

    }

    auto CommandList::SetViewport( eastl::span<const Viewport> viewports ) -> void {
        // Viewports
        eastl::fixed_vector<D3D11_VIEWPORT, kMaxViewports> d3d11Viewports{};
        for (const auto& v : viewports) {
            d3d11Viewports.emplace_back(D3D11_VIEWPORT{
                .TopLeftX = v.mMinX,
                .TopLeftY = v.mMinY,
                .Width    = v.GetWidth(),
                .Height   = v.GetHeight(),
                .MinDepth = v.mMinZ,
                .MaxDepth = v.mMaxZ
            });
        }

        mDeviceContextDeferred->RSSetViewports(
            as<UINT>(d3d11Viewports.size()),
            d3d11Viewports.data()
        );
    }

    auto CommandList::SetScissors( eastl::span<const Rect> scissorRects ) -> void {
        eastl::fixed_vector<D3D11_RECT, kMaxViewports> d3d11Rects{};

        for (const auto& r : scissorRects) {
            d3d11Rects.emplace_back(D3D11_RECT{
                .left   = r.mMinX,
                .top    = r.mMinY,
                .right  = r.mMaxX,
                .bottom = r.mMaxY
            });
        }

        mDeviceContextDeferred->RSSetScissorRects(
            as<UINT>(d3d11Rects.size()),
            d3d11Rects.data()
        );
    }

    auto CommandList::SetViewportState( const ViewportState &vs ) -> void {
        SetViewport( vs.mViewports );
        SetScissors( vs.mScissorRects );
    }

    auto CommandList::SetPolygonLineWidth( core::f32 width ) -> void {
        // Not supported
    }

    auto CommandList::BindIndexBuffer( IBuffer *buffer ) -> void {
        if (!buffer) {
            mDeviceContextDeferred->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
            return;
        }

        ID3D11Buffer* d3dBuffer{ buffer->GetNativeHandle(ObjectType::D3D11_Buffer) };
        const DXGI_FORMAT format{ d3d11::GetFormat(buffer->GetFormat()) };
        mDeviceContextDeferred->IASetIndexBuffer(
            d3dBuffer,
            format,
            0
        );
    }

    auto CommandList::BindVertexBuffer( const VertexBufferBinding& binding ) -> void {
        BindVertexBuffers( eastl::span<const VertexBufferBinding>{ &binding, 1 } );
    }

    auto CommandList::BindVertexBuffers( eastl::span<const VertexBufferBinding> bindings ) -> void {
        if (bindings.empty()) {
            return;
        }

        eastl::fixed_vector<ID3D11Buffer*, 8> buffers{};
        eastl::fixed_vector<UINT, 8> strides{};
        eastl::fixed_vector<UINT, 8> offsets{};

        buffers.reserve(bindings.size());
        strides.reserve(bindings.size());
        offsets.reserve(bindings.size());

        for (const auto& binding : bindings) {
            ID3D11Buffer* d3dBuffer{ binding.mBuffer->GetNativeHandle(ObjectType::D3D11_Buffer) };
            buffers.push_back(d3dBuffer);
            strides.push_back(as<UINT>(binding.mElementStride));
            offsets.push_back(as<UINT>(binding.mOffset));
        }

        mDeviceContextDeferred->IASetVertexBuffers(
            0,
            as<UINT>(buffers.size()),
            buffers.data(),
            strides.data(),
            offsets.data()
        );
    }

    auto CommandList::BindPipelineResources( const BindResourcesDescription& desc ) -> void {
        PipelineLayout* pl{ checked_cast<PipelineLayout*>( desc.mPipelineLayout ) };

        for (const auto& resourceSet : desc.mResourceSets) {
            const BindingSet* set{ checked_cast<const BindingSet*>( resourceSet.second ) };

            // For D3D11 we default to making resources visible to all stages
            set->Bind(mDeviceContextDeferred.Get(), ShaderFlagsBits::kAll);
        }
    }

    auto CommandList::Draw( const DrawArguments &args ) -> void {
        mDeviceContextDeferred->DrawInstanced( args.mVertexCount, args.mInstanceCount, args.mFirstVertex, args.mFirstInstance);
    }

    auto CommandList::BindIndirectBuffer( IBuffer *buffer ) -> void {
        mCurrentIndirectBuffer = buffer;
    }

    auto CommandList::DrawIndexed( const DrawArguments &args ) -> void {
        mDeviceContextDeferred->DrawIndexedInstanced(
                args.mIndexCount,     // IndexCountPerInstance
                args.mInstanceCount,  // InstanceCount
                args.mFirstIndex,     // StartIndexLocation
                args.mVertexOffset,   // BaseVertexLocation
                args.mFirstInstance   // StartInstanceLocation
            );
    }

    auto CommandList::DrawIndirect( u32 offset, u32 drawCount ) -> void {
        ID3D11Buffer* d3dBuffer{ mCurrentIndirectBuffer->GetNativeHandle(ObjectType::D3D11_Buffer) };
        mDeviceContextDeferred->DrawInstancedIndirect(d3dBuffer, 0);
    }

    auto CommandList::DrawIndexedIndirect( u32 offset, u32 drawCount ) -> void {
        ID3D11Buffer* d3dBuffer{ mCurrentIndirectBuffer->GetNativeHandle(ObjectType::D3D11_Buffer) };
        mDeviceContextDeferred->DrawIndexedInstancedIndirect(d3dBuffer, 0);
    }

    auto CommandList::Dispatch( u32 groupsX, u32 groupsY, u32 groupsZ ) -> void {
        mDeviceContextDeferred->Dispatch( groupsX, groupsY, groupsZ );
    }

    auto CommandList::SetPushConstants( IPipelineLayout *pipelineLayout, const void *data, size_t byteSize, ShaderFlags visibility ) -> void {

    }

    auto CommandList::SetDebugName( eastl::string_view name ) -> void {
        if (mCommandList) {
            mCommandList->SetPrivateData(WKPDID_D3DDebugObjectName,as<UINT>(name.size()), name.data() );
        }

        if (mDeviceContextDeferred) {
            mDeviceContextDeferred->SetPrivateData(WKPDID_D3DDebugObjectName,as<UINT>(name.size()), name.data() );
        }
    }

    auto CommandList::GetNativeHandle( ObjectType type ) -> Object {
        if ( type != ObjectType::D3D11_CommandList) {
            return Object( nullptr );
        }

        return Object( mCommandList.Get() );
    }

    auto CommandList::BeginDebugLabel( eastl::string_view name, Color color ) -> void {

    }

    auto CommandList::EnbDebugLabel() -> void {

    }

    CommandList::~CommandList() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto CommandList::Release() -> void {
        mIsAllocated = false;
    }

    auto CommandList::Initialize() -> void {
        auto device{ checked_cast<Device*>(mDevice)->GetDevice() };
        if (FAILED(device->CreateDeferredContext(0, &mDeviceContextDeferred))) {
            MKT_THROW_RUNTIME_ERROR("Failed to create deferred context");
        }

        mIsAllocated = true;
    }

    InputLayout::InputLayout( const InputLayoutCreateDescription& desc )
        : mDesc{ desc } {
    }

    auto InputLayout::GetNativeHandle(ObjectType type) -> Object {
        switch (type) {
            case ObjectType::D3D11_InputLayout:
                return Object(mInputLayout.Get());

            default:
                return Object(nullptr);
        }
    }

    auto InputLayout::GetNativeHandle(ObjectType type) const -> Object {
        switch (type) {
            case ObjectType::D3D11_InputLayout:
                return Object(mInputLayout.Get());

            default:
                return Object(nullptr);
        }
    }

    auto InputLayout::GetNumAttributes() const -> u32 {
        return as<u32>(mDesc.mVertexAttributeDescriptions.size());
    }

    auto InputLayout::GetAttributeDescription( u32 index ) const -> const VertexAttributeDescription & {
        MKT_ASSERT( index <  mDesc.mVertexAttributeDescriptions.size(), "Index out of range" );
        return mDesc.mVertexAttributeDescriptions[index];
    }

    InputLayout::~InputLayout() {
        if (mIsAllocated) {
            Release();
        }
    }

    auto InputLayout::Initialize() -> void {
        // https://shader-slang.org/slang/user-guide/reflection

        // slang::ProgramLayout *layout = checked_cast<Shader*>( mDesc.mShaderModule.GetRaw() )->GetProgram()->getLayout();
        //
        // auto entryPoint{ layout->getEntryPointByIndex( 0 ) };
        // SlangInt paramCount{ entryPoint->getParameterCount() };
        //
        // for ( SlangInt i{ 0 }; i < paramCount; ++i ) {
        //     auto param{ entryPoint->getParameterByIndex( i ) };
        //     auto type{ param->getType() };
        //
        //     if ( type->getKind() != slang::TypeReflection::Kind::Struct )
        //         continue;
        //
        //     // ----------------------------------------
        //     // 3. Iterate struct fields
        //     // ----------------------------------------
        //     SlangInt fieldCount{ type->getFieldCount() };
        //
        //     UINT runningOffset{ 0 };
        //
        //     for ( SlangInt f = 0; f < fieldCount; ++f ) {
        //         auto field = type->getFieldByIndex( f );
        //
        //         D3D11_INPUT_ELEMENT_DESC desc{};
        //         desc.SemanticName = param->getSemanticName();
        //         desc.SemanticIndex = param->getSemanticIndex();
        //
        //         desc.InputSlot = 0;// TODO: multi-stream support later
        //         desc.AlignedByteOffset = runningOffset;
        //
        //         desc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
        //         desc.InstanceDataStepRate = 0;
        //
        //         outDescs.push_back( desc );
        //         //
        //         // // ----------------------------------------
        //         // // 4. Compute offset
        //         // // ----------------------------------------
        //         // auto size = fieldType->getFieldByIndex(i)->;// Slang gives byte size
        //         // runningOffset += static_cast<UINT>( size );
        //     }
        // }

        mInputDescriptions.clear();
        mInputDescriptions.reserve(mDesc.mVertexAttributeDescriptions.size());

        // Semantic name -> semantic index
        ankerl::unordered_dense::map<eastl::string, u32> semantics{};
        for (auto& attr : mDesc.mVertexAttributeDescriptions) {

            const auto& binding{ mDesc.mVertexBindingDescriptions[attr.mBinding] };

            // Properly format semantics
            const auto semantic{ ParseSemantic( attr.mName ) };
            attr.mName = semantic.mName;

            u32& currIndex{ semantics[attr.mName] };
            attr.mLocation = currIndex++;

            auto& v{ mInputDescriptions.emplace_back( GetInputElementDescription(attr, binding.mRate) ) };
        }

        ID3DBlob* byteCode{ mDesc.mShaderModule->GetNativeHandle( ObjectType::D3D11_D3DBlob ) };

        if ( FAILED( checked_cast<Device*>( mDevice )->GetDevice()->CreateInputLayout(
                     mInputDescriptions.data(),
                     mInputDescriptions.size(),
                     byteCode->GetBufferPointer(),
                     byteCode->GetBufferSize(),
                     &mInputLayout ) ) ) {
            MKT_CORE_LOGGER_ERROR( "D3D11: Failed to create default vertex input layout" );
        } else {
            mIsAllocated = true;
        }
    }

    auto InputLayout::Release() -> void {
        mIsAllocated = false;
    }

    Device::Device( const GpuDeviceCreateInfo &createInfo )
        : IGpuDevice{ createInfo.mApi, createInfo.mFeaturesSupport }
    {}

    auto Device::Init() -> void {
        UINT deviceFlags{ D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_BGRA_SUPPORT };

        // Enable debug messages
#if !defined(NDEBUG)
        deviceFlags |= D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG;
#endif

#if false // Choose Adapter (Physical device)
        IDXGIFactory2 *factory{ as<Context *>( RenderSystem::Get()->GetContext() )->GetDxiFactory() };

        IDXGIAdapter* pAdapter{};
        eastl::vector<IDXGIAdapter *> vAdapters{};
        for ( UINT i{};
              factory->EnumAdapters( i, &pAdapter ) != DXGI_ERROR_NOT_FOUND;
              ++i ) {
            vAdapters.push_back( pAdapter );
        }

        // Better way of achieving this
        // https://stackoverflow.com/questions/55435230/how-to-ensure-directx-11-app-use-the-discrete-gpu-on-a-dual-gpu-laptop-with-c

        // Right now I just pick one with highest memory
        SIZE_T maxVideoMemory{};
        for (IDXGIAdapter* adapter : vAdapters) {
            DXGI_ADAPTER_DESC desc{};
            adapter->GetDesc( MKT_ADDRESSOF( desc ) );

            if (desc.DedicatedVideoMemory > maxVideoMemory) {
                maxVideoMemory = desc.DedicatedVideoMemory;
                mAdapter = adapter;
                mName = string::FromWChar( desc.Description, desc.Description + 128 );
            }
        }

        MKT_CORE_LOGGER_DEBUG( "Picked adapter: {}", mName.c_str() );
#endif

        D3D_FEATURE_LEVEL choosenDeviceFeatureLevel{};
        if (FAILED(D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            deviceFlags,
            mDeviceFeatureLevel.data(),
            as<UINT>(mDeviceFeatureLevel.size()),
            D3D11_SDK_VERSION,
            &mDevice,
            &choosenDeviceFeatureLevel,
            &mDeviceContext)))
            {
            MKT_THROW_RUNTIME_ERROR( "Failed to create device and device Context");
        }

        mDevice->QueryInterface(IID_PPV_ARGS(&mDevice3));
        mDeviceContext->QueryInterface(IID_PPV_ARGS(&mDeviceContext3));

        // Store name of main adapter
        mName = "D3D11 Device";

        mQueue = Ref<Queue>::Spawn( QueueType::eGraphics, QueueOpSupportFlagsBits::kGraphics );
        mQueue->Initialize( this );
    }

    auto Device::Shutdown() -> void {
        mQueue.Reset();
    }

    auto Device::CreateBuffer( const BufferCreateDescription &description ) -> BufferHandle {
        BufferHandle buffer{ Ref<Buffer>::Spawn( description ) };

        if ( buffer.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "CreateBuffer - Failed to allocate texture resource." );
            return BufferHandle::CreateEmpty();
        }

        buffer->Initialize( this );

        return buffer;
    }

    auto Device::CreateTexture( const TextureCreateDescription &description ) -> TextureHandle {
        TextureHandle texture{ Ref<Texture>::Spawn( description ) };

        if ( texture.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "CreateTexture - Failed to allocate texture resource." );
            return TextureHandle::CreateEmpty();
        }

        texture->Initialize( this );

        return texture;
    }

    auto Device::CreateTextureNative( ObjectType type, Object object, const TextureCreateDescription &description ) -> TextureHandle {
        return TextureHandle::CreateEmpty();
    }

    auto Device::CreateSampler( const SamplerCreateDescription &description ) -> SamplerHandle {
        SamplerHandle sampler{  Ref<Sampler>::Spawn( description ) };

        if ( sampler.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate sampler resource." );
            return SamplerHandle::CreateEmpty();
        }

        sampler->Initialize( this );

        return sampler;
    }

    auto Device::CreateAccelStructure( const AccelStructureCreateDescription &description ) -> AccelStructureHandle {
        return AccelStructureHandle::CreateEmpty();
    }

    auto Device::CreateCommandList( QueueType type ) -> CommandListHandle {
        CommandListHandle cmd{  Ref<CommandList>::Spawn( type ) };

        if ( cmd.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate command list resource." );
            return CommandListHandle::CreateEmpty();
        }

        cmd->Initialize( this );

        return cmd;
    }

    auto Device::CreateShader( const ShaderModuleCreateDescription &desc ) -> ShaderModuleHandle {
        ShaderModuleHandle shader{ Ref<Shader>::Spawn( desc ) };

        if ( shader.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "CreateShader - Failed to allocate shader resource." );
            return ShaderModuleHandle::CreateEmpty();
        }

        shader->Initialize( this );

        return shader;
    }

    auto Device::CreateInputLayout( const InputLayoutCreateDescription& desc ) -> InputLayoutHandle {
        InputLayoutHandle layout{ Ref<InputLayout>::Spawn( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return InputLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreateBindingLayout( const BindingLayoutDescription &desc ) -> BindingLayoutHandle {
        BindingLayoutHandle layout{ Ref<BindingLayout>::Spawn( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return BindingLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreatePipelineLayout( const PipelineLayoutCreateDescription& desc ) -> PipelineLayoutHandle {
        return PipelineLayoutHandle::CreateEmpty();
    }

    auto Device::CreateBindingSet( const BindingSetDescription &desc, BindingLayoutHandle layout ) -> BindingSetHandle {
        BindingSetHandle set{ Ref<BindingSet>::Spawn( desc, layout ) };

        if ( set.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return BindingSetHandle::CreateEmpty();
        }

        set->Initialize( this );

        return set;
    }

    auto Device::CreateFence( MKT_UNUSED_VAR u64 fenceInitialValue ) -> FenceHandle {
        FenceHandle fence{ Ref<Fence>::Spawn() };

        if ( fence.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate fence resource." );
            return FenceHandle::CreateEmpty();
        }

        fence->Initialize( this );

        return fence;
    }

    auto Device::UnMap( IBuffer *buffer ) -> void {
    }

    auto Device::Map( IBuffer *buffer ) -> void * {
        return nullptr;
    }

    auto Device::CreateBindlessLayout( const BindlessLayoutDescription &desc ) -> BindingLayoutHandle {
        return BindingLayoutHandle::CreateEmpty();
    }

    auto Device::CreateDescriptorTable( BindingLayoutHandle layout ) -> DescriptorTableHandle {
        return DescriptorTableHandle::CreateEmpty();
    }

    auto Device::ResizeDescriptorTable( DescriptorTableHandle descriptorTable, u32 newSize, bool keepContents ) -> bool {
        return false;
    }

    auto Device::WriteDescriptorTable( DescriptorTableHandle descriptorTable, const BindingSetItem &item ) -> bool {
        return false;
    }

    auto Device::GetQueue( QueueType type ) -> IQueue * {
        return mQueue.GetRaw();
    }

    auto Device::WaitIdle() -> void {

    }

    auto Device::CreatePipeline( const ComputePipelineDescription &description ) -> PipelineHandle {
        PipelineHandle computePipeline{ Ref<ComputePipeline>::Spawn( description ) };

        if ( computePipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate compute pipeline resource." );
            return PipelineHandle::CreateEmpty();
        }

        computePipeline->Initialize( this );

        return computePipeline;
    }

    auto Device::CreatePipeline( const GraphicsPipelineDescription &description ) -> PipelineHandle {
        PipelineHandle graphicsPipeline{ Ref<GraphicsPipeline>::Spawn( description ) };

        if ( graphicsPipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate graphics pipeline resource." );
            return PipelineHandle::CreateEmpty();
        }

        graphicsPipeline->Initialize( this );

        return graphicsPipeline;
    }

    auto Device::RunGarbageCollection() -> void {

    }

    auto Device::GetDevice() const -> ID3D11Device * {
        return mDevice.Get();
    }

    auto Device::GetDevice3() const -> ID3D11Device3 * {
        return mDevice3.Get();
    }

    auto Device::GetDeviceContext() -> ID3D11DeviceContext * {
        return mDeviceContext.Get();
    }

    auto Device::GetDeviceContext3() -> ID3D11DeviceContext3 * {
        return mDeviceContext3.Get();
    }

    auto Device::DumpErrorMessages() -> void {
        Microsoft::WRL::ComPtr<ID3D11InfoQueue> infoQueue{};
        if ( FAILED( mDevice->QueryInterface( IID_PPV_ARGS( &infoQueue ) ) ) ) {
            return;
        }

        const auto messageCount{ infoQueue->GetNumStoredMessages() };

        for ( UINT64 i{}; i < messageCount; ++i ) {
            SIZE_T messageLength{};
            infoQueue->GetMessage( i, nullptr, &messageLength );

            auto bytes{ std::make_unique<std::byte[]>( messageLength ) };
            auto *message{ reinterpret_cast<D3D11_MESSAGE *>( bytes.get() ) };

            if ( SUCCEEDED( infoQueue->GetMessage( i, message, &messageLength ) ) ) {
                switch (message->Severity ) {

                    case D3D11_MESSAGE_SEVERITY_CORRUPTION:
                        MKT_CORE_LOGGER_CRITICAL( "[D3D11Device Corruption] {}", message->pDescription );
                        break;
                    case D3D11_MESSAGE_SEVERITY_ERROR:
                        MKT_CORE_LOGGER_ERROR( "[D3D11Device Error] {}", message->pDescription );
                        break;
                    case D3D11_MESSAGE_SEVERITY_WARNING:
                        MKT_CORE_LOGGER_WARN( "[D3D11Device Warning] {}", message->pDescription );
                        break;
                    case D3D11_MESSAGE_SEVERITY_INFO:
                        MKT_CORE_LOGGER_INFO( "[D3D11Device Info] {}", message->pDescription );
                        break;
                    case D3D11_MESSAGE_SEVERITY_MESSAGE:
                        MKT_CORE_LOGGER_TRACE( "[D3D11Device Message] {}", message->pDescription );
                        break;
                }
            }
        }

        infoQueue->ClearStoredMessages();
    }

    auto Device::CreateSwapChain(platform::Window* window, Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory) -> SwapChainHandle {
        auto handle{ SwapChainHandle::Spawn(window, dxgiFactory) };
        if (!handle.IsEmpty()) {
            handle->Initialize(this);
        }

        return handle;
    }
}// namespace Mikoto

#endif
