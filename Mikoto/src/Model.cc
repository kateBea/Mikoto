/**
 * Model.cc
 * Created by kate on 6/29/23.
 * */

// C++ Standard Library
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

// Third Party Libraries
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <assimp/Importer.hpp>

// Project Headers
#include <Assets//Mesh.hh>
#include <Assets/Model.hh>
#include <Common/Common.hh>
#include <Core/Logger.hh>
#include <STL/String/String.hh>
#include <STL/Utility/Types.hh>
#include <Threading/TaskManager.hh>

#include "Renderer/Buffer/IndexBuffer.hh"
#include "Renderer/Buffer/VertexBuffer.hh"

namespace Mikoto {
    Model::Model( const ModelLoadInfo& info )
        : m_ModelAbsolutePath{ info.ModelPath }, m_ModelName{ info.ModelPath.stem().string() }, m_InvertedY{ info.InvertedY } {
        Load( info.WantTextures );
    }

    auto Model::Load( const bool wantLoadTextures ) -> void {
        if ( !m_ModelAbsolutePath.has_filename() ) {
            MKT_THROW_RUNTIME_ERROR( "Model - Not valid path for model object" );
        }

        Assimp::Importer importer{};

        // See more postprocessing options: https://assimp.sourceforge.net/lib_html/postprocess_8h.html
        constexpr auto importerFlags{ static_cast<aiPostProcessSteps>( aiProcess_Triangulate |
                                                                       aiProcess_FlipUVs |
                                                                       aiProcess_GenSmoothNormals |
                                                                       aiProcess_JoinIdenticalVertices ) };

        const auto scene{ importer.ReadFile( m_ModelAbsolutePath.string(), importerFlags ) };

        if ( scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "Model - Failed to load model: '{}'", importer.GetErrorString() ) );
        }
        m_Meshes.reserve( scene->mRootNode->mNumMeshes );

        // Contains the model's directory. Since substr will not engine the ast character in the range,
        // this variable does not engine the last slash of the path string
        auto modelDirectory{ m_ModelAbsolutePath };
        modelDirectory.remove_filename();
        const Path_T modelDirectoryFormatted{ modelDirectory.string().substr( 0, modelDirectory.string().find_last_of( '/' ) ) };
        ProcessNode( scene->mRootNode, scene, modelDirectoryFormatted, wantLoadTextures );

        for ( const auto& mesh: GetMeshes() ) {
            m_TotalVertices += mesh.GetVertexBuffer()->GetCount();
            m_TotalIndices += mesh.GetIndexBuffer()->GetCount();
        }
    }

    auto Model::ProcessNode( aiNode* root, const aiScene* scene, const Path_T& modelDirectory, bool wantLoadTextures ) -> void {
        // Process all the meshes from this node
        for ( UInt64_T index{}; index < root->mNumMeshes; index++ ) {
            auto result{ ProcessMesh( scene->mMeshes[root->mMeshes[index]], scene, modelDirectory, wantLoadTextures ) };
            m_Meshes.emplace_back( std::move( result ) );
        }

        // then do the same for each of its children
        for ( UInt64_T i{}; i < root->mNumChildren; i++ ) {
            ProcessNode( root->mChildren[i], scene, modelDirectory, wantLoadTextures );
        }
    }

    auto Model::ProcessMesh( aiMesh* mesh, const aiScene* scene, const Path_T& modelDirectory, bool wantLoadTextures ) -> Mesh {
        std::vector<float> vertices{};
        std::vector<UInt32_T> indices{};
        std::vector<std::shared_ptr<Texture2D>> textures{};

        for ( UInt64_T index{}; index < mesh->mNumVertices; index++ ) {
            vertices.push_back( mesh->mVertices[index].x );
            vertices.push_back( mesh->mVertices[index].y );
            vertices.push_back( mesh->mVertices[index].z );

            // The way we construct the vertex buffer data is not guaranteed to follow
            // the buffer layout, which is default for Models. Which means if the mesh
            // has no normal or texture coordinates, we have to insert default initialized
            // data to follow the default layout. We also have to introduce default
            // values for the color attribute

            if ( mesh->HasNormals() ) {
                vertices.push_back( mesh->mNormals[index].x );
                vertices.push_back( mesh->mNormals[index].y );
                vertices.push_back( mesh->mNormals[index].z );
            } else {
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
            }

            vertices.emplace_back( 0.0f );
            vertices.emplace_back( 0.0f );
            vertices.emplace_back( 0.0f );

            if ( mesh->mTextureCoords[0] != nullptr ) {
                vertices.push_back( mesh->mTextureCoords[0][index].x );

                vertices.push_back( m_InvertedY ? -mesh->mTextureCoords[0][index].y : mesh->mTextureCoords[0][index].y );
            } else {
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
            }
        }

        // Retrieve mesh indices
        for ( UInt64_T i{}; i < mesh->mNumFaces; i++ ) {
            auto face{ mesh->mFaces[i] };

            for ( UInt64_T index{}; index < face.mNumIndices; index++ ) { indices.emplace_back( face.mIndices[index] ); }
        }

        // process material
        if ( mesh->mMaterialIndex >= 0 ) {
            auto material{ scene->mMaterials[mesh->mMaterialIndex] };

            if ( wantLoadTextures ) {
                std::vector diffuseMaps{ LoadTextures( material, aiTextureType_DIFFUSE, MapType::TEXTURE_2D_DIFFUSE, scene, modelDirectory ) };
                std::vector specularMaps{ LoadTextures( material, aiTextureType_SPECULAR, MapType::TEXTURE_2D_SPECULAR, scene, modelDirectory ) };
                std::vector normalMaps{ LoadTextures( material, aiTextureType_NORMALS, MapType::TEXTURE_2D_NORMAL, scene, modelDirectory ) };
                std::vector emissiveMaps{ LoadTextures( material, aiTextureType_EMISSIVE, MapType::TEXTURE_2D_EMISSIVE, scene, modelDirectory ) };
                std::vector metallic{ LoadTextures( material, aiTextureType_METALNESS, MapType::TEXTURE_2D_ROUGHNESS, scene, modelDirectory ) };
                std::vector roughness{ LoadTextures( material, aiTextureType_DIFFUSE_ROUGHNESS, MapType::TEXTURE_2D_METALLIC, scene, modelDirectory ) };
                std::vector ao{ LoadTextures( material, aiTextureType_AMBIENT_OCCLUSION, MapType::TEXTURE_2D_AMBIENT_OCCLUSION, scene, modelDirectory ) };

                for ( auto& item: diffuseMaps )
                    textures.push_back( std::move( item ) );

                for ( auto& item: specularMaps )
                    textures.push_back( std::move( item ) );

                for ( auto& item: normalMaps )
                    textures.push_back( std::move( item ) );

                for ( auto& item: emissiveMaps )
                    textures.push_back( std::move( item ) );

                for ( auto& item: metallic )
                    textures.push_back( std::move( item ) );

                for ( auto& item: roughness )
                    textures.push_back( std::move( item ) );

                for ( auto& item: ao )
                    textures.push_back( std::move( item ) );
            }
        }

        auto vertexBuffer{ VertexBuffer::Create( vertices, VertexBuffer::GetDefaultBufferLayout() ) };
        auto indexBuffer{ IndexBuffer::Create( indices ) };

        MeshData meshData{};
        meshData.SetVertices( vertexBuffer );
        meshData.SetIndices( indexBuffer );
        meshData.SetName( mesh->mName.C_Str() );
        meshData.SetTextures( std::move( textures ) );

        MKT_CORE_LOGGER_DEBUG( "Texture count for mesh {}, model is {}", textures.size(), modelDirectory.string() );

        return Mesh{ std::move( meshData ) };
    }

    auto Model::LoadTextures( aiMaterial* mat, aiTextureType type, MapType tType, const aiScene* scene, const Path_T& modelDirectory ) -> std::vector<std::shared_ptr<Texture2D>> {
        std::vector<std::shared_ptr<Texture2D>> textures{};

        for ( Size_T index{}; index < mat->GetTextureCount( type ); index++ ) {
            aiString texturePath{};

            if ( mat->GetTexture( type, index, std::addressof( texturePath ) ) == AI_SUCCESS ) {
                // Assumes the textures are in the same directory as the model files
                Path_T path{ modelDirectory.string() / Path_T{ texturePath.C_Str() } };

                // remove backslashes if any
                std::string pathStr{ path.string() };
                StringUtils::ReplaceWith( pathStr, '\\', '/' );

                path = Path_T{ pathStr };

                auto ptr{ Texture2D::Create( path, tType ) };

                if ( ptr ) {
                    textures.push_back( ptr );
                }
            }

            auto result{ scene->GetEmbeddedTextureAndIndex( texturePath.C_Str() ) };
            if ( result.second != -1 ) {
                MKT_CORE_LOGGER_DEBUG( "Texture is embedded! Index is {}", result.second );
            }
        }

        return textures;
    }

    Model::Model( Model&& other ) noexcept
        : m_ModelAbsolutePath{ std::move( other.m_ModelAbsolutePath ) }, m_Meshes{ std::move( other.m_Meshes ) }, m_ModelName{ std::move( other.m_ModelName ) } {
    }

    auto Model::operator=( Model&& other ) noexcept -> Model& {
        m_Meshes = std::move( other.m_Meshes );
        m_ModelAbsolutePath = std::move( other.m_ModelAbsolutePath );
        m_ModelName = std::move( other.m_ModelName );

        return *this;
    }
}// namespace Mikoto