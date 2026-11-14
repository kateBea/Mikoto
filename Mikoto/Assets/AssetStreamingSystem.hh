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

#ifndef MIKOTO_ASSETS_ASSET_STREAMING_SYSTEM_HH
#define MIKOTO_ASSETS_ASSET_STREAMING_SYSTEM_HH

#include <mutex>
#include <chrono>
#include <type_traits>
#include <condition_variable>

#include <EASTL/tuple.h>
#include <EASTL/vector.h>
#include <EASTL/algorithm.h>
#include <EASTL/functional.h>
#include <EASTL/type_traits.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>

#include <Assets/AssetFont.hh>
#include <Assets/AssetModel.hh>
#include <Assets/AssetVideo.hh>
#include <Assets/AssetSkybox.hh>
#include <Assets/AssetSprite.hh>
#include <Assets/AssetTexture.hh>
#include <Assets/AssetMaterial.hh>
#include <Assets/AssetSkeleton.hh>
#include <Assets/AssetAudioClip.hh>
#include <Assets/AssetAnimation.hh>
#include <Assets/AssetLoadRegistry.hh>
#include <Assets/AssetScriptSource.hh>
#include <Assets/AssetParticleEffect.hh>

#include <Material/Material.hh>

#include <Audio/AudioClip.hh>

#include <Renderer/Text/Font.hh>
#include <Renderer/Rhi/Texture.hh>

namespace mikoto::asset {

    struct AssetStreamingSettings {
        // Importers already use bounded parallel jobs internally. Avoid running
        // many complete model imports concurrently on top of that worker budget.
        core::u32 mConcurrentLoads{ 8 };
        core::u32 mRetentionSeconds{ 10 };

        // Admission estimates limit overlapping imports. Unknown source work
        // reserves a conservative slot; a single oversized asset may run alone.
        AssetMemoryUsage mInFlightBudget{ core::usize{ 512 } * 1024 * 1024, core::usize{ 256 } * 1024 * 1024 };

        // Soft payload budgets: requested or externally referenced assets cannot
        // be forcibly freed. These exclude renderer targets and GPU driver heaps.
        core::usize mCpuBudgetBytes{ core::usize{ 512 } * 1024 * 1024 };
        core::usize mGpuBudgetBytes{ core::usize{ 2048 } * 1024 * 1024 };
    };

    struct AssetStreamingStatistics {
        core::usize mQueued{};
        core::usize mLoading{};
        core::usize mEvictedAssets{};

        AssetMemoryUsage mResidentPayload{};
        AssetMemoryUsage mReserved{};

        bool mOverBudget{};
    };

    /** Lightweight snapshot of the current loading batch, without inspecting resident payloads. */
    struct AssetLoadingProgress {
        core::usize mCompleted{};
        core::usize mTotal{};
        eastl::vector<AssetID> mQueued{};
        eastl::vector<AssetID> mLoading{};
    };

    /**
     * Owns resident asset registries and bounded asynchronous work. AssetRegistryService
     * remains the public facade and owns metadata independently of these caches.
     * Update runs on the engine thread; loaders never modify scenes or editor UI.
     */
    class AssetStreamingSystem final : public core::IService {
    public:
        explicit AssetStreamingSystem( const AssetStreamingSettings& settings = {} );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto Update( bool loadOnOwnerThread = false ) -> void;
        auto StopRequests() -> void;

        auto Promote( AssetID id, LoadPriority priority ) -> void;
        auto Enqueue( eastl::function<void()> job, LoadPriority priority, AssetID id = 0, AssetMemoryUsage reservation = {} ) -> void;

        MKT_NODISCARD auto GetStatistics() const -> AssetStreamingStatistics;

        MKT_NODISCARD auto GetLoadingProgress() const -> AssetLoadingProgress;

        template<typename T>
        MKT_NODISCARD auto GetRegistry() -> AssetLoadRegistry<T>& {
            return eastl::get<AssetLoadRegistry<T>>( mRegistries );
        }

        template<typename T>
        MKT_NODISCARD auto GetRegistry() const -> const AssetLoadRegistry<T>& {
            return eastl::get<AssetLoadRegistry<T>>( mRegistries );
        }

        ~AssetStreamingSystem() override = default;

    private:
        struct Job {
            AssetID mID{};
            LoadPriority mPriority{};
            eastl::function<void()> mExecute{};
            AssetMemoryUsage mReservation{};
        };

    private:
        auto SubmitJob( Job job ) -> void;
        auto ExecuteJob( Job& job ) -> void;
        auto CollectUnusedAssets() -> void;

        MKT_NODISCARD auto TryDequeue( Job& job ) -> bool;
        MKT_NODISCARD auto GetPayloadUsage() const -> AssetMemoryUsage;

    private:
        mutable std::mutex mMutex{};

        eastl::vector<Job> mQueued{};
        AssetStreamingSettings mSettings{};
        std::condition_variable mCompletion{};

        core::usize mActive{};
        eastl::vector<AssetID> mLoadingIDs{};
        core::usize mCompletedLoads{};
        core::usize mTotalLoads{};
        core::usize mEvicted{};
        AssetMemoryUsage mReserved{};

        std::chrono::steady_clock::time_point mLastCollection{};

        eastl::tuple<
            AssetLoadRegistry<Model>,
            AssetLoadRegistry<ScriptSource>,
            AssetLoadRegistry<Texture>,
            AssetLoadRegistry<AudioClip>,
            AssetLoadRegistry<Font>,
            AssetLoadRegistry<Material>,
            AssetLoadRegistry<Animation>,
            AssetLoadRegistry<Skeleton>,
            AssetLoadRegistry<Skybox>,
            AssetLoadRegistry<Sprite>,
            AssetLoadRegistry<ParticleEffect>,
            AssetLoadRegistry<VideoClip>> mRegistries{};

        bool mAccepting{};
        bool mInitialized{};
    };
}// namespace mikoto::asset
#endif
