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
#include <Core/Logging/Logger.hh>
#include <Core/System/AssetsSystem.hh>
#include <Library/Filesystem/PathBuilder.hh>
#include <Library/String/String.hh>
#include <Library/Utility/Types.hh>

#include "Renderer/Buffer/IndexBuffer.hh"
#include "Renderer/Buffer/VertexBuffer.hh"

namespace Mikoto {
    Model::Model( const ModelLoadInfo &info )
        : m_ModelAbsolutePath{ info.Path }, m_ModelName{ info.Path.stem().string() }, m_InvertedY{ info.InvertedY } {
        Load( info.WantTextures );
    }

    auto Model::Load( const bool wantLoadTextures ) -> void {
        if ( !m_ModelAbsolutePath.has_filename() ) {
            MKT_THROW_RUNTIME_ERROR( "Model::Load - Not valid path for model object" );
        }

        Assimp::Importer importer{};

        // See more postprocessing options: https://assimp.sourceforge.net/lib_html/postprocess_8h.html
        constexpr auto importerFlags{ static_cast<aiPostProcessSteps>( aiProcess_Triangulate |
                                                                       aiProcess_FlipUVs |
                                                                       aiProcess_GenSmoothNormals |
                                                                       aiProcess_JoinIdenticalVertices ) };

        const auto scene{ importer.ReadFile( m_ModelAbsolutePath.string(), importerFlags ) };

        if ( scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr ) {
            MKT_THROW_RUNTIME_ERROR( fmt::format( "Model::Load - Failed to load model: '{}'", importer.GetErrorString() ) );
        }

        m_Meshes.reserve( scene->mRootNode->mNumMeshes );

        // Contains the model's directory. Since substr will not engine the ast character in the range,
        // this variable does not engine the last slash of the path string
        auto modelDirectory{ m_ModelAbsolutePath };
        modelDirectory.remove_filename();
        const Path_T modelDirectoryFormatted{ modelDirectory.string().substr( 0, modelDirectory.string().find_last_of( '/' ) ) };

        ProcessNode( scene->mRootNode, scene, modelDirectoryFormatted, wantLoadTextures );
    }

    auto Model::ProcessNode( const aiNode* root, const aiScene* scene, const Path_T& modelDirectory, const bool wantLoadTextures ) -> void {
        // Process all the meshes from this node
        for ( UInt64_T indexMesh{}; indexMesh < root->mNumMeshes; indexMesh++ ) {
            Scope_T<Mesh> result{ std::move(ProcessMesh( scene->mMeshes[root->mMeshes[indexMesh]], scene, modelDirectory, wantLoadTextures ) ) };

            m_TotalVertices += result->GetVertexBuffer()->GetCount();
            m_TotalIndices += result->GetIndexBuffer()->GetCount();

            m_Meshes.emplace_back( std::move( result ) );
        }

        // then do the same for each of its children
        for ( UInt64_T indexChildNode{}; indexChildNode < root->mNumChildren; indexChildNode++ ) {
            ProcessNode( root->mChildren[indexChildNode], scene, modelDirectory, wantLoadTextures );
        }
    }

    auto Model::ProcessMesh( const aiMesh* mesh, const aiScene* scene, const Path_T& modelDirectory, const bool wantLoadTextures ) const -> Scope_T<Mesh> {
        std::vector<float> vertices{};
        std::vector<UInt32_T> indices{};
        std::vector<Texture2D*> textures{};

        // The way we construct the vertex buffer data is not guaranteed to follow
        // the buffer layout, which is default for Models. Which means if the mesh
        // has no normal or texture coordinates, we have to insert default initialized
        // data to follow the default layout. We also have to introduce default
        // values for the color attribute

        for ( UInt64_T index{}; index < mesh->mNumVertices; index++ ) {
            // Vertices -----
            vertices.push_back( mesh->mVertices[index].x );
            vertices.push_back( mesh->mVertices[index].y );
            vertices.push_back( mesh->mVertices[index].z );

            // Normals -----
            if ( mesh->HasNormals() ) {
                vertices.push_back( mesh->mNormals[index].x );
                vertices.push_back( mesh->mNormals[index].y );
                vertices.push_back( mesh->mNormals[index].z );
            } else {
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
            }

            // Colors -----
            if ( mesh->HasVertexColors( index )) {
                vertices.emplace_back( mesh->mColors[index]->r );
                vertices.emplace_back( mesh->mColors[index]->g );
                vertices.emplace_back( mesh->mColors[index]->b );
            } else {
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
            }

            // Texture coordinates -----
            if ( mesh->HasTextureCoords( index ) && mesh->mTextureCoords[0] != nullptr ) {
                vertices.push_back( mesh->mTextureCoords[0][index].x );

                vertices.push_back( m_InvertedY ? -mesh->mTextureCoords[0][index].y : mesh->mTextureCoords[0][index].y );
            } else {
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
            }
        }

        // Retrieve mesh indices
        for ( UInt64_T i{}; i < mesh->mNumFaces; i++ ) {
            const auto face{ mesh->mFaces[i] };

            for ( UInt64_T index{}; index < face.mNumIndices; index++ ) {
                indices.emplace_back( face.mIndices[index] );
            }
        }

        // process material
        if ( mesh->mMaterialIndex > 0 ) {
            const aiMaterial* material{ scene->mMaterials[mesh->mMaterialIndex] };

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

        return CreateScope<Mesh>(
                mesh->mName.C_Str(),
                VertexBuffer::Create( vertices ),
                IndexBuffer::Create( indices ),
                std::move( textures ),
                m_ModelAbsolutePath);
    }

    auto Model::LoadTextures( const aiMaterial* mat, const aiTextureType type, const MapType tType, const aiScene* scene, const Path_T& modelDirectory ) -> std::vector<Texture2D*>  {
        std::vector<Texture2D*> textures{};
        AssetsSystem& assetsSystem{ Engine::GetSystem<AssetsSystem>() };

        for ( Size_T index{}; index < mat->GetTextureCount( type ); index++ ) {
            aiString texturePath{};

            if ( mat->GetTexture( type, index, std::addressof( texturePath ) ) == AI_SUCCESS ) {
                // Assumes the textures are in the same directory as the model files
                Path_T path{ PathBuilder()
                    .WithPath( modelDirectory.string() )
                    .WithPath( texturePath.C_Str() )
                    .Build() };

                if ( auto ptr{ assetsSystem.LoadTexture ( TextureLoadInfo{
                    .Path{ path },
                    .Type{ tType },
                } ) } ) {
                    textures.emplace_back( dynamic_cast<Texture2D*>( ptr ) );
                }
            }

            // Temporary. See if it is an embedded texture
            auto [embeddedTexturePtr, embeddedTextureIndex]{ scene->GetEmbeddedTextureAndIndex( texturePath.C_Str() ) };
            if ( embeddedTextureIndex != -1 ) {
                MKT_CORE_LOGGER_WARN( "Model::LoadTextures - Texture is embedded! Index is {}", embeddedTextureIndex );
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