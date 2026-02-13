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

#include <array>
#include <memory>
#include <vector>
#include <cstdlib>

#include <assimp/scene.h>
#include <assimp/DefaultLogger.hpp>
#include <assimp/postprocess.h>

#include <Common/String.hh>

#include <Math/Math.hh>
#include <Assets/Model.hh>
#include <Assets/AssetsService.hh>
#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>

#include <Library/Utility/Types.hh>
#include <Renderer/Core/RenderUtility.hh>

#include <Logging/Logger.hh>
#include <Material/PBRMaterial.hh>
#include <Threading/ThreadUtility.hh>
#include <Assets/MainImporter.hh>

namespace Mikoto {

    class CustomLogStream final : public Assimp::LogStream {
    public:
        auto write( const char *message ) -> void override {
            MKT_CORE_LOGGER_TRACE( "[Assimp] {}", message );
        }
    };

    static constexpr std::array ASSIMP_TEXTURE_TYPES{
            aiTextureType_DIFFUSE,
            aiTextureType_SPECULAR,
            aiTextureType_AMBIENT,
            aiTextureType_EMISSIVE,
            aiTextureType_HEIGHT,
            aiTextureType_NORMALS,
            aiTextureType_SHININESS,
            aiTextureType_OPACITY,
            aiTextureType_DISPLACEMENT,
            aiTextureType_LIGHTMAP,
            aiTextureType_REFLECTION,
            aiTextureType_BASE_COLOR,
            aiTextureType_NORMAL_CAMERA,
            aiTextureType_EMISSION_COLOR,
            aiTextureType_METALNESS,
            aiTextureType_DIFFUSE_ROUGHNESS,
            aiTextureType_AMBIENT_OCCLUSION,
            aiTextureType_UNKNOWN,
            aiTextureType_SHEEN,
            aiTextureType_CLEARCOAT,
            aiTextureType_TRANSMISSION,
            aiTextureType_MAYA_BASE,
            aiTextureType_MAYA_SPECULAR,
            aiTextureType_MAYA_SPECULAR_COLOR,
            aiTextureType_MAYA_SPECULAR_ROUGHNESS
    };

    static auto InferMikotoTextureType( const aiTextureType type ) -> TextureType {
        switch (type) {
            case aiTextureType_DIFFUSE:
            case aiTextureType_SPECULAR:
            case aiTextureType_NORMALS:
            case aiTextureType_EMISSIVE:
            case aiTextureType_METALNESS:
            case aiTextureType_DIFFUSE_ROUGHNESS:
            case aiTextureType_AMBIENT_OCCLUSION:
                return TextureType::TEXTURE_2D;
            default:
                return TextureType::TEXTURE_UNKNOWN;
        }
    }

    static auto InferMapType( const aiTextureType type ) -> MapType {
        switch ( type ) {
            case aiTextureType_DIFFUSE:
            case aiTextureType_BASE_COLOR:
                return MapType::ALBEDO_TEXTURE;

            case aiTextureType_NORMALS:
            case aiTextureType_NORMAL_CAMERA:
                return MapType::NORMAL_TEXTURE;

            case aiTextureType_METALNESS:
                return MapType::METALLIC_TEXTURE;

            case aiTextureType_DIFFUSE_ROUGHNESS:
                return MapType::ROUGHNESS_TEXTURE;

            case aiTextureType_SHININESS:
                return MapType::ROUGHNESS_TEXTURE;

            case aiTextureType_MAYA_SPECULAR_ROUGHNESS:
                return MapType::ROUGHNESS_TEXTURE;

            case aiTextureType_AMBIENT_OCCLUSION:
            case aiTextureType_LIGHTMAP:
                return MapType::AMBIENT_OCCLUSION_TEXTURE;

            case aiTextureType_EMISSIVE:
            case aiTextureType_EMISSION_COLOR:
                return MapType::EMISSIVE_TEXTURE;

            default:
                return MapType::UNDEFINED_TEXTURE;
        }
    }

    static auto LoadVertices( const aiMesh *mesh, MeshNodeData& meshNodeData ) -> void {
        for (UInt64 index{}; index < mesh->mNumVertices; index++) {
            auto& vertex{ meshNodeData.Vertices.emplace_back() };

            vertex.Position.x = mesh->mVertices[index].x;
            vertex.Position.y = mesh->mVertices[index].y;
            vertex.Position.z = mesh->mVertices[index].z;

            if (mesh->HasNormals()) {
                vertex.Normals.x = mesh->mNormals[index].x;
                vertex.Normals.y = mesh->mNormals[index].y;
                vertex.Normals.z = mesh->mNormals[index].z;
            }

            if (mesh->HasVertexColors( index )) {
                vertex.Colors.x = mesh->mColors[index]->r;
                vertex.Colors.y = mesh->mColors[index]->g;
                vertex.Colors.z = mesh->mColors[index]->b;
            }

            if (mesh->HasTextureCoords( 0 )) {
                vertex.UV_0.x = mesh->mTextureCoords[0][index].x;
                vertex.UV_0.y = (float)Math::Abs( 1 - mesh->mTextureCoords[0][index].y);
            }

            if (mesh->HasTextureCoords( 1 )) {
                vertex.UV_1.x = mesh->mTextureCoords[1][index].x;
                vertex.UV_1.y = (float)Math::Abs( 1 - mesh->mTextureCoords[1][index].y);
            }


            // Joints (Animation Bone IDs)
            // TODO

            // Weights
            // TODO
        }
    }

    static auto LoadIndices( const aiMesh *mesh, MeshNodeData& meshNodeData ) -> void {
        for (UInt64 i{}; i < mesh->mNumFaces; i++) {
            const auto face{ mesh->mFaces[i] };

            for (UInt64 index{}; index < face.mNumIndices; index++) {
                meshNodeData.Indices.emplace_back( face.mIndices[index] );
            }
        }
    }

    static auto LoadTexture( const std::string &modelRootPath, const aiMaterial *material, const aiTextureType type, const aiScene *scene ) -> TextureHandle {
        TextureHandle texture{};

        for (Size index{}; index < material->GetTextureCount( type ); index++) {
            aiString assimpTexturePath{};

            if (material->GetTexture( type, index, std::addressof( assimpTexturePath ) ) == AI_SUCCESS) {

                std::string textureUri{ assimpTexturePath.C_Str() };

                if (!StringUtil::Contains(textureUri, "*")) {
                    // Assumes the textures are in the same directory as the model files
                    Path path{ PathBuilder()
                               .WithPath( modelRootPath )
                               .WithPath( assimpTexturePath.C_Str() )
                               .Build() };

                    TextureLoadDescription loadInfo{};
                    loadInfo.WithFile( FileService::Get()->LoadFile( path ) )
                            .WithType( InferMikotoTextureType( type ) )
                            .WithMapType( InferMapType( type ) );

                    try {
                        texture = AssetsService::Get()->LoadAsset<Texture>( loadInfo );
                        textureUri = path.string();
                    } catch (std::exception &e) {
                        MKT_CORE_LOGGER_ERROR( "LoadTexture - Failed to load texture. Reason: {}", e.what() );
                    }
                } else {
                    try {
                        Int32 embeddedIndex{ std::atoi( assimpTexturePath.C_Str() + 1 ) };
                        const aiTexture *tex{ scene->mTextures[embeddedIndex] };

                        Path path{ PathBuilder()
                                           .WithPath( modelRootPath )
                                           .WithPath( assimpTexturePath.C_Str() )
                                           .Build() };

                        TextureDescription loadInfo{};

                        //If mHeight value is zero, pcData points to an
                        //compressed texture in any format (e.g. JPEG). (see texture.h in Assimp)
                        if ( tex->mHeight == 0 ) {
                            const StbImage image{ reinterpret_cast<Byte *>( tex->pcData ), tex->mWidth };

                            loadInfo.WithWidth( image.GetWidth() )
                                    .WithHeight( image.GetHeight() )
                                    .WithChannelCount( image.GetChannels() )
                                    .WithData( image.GetData() )

                                    .WithName( path.string() )
                                    .WithType( InferMikotoTextureType( type ) )
                                    .WithMapType( InferMapType( type ) )
                                    .WithType( TextureType::TEXTURE_2D )
                                    .WithFormat( TextureFormat::RGBA8_UNORM )
                                    .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

                            // Because StbImage is RAII and will free its data when exiting this scope we create the texture here
                            texture = AssetsService::Get()->LoadAsset<Texture>( loadInfo );
                        } else {
                            loadInfo.WithWidth( tex->mWidth )
                                    .WithHeight( tex->mHeight )
                                    .WithChannelCount( 4 )// Assimp always provides the texture with 4 channels
                                    .WithData( reinterpret_cast<unsigned char *>( tex->pcData ) )

                                .WithName( path.string() )
                                    .WithType( InferMikotoTextureType( type ) )
                                    .WithMapType( InferMapType( type ) )
                                    .WithType( TextureType::TEXTURE_2D )
                                    .WithFormat( TextureFormat::RGBA8_UNORM )
                                    .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );

                            texture = AssetsService::Get()->LoadAsset<Texture>( loadInfo );
                        }

                        textureUri = path.string();

                    } catch ( std::exception &e ) {
                        MKT_CORE_LOGGER_ERROR( "LoadTexture - Failed to load embedded texture. Reason: {}", e.what() );
                    }
                }

                if (!texture.IsEmpty()) {
                    texture->SetTextureUri( textureUri );
                }
            }

        }

        return texture;
    }

    static auto LoadTextures( const std::string &modelRootPath, const aiMesh *mesh, const aiScene *scene, MaterialProperties& properties ) -> void {

        if ( static_cast<Int32>( mesh->mMaterialIndex ) > -1) {
            const aiMaterial *material{ scene->mMaterials[mesh->mMaterialIndex] };
            for (const aiTextureType &type: ASSIMP_TEXTURE_TYPES) {
                TextureHandle handle{ LoadTexture( modelRootPath, material, type, scene ) };

                if (!handle.IsEmpty()) {
                    properties.TexturesByUri[handle->GetTextureUri()] = handle;
                }
            }
        }
    }

    static auto LoadMaterial(aiMaterial const* mat, MaterialProperties& properties) -> void {
        aiString name{};
        if(mat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
            properties.Name = name.C_Str();
        }
    }

    static auto ConstructMeshNode( const std::string &rootPath, const aiMesh *mesh, const aiScene *scene, MeshNodeData& meshNodeData, MaterialProperties& material ) -> void {
        meshNodeData.Name = mesh->mName.C_Str();

        LoadVertices( mesh, meshNodeData );
        LoadIndices( mesh, meshNodeData );

        if (mesh->mMaterialIndex > -1) {
            LoadMaterial( scene->mMaterials[mesh->mMaterialIndex], material );
        }

        LoadTextures( rootPath, mesh, scene, material );
    }

    static auto LoadNodeHierarchy( const aiNode *node, std::vector<MeshNodeData>& meshNodes ) -> void {
        for (UInt64 i{}; i < node->mNumMeshes; ++i) {
            // We will fill the mesh data later, for now we just want to create the hierarchy of nodes
            meshNodes.emplace_back();
        }
        // Recurse children
        for (UInt64 i{}; i < node->mNumChildren; ++i) {
            LoadNodeHierarchy( node->mChildren[i], meshNodes );
        }
    }

    static auto LoadNodeHierarchy( NodeHierarchy& dest, const aiNode *src ) -> void {
        dest.Name = src->mName.data;
        dest.Transformation = Mat4F{}; //TODO: convert from assimpt mat4 src->mTransformation;
        dest.ChildrenCount = src->mNumChildren;

        for ( UInt32 i{}; i < src->mNumChildren; i++ ) {
            NodeHierarchy newData{};
            LoadNodeHierarchy( newData, src->mChildren[i] );
            dest.Children.push_back( newData );
        }
    }

    static auto LoadNodes( const std::string &rootPath,
        const aiNode *node, const aiScene *scene,
        const ModelLoadDescription &loadInfo,
        ModelData& modelData ) -> void
    {

        for (UInt64 i{}; i < node->mNumMeshes; ++i) {
            auto& newMesh{ modelData.MeshNodes.emplace_back() };
            auto& material{ modelData.Materials.emplace_back() };

            // Compute material index (since we inserted back, size increased by one last element is size() - 1)
            Int32 index{ (Int32)modelData.Materials.size() - 1 };

            newMesh.MaterialIndex = index;
            ConstructMeshNode( rootPath, scene->mMeshes[node->mMeshes[i]], scene, newMesh, material );
        }

        // Recurse children
        for (UInt64 i{}; i < node->mNumChildren; ++i) {
            LoadNodes( rootPath, node->mChildren[i], scene, loadInfo, modelData );
        }
    }

    MainImporter::MainImporter( GpuDevice *device )
    : ModelImporter{ device } {
        // Allocate max concurrent importers
        for ( Int32 count{}; count < ThreadUtils::InferConcurrentThreads(); ++count ) {
            m_Importers.emplace_back( CreateScope<ImporterInfo>(  ) );
            m_Importers.back()->Index = count;
        }

        // Prepare importers
        for (const auto &importerInfo: m_Importers) {
            importerInfo->CustomFileHandlingImpl = nullptr;
            importerInfo->MeshImporter.SetIOHandler( importerInfo->CustomFileHandlingImpl.get() );
        }

        // Custom Logging
        m_LogImpl = CreateScope<CustomLogStream>();
        Assimp::DefaultLogger::create( "", Assimp::Logger::VERBOSE );
        Assimp::DefaultLogger::get()->attachStream( m_LogImpl.get(), Assimp::Logger::VERBOSE );
    }

    auto MainImporter::Import( const ModelLoadDescription &description, ModelData& out ) -> void {
        auto iter{ m_Importers.end() };
        do {
            iter = TryAcquireImporter();
        } while ( iter == m_Importers.end() );

        MKT_CORE_LOGGER_DEBUG( "Using GLTF importer {}", ( *iter )->Index );

        Import( *( *iter ), description, out );
        ( *iter )->IsFree.store( true, std::memory_order_release );
    }

    auto MainImporter::TryAcquireImporter() -> std::vector<Unique<ImporterInfo>>::iterator {
        return std::ranges::find_if( m_Importers, []( const auto& importer ) -> bool {
            bool expected{ true };
            if ( importer->IsFree.compare_exchange_strong( expected, false, std::memory_order_acquire ) ) {
                return true;
            }

            return false;
        } );
    }

    auto MainImporter::Import( ImporterInfo &loaderData, const ModelLoadDescription &description, ModelData& modelData ) -> void {
        // See more postprocessing options: https://assimp.sourceforge.net/lib_html/postprocess_8h.html
        constexpr auto importerFlags{ static_cast<aiPostProcessSteps>( aiProcess_Triangulate |
                                                                       aiProcess_FlipUVs |
                                                                       aiProcess_GenSmoothNormals |
                                                                       aiProcess_JoinIdenticalVertices ) };
        const File *file{ description.ModelFile };
        const std::string absolutePath{ file->GetPath() };
        const std::string fileName{ Path{ absolutePath }.stem().string() };

        const aiScene *scene{ loaderData.MeshImporter.ReadFile( absolutePath.c_str(), importerFlags ) };

        if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr) {
            MKT_CORE_LOGGER_ERROR( "MainImporter::Import - Model load failed. Assimp error: '{}'", loaderData.MeshImporter.GetErrorString() );
        } else {
            for (UInt32 animationCount{}; animationCount < scene->mNumAnimations; ++animationCount) {
                auto animation{ scene->mAnimations[0] };

                NodeHierarchy hierarchy{};
                LoadNodeHierarchy( hierarchy, scene->mRootNode );
                modelData.Animations.emplace_back( std::move( hierarchy ), animation->mDuration, animation->mTicksPerSecond );
            }

            modelData.Name = scene->mName.C_Str();
            LoadNodes( Filesystem::StripFileName( file->GetPath() ), scene->mRootNode, scene, description, modelData );
        }
    }
}
