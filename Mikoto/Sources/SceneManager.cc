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
#include <Core/String.hh>
#include <Core/Service.hh>

#include <Scene/SceneManager.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>

namespace mikoto::scene {

    auto SceneManager::Initialize() -> void {
        mScenes.reserve( 10 );

        mIsInitialized = true;
    }

    auto SceneManager::Shutdown() -> void {
        if (!mIsInitialized) {
            return;
        }

        mScenes.clear();

        mIsInitialized = false;
    }

    auto SceneManager::LoadFromDisk() -> Scene* {
        // File filters
        const std::initializer_list<FileDialogPair> filters{
            { "Mikoto Scene", "mktscn" },
            { "Mikoto Project", "mktprj" }
        };

        const Path savePath{ SaveFileDialog( "Mikoto Scene", filters ) };
        return Load( savePath );
    }

    auto SceneManager::Load( const Path &path ) -> Scene * {
        auto deserialized{ mSerializer.Deserialize( path ) };
        return Register( deserialized.As<Scene>()->GetName().data(), eastl::move( deserialized ) );
    }

    auto SceneManager::Save(const Scene* scene, const Path &path ) -> void {
        mSerializer.Serialize( scene, path );
    }

    auto SceneManager::SaveToDisk( const Scene* scene) -> void {
        const std::initializer_list<FileDialogPair> filters{
                { "Mikoto Scene", "mktscn" },
                { "Mikoto Project", "mktprj" }
        };

        const Path savePath{ SaveFileDialog( "Mikoto Scene", filters ) };
        Save( scene, savePath );
    }

    auto SceneManager::CreateScene( eastl::string_view name ) -> Scene * {
        return Register( name, SceneHandle::New( name ) );
    }

    auto SceneManager::GetByName( const eastl::string_view name ) -> Scene * {
        return mScenes.at( name.data() ).GetRaw();
    }

    auto SceneManager::Register( const eastl::string_view name, SceneHandle scene ) -> Scene * {
        std::lock_guard lock{ mSceneRegisterMutex };

        const auto [it, success] {
            mScenes.try_emplace( name.data(), std::move(scene) )
        };

        if (success) {
            return it->second.GetRaw();
        }

        return nullptr;
    }
}// namespace Mikoto