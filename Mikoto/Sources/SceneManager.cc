//
// Created by kate on 10/30/25.
//

#include <string>
#include <initializer_list>

#include <Library/Utility/Types.hh>

#include "Scene/SceneManager.hh"

#include "Filesystem/FileService.hh"

namespace Mikoto {

    auto SceneManager::Init() -> void {
        m_Scenes.reserve( 10 );
    }

    auto SceneManager::Shutdown() -> void {
        m_Scenes.clear();
    }

    auto SceneManager::LoadSceneFromDisk( SceneSerializer *serializer ) -> Scene* {
        // File filters
        const std::initializer_list<std::pair<std::string, std::string>> filters{
                { "Mikoto Scene files", "mkts,mktscene" },
                { "Mikoto Project Files", "mkt,mktp,mktproject" }
        };

        const Path savePath{ FileService::Get()->SaveDialog( "Mikoto Scene", filters ) };

        auto deserialized{ serializer->Deserialize( savePath ) };

        return RegisterNewScene( deserialized->GetName(), std::move( deserialized ) );
    }

    auto SceneManager::SaveSceneFromDisk(Scene* scene,  SceneSerializer *serializer ) -> void {
        const std::initializer_list<std::pair<std::string, std::string>> filters{
                { "Mikoto Scene files", "mkts,mktscene" },
                { "Mikoto Project Files", "mkt,mktp,mktproject" }
        };

        const Path savePath{ FileService::Get()->SaveDialog( "Mikoto Scene", filters ) };

        serializer->Serialize( *scene, savePath );
    }

    auto SceneManager::CreateScene( std::string_view name ) -> Scene * {
        return RegisterNewScene( name, CreateScope<Scene>( name ) );
    }

    auto SceneManager::GetByName( const std::string_view name ) -> Scene * {
        return m_Scenes.at( std::string{ name } ).get();
    }

    auto SceneManager::RegisterNewScene( const std::string_view name, Unique<Scene> &&scene ) -> Scene * {
        const auto [it, success] {
            m_Scenes.try_emplace( std::string{ name }, std::move(scene) )
        };

        if (success) {
            return it->second.get();
        }

        return nullptr;
    }

}// namespace Mikoto