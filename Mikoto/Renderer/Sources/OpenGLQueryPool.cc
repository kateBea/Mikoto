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

#include <utility>
#include <iterator>

#include <EASTL/utility.h>

#include <glm/gtx/compatibility.hpp>

#include <Core/Exception.hh>

#include <Renderer/Rhi/OpenGL/OpenGLHelpers.hh>
#include <Renderer/Rhi/OpenGL/OpenGLQueryPool.hh>

namespace mikoto::renderer::opengl {

    static constexpr GLenum kStatistics[]{ GL_VERTICES_SUBMITTED_ARB,
        GL_PRIMITIVES_SUBMITTED_ARB,
        GL_VERTEX_SHADER_INVOCATIONS_ARB,
        GL_GEOMETRY_SHADER_INVOCATIONS,
        GL_GEOMETRY_SHADER_PRIMITIVES_EMITTED_ARB,
        GL_CLIPPING_INPUT_PRIMITIVES_ARB,
        GL_CLIPPING_OUTPUT_PRIMITIVES_ARB,
        GL_FRAGMENT_SHADER_INVOCATIONS_ARB,
        GL_TESS_CONTROL_SHADER_PATCHES_ARB,
        GL_TESS_EVALUATION_SHADER_INVOCATIONS_ARB,
        GL_COMPUTE_SHADER_INVOCATIONS_ARB };


    auto QueryPool::Capabilities( renderer::rhi::QueryType type, renderer::rhi::QueueType queue ) -> renderer::rhi::QueryCapabilities {
        renderer::rhi::QueryCapabilities result{};

        if ( queue != renderer::rhi::QueueType::eGraphics && queue != renderer::rhi::QueueType::eCompute && queue != renderer::rhi::QueueType::eTransfer ) {
            result.mReason = "No OpenGL query support on this queue.";
            return result;
        }

        if ( type == renderer::rhi::QueryType::eTimestamp || type == renderer::rhi::QueryType::eElapsedTimer ) {
            GLint bits{};
            glGetQueryiv( type == renderer::rhi::QueryType::eTimestamp ? GL_TIMESTAMP : GL_TIME_ELAPSED, GL_QUERY_COUNTER_BITS, &bits );
            result.mSupported = bits > 0;
            result.mTimestampPeriodNs = 1.0;
            result.mTimestampValidBits = bits;
        }
        else if ( type == renderer::rhi::QueryType::eOcclusion && queue == renderer::rhi::QueueType::eGraphics ) {
            result.mSupported = true;
            result.mPreciseOcclusion = true;
        }
        else if ( type == renderer::rhi::QueryType::ePipelineStatistics && queue != renderer::rhi::QueueType::eTransfer &&
                  ( GLAD_GL_VERSION_4_6 || GLAD_GL_ARB_pipeline_statistics_query ) ) {

            for ( core::u32 i{}; i < std::size( kStatistics ); ++i ) {

                if ( queue == renderer::rhi::QueueType::eCompute && i != static_cast<core::u32>( renderer::rhi::PipelineStatistic::eComputeInvocations ) ) {
                    continue;
                }

                GLint bits{};
                glGetQueryiv( kStatistics[i], GL_QUERY_COUNTER_BITS, &bits );

                if ( bits > 0 ) {
                    result.mStatisticsMask |= core::u64{ 1 } << i;
                }
            }

            result.mSupported = result.mStatisticsMask != 0;
        }
        else if ( type == renderer::rhi::QueryType::eTransformFeedback && queue == renderer::rhi::QueueType::eGraphics ) {
            result.mSupported = true;
        }

        if ( result.mSupported ) {
            result.mReason = "";
        }
        else {
            result.mReason = "Query type is unavailable in this OpenGL backend or queue.";
        }

        return result;
    }

    QueryPool::QueryPool( const renderer::rhi::QueryPoolDescription &desc, eastl::shared_ptr<DeviceState> state )
        : mState{ eastl::move( state ) }, mDescription{ desc } {
    }

    QueryPool::~QueryPool() {
        Destroy();
    }

    auto QueryPool::Initialize() -> void {
        mCapabilities = mDevice->GetQueryCapabilities( mDescription.mType, mDescription.mQueue );

        if ( !mCapabilities.mSupported || !mDescription.mCount || mDescription.mCount > 65536 ) {
            MKT_THROW_RUNTIME_ERROR( "Unsupported query pool type or invalid query count." );
        }

        if ( ( mDescription.mType == renderer::rhi::QueryType::eTimestamp || mDescription.mType == renderer::rhi::QueryType::eElapsedTimer ) &&
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


        mState->RequireOwner();

        if ( mDescription.mType == renderer::rhi::QueryType::ePipelineStatistics ) {

            for ( core::u32 i{}; i < std::size( kStatistics ); ++i ) {

                if ( mDescription.mStatisticsMask & ( core::u64{ 1 } << i ) ) {
                    mTargets.push_back( kStatistics[i] );
                    mResultIndices.push_back( i );
                }
            }
        }
        else if ( mDescription.mType == renderer::rhi::QueryType::eTransformFeedback ) {
            GLint streams{};
            glGetIntegerv( GL_MAX_VERTEX_STREAMS, &streams );

            if ( mDescription.mStream >= static_cast<core::u32>( streams ) ) {
                MKT_THROW_RUNTIME_ERROR( "Transform feedback stream is invalid." );
            }

            mTargets = { GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, GL_PRIMITIVES_GENERATED };
            mResultIndices = { 0, 1 };
        }
        else {
            mTargets.push_back( mDescription.mType == renderer::rhi::QueryType::eTimestamp      ? GL_TIMESTAMP
                                : mDescription.mType == renderer::rhi::QueryType::eElapsedTimer ? GL_TIME_ELAPSED
                                : mDescription.mPreciseOcclusion                                ? GL_SAMPLES_PASSED
                                                                                                : GL_ANY_SAMPLES_PASSED );
            mResultIndices.push_back( 0 );
        }

        mQueries.resize( mDescription.mCount * mTargets.size() );

        for ( core::u32 slot{}; slot < mDescription.mCount; ++slot ) {

            for ( core::u32 i{}; i < mTargets.size(); ++i ) {
                glCreateQueries( mTargets[i], 1, &mQueries[slot * mTargets.size() + i] );
            }
        }

        mSlots.resize( mDescription.mCount, SlotState::eNeedsReset );
        CheckError( "query pool creation" );
        mIsAllocated = true;
    }

    auto QueryPool::Destroy() -> void {
        if ( !mQueries.empty() ) {
            mState->Retire( [queries = eastl::move( mQueries )] {
                glDeleteQueries( static_cast<GLsizei>( queries.size() ), queries.data() );
            } );
        }

        mIsAllocated = false;
    }

    auto QueryPool::ReadCompleted( core::u32 index, renderer::rhi::QueryResult &result ) -> bool {
        mState->RequireOwner();

        if ( mSlots[index] != SlotState::eResolved ) {
            return false;
        }

        for ( core::u32 i{}; i < mTargets.size(); ++i ) {
            GLuint available{};
            glGetQueryObjectuiv( mQueries[index * mTargets.size() + i], GL_QUERY_RESULT_AVAILABLE, &available );

            if ( !available ) {
                return false;
            }
        }

        for ( core::u32 i{}; i < mTargets.size(); ++i ) {
            GLuint64 value{};
            glGetQueryObjectui64v( mQueries[index * mTargets.size() + i], GL_QUERY_RESULT, &value );
            result.mValues[mResultIndices[i]] = value;
            result.mValidMask |= core::u64{ 1 } << mResultIndices[i];
        }

        return true;
    }

    auto QueryPool::GetDescription() const -> const renderer::rhi::QueryPoolDescription & {
        return mDescription;
    }

    auto QueryPool::GetCapabilities() const -> const renderer::rhi::QueryCapabilities & {
        return mCapabilities;
    }

    auto QueryPool::ValidateRange( core::u32 first, core::u32 count ) const -> void {
        if ( !count || first >= mDescription.mCount || count > mDescription.mCount - first ) {
            MKT_THROW_RUNTIME_ERROR( "GPU query range exceeds its pool." );
        }
    }

    auto QueryPool::TryGetResult( core::u32 index, const renderer::rhi::IFence &completion, core::u64 value, renderer::rhi::QueryResult &result ) -> bool {
        ValidateRange( index, 1 );
        result = {};
        mState->RequireOwner();

        if ( completion.GetCompletionValue() < value ) {
            return false;
        }

        return ReadCompleted( index, result );
    }

    auto QueryPool::GetElapsedMilliseconds( core::u64 begin, core::u64 end ) const -> core::f64 {
        const core::u32 bits{ mCapabilities.mTimestampValidBits };

        if ( ( mDescription.mType != renderer::rhi::QueryType::eTimestamp && mDescription.mType != renderer::rhi::QueryType::eElapsedTimer ) || !bits ) {
            return 0;
        }

        // Subtract integer ticks before conversion and mask counter wraparound.
        const core::u64 mask{ bits == 64 ? ~core::u64{} : ( core::u64{ 1 } << bits ) - 1 };
        return static_cast<core::f64>( ( end - begin ) & mask ) * mCapabilities.mTimestampPeriodNs * 1e-6;
    }
}// namespace mikoto::renderer::opengl
