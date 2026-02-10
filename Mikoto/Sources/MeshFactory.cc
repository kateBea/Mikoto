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

#include <Common/String.hh>

#include <Renderer/Core/Pipeline.hh>
#include <Renderer/Core/RenderService.hh>

#include <Assets/Model.hh>
#include <Assets/AssetsService.hh>
#include <Assets/MeshFactory.hh>
#include <Filesystem/FileService.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/RenderUtility.hh>

#include <Math/Math.hh>

#include "assimp/DefaultLogger.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

#include <Assets/GltfImporter.hh>
#include <Assets/MainImporter.hh>
#include <Assets/MeshOptimizer.hh>

namespace Mikoto {

    class CustomLogStream final : public Assimp::LogStream {
    public:
        auto write( const char *message ) -> void override {
            MKT_CORE_LOGGER_TRACE( "[Assimp] {}", message );
        }
    };

    struct MeshNodeCreateInfo {
        BufferHandle VertexBuffer{};
        BufferHandle IndexBuffer{};

        std::vector<TextureHandle> Textures{};
    };

    static constexpr std::array ASSIMP_TEXTURE_TYPES{
        aiTextureType_DIFFUSE,
        aiTextureType_SPECULAR,
        aiTextureType_NORMALS,
        aiTextureType_EMISSIVE,
        aiTextureType_METALNESS,
        aiTextureType_DIFFUSE_ROUGHNESS,
        aiTextureType_AMBIENT_OCCLUSION
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
        switch (type) {
            case aiTextureType_DIFFUSE:
                // Base color / albedo in PBR
                return MapType::ALBEDO_TEXTURE;

            case aiTextureType_NORMALS:
                return MapType::NORMAL_TEXTURE;

            case aiTextureType_METALNESS:
                return MapType::METALLIC_TEXTURE;

            case aiTextureType_DIFFUSE_ROUGHNESS:
                return MapType::ROUGHNESS_TEXTURE;

            case aiTextureType_AMBIENT_OCCLUSION:
                return MapType::AMBIENT_OCCLUSION_TEXTURE;

            case aiTextureType_EMISSIVE:
                return MapType::EMISSIVE_TEXTURE;

            default:
                return MapType::ALBEDO_TEXTURE;
        }
    }

    static auto LoadVertices( const aiMesh *mesh ) -> std::vector<float> {
        // special case for vulkan
        const bool invertY{ RenderService::Get()->IsGraphicsActive( GraphicsAPI::VULKAN_API ) };

        // The way the mesh is described is not guaranteed to follow the default
        // buffer layout, which is default for Models. Which means if the mesh
        // has no normals or texture coordinates, we have to insert default initialized
        // data to follow the default layout. We also have to introduce default
        // values for the color attribute

        std::vector<float> vertices{};

        const BufferLayout &defaultBufferLayout{ DEFAULT_VERTEX_BUFFER_LAYOUT };

        // floats per vertex (for instance, position is 3 floats, normals with 3 floats, etc.) times number of vertices
        vertices.reserve( ( defaultBufferLayout.GetStride() / sizeof( float ) ) * mesh->mNumVertices );

        for (UInt64 index{}; index < mesh->mNumVertices; index++) {
            // Vertices -----
            vertices.emplace_back( mesh->mVertices[index].x );
            vertices.emplace_back( mesh->mVertices[index].y );
            vertices.emplace_back( mesh->mVertices[index].z );

            // Normals -----
            if (mesh->HasNormals()) {
                vertices.emplace_back( mesh->mNormals[index].x );
                vertices.emplace_back( mesh->mNormals[index].y );
                vertices.emplace_back( mesh->mNormals[index].z );
            } else {
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
            }

            // Colors -----
            if (mesh->HasVertexColors( index )) {
                vertices.emplace_back( mesh->mColors[index]->r );
                vertices.emplace_back( mesh->mColors[index]->g );
                vertices.emplace_back( mesh->mColors[index]->b );
            } else {
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
            }

            // UV0 -----
            if (mesh->HasTextureCoords( 0 )) {
                vertices.emplace_back( mesh->mTextureCoords[0][index].x );
                vertices.emplace_back( Math::Abs( 1 - mesh->mTextureCoords[0][index].y) );
            } else {
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
            }

            // UV1 -----
            if (mesh->HasTextureCoords( 1 )) {
                vertices.emplace_back( mesh->mTextureCoords[1][index].x );
                vertices.emplace_back( mesh->mTextureCoords[1][index].y );
            } else {
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
            }

            // Joints (Animation Bone IDs)
            vertices.emplace_back( 0.0f );
            vertices.emplace_back( 0.0f );
            vertices.emplace_back( 0.0f );
            vertices.emplace_back( 0.0f );

            // Weights
            vertices.emplace_back( 0.0f );
            vertices.emplace_back( 0.0f );
            vertices.emplace_back( 0.0f );
            vertices.emplace_back( 0.0f );
        }

        return vertices;
    }

    static auto LoadIndices( const aiMesh *mesh ) -> std::vector<UInt32> {
        std::vector<UInt32> indices{};

        for (UInt64 i{}; i < mesh->mNumFaces; i++) {
            const auto face{ mesh->mFaces[i] };

            for (UInt64 index{}; index < face.mNumIndices; index++) {
                indices.emplace_back( face.mIndices[index] );
            }
        }

        return indices;
    }

    static auto LoadTexture( const std::string &modelRootPath, const aiMaterial *material, const aiTextureType type, const aiScene *scene ) -> TextureHandle {
        TextureHandle texture{};

        for (Size index{}; index < material->GetTextureCount( type ); index++) {
            aiString assimpTexturePath{};

            if (material->GetTexture( type, index, std::addressof( assimpTexturePath ) ) == AI_SUCCESS) {

                if (!StringUtil::Contains(assimpTexturePath.C_Str(), "*")) {
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
                            // TODO: untested
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

                    } catch ( std::exception &e ) {
                        MKT_CORE_LOGGER_ERROR( "LoadTexture - Failed to load embedded texture. Reason: {}", e.what() );
                    }
                }
            }

        }

        return texture;
    }

    static auto LoadTextures( const std::string &modelRootPath, const aiMesh *mesh, const aiScene *scene ) -> std::vector<TextureHandle> {
        std::vector<TextureHandle> textures{};

        if (mesh->mMaterialIndex > 0) {
            const aiMaterial *material{ scene->mMaterials[mesh->mMaterialIndex] };
            for (const aiTextureType &type: ASSIMP_TEXTURE_TYPES) {
                TextureHandle handle{ LoadTexture( modelRootPath, material, type, scene ) };

                if (!handle.IsEmpty()) {
                    textures.emplace_back( handle );
                }
            }
        }

        return textures;
    }

    auto LoadMaterial(aiMaterial const* mat) -> MaterialProperties {
        MaterialProperties properties{};

        aiString name{};
        if(mat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS) {
            properties.Name = name.C_Str();
        }

        return properties;
    }

    static auto ConstructMeshNode( GpuDevice *device, const std::string &rootPath,
                                   const aiMesh *mesh, const aiScene *scene )
        -> std::tuple<BufferHandle, BufferHandle, std::vector<TextureHandle>, std::string, MaterialProperties> {
        const std::string name{ mesh->mName.C_Str() };

        auto vertices{ LoadVertices( mesh ) };
        auto indices{ LoadIndices( mesh ) };
        auto textures{ LoadTextures( rootPath, mesh, scene ) };

        MaterialProperties properties{};
        // Load mesh physical properties
        if (mesh->mMaterialIndex >= 0U) {
            properties = LoadMaterial( scene->mMaterials[mesh->mMaterialIndex] );
        }

        BufferDescription vertexDesc{};
        vertexDesc.WithData( reinterpret_cast<Byte *>( vertices.data() ) )
                  .WithUsage( BufferUsage::VERTEX )
                  .WithBufferDataType( BufferDataType::BUFFER_DATA_FLOAT32 )
                  .WithSizeBytes( InferSize<float>( vertices.size() ) )
                  .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        BufferDescription indexDesc{};
        indexDesc.WithData( reinterpret_cast<Byte *>( indices.data() ) )
                 .WithUsage( BufferUsage::INDEX )
                 .WithSizeBytes( InferSize<UInt32>( indices.size() ) )
                 .WithBufferDataType( BufferDataType::BUFFER_DATA_UINT32 )
                 .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );

        return { device->CreateBuffer( vertexDesc ), device->CreateBuffer( indexDesc ), std::move( textures ), name, properties };
    }

    static auto LoadNodes( GpuDevice *device, const std::string &rootPath, const aiNode *node,
                           const aiScene *scene, const ModelLoadDescription &loadInfo ) -> std::vector<MeshNode> {
        // Assimp structures a scene like a hierarchy of nodes where
        // each node has child nodes and a list of meshes attached to it (the node only holds indices the actual meshes are in the aiScene structure).
        // We will first load all the meshes from the current node and recursively do the same task with children nodes.
        std::vector<MeshNode> result{};

        // Process all the meshes from this node
        for (UInt64 i{}; i < node->mNumMeshes; ++i) {
            auto [vertexBuf, indexBuf, textures, name, materialProperties]{
                ConstructMeshNode( device, rootPath, scene->mMeshes[node->mMeshes[i]], scene )
            };

            result.emplace_back( static_cast<Size>( node->mMeshes[i] ), std::move( vertexBuf ), std::move( indexBuf ), std::move( textures ), std::move( name ), std::move( materialProperties ) );
        }

        // Do the same for all the children nodex from this node
        for (UInt64 i{}; i < node->mNumChildren; ++i) {
            auto children{ LoadNodes( device, rootPath, node->mChildren[i], scene, loadInfo ) };
            std::ranges::move( children, std::back_inserter( result ) );
        }

        return result;
    }

    auto MeshFactory::ImportModel( GpuDevice *device, Assimp::Importer &importer, const ModelLoadDescription &loadInfo ) -> Model * {
        Model *result{ nullptr };

        // See more postprocessing options: https://assimp.sourceforge.net/lib_html/postprocess_8h.html
        constexpr auto importerFlags{ static_cast<aiPostProcessSteps>( aiProcess_Triangulate |
                                                                       aiProcess_FlipUVs |
                                                                       aiProcess_GenSmoothNormals |
                                                                       aiProcess_JoinIdenticalVertices ) };
        const File *file{ loadInfo.ModelFile };
        const std::string absolutePath{ file->GetPath() };
        const std::string fileName{ Path{ absolutePath }.stem().string() };

        const aiScene *scene{ importer.ReadFile( absolutePath.c_str(), importerFlags ) };

        if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr) {
            MKT_CORE_LOGGER_ERROR( "ImportModel - Model load failed. Assimp error: '{}'", importer.GetErrorString() );
        } else {
            Path rootPath{ file->GetPath() };
            rootPath.remove_filename();

            if (auto nodes{ LoadNodes( device, rootPath.string(), scene->mRootNode, scene, loadInfo ) }; !nodes.empty()) {
                result = new Model( fileName, absolutePath );

                for (MeshNode &node: nodes) {
                    result->PushMeshNode( node.GetMeshIndex(), std::move( node ) );
                }
            }
        }

        return result;
    }

    MeshFactory::MeshFactory( const MeshFactoryCreateInfo &createInfo )
        : m_Device{ createInfo.Device },
          m_WantCustomLog{ createInfo.UseCustomLogger } {
        m_Importers.resize( createInfo.ImportersCount );

        for (auto &importerInfo: m_Importers) {
            importerInfo = CreateScope<ImporterInfo>();
        }
    }

    auto MeshFactory::ImportModel( const ModelLoadDescription &loadInfo ) -> ModelHandle {
        Model *result{ nullptr };

        if (loadInfo.ModelFile == nullptr) {
            MKT_CORE_LOGGER_ERROR( "MeshFactory::ImportModel - File is null." );
            return ModelHandle::Create( result );
        }

        // Try to acquire a free importer
        ImporterInfo *acquiredImporter{};
        Size acquiredIndex{};

        for (Size i{ 0 }; i < m_Importers.size(); ++i) {
            bool expected{ true };
            if (m_Importers[i]->IsFree.compare_exchange_strong( expected, false,
                                                                std::memory_order_acquire )) {
                acquiredImporter = m_Importers[i].get();
                acquiredIndex = i;
                break;
            }
        }

        if (acquiredImporter) {
            result = ImportModel( m_Device, acquiredImporter->MeshImporter, loadInfo );

            acquiredImporter->IsFree.store( true, std::memory_order_release );

            MKT_CORE_LOGGER_DEBUG( "Using importer {}", acquiredIndex );
        }

        return ModelHandle::Create( result );
    }

    auto MeshFactory::SetupCustomAssimpLogger() -> void {
        if (m_WantCustomLog) {
            m_CustomLoggingImpl = CreateScope<CustomLogStream>();

            Assimp::DefaultLogger::create( "", Assimp::Logger::VERBOSE );
            Assimp::DefaultLogger::get()->attachStream( m_CustomLoggingImpl.get(), Assimp::Logger::VERBOSE );
        }
    }

    auto MeshFactory::Init() -> void {
        // Allocate importers
        for (const auto &importerInfo: m_Importers) {
            importerInfo->CustomFileHandlingImpl = nullptr;
            importerInfo->MeshImporter.SetIOHandler( importerInfo->CustomFileHandlingImpl.get() );
        }

        // Configure custom logger
        if (m_WantCustomLog) {
            SetupCustomAssimpLogger();
        }

        MeshOptimizer::OptimizerTestRun();

        m_IsInitialized = true;
    }

    auto MeshFactory::Shutdown() -> void {

        if (!m_IsInitialized) { return; }

        MKT_CORE_LOGGER_INFO( "Shutting down AssetsService..." );

        m_Importers.clear();
    }

    auto MeshFactoryCreateInfo::WithImportersCount( const Size count ) -> MeshFactoryCreateInfo & {
        this->ImportersCount = count;

        return *this;
    }

    auto MeshFactoryCreateInfo::WithCustomLogger( const bool enable ) -> MeshFactoryCreateInfo & {
        this->UseCustomLogger = enable;

        return *this;
    }

    auto MeshFactoryCreateInfo::WithCustomLoader( const bool enable ) -> MeshFactoryCreateInfo & {
        this->UseCustomLoader = enable;

        return *this;
    }

}// namespace Mikoto
