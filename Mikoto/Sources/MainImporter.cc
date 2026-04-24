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
//
// #include <assimp/GltfMaterial.h>
// #include <assimp/postprocess.h>
// #include <assimp/scene.h>
//
// #include <Assets/AssetsService.hh>
// #include <Assets/MainImporter.hh>
// #include <Assets/Model.hh>
// #include <Core/String.hh>
// #include <Filesystem/FileService.hh>
// #include <Filesystem/FileSystem.hh>
// #include <Library/Utility/Types.hh>
// #include <Logging/Logger.hh>
// #include <Material/PhysicalMaterial.hh>
// #include <Math/Math.hh>
// #include <Renderer/Core/RenderSystem.hh>
// #include <Renderer/Core/RenderUtility.hh>
// #include <Threading/ThreadUtility.hh>
// #include <array>
// #include <assimp/DefaultLogger.hpp>
// #include <cstdlib>
// #include <memory>
// #include <vector>
//
// // TODO: rework this importer, GLTF is the default one, for any other file type we default to Assimp
//
// namespace mikoto {
//
// #define MKT_ASSIMP_LOAD_UV_SET( PROPERTIES_FIELD, MATERIAL_PTR, TEXTURE_TYPE )                         \
//     do {                                                                                              \
//         aiString __tmpPath{};                                                                         \
//         if ( ( MATERIAL_PTR )->GetTextureCount( TEXTURE_TYPE ) > 0 ) {                                \
//             if ( ( MATERIAL_PTR )->GetTexture( TEXTURE_TYPE, 0, &__tmpPath ) == AI_SUCCESS ) {        \
//                 ( PROPERTIES_FIELD ) = 0;                                                             \
//             } else if ( ( MATERIAL_PTR )->GetTexture( TEXTURE_TYPE, 1, &__tmpPath ) == AI_SUCCESS ) { \
//                 ( PROPERTIES_FIELD ) = 1;                                                             \
//             }                                                                                         \
//         }                                                                                             \
//     } while ( 0 )
//
//     class CustomLogStream final : public Assimp::LogStream {
//     public:
//         auto write( const char *message ) -> void override {
//             MKT_CORE_LOGGER_TRACE( "[Assimp] {}", message );
//         }
//     };
//
//     MKT_NODISCARD static auto ParseAlphaMode(std::string_view mode) -> PBR_AlphaMode {
//         if (mode == "MASK" || mode == "Mask" || mode == "mask") {
//             return PBR_AlphaMode::Mask;
//         }
//
//         if (mode == "BLEND" || mode == "Blend" || mode == "blend") {
//             return PBR_AlphaMode::Blend;
//         }
//
//         return PBR_AlphaMode::Opaque;
//     }
//
//     static auto ToMat4F( const aiMatrix4x4 &from ) -> Mat4F {
//         // GLM is column major, Assimp is row major
//
//         Mat4F to{};
//         // The a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
//         // We are basically doing a transpose operation
//
//         to[0][0] = from.a1;
//         to[1][0] = from.a2;
//         to[2][0] = from.a3;
//         to[3][0] = from.a4;
//         to[0][1] = from.b1;
//         to[1][1] = from.b2;
//         to[2][1] = from.b3;
//         to[3][1] = from.b4;
//         to[0][2] = from.c1;
//         to[1][2] = from.c2;
//         to[2][2] = from.c3;
//         to[3][2] = from.c4;
//         to[0][3] = from.d1;
//         to[1][3] = from.d2;
//         to[2][3] = from.d3;
//         to[3][3] = from.d4;
//
//         return to;
//     }
//
//     static constexpr std::array ASSIMP_TEXTURE_TYPES{
//             aiTextureType_DIFFUSE,
//             aiTextureType_SPECULAR,
//             aiTextureType_AMBIENT,
//             aiTextureType_EMISSIVE,
//             aiTextureType_HEIGHT,
//             aiTextureType_NORMALS,
//             aiTextureType_SHININESS,
//             aiTextureType_OPACITY,
//             aiTextureType_DISPLACEMENT,
//             aiTextureType_LIGHTMAP,
//             aiTextureType_REFLECTION,
//             aiTextureType_BASE_COLOR,
//             aiTextureType_NORMAL_CAMERA,
//             aiTextureType_EMISSION_COLOR,
//             aiTextureType_METALNESS,
//             aiTextureType_DIFFUSE_ROUGHNESS,
//             aiTextureType_AMBIENT_OCCLUSION,
//             aiTextureType_UNKNOWN,
//             aiTextureType_SHEEN,
//             aiTextureType_CLEARCOAT,
//             aiTextureType_TRANSMISSION,
//             aiTextureType_MAYA_BASE,
//             aiTextureType_MAYA_SPECULAR,
//             aiTextureType_MAYA_SPECULAR_COLOR,
//             aiTextureType_MAYA_SPECULAR_ROUGHNESS
//     };
//
//     static auto InferMikotoTextureType( const aiTextureType type ) -> TextureType {
//         switch (type) {
//             case aiTextureType_DIFFUSE:
//             case aiTextureType_SPECULAR:
//             case aiTextureType_NORMALS:
//             case aiTextureType_EMISSIVE:
//             case aiTextureType_METALNESS:
//             case aiTextureType_DIFFUSE_ROUGHNESS:
//             case aiTextureType_AMBIENT_OCCLUSION:
//                 return TextureType::TEXTURE_2D;
//             default:
//                 return TextureType::TEXTURE_UNKNOWN;
//         }
//     }
//
//     static auto InferMapType( const aiTextureType type ) -> MapType {
//         switch ( type ) {
//             case aiTextureType_DIFFUSE:
//             case aiTextureType_AMBIENT: // Not sure if this one should go here, Mikoto does not contemplate an ambient map yet
//             case aiTextureType_BASE_COLOR:
//                 return MapType::BASE_COLOR_TEXTURE;
//
//             case aiTextureType_NORMALS:
//             case aiTextureType_NORMAL_CAMERA:
//                 return MapType::NORMAL_TEXTURE;
//
//             case aiTextureType_METALNESS:
//                 return MapType::METALLIC_TEXTURE;
//
//             case aiTextureType_DIFFUSE_ROUGHNESS:
//                 return MapType::ROUGHNESS_TEXTURE;
//
//             case aiTextureType_SHININESS:
//                 return MapType::ROUGHNESS_TEXTURE;
//
//             case aiTextureType_MAYA_SPECULAR_ROUGHNESS:
//                 return MapType::ROUGHNESS_TEXTURE;
//
//             case aiTextureType_AMBIENT_OCCLUSION:
//             case aiTextureType_LIGHTMAP:
//                 return MapType::AMBIENT_OCCLUSION_TEXTURE;
//
//             case aiTextureType_EMISSIVE:
//             case aiTextureType_EMISSION_COLOR:
//                 return MapType::EMISSIVE_TEXTURE;
//
//             default:
//                 return MapType::UNDEFINED_TEXTURE;
//         }
//     }
//
//     static auto LoadBoneWeights( const aiMesh *mesh, MeshNodeData &meshNodeData, Skeleton& skeleton ) -> void {
//         MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_AQUA, "Mesh {} has [{}] bones\n", mesh->mName.C_Str(), mesh->mNumBones );
//
//         for ( Int32 boneIndex{}; boneIndex < mesh->mNumBones; ++boneIndex ) {
//             std::string boneName{ mesh->mBones[boneIndex]->mName.C_Str() };
//
//             // This should not happen because we first load the skeleton, and then we load the meshes
//             MKT_ASSERT( skeleton.HasJoint( boneName ), StringUtil::Format( "Bone {} not found in skeleton", boneName ) );
//
//             Joint* joint{ skeleton.FindJoint( boneName ) };
//
//             aiVertexWeight *weights{ mesh->mBones[boneIndex]->mWeights };
//             UInt32 numWeights{ mesh->mBones[boneIndex]->mNumWeights };
//
//             for ( UInt32 weightIndex{}; weightIndex < numWeights; ++weightIndex ) {
//                 float weight{ weights[weightIndex].mWeight };
//                 UInt32 vertexId{ weights[weightIndex].mVertexId };
//
//                 skeleton.SetWeights( mesh->mName.C_Str(), boneName, vertexId, weight );
//
//                 MKT_ASSERT( vertexId < meshNodeData.Vertices.size(), "vertexID out of bounds for vertices count" );
//
//                 VertexData &vertex{ meshNodeData.Vertices[vertexId] };
//
//                 // We allow only 4 bones to affect a vertex at max
//                 // We set the first 4 bones that have influence on the vertex
//                 // We might later want to pick the highest contributions
//                 for ( Size i{}; i < MAX_BONE_INFLUENCE; ++i ) {
//                     if ( vertex.Weights[i] == 0 ) {
//                         vertex.Joints[i] = joint->GetID();
//                         vertex.Weights[i] = weight;
//                         break;
//                     }
//                 }
//             }
//         }
//     }
//
//     static auto GetAnimationProperties( const aiAnimation *animation, ModelData& modelData ) -> void {
//         //Skeleton& skeleton{ modelData.SceneSkeleton };
//         //const UInt32 size{ ( animation->mNumChannels ) };
//
//         //std::vector<AnimationSampler> samplers{};
//         //std::vector<AnimationChannel> channels{};
//
//         //for ( UInt32 i{}; i < size; i++ ) {
//         //    aiNodeAnim* channel{ animation->mChannels[i] };
//         //    std::string jointName{ channel->mNodeName.data };
//
//         //    AnimationSampler animationSampler{};
//         //    AnimationChannel animationChannel{};
//
//         //    Joint* joint{ skeleton.FindJoint( jointName ) };
//         //    if (joint) {
//         //        // TODO: fill structures properly
//         //        samplers.emplace_back( animationSampler );
//         //        channels.emplace_back( animationChannel );
//         //    }
//         //}
//     }
//
//     static auto LoadVertices( const aiMesh *mesh, MeshNodeData& meshNodeData ) -> void {
//         meshNodeData.Vertices.resize( mesh->mNumVertices );
//
//         for (UInt64 index{}; index < mesh->mNumVertices; index++) {
//             auto& vertex{ meshNodeData.Vertices[index] };
//
//             vertex.Position.x = mesh->mVertices[index].x;
//             vertex.Position.y = mesh->mVertices[index].y;
//             vertex.Position.z = mesh->mVertices[index].z;
//
//             if (mesh->HasNormals()) {
//                 vertex.Normals.x = mesh->mNormals[index].x;
//                 vertex.Normals.y = mesh->mNormals[index].y;
//                 vertex.Normals.z = mesh->mNormals[index].z;
//             }
//
//             if (mesh->HasVertexColors( index )) {
//                 vertex.Colors.r = mesh->mColors[index]->r;
//                 vertex.Colors.g = mesh->mColors[index]->g;
//                 vertex.Colors.b = mesh->mColors[index]->b;
//                 vertex.Colors.a = mesh->mColors[index]->a;
//             }
//
//             if (mesh->HasTextureCoords( 0 )) {
//                 vertex.UV_0.x = mesh->mTextureCoords[0][index].x;
//                 vertex.UV_0.y = mesh->mTextureCoords[0][index].y;
//             }
//
//             if (mesh->HasTextureCoords( 1 )) {
//                 vertex.UV_1.x = mesh->mTextureCoords[1][index].x;
//                 vertex.UV_1.y =  mesh->mTextureCoords[1][index].y;
//             }
//         }
//     }
//
//     static auto LoadIndices( const aiMesh *mesh, MeshNodeData& meshNodeData ) -> void {
//         for (UInt64 i{}; i < mesh->mNumFaces; i++) {
//             const auto face{ mesh->mFaces[i] };
//
//             for (UInt64 index{}; index < face.mNumIndices; index++) {
//                 meshNodeData.Indices.emplace_back( face.mIndices[index] );
//             }
//         }
//     }
//
//     static auto LoadTexture( const std::string &modelRootPath, const aiMaterial *material, const aiTextureType type, const aiScene *scene ) -> TextureHandle {
//         TextureHandle texture{};
//
//         for (Size index{}; index < material->GetTextureCount( type ); index++) {
//             aiString assimpTexturePath{};
//
//             if (material->GetTexture( type, index, std::addressof( assimpTexturePath ) ) == AI_SUCCESS) {
//
//                 std::string textureUri{ assimpTexturePath.C_Str() };
//
//                 if (!StringUtil::Contains(textureUri, "*")) {
//                     // Assumes the textures are in the same directory as the model files
//                     Path path{ PathBuilder()
//                                .WithPath( modelRootPath )
//                                .WithPath( assimpTexturePath.C_Str() )
//                                .Build() };
//
//                     constexpr std::string_view tifExt{ ".tif" };
//                     constexpr std::string_view pngExt{ ".png" };
//
//                     // FIXME:
//                     // Some models use TIF extension, if they offer a PNG variant use PNG instead
//                     // as we do not have a TIF decoder for the time being
//                     if (StringUtil::Equal( path.extension().string(), tifExt )) {
//                         path.replace_extension(pngExt);
//                     }
//
//                     TextureLoadDescription loadInfo{};
//                     loadInfo.WithFile( FileService::Get()->LoadFile( path ) )
//                             .WithType( InferMikotoTextureType( type ) )
//                             .WithMapType( InferMapType( type ) );
//
//                     try {
//                         texture = AssetsService::Get()->LoadAsset<Texture>( loadInfo );
//                         textureUri = path.string();
//                     } catch (std::exception &e) {
//                         MKT_CORE_LOGGER_ERROR( "LoadTexture - Failed to load texture. Reason: {}", e.what() );
//                     }
//                 } else {
//                     try {
//                         Int32 embeddedIndex{ std::atoi( assimpTexturePath.C_Str() + 1 ) };
//                         const aiTexture *tex{ scene->mTextures[embeddedIndex] };
//
//                         Path path{ PathBuilder()
//                                            .WithPath( modelRootPath )
//                                            .WithPath( assimpTexturePath.C_Str() )
//                                            .Build() };
//
//                         TextureDescription loadInfo{};
//
//                         //If mHeight value is zero, pcData points to an
//                         //compressed texture in any format (e.g. JPEG). (see texture.h in Assimp)
//                         if ( tex->mHeight == 0 ) {
//                             const ImageLoader2D image{ reinterpret_cast<Byte *>( tex->pcData ), tex->mWidth };
//
//                             loadInfo.WithWidth( image.GetWidth() )
//                                     .WithHeight( image.GetHeight() )
//                                     .WithChannelCount( image.GetChannels() )
//                                     .WithData( image.GetData() )
//
//                                     .WithName( path.string() )
//                                     .WithType( InferMikotoTextureType( type ) )
//                                     .WithMapType( InferMapType( type ) )
//                                     .WithType( TextureType::TEXTURE_2D )
//                                     .WithFormat( TextureFormat::RGBA8_UNORM )
//                                     .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );
//
//                             // Because StbImage is RAII and will free its data when exiting this scope we create the texture here
//                             texture = AssetsService::Get()->LoadAsset<Texture>( loadInfo );
//                         } else {
//                             loadInfo.WithWidth( tex->mWidth )
//                                     .WithHeight( tex->mHeight )
//                                     .WithChannelCount( 4 )// Assimp always provides the texture with 4 channels
//                                     .WithData( reinterpret_cast<unsigned char *>( tex->pcData ) )
//
//                                 .WithName( path.string() )
//                                     .WithType( InferMikotoTextureType( type ) )
//                                     .WithMapType( InferMapType( type ) )
//                                     .WithType( TextureType::TEXTURE_2D )
//                                     .WithFormat( TextureFormat::RGBA8_UNORM )
//                                     .WithResourceType( ResourceUsageType::RESOURCE_USAGE_STATIC );
//
//                             texture = AssetsService::Get()->LoadAsset<Texture>( loadInfo );
//                         }
//
//                         textureUri = path.string();
//
//                     } catch ( std::exception &e ) {
//                         MKT_CORE_LOGGER_ERROR( "LoadTexture - Failed to load embedded texture. Reason: {}", e.what() );
//                     }
//                 }
//
//                 if (!texture.IsEmpty()) {
//                     texture->SetTextureUri( textureUri );
//                 }
//             }
//
//         }
//
//         return texture;
//     }
//
//     static auto LoadTextures( const std::string &modelRootPath, const aiMesh *mesh, const aiScene *scene, MaterialProperties& properties ) -> void {
//
//         if ( static_cast<Int32>( mesh->mMaterialIndex ) > -1) {
//             const aiMaterial *material{ scene->mMaterials[mesh->mMaterialIndex] };
//             for (const aiTextureType &type: ASSIMP_TEXTURE_TYPES) {
//                 TextureHandle handle{ LoadTexture( modelRootPath, material, type, scene ) };
//
//                 if (!handle.IsEmpty()) {
//                     properties.TexturesByUri[handle->GetTextureUri()] = handle;
//                 }
//             }
//         }
//     }
//
//     static auto LoadMaterial( aiMaterial const *material, MaterialProperties &properties ) -> void {
//         aiString name{};
//         if ( material->Get( AI_MATKEY_NAME, name ) == AI_SUCCESS ) {
//             properties.Name = name.C_Str();
//         }
//
//         aiColor4D baseColor{};
//         if ( material->Get( AI_MATKEY_BASE_COLOR, baseColor ) == AI_SUCCESS ) {
//             properties.BaseColorFactor = Vec4F( baseColor.r, baseColor.g, baseColor.b, baseColor.a );
//         }
//
//         material->Get( AI_MATKEY_METALLIC_FACTOR, properties.MetallicFactor );
//         material->Get( AI_MATKEY_ROUGHNESS_FACTOR, properties.RoughnessFactor );
//
//         aiColor3D emissive{};
//         if ( material->Get( AI_MATKEY_COLOR_EMISSIVE, emissive ) == AI_SUCCESS ) {
//             properties.EmissiveFactor = Vec4F( emissive.r, emissive.g, emissive.b, 1.0f );
//         }
//
//         material->Get( AI_MATKEY_EMISSIVE_INTENSITY, properties.EmissiveStrength );
//
//         aiString alphaMode{};
//         if ( material->Get( AI_MATKEY_GLTF_ALPHAMODE, alphaMode ) == AI_SUCCESS ) {
//             properties.AlphaMask = ParseAlphaMode(alphaMode.C_Str());
//         }
//
//         material->Get( AI_MATKEY_GLTF_ALPHACUTOFF, properties.AlphaMaskCutoff );
//
//         // Just load the texture set these textures are using, actual texture load logic is separated
//         MKT_ASSIMP_LOAD_UV_SET(properties.BaseColorTextureSet, material, aiTextureType_BASE_COLOR);
//         MKT_ASSIMP_LOAD_UV_SET(properties.MetallicRoughnessTextureSet, material, aiTextureType_METALNESS);
//         MKT_ASSIMP_LOAD_UV_SET(properties.NormalTextureSet, material, aiTextureType_NORMALS);
//         MKT_ASSIMP_LOAD_UV_SET(properties.OcclusionTextureSet, material, aiTextureType_LIGHTMAP);
//
//         float glossiness{ 0.0f };
//         if ( material->Get( AI_MATKEY_GLOSSINESS_FACTOR, glossiness ) == AI_SUCCESS ) {
//             properties.GlossinessFactor = glossiness;
//         }
//
//         // Workflow
// #if !defined( NDEBUG )
//         MKT_COLOR_STYLE_PRINT_FORMATTED_FLUSH(
//                 MKT_FMT_COLOR_CYAN, MKT_FMT_STYLE_BOLD,
//                 "\n========= MATERIAL DEBUG =========\n" );
//
//         MKT_COLOR_PRINT_FORMATTED_FLUSH(
//                 MKT_FMT_COLOR_LIGHT_SKY_BLUE,
//                 "Name: {}\n",
//                 material->GetName().C_Str() );
//
//         MKT_COLOR_PRINT_FORMATTED_FLUSH(
//                 MKT_FMT_COLOR_LIGHT_SKY_BLUE,
//                 "Property Count: {}\n",
//                 material->mNumProperties );
//
//         for ( UInt32 i{ 0 }; i < material->mNumProperties; ++i ) {
//             aiMaterialProperty *prop{ material->mProperties[i] };
//
//             MKT_COLOR_PRINT_FORMATTED_FLUSH(
//                     MKT_FMT_COLOR_DARK_GRAY,
//                     "-----------------------------------\n" );
//
//             MKT_COLOR_STYLE_PRINT_FORMATTED_FLUSH(
//                     MKT_FMT_COLOR_YELLOW, MKT_FMT_STYLE_BOLD,
//                     "Key: {}\n",
//                     prop->mKey.C_Str() );
//
//             MKT_COLOR_PRINT_FORMATTED_FLUSH(
//                     MKT_FMT_COLOR_WHITE,
//                     "Semantic: {}\nIndex: {}\nDataLength: {}\nType: {}\n",
//                     prop->mSemantic,
//                     prop->mIndex,
//                     prop->mDataLength,
//                     static_cast<Int32>(prop->mType) );
//
//             if ( prop->mType == aiPTI_Float && prop->mDataLength >= sizeof( float ) ) {
//                 Int32 count{ static_cast<Int32>( prop->mDataLength / sizeof( float ) ) };
//                 float *data{ reinterpret_cast<float *>( prop->mData ) };
//
//                 std::string out{};
//                 out.reserve( 128 );
//
//                 for ( Int32 j{ 0 }; j < count; ++j ) {
//                     out += StringUtil::Format( "{} ", data[j] );
//                 }
//
//                 MKT_COLOR_PRINT_FORMATTED_FLUSH(
//                         MKT_FMT_COLOR_GREEN_YELLOW,
//                         "Float values: {}\n",
//                         out );
//             }
//
//             if ( prop->mType == aiPTI_Integer && prop->mDataLength >= sizeof( Int32 ) ) {
//                 Int32 count{ static_cast<Int32>( prop->mDataLength / sizeof( Int32 ) ) };
//                 Int32 *data{ reinterpret_cast<Int32 *>( prop->mData ) };
//
//                 std::string out{};
//                 out.reserve( 128 );
//
//                 for ( Int32 j{ 0 }; j < count; ++j ) {
//                     out += StringUtil::Format( "{} ", data[j] );
//                 }
//
//                 MKT_COLOR_PRINT_FORMATTED_FLUSH(
//                         MKT_FMT_COLOR_LIGHT_GREEN,
//                         "Int values: {}\n",
//                         out );
//             }
//
//             if ( prop->mType == aiPTI_String ) {
//                 aiString str{};
//                 if ( material->Get( prop->mKey.C_Str(), prop->mSemantic, prop->mIndex, str ) == AI_SUCCESS ) {
//                     MKT_COLOR_PRINT_FORMATTED_FLUSH(
//                             MKT_FMT_COLOR_CORNFLOWER_BLUE,
//                             "String: {}\n",
//                             str.C_Str() );
//                 }
//             }
//         }
//
//         MKT_COLOR_STYLE_PRINT_FORMATTED_FLUSH(
//                 MKT_FMT_COLOR_CYAN, MKT_FMT_STYLE_BOLD,
//                 "===================================\n" );
//
// #endif
//     }
//
//     static auto ConstructMeshNode( const std::string &rootPath, const aiMesh *mesh, const aiScene *scene, MeshNodeData& meshNodeData, MaterialProperties& material ) -> void {
//         meshNodeData.Name = mesh->mName.C_Str();
//
//         LoadVertices( mesh, meshNodeData );
//         LoadIndices( mesh, meshNodeData );
//
//         LoadMaterial( scene->mMaterials[mesh->mMaterialIndex], material );
//         LoadTextures( rootPath, mesh, scene, material );
//     }
//
//     static auto LoadHierarchyTransformation( const aiNode *src, Skeleton &skeleton) -> void {
//         Joint* parentJoint{ skeleton.FindJoint( src->mName.data ) };
//         if (parentJoint) {
//             //parentJoint->SetParentRelativeTransform( ToMat4F( src->mTransformation ) );
//         }
//
//         for ( UInt32 i{}; i < src->mNumChildren; i++ ) {
//             if ( Joint * childJoint{ skeleton.FindJoint( src->mChildren[i]->mName.data ) }; parentJoint && childJoint) {
//                 childJoint->SetParentID( parentJoint->GetID() );
//             }
//
//             LoadHierarchyTransformation( src->mChildren[i], skeleton );
//         }
//     }
//
//     static auto LoadMeshWeights(const aiNode *node, const aiScene *scene, ModelData& modelData) -> void {
//         for (UInt64 i{}; i < node->mNumMeshes; ++i) {
//             if (scene->mMeshes[node->mMeshes[i]]->HasBones()) {
//                 //LoadBoneWeights(scene->mMeshes[node->mMeshes[i]], modelData.MeshNodes[i], modelData.SceneSkeleton);
//             }
//         }
//     }
//
//     static auto LoadModelMeshes( const std::string &rootPath,
//         const aiNode *node, const aiScene *scene,
//         const ModelLoadDescription &loadInfo,
//         ModelData& modelData ) -> void
//     {
//         for (UInt64 i{}; i < node->mNumMeshes; ++i) {
//             auto& newMesh{ modelData.MeshNodes.emplace_back() };
//             auto& material{ modelData.Materials.emplace_back() };
//
//             const aiMesh *meshNode{ scene->mMeshes[node->mMeshes[i]] };
//
//             // Compute material index (since we inserted back, size increased by one last element is size() - 1)
//             const UInt32 index{ static_cast<UInt32>( modelData.Materials.size() ) - 1 };
//
//             newMesh.MaterialIndex = index;
//             ConstructMeshNode( rootPath, meshNode, scene, newMesh, material );
//
//             if ( meshNode->HasBones() ) {
//                 //LoadBoneWeights( meshNode, newMesh, modelData.SceneSkeleton );
//             }
//         }
//
//         // Recurse children
//         for (UInt64 i{}; i < node->mNumChildren; ++i) {
//             LoadModelMeshes( rootPath, node->mChildren[i], scene, loadInfo, modelData );
//         }
//     }
//
//     MainImporter::MainImporter( GpuDevice *device )
//     : ModelImporter{ device } {
//         // Allocate max concurrent importers
//         for ( Int32 count{}; count < ThreadUtils::InferConcurrentThreads(); ++count ) {
//             m_Importers.emplace_back( CreateScope<ImporterInfo>(  ) );
//             m_Importers.back()->Index = count;
//         }
//
//         // Prepare importers
//         for (const auto &importerInfo: m_Importers) {
//             importerInfo->CustomFileHandlingImpl = nullptr;
//             importerInfo->MeshImporter.SetIOHandler( importerInfo->CustomFileHandlingImpl.get() );
//         }
//
//         // Custom Logging
//         m_LogImpl = CreateScope<CustomLogStream>();
//         Assimp::DefaultLogger::create( "", Assimp::Logger::VERBOSE );
//         Assimp::DefaultLogger::get()->attachStream( m_LogImpl.get(), Assimp::Logger::VERBOSE );
//     }
//
//     auto MainImporter::Import( const ModelLoadDescription &description, ModelData& out ) -> void {
//         auto iter{ m_Importers.end() };
//         do {
//             iter = TryAcquireImporter();
//         } while ( iter == m_Importers.end() );
//
//         MKT_CORE_LOGGER_DEBUG( "Using Assimp importer at index: {}", ( *iter )->Index );
//
//         Import( *( *iter ), description, out );
//         ( *iter )->MeshImporter.FreeScene();
//         ( *iter )->IsFree.store( true, std::memory_order_release );
//     }
//
//     auto MainImporter::TryAcquireImporter() -> std::vector<Unique<ImporterInfo>>::iterator {
//         return std::ranges::find_if( m_Importers, []( const auto& importer ) -> bool {
//             bool expected{ true };
//             if ( importer->IsFree.compare_exchange_strong( expected, false, std::memory_order_acquire ) ) {
//                 return true;
//             }
//
//             return false;
//         } );
//     }
//
//     static auto PrepareJointHierarchy(const aiScene* scene, ModelData& modelData) -> void {
//         // The ID will be the bone count value this is important as when we upload the data loater
//         // in the shaders bone with ID = 0 goes to FinalMatrices[0]
//         /*Int32 boneID{ 0 };
//         Skeleton &skeleton{ modelData.SceneSkeleton };
//         for ( UInt32 meshIndex{}; meshIndex < scene->mNumMeshes; ++meshIndex ) {
//             const aiMesh *mesh{ scene->mMeshes[meshIndex] };
//
//             if (!mesh->HasBones()) {
//                 continue;
//             }
//
//             for ( Int32 boneIndex{}; boneIndex < mesh->mNumBones; ++boneIndex ) {
//                 const aiBone *bone{ mesh->mBones[boneIndex] };
//                 std::string boneName{ bone->mName.C_Str() };
//
//                 if ( !skeleton.HasJoint( boneName ) ) {
//                     skeleton.RegisterJoint( boneName, boneID++ );
//                 }
//             }
//         }
//
//         Node hierarchy{};
//         BuildHierarchy( scene->mRootNode, hierarchy, skeleton );*/
//
//         //skeleton.SetHierarchy( std::move( hierarchy ) );
//     }
//
//     auto MainImporter::Import( ImporterInfo &loaderData, const ModelLoadDescription &description, ModelData& modelData ) -> void {
//         auto importerFlags{ static_cast<aiPostProcessSteps>( aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_GenUVCoords | aiProcess_TransformUVCoords /*| aiProcess_FlipUVs*/ ) };
//
//         const File *file{ description.ModelFile };
//         const std::string absolutePath{ file->GetPath() };
//         const std::string fileName{ Path{ absolutePath }.stem().string() };
//
//         const aiScene *scene{ loaderData.MeshImporter.ReadFile( absolutePath.c_str(), importerFlags ) };
//
//         if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr) {
//             MKT_CORE_LOGGER_ERROR( "MainImporter::Import - Model load failed. Assimp error: '{}'", loaderData.MeshImporter.GetErrorString() );
//
//             return;
//         }
//
//         modelData.Name = scene->mName.C_Str();
//
//         PrepareJointHierarchy( scene, modelData );
//
//         LoadModelMeshes( filesystem::StripFileName( file->GetPath() ), scene->mRootNode, scene, description, modelData );
//     }
// }
