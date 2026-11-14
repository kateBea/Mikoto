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

#include <Logging/Logger.hh>

#include <Threading/TaskService.hh>

#include <Assets/AssetStreamingSystem.hh>

namespace mikoto::asset {

    AssetStreamingSystem::AssetStreamingSystem( const AssetStreamingSettings& settings )
        : mSettings{ settings } {
    }

    auto AssetStreamingSystem::Initialize() -> void {
        const std::lock_guard lock{ mMutex };

        if ( mInitialized ) {
            return;
        }

        mSettings.mConcurrentLoads = eastl::max( core::u32{ 1 }, mSettings.mConcurrentLoads );
        mCompletedLoads = 0;
        mTotalLoads = 0;
        mAccepting = true;
        mInitialized = true;
        mLastCollection = std::chrono::steady_clock::now();
    }

    auto AssetStreamingSystem::StopRequests() -> void {
        eastl::vector<Job> discarded{};

        const std::lock_guard lock{ mMutex };
        mAccepting = false;
        discarded.swap( mQueued );
        mTotalLoads -= discarded.size();
    }

    auto AssetStreamingSystem::Shutdown() -> void {
        {
            const std::lock_guard lock{ mMutex };

            if ( !mInitialized ) {
                return;
            }
        }

        StopRequests();

        // Active loads may still use importer services and resident registries.
        // Wait for their completion before releasing any cached assets.
        {
            std::unique_lock lock{ mMutex };
            mCompletion.wait( lock, [this] {
                return mActive == 0;
            } );
        }

        eastl::apply( []( auto&... registries ) {
            ( registries.Clear(), ... );
        }, mRegistries );

        {
            const std::lock_guard lock{ mMutex };
            mInitialized = false;
        }
    }

    auto AssetStreamingSystem::Enqueue( eastl::function<void()> job, LoadPriority priority, AssetID id, AssetMemoryUsage reservation ) -> void {
        const std::lock_guard lock{ mMutex };

        if ( !mAccepting ) {
            MKT_THROW_RUNTIME_ERROR( "Asset streaming is shutting down." );
        }

        if ( !reservation.mCpuBytes && !reservation.mGpuBytes ) {
            reservation = { core::usize{ 64 } * 1024 * 1024, core::usize{ 64 } * 1024 * 1024 };
        }

        const bool newBatch{ mQueued.empty() && mActive == 0 };
        mQueued.push_back( { .mID = id,
            .mPriority = priority,
            .mExecute = eastl::move( job ),
            .mReservation = reservation } );

        // New work after an idle period starts a fresh batch rather than a lifetime percentage.
        if ( newBatch ) {
            mCompletedLoads = 0;
            mTotalLoads = 0;
        }

        ++mTotalLoads;
    }

    auto AssetStreamingSystem::Promote( AssetID id, LoadPriority priority ) -> void {
        const std::lock_guard lock{ mMutex };

        for ( auto& job: mQueued ) {
            if ( job.mID == id ) {
                job.mPriority = eastl::max( job.mPriority, priority );
            }
        }
    }

    auto AssetStreamingSystem::Update( bool loadOnOwnerThread ) -> void {
        Job job{};

        if ( loadOnOwnerThread ) {
            // These importers create GPU resources before returning. Run one per tick on the context thread.
            if ( TryDequeue( job ) ) {
                ExecuteJob( job );
            }
        }
        else {
            while ( TryDequeue( job ) ) {
                SubmitJob( eastl::move( job ) );
            }
        }

        CollectUnusedAssets();
    }

    auto AssetStreamingSystem::TryDequeue( Job& job ) -> bool {
        const std::lock_guard lock{ mMutex };

        if ( !mAccepting || mActive >= mSettings.mConcurrentLoads || mQueued.empty() ) {
            return false;
        }

        // Only replace the candidate for a higher priority, preserving FIFO
        // order among requests with the same priority.
        core::usize selectedIndex{};

        for ( core::usize index{ 1 }; index < mQueued.size(); ++index ) {
            if ( mQueued[index].mPriority > mQueued[selectedIndex].mPriority ) {
                selectedIndex = index;
            }
        }

        // Check the remaining capacity without overflowing the byte counters.
        // An oversized import may run alone so it is not blocked indefinitely.
        const auto& reservation{ mQueued[selectedIndex].mReservation };
        const auto& budget{ mSettings.mInFlightBudget };

        const bool cpuFits{ mReserved.mCpuBytes <= budget.mCpuBytes &&
                            reservation.mCpuBytes <= budget.mCpuBytes - mReserved.mCpuBytes };
        const bool gpuFits{ mReserved.mGpuBytes <= budget.mGpuBytes &&
                            reservation.mGpuBytes <= budget.mGpuBytes - mReserved.mGpuBytes };

        if ( mActive != 0 && ( !cpuFits || !gpuFits ) ) {
            return false;
        }

        // Reserve the slot before handing the job to a worker.
        mLoadingIDs.push_back( mQueued[selectedIndex].mID );
        mReserved.mCpuBytes += reservation.mCpuBytes;
        mReserved.mGpuBytes += reservation.mGpuBytes;
        ++mActive;

        job = eastl::move( mQueued[selectedIndex] );
        mQueued.erase( mQueued.begin() + selectedIndex );

        return true;
    }

    auto AssetStreamingSystem::SubmitJob( Job job ) -> void {
        try {
            // Keep the original callable available if submission throws.
            threading::TaskService::Get()->ExecuteWorkerTask( [this, job]() mutable {
                ExecuteJob( job );
            } );
        } catch ( ... ) {
            const std::lock_guard lock{ mMutex };

            // Undo admission and restore the request for a later update.
            mReserved.mCpuBytes -= job.mReservation.mCpuBytes;
            mReserved.mGpuBytes -= job.mReservation.mGpuBytes;
            --mActive;
            mLoadingIDs.erase( eastl::find( mLoadingIDs.begin(), mLoadingIDs.end(), job.mID ) );

            mQueued.push_back( eastl::move( job ) );
            mCompletion.notify_all();

            throw;
        }
    }

    auto AssetStreamingSystem::ExecuteJob( Job& job ) -> void {
        try {
            job.mExecute();
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_ERROR( "Asset streaming job failed: {}", error.what() );
        } catch ( ... ) {
            MKT_CORE_LOGGER_ERROR( "Asset streaming job failed with an unknown exception." );
        }

        // Release captured asset handles before publishing completion. Shutdown
        // can destroy this system once the final active worker releases the lock.
        job.mExecute = {};

        const std::lock_guard lock{ mMutex };
        mReserved.mCpuBytes -= job.mReservation.mCpuBytes;
        mReserved.mGpuBytes -= job.mReservation.mGpuBytes;
        --mActive;
        ++mCompletedLoads;
        mLoadingIDs.erase( eastl::find( mLoadingIDs.begin(), mLoadingIDs.end(), job.mID ) );

        mCompletion.notify_all();
    }

    auto AssetStreamingSystem::GetLoadingProgress() const -> AssetLoadingProgress {
        const std::lock_guard lock{ mMutex };
        AssetLoadingProgress result{};
        result.mCompleted = mCompletedLoads;
        result.mTotal = mTotalLoads;
        result.mLoading = mLoadingIDs;
        result.mQueued.reserve( mQueued.size() );

        for ( const auto& job: mQueued ) {
            result.mQueued.push_back( job.mID );
        }

        return result;
    }

    auto AssetStreamingSystem::CollectUnusedAssets() -> void {
        constexpr std::chrono::seconds collectionInterval{ 1 };
        const auto now{ std::chrono::steady_clock::now() };

        if ( now - mLastCollection < collectionInterval ) {
            return;
        }

        // Memory pressure removes the retention delay. Registry collection still
        // respects active requests and external strong references to each asset.
        const auto usage{ GetPayloadUsage() };
        const bool overBudget{ usage.mCpuBytes > mSettings.mCpuBudgetBytes ||
                               usage.mGpuBytes > mSettings.mGpuBudgetBytes };
        const std::chrono::seconds retention{ overBudget ? 0 : mSettings.mRetentionSeconds };

        // Collect owners first so their texture, audio and font references are
        // released before those registries are visited.
        core::usize retired{};
        eastl::apply( [&]( auto&... registries ) {
            ( ( retired += registries.CollectUnused( retention ) ), ... );
        }, mRegistries );

        const std::lock_guard lock{ mMutex };
        mEvicted += retired;
        mLastCollection = now;
    }

    auto AssetStreamingSystem::GetPayloadUsage() const -> AssetMemoryUsage {
        AssetMemoryUsage result{};
        eastl::apply( [&]( const auto&... registries ) {
            const auto add = [&]( const auto& registry ) {
                const auto memory{ registry.GetMemoryUsage() };
                result.mCpuBytes += memory.mCpuBytes;
                result.mGpuBytes += memory.mGpuBytes;
            };
            ( add( registries ), ... );
        }, mRegistries );

        return result;
    }

    auto AssetStreamingSystem::GetStatistics() const -> AssetStreamingStatistics {
        const std::lock_guard lock{ mMutex };
        const auto usage{ GetPayloadUsage() };
        const bool overBudget{ usage.mCpuBytes > mSettings.mCpuBudgetBytes ||
                               usage.mGpuBytes > mSettings.mGpuBudgetBytes };

        return {
            .mQueued = mQueued.size(),
            .mLoading = mActive,
            .mEvictedAssets = mEvicted,
            .mResidentPayload = usage,
            .mReserved = mReserved,
            .mOverBudget = overBudget
        };
    }
}// namespace mikoto::asset
