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

#ifndef MIKOTO_ASSET_CACHE_HH
#define MIKOTO_ASSET_CACHE_HH

#include <mutex>

#include <ankerl/unordered_dense.h>

#include <Assets/Asset.hh>

namespace mikoto::asset {

    /**
     * Thread-safe registry that coordinates one load operation per asset path.
     *
     * A cache entry is inserted before invoking the loader. Subsequent callers
     * therefore observe @ref LoadState::eLoading and return immediately instead
     * of scheduling or waiting for another load. The loader always runs outside
     * the registry mutex, so unrelated assets can be requested concurrently.
     *
     * The registry does not make an asset loader thread-safe. Loaders must still
     * obey the ownership rules of the systems they call, especially for GPU and
     * editor state.
     *
     * @tparam AssetType Type of asset retained by the registry.
     */
    template<typename AssetType>
    class AssetLoadRegistry {
    public:
        /**
         * Describes the visible state of an asset request.
         */
        enum class LoadState {
            eNotRequested,
            eLoading,
            eReady,
            eFailed
        };

    public:
        /**
         * Returns a ready asset or invokes @p loader exactly once for a new path.
         *
         * When another thread is already loading @p path, this method returns an
         * empty handle immediately. Call @ref GetLoadState or @ref GetIfReady on
         * a later frame instead of waiting on a worker thread.
         *
         * @param path Asset path used as the registry key.
         * @param loader Function that loads the asset for a new request.
         * @returns The ready or newly loaded asset; an empty handle when loading
         *          is already in progress or loading fails.
         * @tparam LoaderFn Callable returning @c Ref<AssetType>.
         */
        template<typename LoaderFn>
        MKT_NODISCARD auto GetOrLoad( const filesystem::Path& path, LoaderFn&& loader ) -> core::Ref<AssetType> {
            const AssetID assetId{ GetHashedAssetID( path ) };
            LoadTicket ticket{};

            {
                std::lock_guard lock{ mMutex };

                auto it{ mEntries.find( assetId ) };
                if ( it != mEntries.end() ) {
                    return it->second.mState == LoadState::eReady
                        ? it->second.mAsset
                        : core::Ref<AssetType>::CreateEmpty();
                }

                ticket = { assetId, mNextGeneration++ };
                mEntries.emplace( assetId, Entry{ .mGeneration = ticket.mGeneration } );
            }

            // The loader may block. Never hold the registry mutex while it runs.
            core::Ref<AssetType> asset{ loader() };
            {
                std::lock_guard lock{ mMutex };

                auto it{ mEntries.find( ticket.mAssetId ) };
                if ( it == mEntries.end() || it->second.mGeneration != ticket.mGeneration ) {
                    // The registry was cleared or this path was invalidated and
                    // requested again while the loader was running.
                    return core::Ref<AssetType>::CreateEmpty();
                }

                it->second.mAsset = asset;
                it->second.mState = asset.IsEmpty() ? LoadState::eFailed : LoadState::eReady;

                return it->second.mAsset;
            }
        }

        /**
         * Returns a loaded asset without starting or waiting for a load.
         *
         * @param path Asset path to query.
         * @returns The ready asset, or an empty handle for every other state.
         */
        MKT_NODISCARD auto GetIfReady( const filesystem::Path& path ) const -> core::Ref<AssetType> {
            std::lock_guard lock{ mMutex };

            auto it{ mEntries.find( GetHashedAssetID(path) ) };
            if ( it == mEntries.end() ) {
                return core::Ref<AssetType>::CreateEmpty();
            }

            if ( it->second.mState == LoadState::eReady ) {
                return it->second.mAsset;
            }

            return core::Ref<AssetType>::CreateEmpty();
        }

        /**
         * Returns the current request state without modifying the registry.
         *
         * @param path Asset path to query.
         * @returns The current load state for @p path.
         */
        MKT_NODISCARD auto GetLoadState( const filesystem::Path& path ) const -> LoadState {
            std::lock_guard lock{ mMutex };

            auto it{ mEntries.find( GetHashedAssetID( path ) ) };
            return it == mEntries.end() ? LoadState::eNotRequested : it->second.mState;
        }

        /**
         * Removes one entry so a subsequent request can load the path again.
         *
         * An in-flight loader is not cancelled. Its completion ticket will no
         * longer match and will be discarded safely.
         *
         * @param path Asset path to invalidate.
         */
        auto Invalidate( const filesystem::Path& path ) -> void {
            std::lock_guard lock{ mMutex };
            mEntries.erase( GetHashedAssetID( path ) );
        }

        /**
         * Removes all retained entries.
         *
         * In-flight loaders are not cancelled; their stale completion tickets are
         * discarded safely.
         */
        auto Clear() -> void {
            std::lock_guard lock{ mMutex };
            mEntries.clear();
        }

    private:
        struct LoadTicket {
            AssetID mAssetId{};
            core::u64 mGeneration{};
        };

        struct Entry {
            core::Ref<AssetType> mAsset{};
            LoadState mState{ LoadState::eLoading };
            core::u64 mGeneration{};
        };

    private:
        mutable std::mutex mMutex{};
        ankerl::unordered_dense::map<AssetID, Entry> mEntries{};
        core::u64 mNextGeneration{ 1 };
    };
}

#endif//MIKOTO_ASSET_CACHE_HH
