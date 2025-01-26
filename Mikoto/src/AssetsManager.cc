/**
 * AssetsManager.cc
 * Created by kate on 10/15/23.
 * */

// Project Headers
#include <Assets/AssetsManager.hh>
#include <Core/Logger.hh>
#include <Renderer/Buffer/IndexBuffer.hh>
#include <Renderer/Core/Renderer.hh>
#include <STL/Filesystem/PathBuilder.hh>

namespace Mikoto {

    auto AssetsManager::Init( AssetsManagerSpec&& spec ) -> void {
        s_Spec = std::move( spec );
    }

    auto AssetsManager::Shutdown() -> void {
        s_Models.clear();
    }

    auto AssetsManager::LoadPrefabs() -> void {
        // Move to the app it is the editor that needs these at the start
        AddSpritePrefab();

        const bool invertedY{ Renderer::GetActiveGraphicsAPI() == GraphicsAPI::VULKAN_API };

        ModelLoadInfo modelLoadInfo{};
        modelLoadInfo.InvertedY = invertedY;
        modelLoadInfo.WantTextures = true;

        modelLoadInfo.ModelPath = PathBuilder()
            .WithPath( GetAssetRootDirectory().string() )
            .WithPath( "Models" )
            .WithPath( "Prefabs" )
            .WithPath( "sponza" )
            .WithPath( "glTF" )
            .WithPath( "Sponza.gltf" )
            .Build();
        s_LoadedPrefabModels.emplace( GetSponzaPrefabName(), modelLoadInfo );

        modelLoadInfo.ModelPath = PathBuilder()
            .WithPath( GetAssetRootDirectory().string() )
            .WithPath( "Models" )
            .WithPath( "Prefabs" )
            .WithPath( "cube" )
            .WithPath( "gltf" )
            .WithPath( "scene.gltf" )
            .Build();
        s_LoadedPrefabModels.emplace( GetCubePrefabName(), modelLoadInfo );

        modelLoadInfo.ModelPath = PathBuilder()
            .WithPath( GetAssetRootDirectory().string() )
            .WithPath( "Models" )
            .WithPath( "Prefabs" )
            .WithPath( "sphere" )
            .WithPath( "gltf" )
            .WithPath( "scene.gltf" )
            .Build();
        s_LoadedPrefabModels.emplace( GetSpherePrefabName(), modelLoadInfo );

        modelLoadInfo.ModelPath = PathBuilder()
            .WithPath( GetAssetRootDirectory().string() )
            .WithPath( "Models" )
            .WithPath( "Prefabs" )
            .WithPath( "cylinder" )
            .WithPath( "gltf" )
            .WithPath( "scene.gltf" )
            .Build();
        s_LoadedPrefabModels.emplace( GetCylinderPrefabName(), modelLoadInfo );

        modelLoadInfo.ModelPath = PathBuilder()
            .WithPath( GetAssetRootDirectory().string() )
            .WithPath( "Models" )
            .WithPath( "Prefabs" )
            .WithPath( "cone" )
            .WithPath( "gltf" )
            .WithPath( "scene.gltf" )
            .Build();
        s_LoadedPrefabModels.emplace( GetConePrefabName(), modelLoadInfo );
    }

    auto AssetsManager::AddSpritePrefab() -> void {
        const std::vector squareData{
            /* Positions    -       // Normals   -       // Colors    -       // Texture coordinates */
            -0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,// bottom left
            0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, // bottom right
            0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,  // top right
            -0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.3f, 0.4f, 0.0f, 1.0f, // top left
        };

        // Set index and vertex buffers
        auto vertexBuffer{ VertexBuffer::Create( squareData, VertexBuffer::GetDefaultBufferLayout() ) };
        auto indexBuffer{ IndexBuffer::Create( { 0, 1, 2, 2, 3, 0 } ) };

        // Add model to the list of prefabs

        if ( auto result{ s_LoadedPrefabModels.emplace( GetSpritePrefabName(), Model{} ) }; result.second ) {
            MeshData meshData{};
            meshData.SetVertices( vertexBuffer );
            meshData.SetIndices( indexBuffer );

            result.first->second.AddMesh( std::move( meshData ) );
        }
    }

    auto AssetsManager::GetPrefabModel( PrefabSceneObject type ) -> Model* {
        Model* result{ nullptr };
        auto it{ s_LoadedPrefabModels.end() };

        switch ( type ) {
            case PrefabSceneObject::SPRITE_PREFAB_OBJECT:
                it = s_LoadedPrefabModels.find( GetSpritePrefabName() );
                break;
            case PrefabSceneObject::CUBE_PREFAB_OBJECT:
                it = s_LoadedPrefabModels.find( GetCubePrefabName() );
                break;
            case PrefabSceneObject::SPHERE_PREFAB_OBJECT:
                it = s_LoadedPrefabModels.find( GetSpherePrefabName() );
                break;
            case PrefabSceneObject::CYLINDER_PREFAB_OBJECT:
                it = s_LoadedPrefabModels.find( GetCylinderPrefabName() );
                break;
            case PrefabSceneObject::CONE_PREFAB_OBJECT:
                it = s_LoadedPrefabModels.find( GetConePrefabName() );
                break;
            case PrefabSceneObject::SPONZA_PREFAB_OBJECT:
                it = s_LoadedPrefabModels.find( GetSponzaPrefabName() );
                break;

            case PrefabSceneObject::COUNT_PREFAB_OBJECT:
            case PrefabSceneObject::NO_PREFAB_OBJECT:
                it = s_LoadedPrefabModels.end();
                break;
        }

        if ( it != s_LoadedPrefabModels.end() ) {
            result = std::addressof( it->second );
        }

        return result;
    }

    auto AssetsManager::GetModel( const Path_T& modelPath ) -> const Model* {
        const auto modelFullPath{ absolute( modelPath ) };

        // The key to the model will be its full path converted to a string
        const auto key{ modelFullPath.string() };
        if ( const auto it{ s_LoadedModels.find( key ) }; it != s_LoadedModels.end() ) {
            return std::addressof( it->second );
        }

        return nullptr;
    }

    auto AssetsManager::GetModifiableModel( const Path_T& modelPath ) -> Model* {
        const auto modelFullPath{ std::filesystem::absolute( modelPath ) };

        // The key to the model will be its full path converted to a string
        const auto key{ modelFullPath.string() };
        if ( const auto it{ s_LoadedModels.find( key ) }; it != s_LoadedModels.end() ) {
            return std::addressof( it->second );
        }

        return nullptr;
    }

    auto AssetsManager::LoadModel( const ModelLoadInfo& info ) -> Model* {
        Model* result{ nullptr };
        ModelLoadInfo modelLoadInfo{ info };
        modelLoadInfo.ModelPath = absolute( info.ModelPath );

        if ( !s_LoadedPrefabModels.contains( modelLoadInfo.ModelPath.string() ) ) {
            const auto key{ modelLoadInfo.ModelPath.string() };
            result = std::addressof( s_LoadedModels.emplace( key, modelLoadInfo ).first->second );
        } else {
            MKT_CORE_LOGGER_INFO( "AssetsManager - Model [{}] already exists!", modelLoadInfo.ModelPath.string() );
        }

        return result;
    }
}
