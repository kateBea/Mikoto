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

#include <mutex>

#include <EASTL/utility.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Service.hh>
#include <Core/Profiler.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>

#include <Animation/AnimationSystem.hh>

#include <Scene/SceneManager.hh>

namespace mikoto::scene {

    auto SceneManager::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        mScenes.reserve( 10 );

        mIsInitialized = true;
    }

    auto SceneManager::Shutdown() -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsInitialized ) {
            return;
        }

        mScenes.clear();
        mRetiredScenes.clear();

        mIsInitialized = false;
    }

    auto SceneManager::LoadFromDisk() -> Scene* {
        MKT_BEGIN_PROFILER();
        // File filters
        const std::initializer_list<filesystem::FileDialogPair> filters{
            { "Mikoto Scene", "mktscn" },
            { "Mikoto Project", "mktprj" }
        };

        const filesystem::Path savePath{ OpenFileDialog( filters ) };
        return Load( savePath );
    }

    auto SceneManager::Load( const filesystem::Path& path ) -> Scene* {
        MKT_BEGIN_PROFILER();

        if ( path.IsEmpty() ) {
            return nullptr;
        }

        auto deserialized{ mSerializer.Deserialize( path ) };

        if ( deserialized.IsEmpty() ) {
            return nullptr;
        }

        const eastl::string name{ deserialized.As<Scene>()->GetName() };
        return Register( name, eastl::move( deserialized ) );
    }

    auto SceneManager::Save( const Scene* scene, const filesystem::Path& path ) -> void {
        MKT_BEGIN_PROFILER();
        mSerializer.Serialize( scene, path );
    }

    auto SceneManager::SaveToDisk( const Scene* scene ) -> void {
        MKT_BEGIN_PROFILER();
        const std::initializer_list<filesystem::FileDialogPair> filters{
            { "Mikoto Scene", "mktscn" },
            { "Mikoto Project", "mktprj" }
        };

        const filesystem::Path savePath{ SaveFileDialog( "Mikoto Scene", filters ) };
        Save( scene, savePath );
    }

    auto SceneManager::CreateScene( eastl::string_view name ) -> Scene* {
        MKT_BEGIN_PROFILER();
        return Register( name, SceneHandle::New( name ) );
    }

    auto SceneManager::GetByName( const eastl::string_view name ) -> Scene* {
        MKT_BEGIN_PROFILER();
        return mScenes.at( name.data() ).GetPtr();
    }

    auto SceneManager::Unload( Scene* scene ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !scene ) {
            return;
        }

        std::lock_guard lock{ mSceneRegisterMutex };

        for ( auto it{ mScenes.begin() }; it != mScenes.end(); ++it ) {
            if ( it->second.GetPtr() != scene ) {
                continue;
            }

            scene->SetState( SceneState::eIdle );
            // A pending import may keep this scene alive briefly. It must no
            // longer animate or play audio while awaiting main-thread cleanup.
            for ( auto [entity, component]: scene->GetRegistry().view<AnimatorComponent>().each() ) {
                if ( auto* animator{ animation::AnimationSystem::Get()->GetAnimator( component.GetAnimatorID() ) } ) {
                    animator->SetSuspended( true );
                }
            }

            for ( auto [entity, component]: scene->GetRegistry().view<AudioSourceComponent>().each() ) {
                if ( auto source{ component.GetSource() } ) {
                    source->Stop();
                }
            }

            mRetiredScenes.push_back( eastl::move( it->second ) );
            mScenes.erase( it );
            return;
        }
    }

    auto SceneManager::CollectRetiredScenes() -> void {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mSceneRegisterMutex };

        for ( auto it{ mRetiredScenes.begin() }; it != mRetiredScenes.end(); ) {
            if ( ( *it )->GetRefCount() == 1 ) {
                it = mRetiredScenes.erase( it );
            }
            else {
                ++it;
            }
        }
    }

    auto SceneManager::Register( const eastl::string_view name, SceneHandle scene ) -> Scene* {
        MKT_BEGIN_PROFILER();
        std::lock_guard lock{ mSceneRegisterMutex };

        // Loading a saved scene while its source is still open must not discard the newly loaded scene.
        eastl::string uniqueName{ name };

        for ( core::u32 suffix = 1; mScenes.contains( uniqueName ); ++suffix ) {
            uniqueName = string::Format( "{} ({})", name, suffix );
        }
        // Only the manager key is unique; preserve the name stored in the scene.

        const auto [it, success]{
            mScenes.try_emplace( uniqueName, eastl::move( scene ) )
        };

        if ( success ) {
            return it->second.GetPtr();
        }

        return nullptr;
    }
}// namespace Mikoto
