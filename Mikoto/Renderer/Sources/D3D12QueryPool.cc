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

#include <cstring>
#include <stdexcept>

#include <EASTL/algorithm.h>

#include <glm/gtx/compatibility.hpp>

#include <Core/Exception.hh>

#include <Renderer/Rhi/D3D12/D3D12QueryPool.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )
#include <Renderer/Rhi/D3D12/D3D12Device.hh>

namespace mikoto::renderer::d3d12 {
    using namespace core;
    using namespace rhi;
    QueryPool::QueryPool( const renderer::rhi::QueryPoolDescription& description )
        : mDescription{ description } {
    }

    auto QueryPool::Initialize() -> void {
        mCapabilities = mDevice->GetQueryCapabilities( mDescription.mType, mDescription.mQueue );

        if ( !mCapabilities.mSupported || !mDescription.mCount || mDescription.mCount > 65536 ) {
            MKT_THROW_RUNTIME_ERROR( "Unsupported query pool type or invalid query count." );
        }

        if ( mDescription.mType == renderer::rhi::QueryType::eTimestamp &&
             ( !mCapabilities.mTimestampValidBits || mCapabilities.mTimestampValidBits > 64 || mCapabilities.mTimestampPeriodNs <= 0 ) ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid timestamp counter precision or period." );
        }

        if ( mDescription.mStatisticsMask & ~mCapabilities.mStatisticsMask ) {
            MKT_THROW_RUNTIME_ERROR( "Query pool requests unavailable pipeline statistics." );
        }

        if ( !mDescription.mStatisticsMask ) {
            mDescription.mStatisticsMask = mCapabilities.mStatisticsMask;
        }

        if ( mDescription.mPreciseOcclusion && !mCapabilities.mPreciseOcclusion ) {
            MKT_THROW_RUNTIME_ERROR( "Precise occlusion queries are unavailable." );
        }

        D3D12_QUERY_HEAP_DESC heap{};
        heap.Count = mDescription.mCount;
        mStride = sizeof( core::u64 );

        switch ( mDescription.mType ) {
            case renderer::rhi::QueryType::eTimestamp:
                heap.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
                mType = D3D12_QUERY_TYPE_TIMESTAMP;
                break;
            case renderer::rhi::QueryType::eOcclusion:
                heap.Type = D3D12_QUERY_HEAP_TYPE_OCCLUSION;
                mType = mDescription.mPreciseOcclusion ? D3D12_QUERY_TYPE_OCCLUSION : D3D12_QUERY_TYPE_BINARY_OCCLUSION;
                break;
            case renderer::rhi::QueryType::ePipelineStatistics:
                heap.Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS;
                mType = D3D12_QUERY_TYPE_PIPELINE_STATISTICS;
                mStride = sizeof( D3D12_QUERY_DATA_PIPELINE_STATISTICS );
                break;
            case renderer::rhi::QueryType::eMeshShader:
                heap.Type = D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS1;
                mType = D3D12_QUERY_TYPE_PIPELINE_STATISTICS1;
                mStride = sizeof( D3D12_QUERY_DATA_PIPELINE_STATISTICS1 );
                break;
            case renderer::rhi::QueryType::eTransformFeedback:
                if ( mDescription.mStream > 3 ) {
                    MKT_THROW_RUNTIME_ERROR( "D3D12 stream output index must be in [0, 3]." );
                }

                heap.Type = D3D12_QUERY_HEAP_TYPE_SO_STATISTICS;
                mType = static_cast<D3D12_QUERY_TYPE>( D3D12_QUERY_TYPE_SO_STATISTICS_STREAM0 + mDescription.mStream );
                mStride = sizeof( D3D12_QUERY_DATA_SO_STATISTICS );
                break;
            default:
                MKT_THROW_RUNTIME_ERROR( "Unsupported D3D12 query type." );
        }

        Device* device{ core::checked_cast<Device*>( mDevice ) };

        if ( FAILED( device->GetDevice()->CreateQueryHeap( &heap, IID_PPV_ARGS( &mHeap ) ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 query heap creation failed." );
        }

        D3D12_HEAP_PROPERTIES properties{};
        properties.Type = D3D12_HEAP_TYPE_READBACK;
        properties.CreationNodeMask = properties.VisibleNodeMask = 1;
        D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buffer.Width = core::u64{ mStride } * mDescription.mCount;
        buffer.Height = 1;
        buffer.DepthOrArraySize = buffer.MipLevels = 1;
        buffer.SampleDesc.Count = 1;
        buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        if ( FAILED( device->GetDevice()->CreateCommittedResource( &properties, D3D12_HEAP_FLAG_NONE, &buffer,
                 D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS( &mReadback ) ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 query readback buffer creation failed." );
        }

        mStates.resize( mDescription.mCount, SlotState::eNeedsReset );
        mIsAllocated = true;
    }

    auto QueryPool::Destroy() -> void {
        mReadback.Reset();
        mHeap.Reset();
        mIsAllocated = false;
    }

    QueryPool::~QueryPool() {
        Destroy();
    }

    auto QueryPool::ReadCompleted( core::u32 index, renderer::rhi::QueryResult& result ) -> bool {
        if ( mStates[index] != SlotState::eResolved ) {
            return false;
        }

        const core::usize offset{ core::usize{ index } * mStride };
        D3D12_RANGE range{ offset, offset + mStride };
        void* mapped{};

        if ( FAILED( mReadback->Map( 0, &range, &mapped ) ) ) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 query readback mapping failed." );
        }

        eastl::array<core::u64, static_cast<core::usize>( renderer::rhi::PipelineStatistic::eCount )> values{};
        const auto bytes{ eastl::min<core::usize>( mStride, sizeof( values ) ) };
        std::memcpy( values.data(), static_cast<const core::ubyte*>( mapped ) + offset, bytes );
        const D3D12_RANGE noWrites{ 0, 0 };
        mReadback->Unmap( 0, &noWrites );

        if ( mDescription.mType == renderer::rhi::QueryType::ePipelineStatistics || mDescription.mType == renderer::rhi::QueryType::eMeshShader ) {
            result.mValidMask = mDescription.mStatisticsMask;
        }
        else {
            result.mValidMask = mDescription.mType == renderer::rhi::QueryType::eTransformFeedback ? 3 : 1;
        }

        for ( core::u32 counter{}; counter < values.size(); ++counter ) {
            if ( result.mValidMask & ( core::u64{ 1 } << counter ) ) {
                result.mValues[counter] = values[counter];
            }
        }

        return true;
    }

    auto QueryPool::GetDescription() const -> const renderer::rhi::QueryPoolDescription& {
        return mDescription;
    }

    auto QueryPool::GetCapabilities() const -> const renderer::rhi::QueryCapabilities& {
        return mCapabilities;
    }

    auto QueryPool::ValidateRange( core::u32 first, core::u32 count ) const -> void {
        if ( !count || first >= mDescription.mCount || count > mDescription.mCount - first ) {
            MKT_THROW_RUNTIME_ERROR( "GPU query range exceeds its pool." );
        }
    }

    auto QueryPool::TryGetResult( core::u32 index, const renderer::rhi::IFence& completion, core::u64 value, renderer::rhi::QueryResult& result ) -> bool {
        ValidateRange( index, 1 );
        result = {};

        if ( completion.GetCompletionValue() < value ) {
            return false;
        }

        return ReadCompleted( index, result );
    }

    auto QueryPool::GetElapsedMilliseconds( core::u64 begin, core::u64 end ) const -> core::f64 {
        const core::u32 bits{ mCapabilities.mTimestampValidBits };

        if ( mDescription.mType != renderer::rhi::QueryType::eTimestamp || !bits ) {
            return 0;
        }

        // Subtract integer ticks before conversion and mask counter wraparound.
        const core::u64 mask{ bits == 64 ? ~core::u64{} : ( core::u64{ 1 } << bits ) - 1 };
        return static_cast<core::f64>( ( end - begin ) & mask ) * mCapabilities.mTimestampPeriodNs * 1e-6;
    }
}// namespace mikoto::renderer::d3d12
#endif
