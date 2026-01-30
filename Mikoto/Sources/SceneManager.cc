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

#include <string>
#include <initializer_list>

#include <Library/Utility/Types.hh>

#include <Scene/SceneManager.hh>
#include <Filesystem/FileService.hh>

namespace Mikoto {

    auto SceneManager::Init() -> void {
        m_Scenes.reserve( 10 );

        m_IsInitialized = true;
    }

    auto SceneManager::Shutdown() -> void {
        if (!m_IsInitialized) {
            return;
        }

        m_Scenes.clear();
    }

    auto SceneManager::LoadFromDisk() -> Scene* {
        // File filters
        const std::initializer_list<std::pair<std::string, std::string>> filters{
                { "Mikoto Scene files", "mkts,mktscene" },
                { "Mikoto Project Files", "mkt,mktp,mktproject" }
        };

        const Path savePath{ FileService::Get()->SaveDialog( "Mikoto Scene", filters ) };
        return Load( savePath );
    }

    auto SceneManager::Load( const Path &path ) -> Scene * {
        auto deserialized{ m_Serializer.Deserialize( path ) };
        return Register( deserialized->GetName(), std::move( deserialized ) );
    }

    auto SceneManager::Save(const Scene* scene, const Path &path ) -> void {
        m_Serializer.Serialize( *scene, path );
    }

    auto SceneManager::SaveToDisk( const Scene* scene) -> void {
        const std::initializer_list<std::pair<std::string, std::string>> filters{
                { "Mikoto Scene files", "mkts,mktscene" },
                { "Mikoto Project Files", "mkt,mktp,mktproject" }
        };

        const Path savePath{ FileService::Get()->SaveDialog( "Mikoto Scene", filters ) };
        Save( scene, savePath );
    }

    auto SceneManager::CreateScene( std::string_view name ) -> Scene * {
        return Register( name, CreateScope<Scene>( name ) );
    }

    auto SceneManager::GetByName( const std::string_view name ) -> Scene * {
        return m_Scenes.at( std::string{ name } ).get();
    }

    auto SceneManager::Register( const std::string_view name, Unique<Scene> &&scene ) -> Scene * {
        const auto [it, success] {
            m_Scenes.try_emplace( std::string{ name }, std::move(scene) )
        };

        if (success) {
            return it->second.get();
        }

        return nullptr;
    }
}// namespace Mikoto