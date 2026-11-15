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

#include <EASTL/utility.h>
#include <EASTL/sort.h>
#include <EASTL/algorithm.h>

#include <ankerl/unordered_dense.h>

#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Core/FrameGraphTypes.hh>
#include <Renderer/Core/FrameGraphScheduler.hh>

namespace mikoto::renderer {

    static auto AddFenceValue( auto& entries, const rhi::SignalInfo& info ) -> void {
        for ( auto& entry: entries ) {
            if ( entry.mSignalFence == info.mSignalFence ) {
                entry.mSignalValue = eastl::max( entry.mSignalValue, info.mSignalValue );
                return;
            }
        }

        entries.push_back( info );
    }

    static auto VideoBarrier( rhi::ITexture* texture, bool reference ) -> rhi::TextureBarrierDescription {
        const bool undefined{ texture->GetResourceState() == rhi::ResourceStates::eUnknown };

        return rhi::TextureBarrierDescription{}
            .SetTexture( texture )
            .SetBeforeLayout( GetTextureLayout( texture->GetResourceState() ) )
            .SetBeforeStage( undefined ? rhi::PipelineStageFlagsBits::None : rhi::PipelineStageFlagsBits::AllCommands )
            .SetBeforeAccess( undefined ? rhi::BarrierAccessFlagsBits::None :
                rhi::BarrierAccessFlagsBits::MemoryRead | rhi::BarrierAccessFlagsBits::MemoryWrite )
            .SetAfterLayout( reference ? rhi::TextureLayoutBits::VideoDecodeReference : rhi::TextureLayoutBits::VideoDecodeOutput )
            .SetAfterStage( rhi::PipelineStageFlagsBits::VideoDecode )
            .SetAfterAccess( reference ? rhi::BarrierAccessFlagsBits::VideoDecodeRead | rhi::BarrierAccessFlagsBits::VideoDecodeWrite :
                rhi::BarrierAccessFlagsBits::VideoDecodeWrite );
    }

    FGScheduler::FGScheduler( rhi::IGpuDevice* device )
        : mDevice{ device } {
        if ( !mDevice ) {
            MKT_THROW_INVALID_ARGUMENT( "Frame graph scheduler requires a GPU device." );
        }

        mGraphicsQueue = mDevice->GetQueue( rhi::QueueType::eGraphics );
        mTransferQueue = mDevice->GetQueue( rhi::QueueType::eTransfer );
        mComputeQueue = mDevice->GetQueue( rhi::QueueType::eCompute );
        mVideoDecodeQueue = mDevice->GetQueue( rhi::QueueType::eVideoDecode );
        mVideoEnCodeQueue = mDevice->GetQueue( rhi::QueueType::eVideoEncode );
    }

    auto FGScheduler::PushRenderCommandList( const rhi::SubmitInfo& info ) -> void {
        auto submission{ info };

        if ( mVideoReady ) {
            submission.AddWait( mVideoFence, mVideoReady );
            mVideoReady = 0;
        }

        PushCommandList( submission, mGraphicsQueue, rhi::QueueType::eGraphics );
    }

    auto FGScheduler::BeginVideoWork() -> void {
        mVideoOutputs.clear();
        mVideoUsed = false;
        mReleaseRecording = false;
    }

    auto FGScheduler::HasVideoWork() const -> bool {
        return mVideoUsed;
    }

    auto FGScheduler::UseVideoResource(
        rhi::ICommandList* commands,
        rhi::TextureHandle texture,
        bool reference, bool graphicsOutput ) -> void
    {
        if ( !mVideoDecodeQueue || !texture ) {
            MKT_THROW_RUNTIME_ERROR( "A video pass requires a decode queue and an image." );
        }

        // An idle video pass must not leave an unsubmitted recording behind.
        if ( !mVideoUsed ) {
            commands->Open( { .mScopeName = "Frame graph video decode" } );
        }

        const auto state{ texture->GetResourceState() };
        const bool graphicsOwned{ state != rhi::ResourceStates::eUnknown &&
            state != rhi::ResourceStates::eVideoDecodeOutput && state != rhi::ResourceStates::eVideoDecodeReference };
        auto barrier{ VideoBarrier( texture.GetPtr(), reference ) };

        // Release previous graphics readers before recycling an output on the video queue.
        // https://docs.vulkan.org/spec/latest/chapters/synchronization.html#synchronization-queue-transfers
        if ( graphicsOwned && mVideoDecodeQueue != mGraphicsQueue ) {
            if ( !mGraphicsRelease ) {
                mGraphicsRelease = mDevice->CreateCommandList( rhi::QueueType::eGraphics );
                mGraphicsRelease->SetEnableAutomaticBarriers( false );
            }

            if ( !mReleaseRecording ) {
                mGraphicsRelease->Open( { .mScopeName = "Release video images from graphics" } );
                mReleaseRecording = true;
            }

            mGraphicsRelease->ReleaseResource( barrier, mVideoDecodeQueue );
            commands->AcquireResource( barrier, mGraphicsQueue );
        } else {
            commands->SetBarrier( rhi::BarrierDescription{}.AddTexture( barrier ) );
        }

        texture->SetResourceState( reference ? rhi::ResourceStates::eVideoDecodeReference : rhi::ResourceStates::eVideoDecodeOutput );
        mVideoUsed = true;

        if ( graphicsOutput && eastl::find( mVideoOutputs.begin(), mVideoOutputs.end(), texture ) == mVideoOutputs.end() ) {
            mVideoOutputs.push_back( eastl::move( texture ) );
        }
    }

    auto FGScheduler::RestoreGraphicsResources( rhi::ICommandList* graphics, rhi::ICommandList& video ) -> void {
        for ( auto texture: mVideoOutputs ) {
            auto barrier{ rhi::TextureBarrierDescription{}
                .SetTexture( texture.GetPtr() )
                .SetBeforeLayout( GetTextureLayout( texture->GetResourceState() ) )
                .SetBeforeStage( rhi::PipelineStageFlagsBits::VideoDecode )
                .SetBeforeAccess( rhi::BarrierAccessFlagsBits::VideoDecodeRead | rhi::BarrierAccessFlagsBits::VideoDecodeWrite )
                .SetAfterLayout( rhi::TextureLayoutBits::General )
                .SetAfterStage( rhi::PipelineStageFlagsBits::AllCommands )
                .SetAfterAccess( rhi::BarrierAccessFlagsBits::MemoryRead | rhi::BarrierAccessFlagsBits::MemoryWrite ) };

            if ( mVideoDecodeQueue != mGraphicsQueue ) {
                video.ReleaseResource( barrier, mGraphicsQueue );
                graphics->AcquireResource( barrier, mVideoDecodeQueue );
            } else {
                graphics->SetBarrier( rhi::BarrierDescription{}.AddTexture( barrier ) );
            }

            texture->SetResourceState( rhi::ResourceStates::eCommon );
        }
    }

    auto FGScheduler::PushVideoCommandList( rhi::CommandListHandle commands ) -> void {
        if ( !mVideoUsed ) {
            return;
        }

        if ( !mVideoFence ) {
            mVideoFence = mDevice->CreateFence( 0 );
        }

        auto videoSubmitInfo{ rhi::SubmitInfo{}
            .AddCommandList( eastl::move( commands ) )
        };

        if ( mReleaseRecording ) {
            mGraphicsRelease->Close();
            const auto released{ ++mVideoFenceValue };

            PushCommandList(
                rhi::SubmitInfo{}
                    .AddCommandList( mGraphicsRelease )
                    .AddSignal( mVideoFence, released ),
                mGraphicsQueue, rhi::QueueType::eGraphics );

            videoSubmitInfo.AddWait( mVideoFence, released );
        }

        mVideoReady = ++mVideoFenceValue;
        videoSubmitInfo.AddSignal( mVideoFence, mVideoReady );

        PushCommandList( videoSubmitInfo, mVideoDecodeQueue, rhi::QueueType::eVideoDecode );
    }

    auto FGScheduler::PushComputeCommandList( const rhi::SubmitInfo& info ) -> void {
        PushCommandList( info, mComputeQueue, rhi::QueueType::eCompute );
    }

    auto FGScheduler::PushTransferCommandList( const rhi::SubmitInfo& info ) -> void {
        PushCommandList( info, mTransferQueue, rhi::QueueType::eTransfer );
    }

    auto FGScheduler::PushCommandList( const rhi::SubmitInfo& info, rhi::IQueue* queue, rhi::QueueType type ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !queue ) {
            MKT_THROW_INVALID_ARGUMENT( "Frame graph submission targets an unavailable queue." );
        }

        if ( info.mCommands.empty() && info.mWaits.empty() && info.mSignals.empty() ) {
            return;
        }

        Submission submission{ .mQueue = queue, .mQueueType = type };
        ankerl::unordered_dense::set<const rhi::ICommandList*> commands{};

        for ( const auto& command: info.mCommands ) {
            if ( !command || command->GetOwningDevice() != mDevice ||
                 mDevice->GetQueue( command->GetQueueType() ) != queue ) {
                MKT_THROW_INVALID_ARGUMENT( "Frame graph command list must belong to the target device and queue." );
            }

            if ( !commands.insert( command.GetPtr() ).second ) {
                MKT_THROW_INVALID_ARGUMENT( "Frame graph submission contains the same command list twice." );
            }

            submission.mInfo.mCommands.push_back( command );
        }

        for ( const auto& wait: info.mWaits ) {
            if ( !wait.mSignalFence || wait.mSignalFence->GetOwningDevice() != mDevice ) {
                MKT_THROW_INVALID_ARGUMENT( "Frame graph wait requires a fence from the target device." );
            }

            AddFenceValue( submission.mInfo.mWaits, wait );
        }

        for ( const auto& signal: info.mSignals ) {
            if ( !signal.mSignalFence || signal.mSignalFence->GetOwningDevice() != mDevice ) {
                MKT_THROW_INVALID_ARGUMENT( "Frame graph signal requires a fence from the target device." );
            }

            AddFenceValue( submission.mInfo.mSignals, signal );
        }

        for ( const auto& signal: submission.mInfo.mSignals ) {
            for ( const auto& wait: submission.mInfo.mWaits ) {
                if ( signal.mSignalFence == wait.mSignalFence && signal.mSignalValue <= wait.mSignalValue ) {
                    MKT_THROW_INVALID_ARGUMENT( "A submission must signal a value greater than its wait on the same fence." );
                }
            }
        }

        const std::lock_guard lock{ mMutex };

        for ( const auto& pending: mSubmissions ) {
            for ( const auto& command: pending.mInfo.mCommands ) {
                if ( commands.contains( command.GetPtr() ) ) {
                    MKT_THROW_INVALID_ARGUMENT( "A command list cannot be queued for execution more than once." );
                }
            }
        }

        mSubmissions.push_back( eastl::move( submission ) );
    }

    auto FGScheduler::ResolveDependencies() -> void {
        MKT_BEGIN_PROFILER();
        struct Signal {
            core::usize mSubmission{};
            core::u64 mValue{};
        };

        struct Fence {
            rhi::FenceHandle mHandle{};
            core::u64 mCompleted{};
            eastl::vector<Signal> mSignals{};
        };
        
        ankerl::unordered_dense::map<const rhi::IFence*, Fence> fences{};
        const auto fenceState = [&]( const rhi::FenceHandle& fence ) -> Fence& {
            auto [entry, inserted]{ fences.try_emplace( fence.GetPtr() ) };

            if ( inserted ) {
                entry->second.mHandle = fence;
                entry->second.mCompleted = fence->GetCompletionValue();
            }

            return entry->second;
        };

        eastl::vector<eastl::vector<core::usize>> successors( mSubmissions.size() );
        eastl::vector<core::usize> inDegree( mSubmissions.size(), 0 );
        const auto depend = [&]( core::usize producer, core::usize consumer ) {
            if ( producer != consumer &&
                 eastl::find( successors[producer].begin(), successors[producer].end(), consumer ) == successors[producer].end() ) {
                successors[producer].push_back( consumer );
                ++inDegree[consumer];
            }
        };

        for ( core::usize index{}; index < mSubmissions.size(); ++index ) {
            for ( const auto& signal: mSubmissions[index].mInfo.mSignals ) {
                auto& fence{ fenceState( signal.mSignalFence ) };

                if ( signal.mSignalValue <= fence.mCompleted ) {
                    MKT_THROW_INVALID_ARGUMENT( "Frame graph fence signals must increase beyond their completed value." );
                }

                fence.mSignals.push_back( Signal{ index, signal.mSignalValue } );
            }
        }

        for ( auto& [handle, fence]: fences ) {
            eastl::sort( fence.mSignals.begin(), fence.mSignals.end(), []( const Signal& left, const Signal& right ) {
                return left.mValue < right.mValue;
            } );

            for ( core::usize index{ 1 }; index < fence.mSignals.size(); ++index ) {
                const auto& previous{ fence.mSignals[index - 1] };
                const auto& signal{ fence.mSignals[index] };

                if ( previous.mValue == signal.mValue ) {
                    MKT_THROW_INVALID_ARGUMENT( "Two frame graph submissions cannot signal the same fence value." );
                }

                depend( previous.mSubmission, signal.mSubmission );
            }
        }

        for ( core::usize index{}; index < mSubmissions.size(); ++index ) {
            for ( const auto& wait: mSubmissions[index].mInfo.mWaits ) {
                const auto& fence{ fenceState( wait.mSignalFence ) };

                if ( wait.mSignalValue <= fence.mCompleted ) {
                    continue;
                }

                for ( const auto& signal: fence.mSignals ) {
                    if ( signal.mValue >= wait.mSignalValue ) {
                        // A wait preceding this submission's own signal needs an external producer.
                        depend( signal.mSubmission, index );
                        break;
                    }
                }
            }
        }

        mExecutionOrder.clear();
        mExecutionOrder.reserve( mSubmissions.size() );
        eastl::vector<bool> scheduled( mSubmissions.size(), false );

        while ( mExecutionOrder.size() < mSubmissions.size() ) {
            core::usize next{ mSubmissions.size() };
            // A stable choice keeps the original push order when no dependency requires reordering.
            for ( core::usize index{}; index < mSubmissions.size(); ++index ) {
                if ( !scheduled[index] && inDegree[index] == 0 ) {
                    next = index;
                    break;
                }
            }

            if ( next == mSubmissions.size() ) {
                MKT_THROW_RUNTIME_ERROR( "Frame graph submissions contain a fence dependency cycle." );
            }

            scheduled[next] = true;
            mExecutionOrder.push_back( next );

            for ( const auto successor: successors[next] ) {
                --inDegree[successor];
            }
        }

        // Host submission order alone cannot order signals on different GPU queues.
        // Chain increasing values of the same timeline with an actual GPU wait.
        for ( const auto& [handle, fence]: fences ) {
            for ( core::usize index{ 1 }; index < fence.mSignals.size(); ++index ) {
                const auto& previous{ fence.mSignals[index - 1] };
                const auto& signal{ fence.mSignals[index] };

                if ( mSubmissions[previous.mSubmission].mQueue != mSubmissions[signal.mSubmission].mQueue ) {
                    AddFenceValue( mSubmissions[signal.mSubmission].mInfo.mWaits,
                        rhi::SignalInfo{ previous.mValue, fence.mHandle } );
                }
            }
        }
    }

    auto FGScheduler::Execute() -> void {
        MKT_BEGIN_PROFILER();
        const std::lock_guard lock{ mMutex };

        if ( mSubmissions.empty() ) {
            return;
        }

        ResolveDependencies();

        core::usize enqueued{};

        try {
            for ( const auto index: mExecutionOrder ) {
                // Keep native execution on the render-context thread, including OpenGL.
                // Separate batches preserve waits/signals even when queue roles alias.
                auto info{ mSubmissions[index].mInfo };
                RenderSystem::Get()->BatchSubmission( eastl::move( info ), mSubmissions[index].mQueueType );
                ++enqueued;
            }
        } catch ( ... ) {
            // Accepted batches already belong to the render context; do not enqueue them twice on retry.
            for ( core::usize index{}; index < enqueued; ++index ) {
                mSubmissions[mExecutionOrder[index]].mQueue = nullptr;
            }

            eastl::erase_if( mSubmissions, []( const Submission& submission ) {
                return !submission.mQueue;
            } );

            mExecutionOrder.clear();
            throw;
        }

        mSubmissions.clear();
        mExecutionOrder.clear();
    }
}// namespace mikoto::renderer