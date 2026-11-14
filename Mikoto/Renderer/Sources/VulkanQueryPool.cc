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
#include <stdexcept>

#include <EASTL/algorithm.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Exception.hh>

#include <Renderer/Rhi/Vulkan/VulkanDevice.hh>
#include <Renderer/Rhi/Vulkan/VulkanQueryPool.hh>

// https://nikitablack.github.io/post/how_to_use_vulkan_timestamp_queries/

namespace mikoto::renderer::vulkan {
    using namespace mikoto::core;
    using namespace mikoto::renderer::rhi;

    QueryPool::QueryPool( const renderer::rhi::QueryPoolDescription& description )
        : mDescription{ description } {
    }

    auto QueryPool::SetDebugName( eastl::string_view name ) -> void {
        if ( !mDevice || !mPool || name.empty() ) {
            return;
        }

        mDebugName = name;
        Device* device{ core::checked_cast<Device*>( mDevice ) };
        device->SetDebugName( VK_OBJECT_TYPE_QUERY_POOL, core::rc_cast<core::u64>( mPool ), mDebugName );
    }

    auto QueryPool::GetNativeHandle( renderer::rhi::ObjectType type ) -> renderer::rhi::Object {
        return renderer::rhi::IQueryPool::GetNativeHandle( type );
    }

    auto QueryPool::GetNativeHandle( renderer::rhi::ObjectType type ) const -> renderer::rhi::Object {
        return renderer::rhi::IQueryPool::GetNativeHandle( type );
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

        VkQueryPoolCreateInfo info{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
        info.queryCount = mDescription.mCount;

        switch ( mDescription.mType ) {
            case renderer::rhi::QueryType::eTimestamp:
                info.queryType = VK_QUERY_TYPE_TIMESTAMP;
                break;
            case renderer::rhi::QueryType::eOcclusion:
                info.queryType = VK_QUERY_TYPE_OCCLUSION;
                break;
            case renderer::rhi::QueryType::ePipelineStatistics:
                info.queryType = VK_QUERY_TYPE_PIPELINE_STATISTICS;
                // RHI core counter indices deliberately match Vulkan's first eleven bits.
                info.pipelineStatistics = static_cast<VkQueryPipelineStatisticFlags>( mDescription.mStatisticsMask );
                break;
            default:
                MKT_THROW_RUNTIME_ERROR( "Unsupported Vulkan query type." );
        }

        Device* device{ core::checked_cast<Device*>( mDevice ) };

        if ( vkCreateQueryPool( device->GetDevice(), &info, nullptr, &mPool ) != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "vkCreateQueryPool failed." );
        }

        mStates.resize( mDescription.mCount, SlotState::eNeedsReset );
        mIsAllocated = true;
    }

    auto QueryPool::Destroy() -> void {
        if ( mPool ) {
            Device* device{ core::checked_cast<Device*>( mDevice ) };
            vkDestroyQueryPool( device->GetDevice(), mPool, nullptr );
            mPool = VK_NULL_HANDLE;
        }

        mIsAllocated = false;
    }

    QueryPool::~QueryPool() {
        Destroy();
    }

    auto QueryPool::ReadCompleted( core::u32 index, renderer::rhi::QueryResult& result ) -> bool {
        if ( mStates[index] != SlotState::eResolved ) {
            return false;
        }

        Device* device{ core::checked_cast<Device*>( mDevice ) };
        const core::u32 values{ mDescription.mType == renderer::rhi::QueryType::ePipelineStatistics ? static_cast<core::u32>( std::popcount( mDescription.mStatisticsMask ) ) : 1u };

        eastl::array<core::u64, static_cast<core::usize>( renderer::rhi::PipelineStatistic::eCount ) + 1> data{};
        const core::usize size{ core::usize{ values + 1 } * sizeof( core::u64 ) };

        // https://nikitablack.github.io/post/how_to_use_vulkan_timestamp_queries/
        const auto status{ vkGetQueryPoolResults( device->GetDevice(), mPool,
            index, 1, size, data.data(), size,
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT ) };

        if ( status == VK_NOT_READY ) {
            return false;
        }

        if ( status != VK_SUCCESS ) {
            MKT_THROW_RUNTIME_ERROR( "vkGetQueryPoolResults failed." );
        }

        if ( !data[values] ) {
            return false;
        }

        if ( mDescription.mType == renderer::rhi::QueryType::ePipelineStatistics ) {
            core::u32 source{};
            result.mValidMask = mDescription.mStatisticsMask;

            for ( core::u32 counter{}; counter < result.mValues.size(); ++counter ) {
                if ( result.mValidMask & ( core::u64{ 1 } << counter ) ) {
                    result.mValues[counter] = data[source++];
                }
            }
        }
        else {
            result.mValidMask = 1;
            result.mValues[0] = data[0];

            if ( mDescription.mType == renderer::rhi::QueryType::eOcclusion && !mDescription.mPreciseOcclusion ) {
                result.mValues[0] = data[0] != 0 ? 1 : 0;
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
}// namespace mikoto::renderer::vulkan
