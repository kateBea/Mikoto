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

#ifndef MIKOTO_ASSETS_SERVICE_HH
#define MIKOTO_ASSETS_SERVICE_HH

#include <mutex>

#include <EASTL/tuple.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/utility.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>
#include <Core/ResourcePool.hh>

#include <Assets/Asset.hh>
#include <Assets/Model.hh>
#include <Audio/AudioClip.hh>
#include <Audio/AudioDevice.hh>
#include <Assets/MeshFactory.hh>

#include <Filesystem/FileSystem.hh>

#include <Material/PhysicalMaterial.hh>
#include <Material/PostProcessMaterial.hh>

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Text/Font.hh>

#include <Threading/TaskService.hh>
#include <Renderer/Core/GpuDevice.hh>
#include <Renderer/Core/FontFactory.hh>

namespace mikoto::asset {

    using namespace mikoto::core;
    using namespace mikoto::audio;
    using namespace mikoto::renderer;
    using namespace mikoto::material;

    struct TextureLoadDescription {
        Path mPath{};
        TextureDimension mDimension{ TextureDimension::eTexture2D };

        auto SetPath( const Path& path ) -> TextureLoadDescription&;
        auto SetDimensions( TextureDimension dim ) -> TextureLoadDescription&;
    };

    template<typename AssetType>
    class AssetCache {
    public:
        enum class LoadState {
            eLoading,
            eReady
        };

    public:
        template<typename LoaderFn>
        auto RequestLoad(const Path& path, LoaderFn&& loader) -> void {
            std::unique_lock lock{ mMutex };

            auto it{ mEntries.find(GetHashedAssetID(path)) };

            // Already loading or ready -> do nothing
            if (it != mEntries.end()) {
                return;
            }

            // Create entry in loading state
            auto entry{ eastl::make_unique<Entry>() };
            entry->mState = LoadState::eLoading;

            Entry* entryPtr{ entry.get() };
            mEntries.emplace(GetHashedAssetID(path), eastl::move(entry));

            lock.unlock();

            // Dispatch async task instead of blocking
            threading::TaskService::Get()->Submit([this, entryPtr, loader = std::forward<LoaderFn>(loader)]() mutable {
                Ref<AssetType> asset{ loader() };

                std::lock_guard lock{ mMutex };

                entryPtr->mAsset = asset;
                entryPtr->mState = LoadState::eReady;
                entryPtr->mCv.notify_all(); // optional
            });
        }

        template<typename LoaderFn>
        MKT_NODISCARD auto LoadOrGet( const Path& path, LoaderFn&& loader ) -> Ref<AssetType> {
            Entry* entryPtr{ nullptr };
            {
                std::unique_lock lock{ mMutex };

                auto it{ mEntries.find( GetHashedAssetID(path) ) };
                if ( it == mEntries.end() ) {
                    // Create entry ONLY here
                    auto newEntry{ eastl::make_unique<Entry>() };
                    newEntry->mState = LoadState::eLoading;

                    auto [insertIt, _]{ mEntries.emplace( GetHashedAssetID(path), eastl::move( newEntry ) ) };
                    entryPtr = insertIt->second.get();
                    // We are the loader -> continue below
                } else {
                    entryPtr = it->second.get();

                    if ( entryPtr->mState == LoadState::eReady ) {
                        return entryPtr->mAsset;
                    }

                    // Wait until ready
                    entryPtr->mCv.wait( lock, [entryPtr]() {
                        return entryPtr->mState == LoadState::eReady;
                    } );

                    return entryPtr->mAsset;
                }
            }

            // Outside lock -> perform heavy load
            // We avoid locking above for the loading because we
            // would block the whole cache even for read only purposes
            Ref<AssetType> asset{ loader() };
            {
                std::lock_guard lock{ mMutex };

                entryPtr->mAsset = asset;
                entryPtr->mState = LoadState::eReady;

                entryPtr->mCv.notify_all();

                return entryPtr->mAsset;
            }
        }

        MKT_NODISCARD auto GetIfReady( const Path& path ) const -> Ref<AssetType> {
            std::lock_guard lock{ mMutex };

            auto it{ mEntries.find( GetHashedAssetID(path) ) };
            if ( it == mEntries.end() ) {
                return Ref<AssetType>::CreateEmpty();
            }

            Entry* entry{ it->second.get() };
            if ( entry->mState == LoadState::eReady ) {
                return entry->mAsset;
            }

            return Ref<AssetType>::CreateEmpty();
        }

        auto Clear() -> void {
            std::lock_guard lock{ mMutex };
            mEntries.clear();
        }

        MKT_NODISCARD auto operator[]( const Path& path ) -> Ref<AssetType> {
            return GetIfReady( path );
        }

        MKT_NODISCARD auto operator[]( const Path& path ) const -> Ref<AssetType> {
            return GetIfReady( path );
        }

    private:
        struct Entry {
            Ref<AssetType> mAsset{};
            LoadState mState{ LoadState::eLoading };
            std::condition_variable mCv{};
        };

    private:
        mutable std::mutex mMutex{};
        ankerl::unordered_dense::map<AssetID, eastl::unique_ptr<Entry>> mEntries{};
    };

    struct AssetsServiceDescription {};

    class AssetsService final : public IService, public Singleton<AssetsService> {
    public:

        explicit AssetsService( const AssetsServiceDescription& options );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        MKT_NODISCARD auto GetDummyTexture() -> TextureHandle;
        MKT_NODISCARD auto GetAssetCacheBasePath() const  -> const Path&;

        template<typename AssetType>
        MKT_NODISCARD auto GetAssetByUri( const eastl::string_view uri ) -> Ref<AssetType> {
            Path fullPath{ Path{ uri }.GetAbsolute() };

            if constexpr (std::is_same_v<AssetType, Model>) {
                return mModels[uri];
            }
            else if constexpr (std::is_same_v<AssetType, ITexture>) {
                return mTextures2D[uri];
            }
            else if constexpr (std::is_same_v<AssetType, Audio>) {
                return mAudios[uri];
            }
            else if constexpr (std::is_same_v<AssetType, Font>) {
                return mFonts[uri];
            }
            else if constexpr (std::is_same_v<AssetType, Material>) {
                return mMaterials[uri];
            }

            return Ref<AssetType>::CreateEmpty();
        }

        template<typename AssetType>
        MKT_NODISCARD auto LoadAsset( auto&&... args ) -> Ref<AssetType> {
            if constexpr (std::is_same_v<AssetType, Model>) {
                return LoadModel( eastl::forward<decltype(args)>(args)... );
            }
            else if constexpr (std::is_same_v<AssetType, ITexture>) {
                return LoadTexture( eastl::forward<decltype(args)>(args)... );
            }
            else if constexpr (std::is_same_v<AssetType, Audio>) {
                return LoadAudio( eastl::forward<decltype(args)>(args)... );
            }
            else if constexpr (std::is_same_v<AssetType, Font>) {
                return LoadFont( eastl::forward<decltype(args)>(args)... );
            }
            else if constexpr (std::is_same_v<AssetType, Material>) {
                return LoadMaterial( eastl::forward<decltype(args)>(args)... );
            }

            return Ref<AssetType>::CreateEmpty();
        }

        template<typename AssetType>
        auto LoadAssetAsync( auto&&... args ) -> void {
            auto tuple{ eastl::make_tuple(eastl::forward<decltype(args)>(args)...) };

            threading::TaskService::Get()->Submit([this, argsTuple = eastl::move(tuple)]() mutable -> void {
                eastl::apply([this]<typename... Args>(Args&&... unpackedArgs) {
                    (void)LoadAsset<AssetType>(eastl::forward<Args>(unpackedArgs)...);
                }, eastl::move(argsTuple));
            });
        }

        MKT_NODISCARD auto CreateMaterial( const MaterialProperties& spec = {} ) -> MaterialHandle;

        ~AssetsService() override = default;

    private:
        auto LoadModel( const Path& uri ) -> ModelHandle;
        auto LoadModel( const ModelLoadDescription& description) -> ModelHandle;

        auto LoadTexture( const Path& uri, TextureDimension dimension ) -> TextureHandle;
        auto LoadTexture( const TextureLoadDescription& description ) -> TextureHandle;

        auto LoadAudio( const AudioLoadDescription& description) -> AudioHandle;

        auto LoadFont( const Path& uri ) -> FontHandle;
        auto LoadFont( const FontLoadDescription& description) -> FontHandle;

        auto LoadMaterial( const Path& uri) -> MaterialHandle;

        auto LoadDummyAssets() -> void;

        auto CreateAssetCacheFolder(const Path& path) -> void;

    private:
        static inline const Path kDummyTexturePath{ "Resources/Textures/texture.png" };

    private:
        eastl::unique_ptr<MeshFactory> mMeshFactory{};
        eastl::unique_ptr<FontFactory> mFontFactory{};

        GpuDevice* mGpuDevice{ nullptr };
        AudioDevice* mAudioDevice{ nullptr };

        AssetCache<Font> mFonts{};
        AssetCache<Model> mModels{};
        AssetCache<Audio> mAudios{};
        AssetCache<Material> mMaterials{};

        AssetCache<ITexture> mTextures2D{};
        AssetCache<ITexture> mTexturesCubes{};

        Path mAnimationCachePathBase{ "AssetCache" };
    };
}

#endif//MIKOTO_ASSETS_SERVICE_HH
