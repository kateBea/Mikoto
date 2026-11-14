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

#include <bit>
#include <thread>
#include <cstring>

#include <EASTL/utility.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Memory/Allocator.hh>

#include <Filesystem/Path.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuAllocator.hh>
#include <Renderer/Core/RenderSystem.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#include <Platform/Windows/Windows.hh>

#include <Renderer/Rhi/D3D11/D3D11Buffer.hh>
#include <Renderer/Rhi/D3D11/D3D11Device.hh>
#include <Renderer/Rhi/D3D11/D3D11Context.hh>
#include <Renderer/Rhi/D3D11/D3D11Texture.hh>
#include <Renderer/Rhi/D3D11/D3D11Pipeline.hh>
#include <Renderer/Rhi/D3D11/Direct3D11Helpers.hh>

#if !defined( NDEBUG )
#include <dxgidebug.h>

#pragma comment( lib, "dxguid.lib" )
#endif

#include <dxgi1_4.h>

namespace mikoto::renderer::d3d11 {

    using namespace mikoto::renderer::rhi;


    auto CommandList::AcquireResource( const rhi::BufferBarrierDescription &, rhi::IQueue * ) -> void {
        // This backend has no explicit queue-family ownership to transfer.
    }

    auto CommandList::ReleaseResource( const rhi::BufferBarrierDescription &, rhi::IQueue * ) -> void {
        // This backend has no explicit queue-family ownership to transfer.
    }

    auto CommandList::AcquireResource( const rhi::TextureBarrierDescription &, rhi::IQueue * ) -> void {
        // This backend has no explicit queue-family ownership to transfer.
    }

    auto CommandList::ReleaseResource( const rhi::TextureBarrierDescription &, rhi::IQueue * ) -> void {
        // This backend has no explicit queue-family ownership to transfer.
    }

    auto CommandList::Reset( rhi::IQueryPool * ) -> void {
        MKT_THROW_RUNTIME_ERROR( "Reset is unsupported by this backend." );
    }

    auto CommandList::BeginQuery( rhi::IQueryPool *, core::u32 ) -> void {
        MKT_THROW_RUNTIME_ERROR( "BeginQuery is unsupported by this backend." );
    }

    auto CommandList::EndQuery( rhi::IQueryPool *, core::u32 ) -> void {
        MKT_THROW_RUNTIME_ERROR( "EndQuery is unsupported by this backend." );
    }

    auto CommandList::WriteTimestamp( rhi::IQueryPool *, core::u32 ) -> void {
        MKT_THROW_RUNTIME_ERROR( "WriteTimestamp is unsupported by this backend." );
    }

    auto CommandList::Resolve( rhi::IQueryPool *, core::u32, core::u32 ) -> void {
        MKT_THROW_RUNTIME_ERROR( "Resolve is unsupported by this backend." );
    }

    auto CommandList::Build( rhi::IAccelerationStructure * ) -> void {
        MKT_THROW_RUNTIME_ERROR( "Build is unsupported by this backend." );
    }


    auto CommandList::SetTransition( const renderer::rhi::TransitionDescription &description ) -> void {
        MKT_BEGIN_PROFILER();

        for ( const auto &transition: description.mBuffers ) {
            SetTransition( transition.mBuffer, transition.mState );
        }

        for ( const auto &transition: description.mTextures ) {
            SetTransition( transition.mTexture, transition.mState );
        }
    }

    auto CommandList::SetBarrier( const renderer::rhi::BarrierDescription & ) -> void {
        // D3D11 has implicit resource hazards; explicit enhanced barriers are a no-op.
    }

    auto Fence::GetCompletionValue() const -> core::u64 {
        MKT_BEGIN_PROFILER();
        return 0;
    }

    auto Fence::Signal( core::u64 fenceValue ) -> bool {
        MKT_BEGIN_PROFILER();
        return true;
    }

    auto Fence::Wait( core::u64 fenceValue, core::u64 timeoutMs ) -> bool {
        MKT_BEGIN_PROFILER();
        return true;
    }

    Fence::~Fence() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto Fence::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = true;
    }

    auto Fence::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    BindingLayout::BindingLayout( const renderer::rhi::BindingLayoutDescription &desc )
        : mBindingLayoutDesc{ desc } {
    }

    BindingLayout::BindingLayout( const renderer::rhi::DescriptorTableLayoutDescription &desc )
        : mBindlessLayoutDesc{ desc } {
    }

    auto BindingLayout::IsBindless() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsBindless;
    }

    auto BindingLayout::GetRegisterSpace() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return 0;//TODO
    }

    BindingLayout::~BindingLayout() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto BindingLayout::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = true;
    }

    auto BindingLayout::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    BindingTable::BindingTable( const renderer::rhi::BindingTableDescription &desc, renderer::rhi::BindingLayoutHandle layout )
        : mBindingLayout{ eastl::move( layout ) }, mBindingDescription{ desc } {
    }

    BindingTable::~BindingTable() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto BindingTable::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        mResolvedBindings.clear();
        mResolvedBindings.resize( mBindingDescription.mBindings.size() );

        for ( core::usize i{}; i < mBindingDescription.mBindings.size(); ++i ) {
            const auto &item = mBindingDescription.mBindings[i];
            auto &out = mResolvedBindings[i];

            switch ( item.mResourceType ) {
                case renderer::rhi::ResourceType::eTexture_SRV:
                    out.mSrv = core::checked_cast<Texture *>( item.mResource )->GetNativeHandle( renderer::rhi::ObjectType::D3D11_SRV );
                    break;

                case renderer::rhi::ResourceType::eConstantBuffer:
                    out.mConstantBuffer = core::checked_cast<Buffer *>( item.mResource )->GetNativeHandle( renderer::rhi::ObjectType::D3D11_Buffer );
                    break;

                case renderer::rhi::ResourceType::eSampler:
                    out.mSampler = core::checked_cast<Sampler *>( item.mResource )->GetNativeHandle( renderer::rhi::ObjectType::D3D11_Sampler );
                    break;

                default:
                    break;
            }
        }

        mIsAllocated = true;
    }

    auto BindingTable::Bind( ID3D11DeviceContext *ctx, renderer::rhi::ShaderFlags visibility ) const -> void {
        MKT_BEGIN_PROFILER();

        for ( core::usize i{ 0 }; i < mBindingDescription.mBindings.size(); ++i ) {
            const auto &item{ mBindingDescription.mBindings[i] };
            const auto &data{ mResolvedBindings[i] };

            // Each resource type has its own register slots. Use the slot declared by the binding.
            // https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-pssetshaderresources
            const UINT slot{ item.mBindingIndex };

            auto bindSRV = [&]( auto fn ) {
                if ( data.mSrv ) {
                    fn( slot, 1, &data.mSrv );
                }
            };

            auto bindCB = [&]( auto fn ) {
                if ( data.mConstantBuffer ) {
                    fn( slot, 1, &data.mConstantBuffer );
                }
            };

            auto bindSampler = [&]( auto fn ) {
                if ( data.mSampler ) {
                    fn( slot, 1, &data.mSampler );
                }
            };

            switch ( item.mResourceType ) {
                case renderer::rhi::ResourceType::eTexture_SRV:
                    if ( visibility & renderer::rhi::ShaderFlagsBits::Vertex ) {
                        bindSRV( [&]( auto... args ) {
                            ctx->VSSetShaderResources( args... );
                        } );
                    }

                    if ( visibility & renderer::rhi::ShaderFlagsBits::Pixel ) {
                        bindSRV( [&]( auto... args ) {
                            ctx->PSSetShaderResources( args... );
                        } );
                    }

                    if ( visibility & renderer::rhi::ShaderFlagsBits::Geometry ) {
                        bindSRV( [&]( auto... args ) {
                            ctx->GSSetShaderResources( args... );
                        } );
                    }

                    if ( visibility & renderer::rhi::ShaderFlagsBits::Hull ) {
                        bindSRV( [&]( auto... args ) {
                            ctx->HSSetShaderResources( args... );
                        } );
                    }

                    if ( visibility & renderer::rhi::ShaderFlagsBits::Domain ) {
                        bindSRV( [&]( auto... args ) {
                            ctx->DSSetShaderResources( args... );
                        } );
                    }

                    if ( visibility & renderer::rhi::ShaderFlagsBits::Compute ) {
                        bindSRV( [&]( auto... args ) {
                            ctx->CSSetShaderResources( args... );
                        } );
                    }

                    break;

                case renderer::rhi::ResourceType::eConstantBuffer:
                    if ( visibility & renderer::rhi::ShaderFlagsBits::Vertex ) {
                        bindCB( [&]( auto... args ) {
                            ctx->VSSetConstantBuffers( args... );
                        } );
                    }

                    if ( visibility & renderer::rhi::ShaderFlagsBits::Pixel ) {
                        bindCB( [&]( auto... args ) {
                            ctx->PSSetConstantBuffers( args... );
                        } );
                    }

                    if ( visibility & renderer::rhi::ShaderFlagsBits::Geometry ) {
                        bindCB( [&]( auto... args ) {
                            ctx->GSSetConstantBuffers( args... );
                        } );
                    }

                    if ( visibility & renderer::rhi::ShaderFlagsBits::Hull ) {
                        bindCB( [&]( auto... args ) {
                            ctx->HSSetConstantBuffers( args... );
                        } );
                    }

                    if ( visibility & renderer::rhi::ShaderFlagsBits::Domain ) {
                        bindCB( [&]( auto... args ) {
                            ctx->DSSetConstantBuffers( args... );
                        } );
                    }

                    if ( visibility & renderer::rhi::ShaderFlagsBits::Compute ) {
                        bindCB( [&]( auto... args ) {
                            ctx->CSSetConstantBuffers( args... );
                        } );
                    }

                    break;

                case renderer::rhi::ResourceType::eSampler:
                    if ( visibility & renderer::rhi::ShaderFlagsBits::Vertex ) {
                        bindSampler( [&]( auto... args ) {
                            ctx->VSSetSamplers( args... );
                        } );
                    }

                    if ( visibility & renderer::rhi::ShaderFlagsBits::Pixel ) {
                        bindSampler( [&]( auto... args ) {
                            ctx->PSSetSamplers( args... );
                        } );
                    }

                    if ( visibility & renderer::rhi::ShaderFlagsBits::Geometry ) {
                        bindSampler( [&]( auto... args ) {
                            ctx->GSSetSamplers( args... );
                        } );
                    }

                    if ( visibility & renderer::rhi::ShaderFlagsBits::Hull ) {
                        bindSampler( [&]( auto... args ) {
                            ctx->HSSetSamplers( args... );
                        } );
                    }

                    if ( visibility & renderer::rhi::ShaderFlagsBits::Domain ) {
                        bindSampler( [&]( auto... args ) {
                            ctx->DSSetSamplers( args... );
                        } );
                    }

                    if ( visibility & renderer::rhi::ShaderFlagsBits::Compute ) {
                        bindSampler( [&]( auto... args ) {
                            ctx->CSSetSamplers( args... );
                        } );
                    }

                    break;

                default:
                    break;
            }
        }
    }

    auto BindingTable::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    PipelineLayout::PipelineLayout( const renderer::rhi::PipelineLayoutCreateDescription &desc )
        : mDesc{ desc } {
    }

    auto PipelineLayout::GetDescription() const -> const renderer::rhi::PipelineLayoutCreateDescription & {
        MKT_BEGIN_PROFILER();
        return mDesc;
    }

    PipelineLayout::~PipelineLayout() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto PipelineLayout::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = true;
    }

    auto PipelineLayout::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    Queue::Queue( renderer::rhi::QueueType type, renderer::rhi::QueueOpSupportFlags flags )
        : mType{ type }, mOpSupportFlags{ flags } {
    }

    auto Queue::ExecuteCommandLists( const renderer::rhi::SubmitInfo &submitInfo ) -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mSubmitMutex };
        PendingResources pending{};

        for ( auto command: submitInfo.mCommands ) {
            core::checked_cast<CommandList *>( command.GetPtr() )->ValidateSubmission();
        }

        const D3D11_QUERY_DESC description{ D3D11_QUERY_EVENT, 0 };

        if ( FAILED( core::checked_cast<Device *>( mDevice )->GetDevice()->CreateQuery( &description, &pending.mCompletion ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot track D3D11 submission completion." );
        }

        mPendingResources.reserve( mPendingResources.size() + 1 );

        for ( auto command: submitInfo.mCommands ) {
            ID3D11CommandList *native{ command->GetNativeHandle( renderer::rhi::ObjectType::D3D11_CommandList ) };
            MKT_ASSERT( native, "Command list is null" );
            mDeviceContext->ExecuteCommandList( native, TRUE );
        }

        mDeviceContext->End( pending.mCompletion.Get() );
        mPendingResources.push_back( eastl::move( pending ) );

        for ( auto command: submitInfo.mCommands ) {
            core::checked_cast<CommandList *>( command.GetPtr() )->MarkSubmitted();
        }
    }

    auto Queue::CollectGarbage( bool wait ) -> void {
        std::lock_guard lock{ mSubmitMutex };

        if ( wait ) {
            mDeviceContext->Flush();
        }

        for ( auto it{ mPendingResources.begin() }; it != mPendingResources.end(); ) {
            HRESULT result{};
            do {
                result = mDeviceContext->GetData( it->mCompletion.Get(), nullptr, 0, D3D11_ASYNC_GETDATA_DONOTFLUSH );

                if ( wait && result == S_FALSE ) {
                    std::this_thread::yield();
                }
            } while ( wait && result == S_FALSE );

            if ( FAILED( result ) ) {
                MKT_THROW_RUNTIME_ERROR( "Failed querying D3D11 submission completion." );
            }

            if ( result == S_FALSE ) {
                break;
            }

            it = mPendingResources.erase( it );
        }
    }

    Queue::~Queue() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto Queue::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    auto Queue::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        Device *device{ core::checked_cast<Device *>( mDevice ) };
        mDeviceContext = device->GetDeviceContext();
        mDeviceContext3 = device->GetDeviceContext3();

        mIsAllocated = true;
    }

    CommandList::CommandList( renderer::rhi::QueueType type )
        : mQueueType{ type } {
    }

    auto CommandList::Open( const renderer::rhi::CommandListBeginDescription &desc ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mRecording || mExecutable ) {
            MKT_THROW_RUNTIME_ERROR( "The previous D3D11 recording has not been submitted." );
        }

        ClearState();

        mRecording = true;
    }

    auto CommandList::Close() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mRecording ) {
            MKT_THROW_RUNTIME_ERROR( "D3D11 command list is not recording." );
        }

        MKT_ASSERT( mDeviceContextDeferred, "Deferred context is null" );
        mCommandList.Reset();// release previous one

        if ( FAILED( mDeviceContextDeferred->FinishCommandList( FALSE, &mCommandList ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to finish D3D11 command list." );
        }

        mRecording = false;
        mExecutable = true;
    }

    auto CommandList::ValidateSubmission() const -> void {
        if ( !mExecutable ) {
            MKT_THROW_RUNTIME_ERROR( "The command recording must be closed and submitted only once." );
        }
    }

    auto CommandList::MarkSubmitted() -> void {
        mExecutable = false;
    }

    auto CommandList::RecordBarrier( const renderer::rhi::BufferBarrierDescription &barrier ) -> void {
        // D3D11 drivers control sync
    }

    auto CommandList::RecordBarrier( const renderer::rhi::TextureBarrierDescription &barrier ) -> void {
        // D3D11 drivers control sync
    }

    auto CommandList::SetBarrier( const renderer::rhi::BufferBarrierDescription &barrier ) -> void {
        // D3D11 drivers control sync
    }

    auto CommandList::SetBarrier( const renderer::rhi::TextureBarrierDescription &barrier ) -> void {
        // D3D11 drivers control sync
    }

    auto CommandList::RecordTransition( renderer::rhi::IBuffer *buffer, renderer::rhi::ResourceStates stateBits ) -> void {
        // D3D11 drivers control sync
    }

    auto CommandList::RecordTransition( renderer::rhi::ITexture *texture, renderer::rhi::ResourceStates stateBits ) -> void {
        // D3D11 drivers control sync
    }

    auto CommandList::SetTransition( renderer::rhi::IBuffer *buffer, renderer::rhi::ResourceStates stateBits ) -> void {
        // D3D11 drivers control sync
    }

    auto CommandList::SetTransition( renderer::rhi::ITexture *texture, renderer::rhi::ResourceStates stateBits ) -> void {
        MKT_BEGIN_PROFILER();
        core::checked_cast<Texture *>( texture )->EnableUsage( stateBits );
    }

    auto CommandList::CommitBarriers() -> void {
        // D3D11 drivers control sync
    }

    auto CommandList::SetEnableAutomaticBarriers( bool enable ) -> void {
        MKT_BEGIN_PROFILER();
        mEnableAutomaticBarriers = enable;
    }

    auto CommandList::SetClearColor( renderer::rhi::ITexture *renderTarget, renderer::rhi::Color color ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !renderTarget ) {
            return;
        }

        ID3D11RenderTargetView *rtv{ *core::checked_cast<Texture *>( renderTarget ) };
        eastl::array float4Color{ color.mR, color.mG, color.mB, color.mA };

        mDeviceContextDeferred->ClearRenderTargetView( rtv, float4Color.data() );
    }

    auto CommandList::ClearState() -> void {
        MKT_BEGIN_PROFILER();
        mDeviceContextDeferred->ClearState();
        MarkSubmitted();

        mCurrentGraphicsStateValid = false;
        mCurrentComputeStateValid = false;

        mCurrentGraphicsPipeline = nullptr;
        mCurrentVertexBuffers.resize( 0 );
        mCurrentIndexBuffer = nullptr;
        mCurrentComputePipeline = nullptr;
        mCurrentIndirectBuffer = nullptr;
        mCurrentClearColor = renderer::rhi::Color{};
    }

    auto CommandList::Write( renderer::rhi::IBuffer *src, renderer::rhi::ITexture *dest ) -> void {
        MKT_BEGIN_PROFILER();
        ID3D11Resource *srcBuffer{ *core::checked_cast<Buffer *>( src ) };
        ID3D11Resource *texture{ *core::checked_cast<Texture *>( dest ) };

        mDeviceContextDeferred->CopySubresourceRegion( texture, 0, 0, 0, 0, srcBuffer, 0, nullptr );
    }

    auto CommandList::Write( renderer::rhi::ITexture *target, const void *data, core::usize byteSize ) -> void {
        MKT_BEGIN_PROFILER();
        //https://eatplayhate.me/2013/09/29/d3d11-texture-update-costs/

        const UINT width{ target->GetWidth() };
        const UINT height{ target->GetHeight() };
        const UINT bytesPerPixel{ GetBytesPerPixel( target->GetFormat() ) };

        const UINT rowPitch{ width * bytesPerPixel };

        MKT_ASSERT( byteSize >= core::as<core::usize>( rowPitch ) * height, "Insufficient texture data" );

        ID3D11Resource *texture{ *core::checked_cast<Texture *>( target ) };
        mDeviceContextDeferred->UpdateSubresource( texture, 0, nullptr, data, rowPitch, 0 );
    }

    auto CommandList::Copy( renderer::rhi::ITexture *src, const renderer::rhi::TextureSlice &srcSlice, renderer::rhi::ITexture *dest, const renderer::rhi::TextureSlice &destSlice ) -> void {
    }

    auto CommandList::Resolve( renderer::rhi::ITexture *src, const renderer::rhi::TextureSlice &srcSlice, renderer::rhi::ITexture *dest, const renderer::rhi::TextureSlice &destSlice ) -> void {
    }

    auto CommandList::Write( renderer::rhi::IBuffer *target, core::usize destOffset, const void *data, core::usize byteSize ) -> void {
    }

    auto CommandList::Write( renderer::rhi::IBuffer *target, const void *data, core::usize byteSize ) -> void {
        MKT_BEGIN_PROFILER();
        ID3D11Buffer *buffer{ target->GetNativeHandle( renderer::rhi::ObjectType::D3D11_Buffer ) };
        D3D11_BUFFER_DESC description{};
        buffer->GetDesc( &description );

        if ( byteSize == 0 || byteSize > description.ByteWidth ) {
            return;
        }

        // Mesh uploads target DEFAULT buffers. Dynamic uniform buffers use DISCARD.
        // https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-updatesubresource
        if ( description.Usage == D3D11_USAGE_DEFAULT ) {
            if ( byteSize == description.ByteWidth ) {
                mDeviceContextDeferred->UpdateSubresource( buffer, 0, nullptr, data, 0, 0 );
            }
            else if ( !( description.BindFlags & D3D11_BIND_CONSTANT_BUFFER ) ) {
                const D3D11_BOX range{ 0, 0, 0, core::as<UINT>( byteSize ), 1, 1 };
                mDeviceContextDeferred->UpdateSubresource( buffer, 0, &range, data, 0, 0 );
            }

            return;
        }

        D3D11_MAPPED_SUBRESOURCE mappedResource{};
        if ( FAILED( mDeviceContextDeferred->Map( buffer, 0, D3D11_MAP::D3D11_MAP_WRITE_DISCARD, 0, &mappedResource ) ) ) {
            MKT_ASSERT( false, "Failed to map buffer" );
        }
        else {
            std::memcpy( mappedResource.pData, data, byteSize );
            mDeviceContextDeferred->Unmap( buffer, 0 );
        }
    }

    auto CommandList::Copy( renderer::rhi::IBuffer *src, renderer::rhi::IBuffer *dest ) -> void {
    }

    auto CommandList::Copy( renderer::rhi::IBuffer *src, renderer::rhi::IBuffer *dest, core::usize destOffset ) -> void {
    }

    auto CommandList::Copy( renderer::rhi::IBuffer *dest, renderer::rhi::ITexture *src ) -> void {
    }

    auto CommandList::Copy( rhi::IBuffer *dest, rhi::ITexture *src, const renderer::rhi::TextureSlice &srcSlice ) -> void {
    }

    auto CommandList::BeginRenderPass( renderer::rhi::RenderPassDescription &state ) -> void {
        MKT_BEGIN_PROFILER();
        // Render targets
        eastl::fixed_vector<ID3D11RenderTargetView *, renderer::rhi::kMaxRenderTargets> renderTargetViews{};

        for ( const auto &v: state.mCurrentRenderTargets ) {
            auto &rtv{ renderTargetViews.emplace_back( core::as<ID3D11RenderTargetView *>( v.mRenderTarget->GetNativeHandle( renderer::rhi::ObjectType::D3D11_RTV ) ) ) };

            const eastl::array clearColor{ v.mClearColor.mR, v.mClearColor.mG, v.mClearColor.mB, v.mClearColor.mA };
            mDeviceContextDeferred->ClearRenderTargetView( rtv, clearColor.data() );
        }

        ID3D11DepthStencilView *dsv{ nullptr };

        if ( !state.mDepthTarget.mRenderTarget.IsEmpty() ) {
            dsv = state.mDepthTarget.mRenderTarget->GetNativeHandle( renderer::rhi::ObjectType::D3D11_DSV );
            mDeviceContextDeferred->ClearDepthStencilView( dsv, D3D11_CLEAR_FLAG::D3D11_CLEAR_DEPTH, 1.0f, 0 );
        }

        mDeviceContextDeferred->OMSetRenderTargets(
            core::as<UINT>( renderTargetViews.size() ),
            renderTargetViews.data(),
            dsv );
    }

    auto CommandList::EndRenderPass() -> void {
        MKT_BEGIN_PROFILER();
        // Unbind all render targets + depth
        mDeviceContextDeferred->OMSetRenderTargets( 0, nullptr, nullptr );
    }

    auto CommandList::BindPipeline( renderer::rhi::IPipeline *pipeline ) -> void {
        MKT_BEGIN_PROFILER();

        if ( pipeline->GetPipelineType() != renderer::rhi::PipelineType::eGraphics ) {
            return;
        }

        GraphicsPipeline *d3d11Pipeline{ core::checked_cast<GraphicsPipeline *>( pipeline ) };
        const auto &description{ d3d11Pipeline->GetDescription() };

        // Buffers and input layout
        ID3D11InputLayout *inputLayout{ description.mInputLayout
            ? description.mInputLayout->GetNativeHandle( renderer::rhi::ObjectType::D3D11_InputLayout )
            : renderer::rhi::Object{} };
        mDeviceContextDeferred->IASetInputLayout( inputLayout );

        // Primitive topology
        mDeviceContextDeferred->IASetPrimitiveTopology( GetPrimitiveTopology( description.mPrimitiveTopology ) );

        // Pipeline states state
        ID3D11BlendState *bs{ d3d11Pipeline->GetNativeHandle( renderer::rhi::ObjectType::D3D11_BlendState ) };
        ID3D11RasterizerState *rs{ d3d11Pipeline->GetNativeHandle( renderer::rhi::ObjectType::D3D11_RasterizerState ) };
        ID3D11DepthStencilState *dss{ d3d11Pipeline->GetNativeHandle( renderer::rhi::ObjectType::D3D11_DepthStencilState ) };

        mDeviceContextDeferred->RSSetState( rs );
        mDeviceContextDeferred->OMSetBlendState( bs, nullptr, 0xFFFFFFFF );
        mDeviceContextDeferred->OMSetDepthStencilState( dss, 0 );

        // Shaders
        MKT_ASSERT( description.mShaders.contains( renderer::rhi::ShaderType::eVertex ), "No vertex shader found" );

        auto vertexShaderHandle{ description.mShaders.at( renderer::rhi::ShaderType::eVertex ) };
        auto pixelShaderHandle{ description.mShaders.at( renderer::rhi::ShaderType::ePixel ) };

        ID3D11VertexShader *vertexShader{ vertexShaderHandle->GetNativeHandle( renderer::rhi::ObjectType::D3D11_Shader ) };
        ID3D11PixelShader *pixelShader{ pixelShaderHandle->GetNativeHandle( renderer::rhi::ObjectType::D3D11_Shader ) };

        mDeviceContextDeferred->VSSetShader(
            vertexShader,
            nullptr,
            0 );

        mDeviceContextDeferred->PSSetShader(
            pixelShader,
            nullptr,
            0 );
    }

    auto CommandList::SetViewport( eastl::span<const renderer::rhi::Viewport> viewports ) -> void {
        MKT_BEGIN_PROFILER();
        // Viewports
        eastl::fixed_vector<D3D11_VIEWPORT, renderer::rhi::kMaxViewports> d3d11Viewports{};

        for ( const auto &v: viewports ) {
            d3d11Viewports.emplace_back( D3D11_VIEWPORT{
                .TopLeftX = v.mMinX,
                .TopLeftY = v.mMinY,
                .Width = v.GetWidth(),
                .Height = v.GetHeight(),
                .MinDepth = v.mMinZ,
                .MaxDepth = v.mMaxZ } );
        }

        mDeviceContextDeferred->RSSetViewports(
            core::as<UINT>( d3d11Viewports.size() ),
            d3d11Viewports.data() );
    }

    auto CommandList::SetScissors( eastl::span<const renderer::rhi::Rect> scissorRects ) -> void {
        MKT_BEGIN_PROFILER();
        eastl::fixed_vector<D3D11_RECT, renderer::rhi::kMaxViewports> d3d11Rects{};

        for ( const auto &r: scissorRects ) {
            d3d11Rects.emplace_back( D3D11_RECT{
                .left = r.mMinX,
                .top = r.mMinY,
                .right = r.mMaxX,
                .bottom = r.mMaxY } );
        }

        mDeviceContextDeferred->RSSetScissorRects(
            core::as<UINT>( d3d11Rects.size() ),
            d3d11Rects.data() );
    }

    auto CommandList::SetViewportState( const renderer::rhi::ViewportState &vs ) -> void {
        MKT_BEGIN_PROFILER();
        SetViewport( vs.mViewports );
        SetScissors( vs.mScissorRects );
    }

    auto CommandList::SetPolygonLineWidth( core::f32 width ) -> void {
        // Not supported
    }

    auto CommandList::BindIndexBuffer( renderer::rhi::IBuffer *buffer ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !buffer ) {
            mDeviceContextDeferred->IASetIndexBuffer( nullptr, DXGI_FORMAT_UNKNOWN, 0 );
            return;
        }

        ID3D11Buffer *d3dBuffer{ buffer->GetNativeHandle( renderer::rhi::ObjectType::D3D11_Buffer ) };
        const DXGI_FORMAT format{ d3d11::GetFormat( buffer->GetFormat() ) };
        mDeviceContextDeferred->IASetIndexBuffer(
            d3dBuffer,
            format,
            0 );
    }

    auto CommandList::BindVertexBuffer( const renderer::rhi::VertexBufferBinding &binding ) -> void {
        MKT_BEGIN_PROFILER();
        BindVertexBuffers( eastl::span<const renderer::rhi::VertexBufferBinding>{ &binding, 1 } );
    }

    auto CommandList::BindVertexBuffers( eastl::span<const renderer::rhi::VertexBufferBinding> bindings ) -> void {
        MKT_BEGIN_PROFILER();

        if ( bindings.empty() ) {
            return;
        }

        eastl::fixed_vector<ID3D11Buffer *, 8> buffers{};
        eastl::fixed_vector<UINT, 8> strides{};
        eastl::fixed_vector<UINT, 8> offsets{};

        buffers.reserve( bindings.size() );
        strides.reserve( bindings.size() );
        offsets.reserve( bindings.size() );

        for ( const auto &binding: bindings ) {
            ID3D11Buffer *d3dBuffer{ binding.mBuffer->GetNativeHandle( renderer::rhi::ObjectType::D3D11_Buffer ) };
            buffers.push_back( d3dBuffer );
            strides.push_back( core::as<UINT>( binding.mElementStride ) );
            offsets.push_back( core::as<UINT>( binding.mOffset ) );
        }

        mDeviceContextDeferred->IASetVertexBuffers(
            0,
            core::as<UINT>( buffers.size() ),
            buffers.data(),
            strides.data(),
            offsets.data() );
    }

    auto CommandList::BindPipelineResources( const renderer::rhi::BindResourcesDescription &desc ) -> void {
        MKT_BEGIN_PROFILER();
        PipelineLayout *pl{ core::checked_cast<PipelineLayout *>( desc.mPipelineLayout ) };

        for ( const auto &resourceSet: desc.mResourceSets ) {
            const BindingTable *set{ core::checked_cast<const BindingTable *>( resourceSet.second ) };

            // For D3D11 we default to making resources visible to all stages
            set->Bind( mDeviceContextDeferred.Get(), renderer::rhi::ShaderFlagsBits::All );
        }
    }

    auto CommandList::Draw( const renderer::rhi::DrawArguments &args ) -> void {
        MKT_BEGIN_PROFILER();
        mDeviceContextDeferred->DrawInstanced( args.mVertexCount, args.mInstanceCount, args.mFirstVertex, args.mFirstInstance );
    }

    auto CommandList::BindIndirectBuffer( renderer::rhi::IBuffer *buffer ) -> void {
        MKT_BEGIN_PROFILER();
        mCurrentIndirectBuffer = buffer;
    }

    auto CommandList::DrawIndexed( const renderer::rhi::DrawArguments &args ) -> void {
        MKT_BEGIN_PROFILER();
        mDeviceContextDeferred->DrawIndexedInstanced(
            args.mIndexCount,   // IndexCountPerInstance
            args.mInstanceCount,// InstanceCount
            args.mFirstIndex,   // StartIndexLocation
            args.mVertexOffset, // BaseVertexLocation
            args.mFirstInstance // StartInstanceLocation
        );
    }

    auto CommandList::DrawIndirect( core::u32 offset, core::u32 drawCount ) -> void {
        MKT_BEGIN_PROFILER();
        ID3D11Buffer *d3dBuffer{ mCurrentIndirectBuffer->GetNativeHandle( renderer::rhi::ObjectType::D3D11_Buffer ) };
        mDeviceContextDeferred->DrawInstancedIndirect( d3dBuffer, 0 );
    }

    auto CommandList::DrawIndexedIndirect( core::u32 offset, core::u32 drawCount ) -> void {
        MKT_BEGIN_PROFILER();
        ID3D11Buffer *d3dBuffer{ mCurrentIndirectBuffer->GetNativeHandle( renderer::rhi::ObjectType::D3D11_Buffer ) };
        mDeviceContextDeferred->DrawIndexedInstancedIndirect( d3dBuffer, 0 );
    }

    auto CommandList::BindShaderTable( rhi::IShaderTable * ) -> void {
        MKT_THROW_RUNTIME_ERROR( "D3D11 does not support ray-tracing shader tables." );
    }

    auto CommandList::DispatchRays( const rhi::RayTracingDispatchDescription & ) -> void {
        MKT_THROW_RUNTIME_ERROR( "D3D11 does not support ray-tracing dispatch." );
    }

    auto CommandList::Dispatch( core::u32 groupsX, core::u32 groupsY, core::u32 groupsZ ) -> void {
        MKT_BEGIN_PROFILER();
        mDeviceContextDeferred->Dispatch( groupsX, groupsY, groupsZ );
    }

    auto CommandList::SetPushConstants( renderer::rhi::IPipelineLayout *pipelineLayout, const void *data, core::usize byteSize, renderer::rhi::ShaderFlags visibility ) -> void {
    }

    auto CommandList::SetDebugName( eastl::string_view name ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mCommandList ) {
            mCommandList->SetPrivateData( WKPDID_D3DDebugObjectName, core::as<UINT>( name.size() ), name.data() );
        }

        if ( mDeviceContextDeferred ) {
            mDeviceContextDeferred->SetPrivateData( WKPDID_D3DDebugObjectName, core::as<UINT>( name.size() ), name.data() );
        }
    }

    auto CommandList::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        if ( type != renderer::rhi::ObjectType::D3D11_CommandList ) {
            return renderer::rhi::Object( nullptr );
        }

        return renderer::rhi::Object( mCommandList.Get() );
    }

    auto CommandList::BeginDebugLabel( eastl::string_view name, renderer::rhi::Color color ) -> void {
    }

    auto CommandList::EndDebugLabel() -> void {
    }

    CommandList::~CommandList() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto CommandList::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    auto CommandList::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        auto device{ core::checked_cast<Device *>( mDevice )->GetDevice() };

        if ( FAILED( device->CreateDeferredContext( 0, &mDeviceContextDeferred ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create deferred context" );
        }

        mIsAllocated = true;
    }

    InputLayout::InputLayout( const renderer::rhi::InputLayoutCreateDescription &desc )
        : mDesc{ desc } {
    }

    auto InputLayout::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::D3D11_InputLayout:
                return renderer::rhi::Object( mInputLayout.Get() );

            default:
                return renderer::rhi::Object( nullptr );
        }
    }

    auto InputLayout::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        MKT_BEGIN_PROFILER();

        switch ( type ) {
            case renderer::rhi::ObjectType::D3D11_InputLayout:
                return renderer::rhi::Object( mInputLayout.Get() );

            default:
                return renderer::rhi::Object( nullptr );
        }
    }

    auto InputLayout::GetNumAttributes() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return core::as<core::u32>( mDesc.mVertexAttributeDescriptions.size() );
    }

    auto InputLayout::GetAttributeDescription( core::u32 index ) const -> const renderer::rhi::VertexAttributeDescription & {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( index < mDesc.mVertexAttributeDescriptions.size(), "Index out of range" );
        return mDesc.mVertexAttributeDescriptions[index];
    }

    InputLayout::~InputLayout() {
        MKT_BEGIN_PROFILER();

        if ( mIsAllocated ) {
            Destroy();
        }
    }

    auto InputLayout::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        // https://shader-slang.org/slang/user-guide/reflection

        // slang::ProgramLayout *layout = core::checked_cast<Shader*>( mDesc.mShaderModule.GetRaw() )->GetProgram()->getLayout();
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
        mInputDescriptions.reserve( mDesc.mVertexAttributeDescriptions.size() );

        // Semantic name -> semantic index
        ankerl::unordered_dense::map<eastl::string, core::u32> semantics{};

        for ( auto &attr: mDesc.mVertexAttributeDescriptions ) {

            const auto &binding{ mDesc.mVertexBindingDescriptions[attr.mBinding] };

            // Properly format semantics
            const auto semantic{ ParseSemantic( attr.mName ) };
            attr.mName = semantic.mName;

            core::u32 &currIndex{ semantics[attr.mName] };
            attr.mLocation = currIndex++;

            auto &v{ mInputDescriptions.emplace_back( GetInputElementDescription( attr, binding.mRate ) ) };
        }

        ID3DBlob *byteCode{ mDesc.mShaderModule->GetNativeHandle( renderer::rhi::ObjectType::D3D11_D3DBlob ) };

        if ( FAILED( core::checked_cast<Device *>( mDevice )->GetDevice()->CreateInputLayout(
                 mInputDescriptions.data(),
                 mInputDescriptions.size(),
                 byteCode->GetBufferPointer(),
                 byteCode->GetBufferSize(),
                 &mInputLayout ) ) ) {
            MKT_CORE_LOGGER_ERROR( "D3D11: Failed to create default vertex input layout" );
        }
        else {
            mIsAllocated = true;
        }
    }

    auto InputLayout::Destroy() -> void {
        MKT_BEGIN_PROFILER();
        mIsAllocated = false;
    }

    Device::Device( const renderer::rhi::GpuDeviceCreateInfo &createInfo )
        : mApi{ createInfo.mApi }, mDeviceType{ createInfo.mDeviceType }, mFeatureSupportFlags{ createInfo.mFeatureSupportFlags } {
    }

    auto Device::GetGraphicsApi() const -> renderer::rhi::GraphicsAPI {
        MKT_BEGIN_PROFILER();
        return mApi;
    }

    auto Device::IsInitialized() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsInitialized;
    }

    auto Device::IsGraphicsApi( renderer::rhi::GraphicsAPI api ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mApi == api;
    }

    auto Device::GetDeviceName() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mDeviceName;
    }

    auto Device::GetVendorID() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mVendorID;
    }

    auto Device::GetDriverVersion() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return mDriverVersion;
    }

    auto Device::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        UINT deviceFlags{ D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_BGRA_SUPPORT };

        // Enable debug messages
#if !defined( NDEBUG )
        deviceFlags |= D3D11_CREATE_DEVICE_FLAG::D3D11_CREATE_DEVICE_DEBUG;
#endif

#if false// Choose Adapter (Physical device)
        IDXGIFactory2 *factory{ core::as<Context *>( RenderSystem::Get()->GetContext() )->GetDxiFactory() };

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
        core::usize maxVideoMemory{};

        for (IDXGIAdapter* adapter : vAdapters) {
            DXGI_ADAPTER_DESC desc{};
            adapter->GetDesc( MKT_ADDRESSOF( desc ) );

            if (desc.DedicatedVideoMemory > maxVideoMemory) {
                maxVideoMemory = desc.DedicatedVideoMemory;
                mAdapter = adapter;
                mDeviceName = string::FromWChar( desc.Description, desc.Description + 128 );
            }
        }

        MKT_CORE_LOGGER_DEBUG( "Picked adapter: {}", mDeviceName.c_str() );
#endif

        D3D_FEATURE_LEVEL chosenDeviceFeatureLevel{};

        if ( FAILED( D3D11CreateDevice(
                 nullptr,
                 D3D_DRIVER_TYPE::D3D_DRIVER_TYPE_HARDWARE,
                 nullptr,
                 deviceFlags,
                 mDeviceFeatureLevel.data(),
                 core::as<UINT>( mDeviceFeatureLevel.size() ),
                 D3D11_SDK_VERSION,
                 &mDevice,
                 &chosenDeviceFeatureLevel,
                 &mDeviceContext ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to create device and device Context" );
        }

        // Upload workers submit through the same immediate context as ImGui and presentation.
        // https://learn.microsoft.com/en-us/windows/win32/api/d3d11_4/nn-d3d11_4-id3d11multithread
        Microsoft::WRL::ComPtr<ID3D11Multithread> multithread{};
        if ( FAILED( mDeviceContext.As( &multithread ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "Failed to enable D3D11 immediate-context thread protection." );
        }
        multithread->SetMultithreadProtected( TRUE );

        // In the case we did not select one query the one selected by d3d11
        if ( !mAdapter || mAdapter3 ) {
            mDevice->QueryInterface( IID_PPV_ARGS( &mDevice3 ) );
            mDeviceContext->QueryInterface( IID_PPV_ARGS( &mDeviceContext3 ) );

            mDevice->QueryInterface( IID_PPV_ARGS( &mDxgiDevice ) );

            mDxgiDevice->GetAdapter( &mAdapter );
            mAdapter.As( &mAdapter3 );
        }

        DXGI_ADAPTER_DESC desc{};

        if ( SUCCEEDED( mAdapter->GetDesc( &desc ) ) ) {
            eastl::wstring wideDeviceName{ desc.Description };
            mDeviceName = string::FromWChar( wideDeviceName.c_str() );
        }

        mQueue = core::Ref<Queue>::New( renderer::rhi::QueueType::eGraphics,
            renderer::rhi::QueueOpSupportFlagsBits::Graphics |
                renderer::rhi::QueueOpSupportFlagsBits::Compute |
                renderer::rhi::QueueOpSupportFlagsBits::Transfer |
                renderer::rhi::QueueOpSupportFlagsBits::Presentation );
        mQueue->Initialize( this );
    }

    auto Device::Shutdown() -> void {
        MKT_BEGIN_PROFILER();
        WaitIdle();
        mQueue.Reset();
    }

    auto Device::CreateBuffer( const renderer::rhi::BufferCreateDescription &description ) -> renderer::rhi::BufferHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::BufferHandle buffer{ core::Ref<Buffer>::New( description ) };

        if ( buffer.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "CreateBuffer - Failed to allocate texture resource." );
            return renderer::rhi::BufferHandle::CreateEmpty();
        }

        buffer->Initialize( this );

        return buffer;
    }

    auto Device::CreateTexture( const renderer::rhi::TextureCreateDescription &description ) -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::TextureHandle texture{ core::Ref<Texture>::New( description ) };

        if ( texture.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "CreateTexture - Failed to allocate texture resource." );
            return renderer::rhi::TextureHandle::CreateEmpty();
        }

        texture->Initialize( this );

        return texture;
    }

    auto Device::CreateTextureNative( renderer::rhi::ObjectType type, renderer::rhi::Object object, const renderer::rhi::TextureCreateDescription &description ) -> renderer::rhi::TextureHandle {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::TextureHandle::CreateEmpty();
    }

    auto Device::CreateSampler( const renderer::rhi::SamplerCreateDescription &description ) -> renderer::rhi::SamplerHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::SamplerHandle sampler{ core::Ref<Sampler>::New( description ) };

        if ( sampler.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate sampler resource." );
            return renderer::rhi::SamplerHandle::CreateEmpty();
        }

        sampler->Initialize( this );

        return sampler;
    }

    auto Device::CreateAccelStructure( const renderer::rhi::AccelStructureCreateDescription &description ) -> renderer::rhi::AccelStructureHandle {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::AccelStructureHandle::CreateEmpty();
    }

    auto Device::CreateCommandList( renderer::rhi::QueueType type ) -> renderer::rhi::CommandListHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::CommandListHandle cmd{ core::Ref<CommandList>::New( type ) };

        if ( cmd.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate command list resource." );
            return renderer::rhi::CommandListHandle::CreateEmpty();
        }

        cmd->Initialize( this );

        return cmd;
    }

    auto Device::CreateCommandList( const rhi::CommandListCreateDescription &desc ) -> rhi::CommandListHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::CommandListHandle cmd{ core::Ref<CommandList>::New( desc.mQueueType ) };

        if ( cmd.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate command list resource." );
            return renderer::rhi::CommandListHandle::CreateEmpty();
        }

        cmd->Initialize( this );
        cmd->SetDebugName( desc.mName );

        return cmd;
    }

    auto Device::CreateShader( const renderer::rhi::ShaderModuleCreateDescription &desc ) -> renderer::rhi::ShaderModuleHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::ShaderModuleHandle shader{ core::Ref<Shader>::New( desc ) };

        if ( shader.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "CreateShader - Failed to allocate shader resource." );
            return renderer::rhi::ShaderModuleHandle::CreateEmpty();
        }

        shader->Initialize( this );

        return shader;
    }

    auto Device::CreateInputLayout( const renderer::rhi::InputLayoutCreateDescription &desc ) -> renderer::rhi::InputLayoutHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::InputLayoutHandle layout{ core::Ref<InputLayout>::New( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return renderer::rhi::InputLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreateBindingLayout( const renderer::rhi::BindingLayoutDescription &desc ) -> renderer::rhi::BindingLayoutHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::BindingLayoutHandle layout{ core::Ref<BindingLayout>::New( desc ) };

        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return renderer::rhi::BindingLayoutHandle::CreateEmpty();
        }

        layout->Initialize( this );

        return layout;
    }

    auto Device::CreatePipelineLayout( const renderer::rhi::PipelineLayoutCreateDescription &desc ) -> renderer::rhi::PipelineLayoutHandle {
        MKT_BEGIN_PROFILER();

        // Keep the RHI layout metadata for resources bound through D3D11 shader-stage slots.
        renderer::rhi::PipelineLayoutHandle layout{ core::Ref<PipelineLayout>::New( desc ) };
        if ( layout.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate D3D11 pipeline layout." );
            return {};
        }

        layout->Initialize( this );
        return layout;
    }

    auto Device::CreateBindingTable( const renderer::rhi::BindingTableDescription &desc, renderer::rhi::BindingLayoutHandle layout ) -> renderer::rhi::BindingTableHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::BindingTableHandle set{ core::Ref<BindingTable>::New( desc, layout ) };

        if ( set.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate binding layout resource." );
            return renderer::rhi::BindingTableHandle::CreateEmpty();
        }

        set->Initialize( this );

        return set;
    }

    auto Device::UpdateBindingTable( const rhi::BindingTableDescription &desc, rhi::BindingTableHandle table ) -> void {
    }

    auto Device::CreateSwapChain( const renderer::rhi::SwapChainDescription &description ) -> rhi::SwapChainHandle {
        MKT_BEGIN_PROFILER();
        return rhi::SwapChainHandle::CreateEmpty();
    }

    auto Device::CreateQueryPool( const renderer::rhi::QueryPoolDescription &description ) -> renderer::rhi::QueryPoolHandle {
        return renderer::rhi::QueryPoolHandle::CreateEmpty();
    }

    auto Device::GetQueryCapabilities( MKT_UNUSED_VAR renderer::rhi::QueryType type, MKT_UNUSED_VAR renderer::rhi::QueueType queue ) const -> renderer::rhi::QueryCapabilities {
        return { .mReason = "GPU queries are not implemented by the D3D11 backend." };
    }

    auto Device::SupportsSceneMultisampling( renderer::rhi::Multisampling samples ) const -> bool {
        return samples == renderer::rhi::Multisampling::eMsaaX1;
    }

    auto Device::SupportsBCTextureCompression() const -> bool {
        return false;
    }

    auto Device::CreateFence( MKT_UNUSED_VAR core::u64 fenceInitialValue ) -> renderer::rhi::FenceHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::FenceHandle fence{ core::Ref<Fence>::New() };

        if ( fence.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate fence resource." );
            return renderer::rhi::FenceHandle::CreateEmpty();
        }

        fence->Initialize( this );

        return fence;
    }

    auto Device::UnMap( renderer::rhi::IBuffer *buffer ) -> void {
    }

    auto Device::Map( renderer::rhi::IBuffer *buffer ) -> void * {
        MKT_BEGIN_PROFILER();
        return nullptr;
    }

    auto Device::CreateDescriptorTableLayout( const renderer::rhi::DescriptorTableLayoutDescription &desc ) -> renderer::rhi::BindingLayoutHandle {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::BindingLayoutHandle::CreateEmpty();
    }

    auto Device::CreateDescriptorTable( renderer::rhi::BindingLayoutHandle layout ) -> renderer::rhi::DescriptorTableHandle {
        MKT_BEGIN_PROFILER();
        return renderer::rhi::DescriptorTableHandle::CreateEmpty();
    }

    auto Device::ResizeDescriptorTable( renderer::rhi::DescriptorTableHandle descriptorTable, core::u32 newSize, bool keepContents ) -> bool {
        MKT_BEGIN_PROFILER();
        return false;
    }

    auto Device::ReleaseDescriptorTableIndex( rhi::DescriptorTableHandle, core::u32, rhi::DescriptorTableIndex ) -> void {
        MKT_BEGIN_PROFILER();
        // This backend does not allocate bindless descriptor table indices.
    }

    auto Device::WriteDescriptorTable( renderer::rhi::DescriptorTableHandle descriptorTable, const renderer::rhi::BindingTableItem &item ) -> rhi::DescriptorTableIndex {
        MKT_BEGIN_PROFILER();
        return rhi::kInvalidBindingItemIndex;
    }

    auto Device::GetQueue( renderer::rhi::QueueType type ) -> renderer::rhi::IQueue * {
        MKT_BEGIN_PROFILER();
        return mQueue.GetPtr();
    }

    auto Device::GetProcessMemoryUsage() const -> rhi::ProcessGpuMemory {
        if ( !mAdapter3 ) {
            return {};
        }

        DXGI_QUERY_VIDEO_MEMORY_INFO local{}, shared{};

        if ( FAILED( mAdapter3->QueryVideoMemoryInfo( 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &local ) ) ||
             FAILED( mAdapter3->QueryVideoMemoryInfo( 0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &shared ) ) ) {
            return {};
        }

        return { core::as<core::usize>( local.CurrentUsage ), core::as<core::usize>( shared.CurrentUsage ), true };
    }

    auto Device::GetMemoryUsage() const -> core::usize {
        MKT_BEGIN_PROFILER();

        if ( mAdapter3 ) {
            DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
            mAdapter3->QueryVideoMemoryInfo( 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo );

            core::u64 currentVramUsage{ memInfo.CurrentUsage };
            return core::as<core::usize>( currentVramUsage );
        }

        return 0;
    }

    auto Device::GetMemoryTotal() const -> core::usize {
        MKT_BEGIN_PROFILER();
        DXGI_ADAPTER_DESC desc{};

        if ( SUCCEEDED( mAdapter3->GetDesc( &desc ) ) ) {
            core::u64 totalVRAM{ desc.DedicatedVideoMemory };

            return totalVRAM;
        }

        return 0;
    }

    auto Device::GetMemoryAvailable() const -> core::usize {
        MKT_BEGIN_PROFILER();

        if ( mAdapter3 ) {
            DXGI_QUERY_VIDEO_MEMORY_INFO memInfo{};
            mAdapter3->QueryVideoMemoryInfo( 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memInfo );

            core::u64 currentVramUsage{ memInfo.CurrentUsage };// Bytes currently used
            core::u64 vramBudget{ memInfo.Budget };            // OS recommended VRAM limit

            return core::as<core::usize>( vramBudget - currentVramUsage );
        }

        return 0;
    }

    auto Device::IsFeatureSupported( rhi::GpuFeatureSupportFlags supportFlags ) const -> bool {
        MKT_BEGIN_PROFILER();
        rhi::GpuFeatureSupportFlags supported{ rhi::GpuFeatureSupportFlagsBits::None };

        // Feature level 11_0 provides these paths independently of creation requests.
        // BC uploads, ray tracing, mesh shaders, bindless tables and video encoding
        // are not implemented by this backend and must not be advertised as usable.
        if ( mDevice ) {
            supported = rhi::GpuFeatureSupportFlagsBits::AnisotropicFiltering |
                        rhi::GpuFeatureSupportFlagsBits::HardwareWireframe |
                        rhi::GpuFeatureSupportFlagsBits::WindowPresentation;
        }

        return ( supported & supportFlags ) == supportFlags;
    }

    auto Device::IsFeatureRequested( rhi::GpuFeatureSupportFlags requestFlags ) const -> bool {
        MKT_BEGIN_PROFILER();
        return ( mFeatureSupportFlags & requestFlags ) == requestFlags;
    }

    auto Device::WaitIdle() -> void {
        if ( mQueue ) {
            core::checked_cast<Queue *>( mQueue.GetPtr() )->CollectGarbage( true );
        }
    }

    auto Device::CreateShaderTable( const rhi::ShaderTableDescription & ) -> rhi::ShaderTableHandle {
        return rhi::ShaderTableHandle::CreateEmpty();
    }

    auto Device::CreatePipeline( const renderer::rhi::ComputePipelineDescription &description ) -> renderer::rhi::PipelineHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::PipelineHandle computePipeline{ core::Ref<ComputePipeline>::New( description ) };

        if ( computePipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate compute pipeline resource." );
            return renderer::rhi::PipelineHandle::CreateEmpty();
        }

        computePipeline->Initialize( this );

        return computePipeline;
    }

    auto Device::CreatePipeline( const renderer::rhi::GraphicsPipelineDescription &description ) -> renderer::rhi::PipelineHandle {
        MKT_BEGIN_PROFILER();
        renderer::rhi::PipelineHandle graphicsPipeline{ core::Ref<GraphicsPipeline>::New( description ) };

        if ( graphicsPipeline.IsEmpty() ) {
            MKT_CORE_LOGGER_ERROR( "Failed to allocate graphics pipeline resource." );
            return renderer::rhi::PipelineHandle::CreateEmpty();
        }

        graphicsPipeline->Initialize( this );

        return graphicsPipeline;
    }

    auto Device::RunGarbageCollection() -> void {
        if ( mQueue ) {
            core::checked_cast<Queue *>( mQueue.GetPtr() )->CollectGarbage();
        }
    }

    auto Device::GetDevice() const -> ID3D11Device * {
        MKT_BEGIN_PROFILER();
        return mDevice.Get();
    }

    auto Device::GetDevice3() const -> ID3D11Device3 * {
        MKT_BEGIN_PROFILER();
        return mDevice3.Get();
    }

    auto Device::GetDeviceContext() -> ID3D11DeviceContext * {
        MKT_BEGIN_PROFILER();
        return mDeviceContext.Get();
    }

    auto Device::GetDeviceContext3() -> ID3D11DeviceContext3 * {
        MKT_BEGIN_PROFILER();
        return mDeviceContext3.Get();
    }

    auto Device::DumpErrorMessages() -> void {
        MKT_BEGIN_PROFILER();
        Microsoft::WRL::ComPtr<ID3D11InfoQueue> infoQueue{};

        if ( FAILED( mDevice->QueryInterface( IID_PPV_ARGS( &infoQueue ) ) ) ) {
            return;
        }

        const auto messageCount{ infoQueue->GetNumStoredMessages() };

        for ( UINT64 i{}; i < messageCount; ++i ) {
            core::usize messageLength{};
            infoQueue->GetMessage( i, nullptr, &messageLength );

            auto bytes{ std::make_unique<std::byte[]>( messageLength ) };
            auto *message{ reinterpret_cast<D3D11_MESSAGE *>( bytes.get() ) };

            if ( SUCCEEDED( infoQueue->GetMessage( i, message, &messageLength ) ) ) {
                switch ( message->Severity ) {

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

    auto Device::CreateSwapChain( platform::IWindow *window, Microsoft::WRL::ComPtr<IDXGIFactory2> dxgiFactory ) -> D3D11SwapChainHandle {
        MKT_BEGIN_PROFILER();
        auto handle{ D3D11SwapChainHandle::New( window, dxgiFactory ) };

        if ( !handle.IsEmpty() ) {
            handle->Initialize( this );
        }

        return handle;
    }

    auto Queue::GetType() const -> rhi::QueueType {
        return mType;
    }

    auto Queue::GetOpSupportFlags() const -> rhi::QueueOpSupportFlags {
        return mOpSupportFlags;
    }

    auto CommandList::GetQueueType() const -> rhi::QueueType {
        return mQueueType;
    }

    auto Device::SupportsRayTracing() const -> bool {
        return false;
    }

    auto Device::CreatePipeline( const rhi::RayTracingPipelineDescription & ) -> rhi::RayTracingPipelineHandle {
        return rhi::RayTracingPipelineHandle::CreateEmpty();
    }
}
#endif
