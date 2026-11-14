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

#ifndef MIKOTO_ASSETS_ASSET_HH
#define MIKOTO_ASSETS_ASSET_HH

#include <mutex>
#include <chrono>
#include <condition_variable>

#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/shared_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Resource.hh>

#include <Filesystem/Path.hh>
#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>

namespace mikoto::asset {

    using AssetID = core::u64;

    /**
     * @brief Hashes an asset's absolute path into a registry identifier.
     * @param path Source file path.
     * @returns Path-derived asset identifier.
     */
    MKT_NODISCARD auto GetHashedAssetID( const filesystem::Path& path ) -> AssetID;

    enum class AssetState {
        eUnloaded,
        eQueued,
        eLoading,
        eReady,
        eFailed,
        eEvicting
    };

    enum class LoadPriority {
        eBackground,
        eLow,
        eNormal,
        eHigh,
        eCritical
    };

    enum class AssetType : core::u32 {
        eUnknown,
        eFont,
        eModel,
        eAudio,
        eScript,
        eTexture,
        eMaterial,
        eParticle,
        eMesh,
        eAnimation,
        eSkeleton,
        eSkybox,
        eSprite,
        eVideo,
    };

    struct AssetMemoryUsage {
        core::usize mCpuBytes{};
        core::usize mGpuBytes{};
    };

    /** Registry-owned metadata. Mutations are serialized by the registry's metadata lock. */
    class AssetMetadata final : public core::RevisionTracked {
    public:
        auto SetID( AssetID id ) -> void;
        auto SetType( AssetType type ) -> void;
        auto SetPath( const filesystem::Path& path ) -> void;
        
        auto AddDependency( AssetID dependency ) -> void;

        auto SetPersistent( bool persistent ) -> void;
        auto SetSourceRevision( core::u64 revision ) -> void;
        auto SetMemoryUsage( AssetMemoryUsage memory ) -> void;
        auto SetDependencies( eastl::vector<AssetID> dependencies ) -> void;

        MKT_NODISCARD auto GetID() const -> AssetID;
        MKT_NODISCARD auto GetType() const -> AssetType;
        MKT_NODISCARD auto GetPath() const -> const filesystem::Path&;
        MKT_NODISCARD auto GetMemoryUsage() const -> AssetMemoryUsage;
        MKT_NODISCARD auto GetDependencies() const -> const eastl::vector<AssetID>&;

        // File timestamp/size fingerprint persisted in the cache, distinct from GetRevision().
        MKT_NODISCARD auto GetSourceRevision() const -> core::u64;
        MKT_NODISCARD auto IsPersistent() const -> bool;

    private:
        AssetID mID{};
        AssetType mType{ AssetType::eUnknown };

        filesystem::Path mPath{};

        AssetMemoryUsage mMemory{};
        eastl::vector<AssetID> mDependencies{};

        core::u64 mSourceRevision{};
        bool mPersistent{};
    };

    /** Serializable identity. Constructing a reference never requests a load. */
    template<typename T>
    class AssetRef final {
    public:
        AssetRef() = default;
        explicit AssetRef( const filesystem::Path& path )
            : mId{ path.IsEmpty() ? 0 : GetHashedAssetID( path ) },
              mPath{ path.IsEmpty() ? filesystem::Path{} : filesystem::Path{ path.GetAbsolute() } } {}

        MKT_NODISCARD auto GetID() const -> AssetID { return mId; }
        MKT_NODISCARD auto GetPath() const -> const filesystem::Path& { return mPath; }

    private:
        AssetID mId{};
        filesystem::Path mPath{};
    };

    template<typename T> struct AssetTraits;

    // Handles/tickets share this token; the cache only observes it through a weak pointer.
    // Its lifetime expresses consumer interest independently of loader/cache ownership.
    struct AssetDemand {};

    template<typename T> class AssetHandle;
    template<typename T> class AssetTicket;
    template<typename T> class AssetLoadRegistry;

    template<typename T>
    class AssetCacheEntry final : public core::RevisionTracked {
    private:
        friend class AssetLoadRegistry<T>;
        friend class AssetHandle<T>;

    private:
        mutable std::mutex mMutex{};
        std::condition_variable mCompletion{};

        AssetRef<T> mReference{};
        core::Ref<T> mAsset{};

        eastl::string mError{};
        AssetState mState{ AssetState::eUnloaded };

        AssetMemoryUsage mMemory{};
        eastl::weak_ptr<AssetDemand> mDemand{};
        std::chrono::steady_clock::time_point mLastUse{ std::chrono::steady_clock::now() };

        bool mInvalidated{};
    };

    // Loading and residency operate on the same record, rather than duplicate caches.
    template<typename T>
    using AssetLoadEntry = AssetCacheEntry<T>;

    /** Keeps residency demanded until the last handle/ticket releases its interest.
     * Revisions describe the shared cache payload. Track GetRef().GetID() as well
     * when comparing bindings that can be reassigned to a different asset.
     */
    template<typename T>
    class AssetHandle final {
    public:
        using Revision = core::RevisionTracked::Revision;

        AssetHandle() = default;

        MKT_NODISCARD auto GetState() const -> AssetState {
            if ( !mEntry ) {
                return AssetState::eUnloaded;
            }

            const std::lock_guard lock{ mEntry->mMutex };
            return mEntry->mInvalidated ? AssetState::eUnloaded : mEntry->mState;
        }

        MKT_NODISCARD auto GetRevision() const -> Revision {
            if ( !mEntry ) {
                return 0;
            }

            const std::lock_guard lock{ mEntry->mMutex };
            return mEntry->GetRevision();
        }

        MKT_NODISCARD auto IsRevision( Revision revision ) const -> bool {
            return GetRevision() == revision;
        }

        MKT_NODISCARD auto GetRef() const -> AssetRef<T> {
            return mEntry ? mEntry->mReference : AssetRef<T>{};
        }

        MKT_NODISCARD auto GetIfReady() const -> core::Ref<T> {
            if ( !mEntry ) {
                return {};
            }

            const std::lock_guard lock{ mEntry->mMutex };
            mEntry->mLastUse = std::chrono::steady_clock::now();
            return !mEntry->mInvalidated && mEntry->mState == AssetState::eReady ? mEntry->mAsset : core::Ref<T>{};
        }

        MKT_NODISCARD auto GetError() const -> eastl::string {
            if ( !mEntry ) { 
                return {}; 
            }

            const std::lock_guard lock{ mEntry->mMutex };
            return mEntry->mError;
        }

        MKT_NODISCARD auto IsReady() const -> bool { return GetState() == AssetState::eReady; }
        MKT_NODISCARD auto GetTicket() const -> AssetTicket<T> { return AssetTicket<T>{ *this }; }

        // Importers currently report completion, not byte-level progress.
        MKT_NODISCARD auto GetProgress() const -> core::f32 { return IsReady() ? 1.0f : 0.0f; }
        MKT_NODISCARD explicit operator bool() const noexcept { return static_cast<bool>( mEntry ); }

    private:
        friend class AssetLoadRegistry<T>;

    private:
        eastl::shared_ptr<AssetLoadEntry<T>> mEntry{};
        eastl::shared_ptr<AssetDemand> mDemand{};
    };

    /** A movable loading interest. Cancel releases this ticket, never another consumer. */
    template<typename T>
    class AssetTicket final : public core::INonCopyable {
    public:
        AssetTicket() = default;
        explicit AssetTicket( AssetHandle<T> handle )
            : mHandle{ eastl::move( handle ) } {}

        AssetTicket( AssetTicket&& ) noexcept = default;
        auto operator=( AssetTicket&& ) noexcept -> AssetTicket& = default;

        MKT_NODISCARD auto GetState() const -> AssetState { return mHandle.GetState(); }
        MKT_NODISCARD auto GetProgress() const -> core::f32 { return mHandle.GetProgress(); }
        MKT_NODISCARD auto GetError() const -> eastl::string { return mHandle.GetError(); }

        MKT_NODISCARD auto IsComplete() const -> bool {
            const auto state{ GetState() };
            return state == AssetState::eReady || state == AssetState::eFailed || state == AssetState::eUnloaded;
        }

        auto Cancel() -> void { mHandle = {}; }
    private:
        AssetHandle<T> mHandle{};
    };
}

#endif// MIKOTO_ASSETS_ASSET_HH
