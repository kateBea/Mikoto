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

#ifndef MIKOTO_RENDERER_RHI_QUERY_POOL_HH
#define MIKOTO_RENDERER_RHI_QUERY_POOL_HH

#include <EASTL/array.h>

#include <Renderer/Rhi/Fence.hh>
#include <Renderer/Rhi/DeviceObject.hh>
#include <Renderer/Rhi/CommandList.hh>

namespace mikoto::renderer::rhi {
    enum class QueryType : core::u32 {
        eTimestamp,
        eOcclusion,
        ePipelineStatistics,
        eTransformFeedback,
        eMeshShader,
        eResultStatus,
        eVideoEncodeFeedback,
        ePerformance,
        eIntelPerformance,
        eElapsedTimer,
        eCount
    };

    enum class PipelineStatistic : core::u32 {
        eInputVertices,
        eInputPrimitives,
        eVertexInvocations,
        eGeometryInvocations,
        eGeometryPrimitives,
        eClippingInvocations,
        eClippingPrimitives,
        ePixelInvocations,
        eHullPatches,
        eDomainInvocations,
        eComputeInvocations,
        eTaskInvocations,
        eMeshInvocations,
        eMeshPrimitives,
        eCount
    };

    struct QueryCapabilities {
        bool mSupported{};
        core::cstr mReason{ "Not implemented by this backend." };
        core::u64 mStatisticsMask{};
        core::f64 mTimestampPeriodNs{};
        core::u32 mTimestampValidBits{};
        bool mPreciseOcclusion{};
    };

    struct QueryPoolDescription {
        QueryType mType{ QueryType::eTimestamp };
        QueueType mQueue{ QueueType::eGraphics };
        core::u32 mCount{ 2 };

        // Zero requests every supported statistic, otherwise select explicit bits.
        core::u64 mStatisticsMask{};

        // False returns binary visibility; true returns samples passing depth/stencil.
        bool mPreciseOcclusion{};
        core::u32 mStream{};

        bool mTrackState{ true };
    };

    struct QueryResult {
        // Timestamps/occlusion/elapsed timers use element 0. Stream output uses written/needed
        // in 0/1. Pipeline statistics use PipelineStatistic indices.
        eastl::array<core::u64, static_cast<core::usize>( PipelineStatistic::eCount )> mValues{};
        core::u64 mValidMask{};
    };

    class IQueryPool : public DeviceObject {
    public:
        /**
         * @brief Returns the immutable description used to create this pool.
         * @returns Query type, queue family, slot count, and type-specific options.
         */
        MKT_NODISCARD virtual auto GetDescription() const -> const QueryPoolDescription& = 0;

        /**
         * @brief Returns capabilities reported by the active graphics backend.
         * @returns Support status, timestamp conversion data, and statistic limits.
         * @note A pool can be created while unsupported; callers should inspect
         *       this value before recording commands.
         */
        MKT_NODISCARD virtual auto GetCapabilities() const -> const QueryCapabilities& = 0;

        /**
         * @brief Attempts to read one resolved query result without waiting.
         *
         * @param index Zero-based slot to read.
         * @param completion Fence from the submission containing Resolve().
         * @param value Completion value that must have been reached.
         * @param result Destination for the result when it is ready.
         * @returns True when the fence and query data are complete; false otherwise.
         * @pre The slot was resolved through the command-list interface.
         * @note This method never blocks and leaves @p result unchanged when false.
         */
        MKT_NODISCARD virtual auto TryGetResult( core::u32 index, const IFence& completion, core::u64 value, QueryResult& result ) -> bool = 0;

        /**
         * @brief Converts two timestamp values to milliseconds.
         *
         * @param begin Beginning timestamp. Pass zero when using an absolute end value.
         * @param end Ending timestamp.
         * @returns Elapsed duration in milliseconds using the pool's timestamp period.
         * @pre The pool describes a timestamp or elapsed-timer query and timestamps
         *      came from the same device and calibration domain.
         * @note Handles backend timestamp wrap according to the reported valid-bit width.
         */
        MKT_NODISCARD virtual auto GetElapsedMilliseconds( core::u64 begin, core::u64 end ) const -> core::f64 = 0;

        ~IQueryPool() override = default;

    protected:
        /**
         * @brief Validates a contiguous slot range for a recording operation.
         * @param first First zero-based slot.
         * @param count Number of consecutive slots.
         * @throws @ref core::RuntimeException If the range is empty or exceeds capacity.
         */
        virtual auto ValidateRange( core::u32 first, core::u32 count ) const -> void = 0;

        /**
         * @brief Reads a result after the submission completion fence has passed.
         * @param index Zero-based slot to read.
         * @param result Destination for backend data.
         * @returns True when the backend result is available without waiting.
         */
        MKT_NODISCARD virtual auto ReadCompleted( core::u32 index, QueryResult& result ) -> bool = 0;
    };

    using QueryPoolHandle = core::Ref<IQueryPool>;

}// namespace mikoto::renderer::rhi

#endif// MIKOTO_RENDERER_RHI_QUERY_POOL_HH
