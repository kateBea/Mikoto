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

#include <Core/Exception.hh>

#include <Renderer/Core/GpuQueryProfiler.hh>

namespace mikoto::renderer {
    using namespace core;
    using namespace rhi;

    GpuQueryProfiler::GpuQueryProfiler( renderer::rhi::IGpuDevice* device )
        : mDevice{ device } {
    }

    auto GpuQueryProfiler::SetEnabled( bool enabled ) -> void {
        mEnabled = enabled;
    }

    auto GpuQueryProfiler::IsEnabled() const -> bool {
        return mEnabled;
    }

    auto GpuQueryProfiler::GetSnapshot() const -> const GpuQuerySnapshot& {
        return mSnapshot;
    }

    auto GpuQueryProfiler::GetDroppedSamples() const -> core::u64 {
        return mDropped;
    }

    auto GpuQueryProfiler::GetError() const -> eastl::string_view {
        return mError;
    }

    auto GpuQueryProfiler::InitializeSlot( Slot& slot ) -> void {
        if ( !slot.mTimestamps && mDevice->GetQueryCapabilities( renderer::rhi::QueryType::eTimestamp, renderer::rhi::QueueType::eGraphics ).mSupported ) {
            slot.mTimestamps = mDevice->CreateQueryPool( { .mType = renderer::rhi::QueryType::eTimestamp, .mCount = 2 + kMaxPasses * 2 } );
        }

        if ( !slot.mStatistics ) {
            const renderer::rhi::QueryType type{ mDevice->GetQueryCapabilities( renderer::rhi::QueryType::eMeshShader, renderer::rhi::QueueType::eGraphics ).mSupported ? renderer::rhi::QueryType::eMeshShader : renderer::rhi::QueryType::ePipelineStatistics };

            if ( mDevice->GetQueryCapabilities( type, renderer::rhi::QueueType::eGraphics ).mSupported ) {
                slot.mStatistics = mDevice->CreateQueryPool( { .mType = type, .mCount = 1 } );
            }
        }
    }

    auto GpuQueryProfiler::Collect() -> void {
        for ( auto& slot: mSlots ) {
            if ( !slot.mPending || slot.mFence->GetCompletionValue() < slot.mCompletion ) {
                continue;
            }

            GpuQuerySnapshot snapshot{};
            snapshot.mSubmission = slot.mCompletion;

            if ( slot.mTimestamps ) {
                renderer::rhi::QueryResult begin{}, end{};

                if ( !slot.mTimestamps->TryGetResult( 0, *slot.mFence, slot.mCompletion, begin ) ||
                     !slot.mTimestamps->TryGetResult( 1, *slot.mFence, slot.mCompletion, end ) ) {
                    continue;
                }

                snapshot.mHasTiming = true;
                snapshot.mFrameMilliseconds = slot.mTimestamps->GetElapsedMilliseconds( begin.mValues[0], end.mValues[0] );
                bool allPassesReady{ true };

                for ( core::u32 index{}; index < slot.mPassNames.size(); ++index ) {
                    if ( slot.mTimestamps->TryGetResult( 2 + index * 2, *slot.mFence, slot.mCompletion, begin ) &&
                         slot.mTimestamps->TryGetResult( 3 + index * 2, *slot.mFence, slot.mCompletion, end ) ) {
                        snapshot.mPasses.push_back( { slot.mPassNames[index],
                            slot.mTimestamps->GetElapsedMilliseconds( begin.mValues[0], end.mValues[0] ) } );
                    }
                    else {
                        allPassesReady = false;
                        break;
                    }
                }

                if ( !allPassesReady ) {
                    continue;
                }
            }

            if ( slot.mStatistics && !slot.mStatistics->TryGetResult( 0, *slot.mFence, slot.mCompletion, snapshot.mStatistics ) ) {
                continue;
            }

            if ( snapshot.mSubmission > mSnapshot.mSubmission ) {
                mSnapshot = eastl::move( snapshot );
            }

            slot.mPending = false;
        }
    }

    auto GpuQueryProfiler::BeginFrame( renderer::rhi::ICommandList& commands, renderer::rhi::FenceHandle completion ) -> void {
        mRecording = nullptr;

        try {
            Collect();

            if ( !mEnabled ) {
                return;
            }

            if ( !completion ) {
                MKT_THROW_INVALID_ARGUMENT( "GPU profiling requires a submission completion fence." );
            }

            for ( auto& slot: mSlots ) {
                if ( slot.mPending ) {
                    continue;
                }

                InitializeSlot( slot );

                if ( !slot.mTimestamps && !slot.mStatistics ) {
                    mError = "This backend exposes no usable timing or pipeline statistics queries.";
                    mEnabled = false;
                    return;
                }

                slot.mFence = completion;
                slot.mPassNames.clear();
                slot.mPassActive = false;

                if ( slot.mTimestamps ) {
                    commands.Reset( slot.mTimestamps.GetPtr() );
                    commands.WriteTimestamp( slot.mTimestamps.GetPtr(), 0 );
                }

                if ( slot.mStatistics ) {
                    commands.Reset( slot.mStatistics.GetPtr() );
                    commands.BeginQuery( slot.mStatistics.GetPtr(), 0 );
                }

                mError.clear();
                mRecording = &slot;
                return;
            }

            ++mDropped;
        } catch ( const std::exception& error ) {
            mError = error.what();
            mEnabled = false;
        }
    }

    auto GpuQueryProfiler::BeginPass( renderer::rhi::ICommandList& commands, eastl::string_view name ) -> void {
        if ( !mRecording || !mRecording->mTimestamps ) {
            return;
        }

        if ( mRecording->mPassActive ) {
            MKT_THROW_LOGIC_ERROR( "GPU profiling pass ranges must not overlap." );
        }

        if ( mRecording->mPassNames.size() >= kMaxPasses ) {
            mRecording->mPassActive = false;
            return;
        }

        const core::u32 index{ static_cast<core::u32>( mRecording->mPassNames.size() ) };
        mRecording->mPassNames.emplace_back( name.data(), name.size() );
        commands.WriteTimestamp( mRecording->mTimestamps.GetPtr(), 2 + index * 2 );
        mRecording->mPassActive = true;
    }

    auto GpuQueryProfiler::EndPass( renderer::rhi::ICommandList& commands ) -> void {
        if ( mRecording && mRecording->mPassActive ) {
            commands.WriteTimestamp( mRecording->mTimestamps.GetPtr(), 1 + static_cast<core::u32>( mRecording->mPassNames.size() ) * 2 );
            mRecording->mPassActive = false;
        }
    }

    auto GpuQueryProfiler::EndFrame( renderer::rhi::ICommandList& commands ) -> void {
        if ( !mRecording ) {
            return;
        }

        EndPass( commands );

        if ( mRecording->mStatistics ) {
            commands.EndQuery( mRecording->mStatistics.GetPtr(), 0 );
            commands.Resolve( mRecording->mStatistics.GetPtr(), 0, 1 );
        }

        if ( mRecording->mTimestamps ) {
            commands.WriteTimestamp( mRecording->mTimestamps.GetPtr(), 1 );
            commands.Resolve( mRecording->mTimestamps.GetPtr(), 0, 2 + static_cast<core::u32>( mRecording->mPassNames.size() ) * 2 );
        }
    }

    auto GpuQueryProfiler::Submitted( core::u64 value ) -> void {
        if ( mRecording ) {
            mRecording->mCompletion = value;
            mRecording->mPending = true;
            mRecording = nullptr;
        }
    }
}// namespace mikoto::renderer
