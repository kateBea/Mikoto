//
// Created by zanet on 3/28/2025.
//

#include <array>
#include <memory>
#include <vector>

#include <Assets/AssetsService.hh>
#include <Assets/MeshFactory.hh>
#include <Assets/Model.hh>
#include <Filesystem/FileService.hh>
#include <Renderer/RenderUtility.hh>
#include <Renderer/Pipeline.hh>
#include <Renderer/RenderService.hh>
#include <Library/Utility/Types.hh>

#include "assimp/DefaultLogger.hpp"
#include "assimp/postprocess.h"
#include "assimp/scene.h"

namespace Mikoto {

    class CustomLogStream final : public Assimp::LogStream {
    public:
        void write(const char* message) override {
            // Redirect to your engine logger
            MKT_CORE_LOGGER_DEBUG("[Assimp] {}", message);
        }
    };

    struct MeshNodeCreateInfo {
        BufferHandle VertexBuffer{};
        BufferHandle IndexBuffer{};

        std::vector<TextureHandle> Textures{};
    };

    static constexpr std::array ASSIMP_TEXTURE_TYPES{
        aiTextureType_BASE_COLOR,
        aiTextureType_NORMAL_CAMERA,
        aiTextureType_EMISSION_COLOR,
        aiTextureType_METALNESS,
        aiTextureType_DIFFUSE_ROUGHNESS,
        aiTextureType_AMBIENT_OCCLUSION
    };

    static auto InferMikotoTextureType( const aiTextureType type ) -> TextureType {
        switch ( type ) {
            case aiTextureType_BASE_COLOR:
            case aiTextureType_NORMALS:
            case aiTextureType_NORMAL_CAMERA:
            case aiTextureType_EMISSION_COLOR:
            case aiTextureType_METALNESS:
            case aiTextureType_DIFFUSE_ROUGHNESS:
            case aiTextureType_AMBIENT_OCCLUSION:
                return TextureType::TEXTURE_2D;
            default:
                return TextureType::TEXTURE_UNKNOWN;
        }
    }

    static auto LoadVertices( const aiMesh* mesh ) -> std::vector<float> {
        // special case for vulkan
        const bool invertY{ RenderService::Get()->IsGraphicsActive( GraphicsAPI::VULKAN_API ) };

        // The way we construct the vertex buffer data is not guaranteed to follow
        // the buffer layout, which is default for Models. Which means if the mesh
        // has no normal or texture coordinates, we have to insert default initialized
        // data to follow the default layout. We also have to introduce default
        // values for the color attribute

        std::vector<float> vertices{};

        const BufferLayout& defaultBufferLayout{ DEFAULT_VERTEX_BUFFER_LAYOUT };

        // floats per vertex (for instance, position is 3 floats, normals with 3 floats, etc.) times number of vertices
        vertices.reserve( ( defaultBufferLayout.GetStride() / sizeof( float ) ) * mesh->mNumVertices );

        for ( UInt64 index{}; index < mesh->mNumVertices; index++ ) {
            // This must follow the order of the default buffer layout

            // Vertices -----
            vertices.emplace_back( mesh->mVertices[index].x );
            vertices.emplace_back( mesh->mVertices[index].y );
            vertices.emplace_back( mesh->mVertices[index].z );

            // Normals -----
            if ( mesh->HasNormals() ) {
                vertices.emplace_back( mesh->mNormals[index].x );
                vertices.emplace_back( mesh->mNormals[index].y );
                vertices.emplace_back( mesh->mNormals[index].z );
            } else {
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
            }

            // Colors -----
            if ( mesh->HasVertexColors( index ) ) {
                vertices.emplace_back( mesh->mColors[index]->r );
                vertices.emplace_back( mesh->mColors[index]->g );
                vertices.emplace_back( mesh->mColors[index]->b );
            } else {
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
            }

            // Texture coordinates -----
            if ( mesh->mTextureCoords[0] != nullptr ) {
                vertices.emplace_back( mesh->mTextureCoords[0][index].x );
                vertices.emplace_back( invertY ? -mesh->mTextureCoords[0][index].y : mesh->mTextureCoords[0][index].y );
            } else {
                vertices.emplace_back( 0.0f );
                vertices.emplace_back( 0.0f );
            }

            // Tangents and Bi tangents
            if ( mesh->HasTangentsAndBitangents() ) {
            }
        }

        return vertices;
    }

    static auto LoadIndices( const aiMesh* mesh ) -> std::vector<UInt32> {
        std::vector<UInt32> indices{};

        for ( UInt64 i{}; i < mesh->mNumFaces; i++ ) {
            const auto face{ mesh->mFaces[i] };

            for ( UInt64 index{}; index < face.mNumIndices; index++ ) {
                indices.emplace_back( face.mIndices[index] );
            }
        }

        return indices;
    }

    static auto LoadTexture( const std::string& modelRootPath, const aiMaterial* material, const aiTextureType type, const aiScene* scene ) -> TextureHandle {
        TextureHandle texture{};

        for ( Size index{}; index < material->GetTextureCount( type ); index++ ) {
            aiString texturePath{};

            if ( material->GetTexture( type, index, std::addressof( texturePath ) ) == AI_SUCCESS ) {
                // Assumes the textures are in the same directory as the model files
                Path path{ PathBuilder()
                    .WithPath( modelRootPath )
                    .WithPath( texturePath.C_Str() )
                    .Build() };

                TextureLoadDescription loadInfo{};
                loadInfo.WithFile( FileService::Get()->LoadFile( path ) )
                    .WithType( InferMikotoTextureType( type ) );

                texture = AssetsService::Get()->LoadAsset<Texture>( loadInfo );
            }

            // Temporary. See if it is an embedded texture
            // This one will probably be loaded in the device directly and not cached in the assets service??
            auto [embeddedTexturePtr, embeddedTextureIndex] {
                scene->GetEmbeddedTextureAndIndex( texturePath.C_Str() )
            };

            if ( embeddedTextureIndex != -1 ) {
                MKT_CORE_LOGGER_WARN( "Model::LoadTextures - Texture is embedded! Index is {}", embeddedTextureIndex );
            }
        }

        return texture;
    }

    static auto LoadTextures(const std::string& modelRootPath, const aiMesh* mesh, const aiScene* scene ) -> std::vector<TextureHandle> {
        std::vector<TextureHandle> textures{};

        if ( mesh->mMaterialIndex > 0 ) {
            const aiMaterial* material{ scene->mMaterials[mesh->mMaterialIndex] };
            for ( const aiTextureType& type: ASSIMP_TEXTURE_TYPES ) {
                TextureHandle handle{ LoadTexture( modelRootPath, material, type, scene ) };
                textures.emplace_back(handle);
            }
        }

        return textures;
    }

    static auto ConstructMeshNode(GpuDevice* device, const std::string& rootPath,
                              const aiMesh* mesh, const aiScene* scene)
    -> std::tuple<BufferHandle, BufferHandle, std::vector<TextureHandle>>
    {
        auto vertices{ LoadVertices(mesh) };
        auto indices{ LoadIndices(mesh) };
        auto textures{ LoadTextures(rootPath, mesh, scene) };

        BufferDescription vertexDesc{};
        vertexDesc.WithData(reinterpret_cast<Byte*>(vertices.data()))
                  .WithUsage(BufferUsage::BUFFER_USAGE_VERTEX)
                  .WithBufferDataType(BufferDataType::BUFFER_DATA_FLOAT32)
                  .WithSizeBytes(InferSize<float>(vertices.size()))
                  .WithResourceUsageType(ResourceUsageType::RESOURCE_USAGE_STATIC);

        BufferDescription indexDesc{};
        indexDesc.WithData(reinterpret_cast<Byte*>(indices.data()))
                 .WithUsage(BufferUsage::BUFFER_USAGE_INDEX)
                 .WithSizeBytes(InferSize<UInt32>(indices.size()))
                 .WithBufferDataType(BufferDataType::BUFFER_DATA_UINT32)
                 .WithResourceUsageType(ResourceUsageType::RESOURCE_USAGE_STATIC);

        return { device->CreateBuffer(vertexDesc), device->CreateBuffer(indexDesc), std::move(textures) };
    }

    static auto LoadNodes(GpuDevice* device, const std::string& rootPath, const aiNode* node,
                      const aiScene* scene, const ModelLoadDescription& loadInfo) -> std::vector<MeshNode>
    {
        // Assimp structures a scene like a hierarchy of nodes where
        // each node has child nodes and a list of meshes attached to it (the node only holds indices the actual meshes are in the aiScene structure).
        // We will first load all the meshes from the current node and recursively do the same task with children nodes.
        std::vector<MeshNode> result{};

        // Process all the meshes from this node
        for (UInt64 i{}; i < node->mNumMeshes; ++i) {
            auto [vertexBuf, indexBuf, textures] {
                ConstructMeshNode(device, rootPath, scene->mMeshes[node->mMeshes[i]], scene)
            };

            result.emplace_back( static_cast<Size>(node->mMeshes[i]), std::move(vertexBuf),std::move(indexBuf), std::move(textures) );
        }

        // Do the same for all the children nodex from this node
        for (UInt64 i{}; i < node->mNumChildren; ++i) {
            auto children{ LoadNodes(device, rootPath, node->mChildren[i], scene, loadInfo) };
            std::ranges::move(children, std::back_inserter(result));
        }

        return result;
    }

    auto MeshFactory::ImportModel(GpuDevice* device, Assimp::Importer& importer, const ModelLoadDescription& loadInfo ) -> Model* {
        Model* result{ nullptr };

        // See more postprocessing options: https://assimp.sourceforge.net/lib_html/postprocess_8h.html
        constexpr auto importerFlags{ static_cast<aiPostProcessSteps>( aiProcess_Triangulate |
                                                                       aiProcess_FlipUVs |
                                                                       aiProcess_GenSmoothNormals |
                                                                       aiProcess_JoinIdenticalVertices ) };
        const File* file{ loadInfo.ModelFile };
        const std::string absolutePath{ file->GetPath() };
        const std::string fileName{ Path{ absolutePath }.stem().string() };

        const aiScene* scene{ importer.ReadFile( absolutePath.c_str(), importerFlags ) };
        //const aiScene* scene{ importer.ReadFileFromMemory(  file->GetFileBytes(), file->GetSizeBytes(), importerFlags ) };
        if ( scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr ) {
            MKT_CORE_LOGGER_ERROR( "ImportModel - Model load failed. Assimp error: '{}'", importer.GetErrorString() );
        } else {

            Path rootPath{file->GetPath() };
            rootPath.remove_filename();

            if ( auto nodes{ LoadNodes(device, rootPath.string(), scene->mRootNode, scene, loadInfo ) }; !nodes.empty() ) {
                result = new Model( fileName, absolutePath );

                for ( MeshNode& node: nodes ) {
                    result->AddMeshNode( node.GetMeshIndex(), std::move( node ) );
                }
            }
        }

        return result;
    }

    MeshFactory::MeshFactory( const MeshFactoryCreateInfo& createInfo )
        : m_Device{ createInfo.Device },
            m_WantCustomLog{ createInfo.UseCustomLogger } {
        m_Importers.resize( createInfo.ImportersCount );

        for (auto& importerInfo : m_Importers) {
            importerInfo = CreateScope<ImporterInfo>();
        }
    }

    auto MeshFactory::ImportModel( const ModelLoadDescription& loadInfo ) -> ModelHandle {
        if ( loadInfo.ModelFile == nullptr ) {
            MKT_CORE_LOGGER_ERROR( "MeshFactory::ImportModel - File is null." );
            return ModelHandle::CreateEmpty();
        }

        Model* result{ nullptr };

        // Find available importer and atomically acquire it
        const auto availableImporter{
            std::ranges::find_if( m_Importers, []( const Unique<ImporterInfo>& info ) {
                bool expected{ true };
                return info->IsFree.compare_exchange_strong( expected, false );
            } )
        };

        if ( availableImporter != m_Importers.end() ) {
            result = ImportModel(m_Device, (*availableImporter)->MeshImporter, loadInfo );

            // Mark importer as free again,
            // std::memory_order_release ensures that any thread that later
            // reads IsFree == true will see all previous writings to the object
            (*availableImporter)->IsFree.store( true, std::memory_order_release );
        }

        return ModelHandle::Create( result );
    }

    auto MeshFactory::SetupCustomAssimpLogger() -> void {
        if (m_WantCustomLog) {
            m_CustomLoggingImpl = CreateScope<CustomLogStream>();

            Assimp::DefaultLogger::create("", Assimp::Logger::VERBOSE);
            Assimp::DefaultLogger::get()->attachStream(m_CustomLoggingImpl.get(), Assimp::Logger::VERBOSE);
        }
    }

    auto MeshFactory::Init() -> void {
        // Allocate importers
        for ( const auto& importerInfo : m_Importers) {
            importerInfo->CustomFileHandlingImpl = nullptr;
            importerInfo->MeshImporter.SetIOHandler(importerInfo->CustomFileHandlingImpl.get());
        }

        // Configure custom logger
        if ( m_WantCustomLog ) {
            SetupCustomAssimpLogger();
        }

        m_IsInitialized = true;
    }

    auto MeshFactory::Shutdown() -> void {

        if ( !m_IsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_INFO( "Shutting down AssetsService..." );

        m_Importers.clear();
    }

    auto MeshFactoryCreateInfo::WithImportersCount( const Size count ) -> MeshFactoryCreateInfo& {
        this->ImportersCount = count;

        return *this;
    }

    auto MeshFactoryCreateInfo::WithCustomLogger( const bool enable ) -> MeshFactoryCreateInfo& {
        this->UseCustomLogger = enable;

        return *this;
    }

    auto MeshFactoryCreateInfo::WithCustomLoader( const bool enable ) -> MeshFactoryCreateInfo& {
        this->UseCustomLoader = enable;

        return *this;
    }

}// namespace Mikoto
