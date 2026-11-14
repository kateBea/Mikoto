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

#ifndef MIKOTO_ASSETS_ASSET_LOAD_REGISTRY_HH
#define MIKOTO_ASSETS_ASSET_LOAD_REGISTRY_HH

#include <mutex>
#include <chrono>
#include <exception>

#include <EASTL/utility.h>
#include <EASTL/vector.h>
#include <EASTL/chrono.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/unordered_map.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Assets/Asset.hh>

namespace mikoto::asset {

    template<typename T>
    class AssetLoadRegistry {
    public:
        using LoadState = AssetState;

        /** Blocking import. Takes over queued dependencies; joins an import already running. */
        template<typename Loader>
        MKT_NODISCARD auto GetOrLoad( const filesystem::Path& path, Loader&& loader ) -> core::Ref<T> {
            return LoadBlocking( GetEntry( path ), eastl::forward<Loader>( loader ) );
        }

        /** Blocking import with a retained demand token and serializable identity. */
        template<typename Loader>
        MKT_NODISCARD auto LoadHandle( const filesystem::Path &path, Loader &&loader ) -> AssetHandle<T> {
            auto entry{ GetEntry( path ) };
            AssetHandle<T> handle{};
            handle.mEntry = entry;
            {
                const std::lock_guard lock{ entry->mMutex };
                handle.mDemand = entry->mDemand.lock();
                if ( !handle.mDemand ) {
                    handle.mDemand = eastl::make_shared<AssetDemand>();
                    entry->mDemand = handle.mDemand;
                }
            }
            LoadBlocking( entry, eastl::forward<Loader>( loader ) );
            return handle;
        }

        /** Scheduler must execute accepted jobs, or discard them only during shutdown. */
        template<typename Loader, typename Scheduler>
        MKT_NODISCARD auto Request( const filesystem::Path &path, Loader &&loader, Scheduler &&schedule ) -> AssetHandle<T> {
            AssetHandle<T> handle{};
            handle.mEntry = GetEntry( path );
            bool start{};
            {
                const std::lock_guard lock{ handle.mEntry->mMutex };
                handle.mDemand = handle.mEntry->mDemand.lock();
                if ( !handle.mDemand ) {
                    handle.mDemand = eastl::make_shared<AssetDemand>();
                    handle.mEntry->mDemand = handle.mDemand;
                }
                handle.mEntry->mLastUse = std::chrono::steady_clock::now();
                start = handle.mEntry->mState == LoadState::eUnloaded && !handle.mEntry->mInvalidated;
                if ( start ) { handle.mEntry->mState = LoadState::eQueued; }
            }
            if ( start ) {
                try {
                    schedule( [entry = handle.mEntry, load = eastl::forward<Loader>( loader )]() mutable {
                        Load( entry, load, true );
                    } );
                } catch ( ... ) {
                    const std::lock_guard lock{ handle.mEntry->mMutex };
                    handle.mEntry->mState = LoadState::eUnloaded;
                    throw;
                }
            }
            return handle;
        }

        MKT_NODISCARD auto GetIfReady( const filesystem::Path &path ) const -> core::Ref<T> {
            const auto entry{ FindEntry( path ) };
            if ( !entry ) { return {}; }
            const std::lock_guard lock{ entry->mMutex };
            entry->mLastUse = std::chrono::steady_clock::now();
            return entry->mState == LoadState::eReady && !entry->mInvalidated ? entry->mAsset : core::Ref<T>{};
        }

        MKT_NODISCARD auto GetLoadState( const filesystem::Path &path ) const -> LoadState {
            const auto entry{ FindEntry( path ) };
            if ( !entry ) { return LoadState::eUnloaded; }
            const std::lock_guard lock{ entry->mMutex };
            return entry->mInvalidated ? LoadState::eUnloaded : entry->mState;
        }

        /** Old handles become unloaded. Already borrowed pointers remain alive with their handles. */
        auto Invalidate( const filesystem::Path& path ) -> void {
            EntryPtr retired{};
            {
                const std::lock_guard lock{ mMutex };
                const auto it{ mEntries.find( GetHashedAssetID( path ) ) };
                if ( it == mEntries.end() ) { return; }
                retired = it->second;
                {
                    const std::lock_guard entryLock{ retired->mMutex };
                    retired->mInvalidated = true;
                    retired->BumpRevision();
                    retired->mCompletion.notify_all();
                }
                mEntries.erase( it );
            }
        }

        auto Clear() -> void {
            decltype( mEntries ) retired{};
            {
                const std::lock_guard lock{ mMutex };
                for ( const auto& [id, entry]: mEntries ) {
                    const std::lock_guard entryLock{ entry->mMutex };
                    entry->mInvalidated = true;
                    entry->BumpRevision();
                    entry->mCompletion.notify_all();
                }
                retired.swap( mEntries );
            }
        }

        MKT_NODISCARD auto GetMemoryUsage() const -> AssetMemoryUsage {
            AssetMemoryUsage result{};
            const std::lock_guard lock{ mMutex };
            for ( const auto& [id, entry]: mEntries ) {
                const std::lock_guard entryLock{ entry->mMutex };
                if ( entry->mState == LoadState::eReady ) {
                    result.mCpuBytes += entry->mMemory.mCpuBytes;
                    result.mGpuBytes += entry->mMemory.mGpuBytes;
                }
            }
            return result;
        }

        MKT_NODISCARD auto CollectUnused( std::chrono::seconds retention ) -> core::usize {
            eastl::vector<core::Ref<T>> retired{};
            const auto now{ std::chrono::steady_clock::now() };
            {
                const std::lock_guard lock{ mMutex };
                for ( const auto& [id, entry]: mEntries ) {
                    const std::lock_guard entryLock{ entry->mMutex };
                    if ( !entry->mDemand.expired() || now - entry->mLastUse < retention ) { continue; }
                    if constexpr ( requires( const T& value ) { value.CanEvict(); } ) {
                        if ( entry->mAsset && !entry->mAsset->CanEvict() ) { continue; }
                    }
                    if ( entry->mState == LoadState::eReady && entry->mAsset && entry->mAsset->GetRefCount() == 1 ) {
                        retired.push_back( eastl::move( entry->mAsset ) );
                        entry->mState = LoadState::eUnloaded;
                        entry->mMemory = {};
                        entry->BumpRevision();
                    }
                }
            }

            return retired.size();
        }

    private:
        using Entry = AssetLoadEntry<T>;
        using EntryPtr = eastl::shared_ptr<Entry>;

        template<typename Loader>
        MKT_NODISCARD static auto LoadBlocking( const EntryPtr &entry, Loader &&loader ) -> core::Ref<T> {
            {
                std::unique_lock lock{ entry->mMutex };
                entry->mCompletion.wait( lock, [&] { return entry->mState != LoadState::eLoading || entry->mInvalidated; } );
                entry->mLastUse = std::chrono::steady_clock::now();
                if ( ( entry->mState != LoadState::eUnloaded && entry->mState != LoadState::eQueued ) || entry->mInvalidated ) {
                    return entry->mState == LoadState::eReady && !entry->mInvalidated ? entry->mAsset : core::Ref<T>{};
                }
                entry->mState = LoadState::eLoading;
            }
            Load( entry, eastl::forward<Loader>( loader ), false );
            const std::lock_guard lock{ entry->mMutex };
            return entry->mInvalidated ? core::Ref<T>{} : entry->mAsset;
        }

        MKT_NODISCARD auto GetEntry( const filesystem::Path &path ) -> EntryPtr {
            const std::lock_guard lock{ mMutex };
            auto& entry{ mEntries[GetHashedAssetID( path )] };
            if ( !entry ) {
                entry = eastl::make_shared<Entry>();
                entry->mReference = AssetRef<T>{ path };
            }
            return entry;
        }

        MKT_NODISCARD auto FindEntry( const filesystem::Path &path ) const -> EntryPtr {
            const std::lock_guard lock{ mMutex };
            const auto it{ mEntries.find( GetHashedAssetID( path ) ) };
            return it == mEntries.end() ? EntryPtr{} : it->second;
        }

        template<typename Loader>
        static auto Load( const EntryPtr& entry, Loader&& loader, bool requested ) -> void {
            {
                const std::lock_guard lock{ entry->mMutex };
                // A blocking dependency load may have claimed/completed this queued job.
                if ( requested && entry->mState != LoadState::eQueued ) { return; }
                if ( entry->mInvalidated || ( requested && entry->mDemand.expired() ) ) {
                    entry->mState = LoadState::eUnloaded;
                    return;
                }
                entry->mState = LoadState::eLoading;
            }

            core::Ref<T> result{};
            eastl::string error{};
            std::exception_ptr exception{};

            try {
                result = loader();
                if ( !result ) { error = "Asset loader returned no resource."; }
            } catch ( const std::exception& failure ) {
                error = failure.what();
                exception = std::current_exception();
            } catch ( ... ) {
                error = "Asset loader failed with an unknown error.";
                exception = std::current_exception();
            }

            {
                const std::lock_guard lock{ entry->mMutex };
                // Obsolete jobs never republish. The local Ref is destroyed outside this lock.
                if ( !entry->mInvalidated ) {
                    entry->mError = eastl::move( error );
                    entry->mLastUse = std::chrono::steady_clock::now();
                    if ( requested && entry->mDemand.expired() ) {
                        entry->mState = LoadState::eUnloaded;
                    } else {
                        entry->mAsset = result;
                        entry->BumpRevision();
                        if constexpr ( requires( const T& value ) { value.GetResidentMemoryUsage(); } ) {
                            entry->mMemory = result ? result->GetResidentMemoryUsage() : AssetMemoryUsage{};
                        } else if constexpr ( requires( const T& value ) { value.GetStorageSizeBytes(); } ) {
                            entry->mMemory = { result ? result->GetSizeBytes() : 0, result ? result->GetStorageSizeBytes() : 0 };
                        }
                        entry->mState = result ? LoadState::eReady : LoadState::eFailed;
                    }
                }
            }

            entry->mCompletion.notify_all();
            if ( exception && !requested ) { 
                std::rethrow_exception( exception ); 
            }
        }

    private:
        mutable std::mutex mMutex{};
        eastl::unordered_map<AssetID, EntryPtr> mEntries{};
    };
}// namespace mikoto::asset
#endif// MIKOTO_ASSETS_ASSET_LOAD_REGISTRY_HH
