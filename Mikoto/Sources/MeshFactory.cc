// //
// // Created by zanet on 3/28/2025.
// //
//
// #include <Assets/AssetsService.hh>
// #include <Assets/MeshFactory.hh>
// #include <Assets/Model.hh>
// #include <FileSystem/FileService.hh>
// #include <Library/IO/PathBuilder.hh>
// #include <Renderer/GpuUtility.hh>
// #include <Renderer/Pipeline.hh>
// #include <Renderer/RenderService.hh>
// #include <Threading/Task.hh>
//
// #include "assimp/postprocess.h"
// #include "assimp/scene.h"
//
// namespace Mikoto {
//
//     struct MeshNodeCreateInfo {
//         BufferHandle VertexBuffer{};
//         BufferHandle IndexBuffer{};
//
//         std::vector<TextureHandle> Textures{};
//     };
//
//     static auto InferMikotoTextureType( const aiTextureType type ) -> TextureType {
//         switch ( type ) {
//             case aiTextureType_BASE_COLOR:
//             case aiTextureType_NORMALS:
//             case aiTextureType_NORMAL_CAMERA:
//             case aiTextureType_EMISSION_COLOR:
//             case aiTextureType_METALNESS:
//             case aiTextureType_DIFFUSE_ROUGHNESS:
//             case aiTextureType_AMBIENT_OCCLUSION:
//                 return TextureType::TEXTURE_2D;
//             default:
//                 return TextureType::TEXTURE_INVALID;
//         }
//     }
//
//     static auto LoadVertices( const aiMesh* mesh ) -> std::vector<float> {
//         // special case for vulkan
//         const bool invertY{ RenderService::GetInstance()->IsGraphicsActive( GraphicsAPI::VULKAN_API ) };
//
//         // The way we construct the vertex buffer data is not guaranteed to follow
//         // the buffer layout, which is default for Models. Which means if the mesh
//         // has no normal or texture coordinates, we have to insert default initialized
//         // data to follow the default layout. We also have to introduce default
//         // values for the color attribute
//
//         std::vector<float> vertices{};
//
//         const BufferLayout& defaultBufferLayout{ DEFAULT_VERTEX_BUFFER_LAYOUT };
//
//         // floats per vertex (a vertex is position with 3 floats, normals with 3 floats, etc.) times number of vertices
//         vertices.reserve( ( defaultBufferLayout.GetStride() / sizeof( float ) ) * mesh->mNumVertices );
//
//         for ( UInt64_T index{}; index < mesh->mNumVertices; index++ ) {
//             // This must follow the order of the default buffer layout
//
//             // Vertices -----
//             vertices.emplace_back( mesh->mVertices[index].x );
//             vertices.emplace_back( mesh->mVertices[index].y );
//             vertices.emplace_back( mesh->mVertices[index].z );
//
//             // Normals -----
//             if ( mesh->HasNormals() ) {
//                 vertices.emplace_back( mesh->mNormals[index].x );
//                 vertices.emplace_back( mesh->mNormals[index].y );
//                 vertices.emplace_back( mesh->mNormals[index].z );
//             } else {
//                 vertices.emplace_back( 0.0f );
//                 vertices.emplace_back( 0.0f );
//                 vertices.emplace_back( 0.0f );
//             }
//
//             // Colors -----
//             if ( mesh->HasVertexColors( index ) ) {
//                 vertices.emplace_back( mesh->mColors[index]->r );
//                 vertices.emplace_back( mesh->mColors[index]->g );
//                 vertices.emplace_back( mesh->mColors[index]->b );
//             } else {
//                 vertices.emplace_back( 0.0f );
//                 vertices.emplace_back( 0.0f );
//                 vertices.emplace_back( 0.0f );
//             }
//
//             // Texture coordinates -----
//             if ( mesh->mTextureCoords[0] != nullptr ) {
//                 vertices.emplace_back( mesh->mTextureCoords[0][index].x );
//                 vertices.emplace_back( invertY ? -mesh->mTextureCoords[0][index].y : mesh->mTextureCoords[0][index].y );
//             } else {
//                 vertices.emplace_back( 0.0f );
//                 vertices.emplace_back( 0.0f );
//             }
//
//             // Tangents and Bitangents
//             if ( mesh->HasTangentsAndBitangents() ) {
//             }
//         }
//
//         return vertices;
//     }
//
//     auto LoadIndices( const aiMesh* mesh ) -> std::vector<UInt32_T> {
//         std::vector<UInt32_T> indices{};
//
//         for ( UInt64_T i{}; i < mesh->mNumFaces; i++ ) {
//             const auto face{ mesh->mFaces[i] };
//
//             for ( UInt64_T index{}; index < face.mNumIndices; index++ ) {
//                 indices.emplace_back( face.mIndices[index] );
//             }
//         }
//
//         return indices;
//     }
//
//     static auto LoadTexture( const std::string& modelRootPath, const aiMaterial* material, const aiTextureType type, const aiScene* scene ) -> Ref<Texture> {
//         Ref<Texture> texture{};
//
//         for ( Size_T index{}; index < material->GetTextureCount( type ); index++ ) {
//             aiString texturePath{};
//
//             if ( material->GetTexture( type, index, std::addressof( texturePath ) ) == AI_SUCCESS ) {
//                 // Assumes the textures are in the same directory as the model files
//                 Path_T path{ PathBuilder()
//                                      .WithPath( modelRootPath )
//                                      .WithPath( texturePath.C_Str() )
//                                      .Build() };
//
//                 TextureLoadDescription loadInfo{};
//                 loadInfo
//                     .WithFile( FileService::GetInstance()->LoadFile( path ) )
//                     .WithType( InferMikotoTextureType( type ) );
//
//                 texture = AssetsService::GetInstance()->LoadAsset<Texture>( loadInfo.TextureFile->GetPath() );
//             }
//
//             // Temporary. See if it is an embedded texture
//             // This one will probably be loaded in the device directly and not cached in the assets service??
//             auto [embeddedTexturePtr, embeddedTextureIndex] {
//                 scene->GetEmbeddedTextureAndIndex( texturePath.C_Str() )
//             };
//
//             if ( embeddedTextureIndex != -1 ) {
//                 MKT_CORE_LOGGER_WARN( "Model::LoadTextures - Texture is embedded! Index is {}", embeddedTextureIndex );
//             }
//         }
//
//         return texture;
//     }
//
//     static auto LoadTextures(const std::string& modelRootPath, const aiMesh* mesh, const aiScene* scene ) -> std::vector<Ref<Texture>> {
//         std::vector<Ref<Texture>> textures{};
//
//         if ( mesh->mMaterialIndex > 0 ) {
//             const aiMaterial* material{ scene->mMaterials[mesh->mMaterialIndex] };
//
//             const std::vector<aiTextureType> types{
//                 aiTextureType_BASE_COLOR,
//                 aiTextureType_NORMAL_CAMERA,
//                 aiTextureType_EMISSION_COLOR,
//                 aiTextureType_METALNESS,
//                 aiTextureType_DIFFUSE_ROUGHNESS,
//                 aiTextureType_AMBIENT_OCCLUSION
//             };
//
//
//             // TODO: Paralelize texture loading
//             for ( const aiTextureType& type: types ) {
//                 TextureHandle handle{ LoadTexture( modelRootPath, material, type, scene ) };
//
//                 textures.emplace_back(handle);
//             }
//         }
//
//         return textures;
//     }
//
//     static auto ConstructMeshNode(GpuDevice* device, const std::string& modelRootPath, const aiMesh* aiMesh, const aiScene* scene ) -> MeshNodeCreateInfo {
//
//         std::vector<float> vertices{ LoadVertices( aiMesh ) };
//         std::vector<UInt32_T> indices{ LoadIndices( aiMesh ) };
//         std::vector<Ref<Texture>> textures{ LoadTextures( modelRootPath, aiMesh, scene ) };
//
//         // Vertex buffer
//         BufferDescription verticesDescription{};
//
//         verticesDescription
//                 .WithData( reinterpret_cast<Byte_T*>( vertices.data() ) )
//                 .WithUsage( BufferUsage::BUFFER_USAGE_VERTEX )
//                 .WithSizeBytes( InferSize<float>( vertices.size() ) )
//                 .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );
//
//         const BufferHandle vertexBufferHandle{ device->CreateBuffer( verticesDescription ) };
//
//         // Index buffer
//         BufferDescription indicesDescription{};
//
//         indicesDescription
//                 .WithData( reinterpret_cast<Byte_T*>( indices.data() ) )
//                 .WithUsage( BufferUsage::BUFFER_USAGE_INDEX )
//                 .WithSizeBytes( InferSize<UInt32_T>( indices.size() ) )
//                 .WithBufferDataType( BufferDataType::BUFFER_DATA_UINT32 )
//                 .WithResourceUsageType( ResourceUsageType::RESOURCE_USAGE_STATIC );
//
//         const BufferHandle indexBufferHandle{ device->CreateBuffer( indicesDescription ) };
//
//         MeshNodeCreateInfo result{
//             .Textures{ std::move( textures ) },
//             .IndexBuffer{ indexBufferHandle },
//             .VertexBuffer{ vertexBufferHandle }
//         };
//
//         return result;
//     }
//
//     static auto LoadNodes( GpuDevice* device, const std::string& rootPath, const aiNode* rootNode, const aiScene* scene, const ModelLoadDescription& loadInfo ) -> std::vector<MeshNode> {
//         // Assimp structures a scene like a hierarchy of nodes where
//         // each node has child nodes and a list of meshes attached to it (the node only holds indices the actual meshes are in the aiScene structure).
//         // We will first load all the meshes from the current node and recursively do the same task with children nodes.
//         std::vector<MeshNode> result{};
//
//         // Process all the meshes from this node
//         for ( UInt64_T meshIndex{}; meshIndex < rootNode->mNumMeshes; meshIndex++ ) {
//             auto [VertexBuffer, IndexBuffer, Textures]{
//                 std::move( ConstructMeshNode(device, rootPath, scene->mMeshes[rootNode->mMeshes[meshIndex]], scene ) )
//             };
//
//             result.emplace_back( rootNode->mMeshes[meshIndex], VertexBuffer, IndexBuffer, std::move( Textures ) );
//         }
//
//         // Do the same for all the children nodex from this node
//         for ( UInt64_T indexChildNode{}; indexChildNode < rootNode->mNumChildren; indexChildNode++ ) {
//             auto children{ LoadNodes(device, rootPath, rootNode->mChildren[indexChildNode], scene, loadInfo ) };
//
//             std::ranges::move( children, std::back_inserter( result ) );
//         }
//
//         return result;
//     }
//
//     static auto ImportModel(GpuDevice* device, Assimp::Importer* importer, const ModelLoadDescription& loadInfo ) -> Model* {
//         Model* result{ nullptr };
//
//         // See more postprocessing options: https://assimp.sourceforge.net/lib_html/postprocess_8h.html
//         constexpr auto importerFlags{ static_cast<aiPostProcessSteps>( aiProcess_Triangulate |
//                                                                        aiProcess_FlipUVs |
//                                                                        aiProcess_GenSmoothNormals |
//                                                                        aiProcess_JoinIdenticalVertices ) };
//
//         const std::string absolutePath{ loadInfo.ModelFile->GetPath() };
//         const std::string fileName{ Path_T{ absolutePath }.stem().string() };
//
//         const aiScene* scene{ importer->ReadFile( absolutePath.c_str(), importerFlags ) };
//
//         if ( scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr ) {
//             MKT_CORE_LOGGER_ERROR( "ImportModel - Model load failed. Assimp error: '{}'", importer->GetErrorString() );
//         } else {
//
//             Path_T rootPath{loadInfo.ModelFile->GetPath() };
//             rootPath.remove_filename();
//
//             if ( auto nodes{ LoadNodes(device, rootPath.string(), scene->mRootNode, scene, loadInfo ) }; !nodes.empty() ) {
//                 result = new Model( fileName, absolutePath );
//
//                 for ( MeshNode& node: nodes ) {
//                     result->AddMeshNode( node.GetMeshIndex(), std::move( node ) );
//                 }
//             }
//         }
//
//         return result;
//     }
//
//     auto MeshFactory::Init() -> void {
//         // Configure custom logger
//         if ( m_WantCustomLog ) {
//             SetupCustomAssimpLogger();
//         }
//
//         // Configure custom importer system for all available importers
//         if ( m_WantCustomLoader ) {
//             SetupCustomLoaderForImporters();
//         }
//
//         m_IsInitialized = true;
//     }
//
//     auto MeshFactory::Shutdown() -> void {
//
//         if ( m_IsInitialized ) {
//         }
//     }
//
//     MeshFactory::MeshFactory( const MeshFactoryCreateInfo& createInfo )
//         : m_Device{ createInfo.Device }, m_WantCustomLog{ createInfo.UseCustomLogger },
//           m_WantCustomLoader{ createInfo.UseCustomLoader } {
//         m_Importers.resize( createInfo.ImportersCount );
//     }
//
//     auto MeshFactory::CreateModel( const ModelLoadDescription& loadInfo ) -> Ref<Model> {
//         if ( loadInfo.ModelFile == nullptr ) {
//             MKT_CORE_LOGGER_ERROR( "MeshFactory::Load - File is null." );
//             return {};
//         }
//
//         Model* result{ nullptr };
//
//         // Find available importer and atomically acquire it
//         const auto availableImporter{
//             std::ranges::find_if( m_Importers, []( ImporterInfo& info ) {
//                 bool expected{ true };
//                 return info.IsFree.compare_exchange_strong( expected, false );
//             } )
//         };
//
//         if ( availableImporter != m_Importers.end() ) {
//             result = ImportModel(m_Device, std::addressof( availableImporter->MeshImporter ), loadInfo );
//
//             // Mark importer as free again,
//             // std::memory_order_release ensures that any thread that later
//             // reads IsFree == true will see all previous writings to the object
//             availableImporter->IsFree.store( true, std::memory_order_release );
//         }
//
//         return Ref<Model>{ result };
//     }
//
//     auto MeshFactory::SetupCustomAssimpLogger() -> void {
//     }
//
//     auto MeshFactory::SetupCustomLoaderForImporters() -> void {
//     }
//
//     auto MeshFactoryCreateInfo::WithImportersCount( const Size_T count ) -> MeshFactoryCreateInfo& {
//         this->ImportersCount = count;
//
//         return *this;
//     }
//
//     auto MeshFactoryCreateInfo::WithCustomLogger( const bool enable ) -> MeshFactoryCreateInfo& {
//         this->UseCustomLogger = enable;
//
//         return *this;
//     }
//
//     auto MeshFactoryCreateInfo::WithCustomLoader( const bool enable ) -> MeshFactoryCreateInfo& {
//         this->UseCustomLoader = enable;
//
//         return *this;
//     }
//
// }// namespace Mikoto
