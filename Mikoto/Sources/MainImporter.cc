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

#include <EASTL/atomic.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>

#include <assimp/scene.h>
#include <assimp/GltfMaterial.h>
#include <assimp/postprocess.h>
#include <assimp/DefaultLogger.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Logging/Logger.hh>

#include <Math/Math.hh>

#include <Assets/Model.hh>
#include <Assets/MainImporter.hh>
#include <Assets/AssetsService.hh>
#include <Assets/ImageProcessor.hh>

#include <Material/PhysicalMaterial.hh>

#include <Filesystem/FileSystem.hh>
#include <Filesystem/FileService.hh>

#include <Threading/ThreadUtility.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Core/RenderSystem.hh>

// TODO: rework this importer, GLTF is the default one, for any other file type we default to Assimp

namespace mikoto::asset {

    using namespace mikoto::core;
    using namespace mikoto::animation;
    using namespace mikoto::material;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

#define MKT_ASSIMP_LOAD_UV_SET( PROPERTIES_FIELD, MATERIAL_PTR, TEXTURE_TYPE )                        \
    do {                                                                                              \
        aiString __tmpPath{};                                                                         \
        if ( ( MATERIAL_PTR )->GetTextureCount( TEXTURE_TYPE ) > 0 ) {                                \
            if ( ( MATERIAL_PTR )->GetTexture( TEXTURE_TYPE, 0, &__tmpPath ) == AI_SUCCESS ) {        \
                ( PROPERTIES_FIELD ) = 0;                                                             \
            } else if ( ( MATERIAL_PTR )->GetTexture( TEXTURE_TYPE, 1, &__tmpPath ) == AI_SUCCESS ) { \
                ( PROPERTIES_FIELD ) = 1;                                                             \
            }                                                                                         \
        }                                                                                             \
    } while ( 0 )

    class CustomLogStream final : public Assimp::LogStream {
    public:
        auto write( const char *message ) -> void override {
            MKT_CORE_LOGGER_TRACE( "[Assimp] {}", message );
        }
    };

    MKT_NODISCARD static auto ParseAlphaMode(eastl::string_view mode) -> AlphaMode {
        if (mode == "MASK" || mode == "Mask" || mode == "mask") {
            return AlphaMode::eMask;
        }

        if (mode == "BLEND" || mode == "Blend" || mode == "blend") {
            return AlphaMode::eBlend;
        }

        return AlphaMode::eOpaque;
    }

    static auto GetFloat4x4( const aiMatrix4x4 &from ) -> float4x4 {
        // GLM is column major, Assimp is row major

        float4x4 to{};
        // The a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
        // We are basically doing a transpose operation

        to[0][0] = from.a1;
        to[1][0] = from.a2;
        to[2][0] = from.a3;
        to[3][0] = from.a4;
        to[0][1] = from.b1;
        to[1][1] = from.b2;
        to[2][1] = from.b3;
        to[3][1] = from.b4;
        to[0][2] = from.c1;
        to[1][2] = from.c2;
        to[2][2] = from.c3;
        to[3][2] = from.c4;
        to[0][3] = from.d1;
        to[1][3] = from.d2;
        to[2][3] = from.d3;
        to[3][3] = from.d4;

        return to;
    }

    static constexpr eastl::array kAssimpTextureTypes{
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

    MKT_NODISCARD constexpr auto GetMapType( aiTextureType assimpType ) noexcept -> MapType {
        switch (assimpType) {
            case aiTextureType_DIFFUSE:
                return MapType::eDiffuse;

            case aiTextureType_AMBIENT:
            case aiTextureType_BASE_COLOR:
                return MapType::eBaseColor;

            case aiTextureType_NORMALS:
            case aiTextureType_NORMAL_CAMERA:
                return MapType::eNormal;

            case aiTextureType_METALNESS:
                return MapType::eMetallic;

            case aiTextureType_MAYA_SPECULAR_ROUGHNESS:
            case aiTextureType_DIFFUSE_ROUGHNESS:
                return MapType::eRoughness;

            case aiTextureType_AMBIENT_OCCLUSION:
                return MapType::eAmbientOcclusion;

            case aiTextureType_EMISSIVE:
            case aiTextureType_EMISSION_COLOR:
                return MapType::eEmissive;

            case aiTextureType_SPECULAR:
            case aiTextureType_SHININESS:
                return MapType::eSpecularGlossiness;

            case aiTextureType_HEIGHT:
            case aiTextureType_OPACITY:
            case aiTextureType_DISPLACEMENT:
            case aiTextureType_LIGHTMAP:
            case aiTextureType_REFLECTION:
            case aiTextureType_UNKNOWN:
            case aiTextureType_SHEEN:
            case aiTextureType_CLEARCOAT:
            case aiTextureType_TRANSMISSION:
            case aiTextureType_MAYA_BASE:
            case aiTextureType_MAYA_SPECULAR:
            case aiTextureType_MAYA_SPECULAR_COLOR:
            default:
                return MapType::eInvalid;
        }
    }


#if false

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
            case aiTextureType_AMBIENT: // Not sure if this one should go here, Mikoto does not contemplate an ambient map yet
            case aiTextureType_BASE_COLOR:
                return MapType::BASE_COLOR_TEXTURE;

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

    static auto LoadBoneWeights( const aiMesh *mesh, MeshNodeData &meshNodeData, Skeleton& skeleton ) -> void {
        MKT_COLOR_PRINT_FORMATTED( MKT_FMT_COLOR_AQUA, "Mesh {} has [{}] bones\n", mesh->mName.C_Str(), mesh->mNumBones );

        for ( Int32 boneIndex{}; boneIndex < mesh->mNumBones; ++boneIndex ) {
            std::string boneName{ mesh->mBones[boneIndex]->mName.C_Str() };

            // This should not happen because we first load the skeleton, and then we load the meshes
            MKT_ASSERT( skeleton.HasJoint( boneName ), StringUtil::Format( "Bone {} not found in skeleton", boneName ) );

            Joint* joint{ skeleton.FindJoint( boneName ) };

            aiVertexWeight *weights{ mesh->mBones[boneIndex]->mWeights };
            UInt32 numWeights{ mesh->mBones[boneIndex]->mNumWeights };

            for ( UInt32 weightIndex{}; weightIndex < numWeights; ++weightIndex ) {
                float weight{ weights[weightIndex].mWeight };
                UInt32 vertexId{ weights[weightIndex].mVertexId };

                skeleton.SetWeights( mesh->mName.C_Str(), boneName, vertexId, weight );

                MKT_ASSERT( vertexId < meshNodeData.Vertices.size(), "vertexID out of bounds for vertices count" );

                VertexData &vertex{ meshNodeData.Vertices[vertexId] };

                // We allow only 4 bones to affect a vertex at max
                // We set the first 4 bones that have influence on the vertex
                // We might later want to pick the highest contributions
                for ( Size i{}; i < MAX_BONE_INFLUENCE; ++i ) {
                    if ( vertex.Weights[i] == 0 ) {
                        vertex.Joints[i] = joint->GetID();
                        vertex.Weights[i] = weight;
                        break;
                    }
                }
            }
        }
    }

    static auto GetAnimationProperties( const aiAnimation *animation, ModelData& modelData ) -> void {
        //Skeleton& skeleton{ modelData.SceneSkeleton };
        //const UInt32 size{ ( animation->mNumChannels ) };

        //std::vector<AnimationSampler> samplers{};
        //std::vector<AnimationChannel> channels{};

        //for ( UInt32 i{}; i < size; i++ ) {
        //    aiNodeAnim* channel{ animation->mChannels[i] };
        //    std::string jointName{ channel->mNodeName.data };

        //    AnimationSampler animationSampler{};
        //    AnimationChannel animationChannel{};

        //    Joint* joint{ skeleton.FindJoint( jointName ) };
        //    if (joint) {
        //        // TODO: fill structures properly
        //        samplers.emplace_back( animationSampler );
        //        channels.emplace_back( animationChannel );
        //    }
        //}
    }

        // Workflow
#if !defined( NDEBUG )
        MKT_COLOR_STYLE_PRINT_FORMATTED_FLUSH(
                MKT_FMT_COLOR_CYAN, MKT_FMT_STYLE_BOLD,
                "\n========= MATERIAL DEBUG =========\n" );

        MKT_COLOR_PRINT_FORMATTED_FLUSH(
                MKT_FMT_COLOR_LIGHT_SKY_BLUE,
                "Name: {}\n",
                material->GetName().C_Str() );

        MKT_COLOR_PRINT_FORMATTED_FLUSH(
                MKT_FMT_COLOR_LIGHT_SKY_BLUE,
                "Property Count: {}\n",
                material->mNumProperties );

        for ( UInt32 i{ 0 }; i < material->mNumProperties; ++i ) {
            aiMaterialProperty *prop{ material->mProperties[i] };

            MKT_COLOR_PRINT_FORMATTED_FLUSH(
                    MKT_FMT_COLOR_DARK_GRAY,
                    "-----------------------------------\n" );

            MKT_COLOR_STYLE_PRINT_FORMATTED_FLUSH(
                    MKT_FMT_COLOR_YELLOW, MKT_FMT_STYLE_BOLD,
                    "Key: {}\n",
                    prop->mKey.C_Str() );

            MKT_COLOR_PRINT_FORMATTED_FLUSH(
                    MKT_FMT_COLOR_WHITE,
                    "Semantic: {}\nIndex: {}\nDataLength: {}\nType: {}\n",
                    prop->mSemantic,
                    prop->mIndex,
                    prop->mDataLength,
                    static_cast<Int32>(prop->mType) );

            if ( prop->mType == aiPTI_Float && prop->mDataLength >= sizeof( float ) ) {
                Int32 count{ static_cast<Int32>( prop->mDataLength / sizeof( float ) ) };
                float *data{ reinterpret_cast<float *>( prop->mData ) };

                std::string out{};
                out.reserve( 128 );

                for ( Int32 j{ 0 }; j < count; ++j ) {
                    out += StringUtil::Format( "{} ", data[j] );
                }

                MKT_COLOR_PRINT_FORMATTED_FLUSH(
                        MKT_FMT_COLOR_GREEN_YELLOW,
                        "Float values: {}\n",
                        out );
            }

            if ( prop->mType == aiPTI_Integer && prop->mDataLength >= sizeof( Int32 ) ) {
                Int32 count{ static_cast<Int32>( prop->mDataLength / sizeof( Int32 ) ) };
                Int32 *data{ reinterpret_cast<Int32 *>( prop->mData ) };

                std::string out{};
                out.reserve( 128 );

                for ( Int32 j{ 0 }; j < count; ++j ) {
                    out += StringUtil::Format( "{} ", data[j] );
                }

                MKT_COLOR_PRINT_FORMATTED_FLUSH(
                        MKT_FMT_COLOR_LIGHT_GREEN,
                        "Int values: {}\n",
                        out );
            }

            if ( prop->mType == aiPTI_String ) {
                aiString str{};
                if ( material->Get( prop->mKey.C_Str(), prop->mSemantic, prop->mIndex, str ) == AI_SUCCESS ) {
                    MKT_COLOR_PRINT_FORMATTED_FLUSH(
                            MKT_FMT_COLOR_CORNFLOWER_BLUE,
                            "String: {}\n",
                            str.C_Str() );
                }
            }
        }

        MKT_COLOR_STYLE_PRINT_FORMATTED_FLUSH(
                MKT_FMT_COLOR_CYAN, MKT_FMT_STYLE_BOLD,
                "===================================\n" );

#endif
    }

    static auto LoadHierarchyTransformation( const aiNode *src, Skeleton &skeleton) -> void {
        Joint* parentJoint{ skeleton.FindJoint( src->mName.data ) };
        if (parentJoint) {
            //parentJoint->SetParentRelativeTransform( ToMat4F( src->mTransformation ) );
        }

        for ( UInt32 i{}; i < src->mNumChildren; i++ ) {
            if ( Joint * childJoint{ skeleton.FindJoint( src->mChildren[i]->mName.data ) }; parentJoint && childJoint) {
                childJoint->SetParentID( parentJoint->GetID() );
            }

            LoadHierarchyTransformation( src->mChildren[i], skeleton );
        }
    }

    static auto LoadMeshWeights(const aiNode *node, const aiScene *scene, ModelData& modelData) -> void {
        for (UInt64 i{}; i < node->mNumMeshes; ++i) {
            if (scene->mMeshes[node->mMeshes[i]]->HasBones()) {
                //LoadBoneWeights(scene->mMeshes[node->mMeshes[i]], modelData.MeshNodes[i], modelData.SceneSkeleton);
            }
        }
    }

    static auto PrepareJointHierarchy(const aiScene* scene, ModelData& modelData) -> void {
        // The ID will be the bone count value this is important as when we upload the data loater
        // in the shaders bone with ID = 0 goes to FinalMatrices[0]
        /*Int32 boneID{ 0 };
        Skeleton &skeleton{ modelData.SceneSkeleton };
        for ( UInt32 meshIndex{}; meshIndex < scene->mNumMeshes; ++meshIndex ) {
            const aiMesh *mesh{ scene->mMeshes[meshIndex] };

            if (!mesh->HasBones()) {
                continue;
            }

            for ( Int32 boneIndex{}; boneIndex < mesh->mNumBones; ++boneIndex ) {
                const aiBone *bone{ mesh->mBones[boneIndex] };
                std::string boneName{ bone->mName.C_Str() };

                if ( !skeleton.HasJoint( boneName ) ) {
                    skeleton.RegisterJoint( boneName, boneID++ );
                }
            }
        }

        Node hierarchy{};
        BuildHierarchy( scene->mRootNode, hierarchy, skeleton );*/

        //skeleton.SetHierarchy( std::move( hierarchy ) );
    }
#endif

    auto MainImporter::LoadTexture( const Path& modelRootPath, const aiMaterial *material, const aiTextureType type, const aiScene *scene ) -> LoadTextureDescription {
        LoadTextureDescription result{};

        for (usize index{}; index < material->GetTextureCount( type ); index++) {
            aiString assimpTexturePath{};

            if (material->GetTexture( type, index, std::addressof( assimpTexturePath ) ) == AI_SUCCESS) {
                // Assumes the textures are in the same directory as the model files
                auto path{ PathBuilder{}
                    .SetPath( modelRootPath )
                    .SetPath( assimpTexturePath.C_Str() )
                    .Build() };

                if (!string::Contains(path.GetC_Str(), "*")) {
                    // FIXME:
                    // Some models use TIF extension, if they offer a PNG variant use PNG instead
                    // as we do not have a TIF decoder for the time being
                    if (filesystem::InferFileTypeFromExtension( path.GetExtension() ) == FileType::eTiff ) {
                        break;
                    }

                    try {
                        result.mTexture = AssetsService::Get()->LoadAsset<ITexture>( path, TextureDimension::eTexture2D );
                        result.mPath = path;
                    } catch ( std::exception &e ) {
                        MKT_CORE_LOGGER_ERROR( "Failed to load texture. Reason: {}", e.what() );
                    }
                } else {
                    // Texture is embedded
                    try {
                        // Handle Assimp image storage
                        const i32 embeddedIndex{ std::atoi( assimpTexturePath.C_Str() + 1 ) };
                        const aiTexture* tex{ scene->mTextures[embeddedIndex] };

                        // If mHeight value is zero, pcData points to a
                        // compressed texture in any format (e.g. JPEG). (see texture.h in Assimp)
                        if ( tex->mHeight == 0 ) {
                            asset::ImageHandle image{ asset::ProcessImage2D( tex->pcData, tex->mWidth, ImageFormat::eRGBA8_UINT ) };
                            auto textureDesc{ TextureCreateDescription{}
                                .SetWidth( image->mWidth )
                                .SetHeight( image->mHeight )
                                .SetDimensions( TextureDimension::eTexture2D )
                                .SetMultisampling( Multisampling::eMsaaX1 )
                                .SetUsage( TextureUsageFlagsBits::kCopyDst | TextureUsageFlagsBits::kShaderResource )
                                .SetImageData( image )
                                .SetFormat( Format::eRGBA8_UNORM ) };

                            result.mTexture = mDevice->CreateTexture( textureDesc );
                            result.mPath = path;
                        } else {
                            constexpr u32 kChannelCount{ 4 };
                            auto textureDesc{ TextureCreateDescription{}
                                .SetWidth( tex->mWidth )
                                .SetHeight( tex->mHeight )
                                .SetDimensions( TextureDimension::eTexture2D )
                                .SetMultisampling( Multisampling::eMsaaX1 )
                                .SetUsage( TextureUsageFlagsBits::kCopyDst | TextureUsageFlagsBits::kShaderResource )
                                .SetBufferData( BufferSpanHandle::New( rc_cast<ubyte*>( tex->pcData ),
                                    as<usize>( tex->mWidth * tex->mHeight * kChannelCount ) ) )
                                .SetFormat( Format::eRGBA8_UNORM ) };

                            result.mTexture = mDevice->CreateTexture( textureDesc );
                            result.mPath = path;
                        }
                    } catch ( std::exception &e ) {
                        MKT_CORE_LOGGER_ERROR( "LoadTexture - Failed to load embedded texture. Reason: {}", e.what() );
                    }
                }
            }
        }

        return result;
    }

    auto MainImporter::LoadTextures(
        const Path& modelRootPath,
        const aiMesh *mesh,
        const aiScene *scene,
        PhysicMaterialDescription& properties ) -> void
    {
        constexpr i32 kErrorResult{ -1 };
        if ( as<i32>( mesh->mMaterialIndex ) > kErrorResult ) {
            const aiMaterial *material{ scene->mMaterials[mesh->mMaterialIndex] };
            for ( const aiTextureType &type: kAssimpTextureTypes ) {
                LoadTextureDescription result{ LoadTexture( modelRootPath, material, type, scene ) };

                if ( !result.mTexture.IsEmpty() ) {
                    properties.mTexturesByUri[result.mPath] = PBRMap{ result.mTexture, GetMapType(type) };
                }
            }
        }
    }

    auto MainImporter::LoadMaterial( aiMaterial const *material,
        PhysicMaterialDescription &properties ) -> void
    {
        aiString name{};
        if ( material->Get( AI_MATKEY_NAME, name ) == AI_SUCCESS ) {
            properties.mName = name.C_Str();
        }

        aiColor4D baseColor{};
        if ( material->Get( AI_MATKEY_BASE_COLOR, baseColor ) == AI_SUCCESS ) {
            properties.BaseColorFactor = float4{ baseColor.r, baseColor.g, baseColor.b, baseColor.a };
        }

        material->Get( AI_MATKEY_METALLIC_FACTOR, properties.MetallicFactor );
        material->Get( AI_MATKEY_ROUGHNESS_FACTOR, properties.RoughnessFactor );

        aiColor3D emissive{};
        if ( material->Get( AI_MATKEY_COLOR_EMISSIVE, emissive ) == AI_SUCCESS ) {
            properties.EmissiveFactor = float4( emissive.r, emissive.g, emissive.b, 1.0f );
        }

        material->Get( AI_MATKEY_EMISSIVE_INTENSITY, properties.EmissiveStrength );

        aiString alphaMode{};
        if ( material->Get( AI_MATKEY_GLTF_ALPHAMODE, alphaMode ) == AI_SUCCESS ) {
            properties.mAlphaMask = ParseAlphaMode( alphaMode.C_Str() );
        }

        material->Get( AI_MATKEY_GLTF_ALPHACUTOFF, properties.AlphaMaskCutoff );

        // Just load the texture set these textures are using, actual texture load logic is separated
        MKT_ASSIMP_LOAD_UV_SET( properties.BaseColorTextureSet, material, aiTextureType_BASE_COLOR );
        MKT_ASSIMP_LOAD_UV_SET( properties.MetallicRoughnessTextureSet, material, aiTextureType_METALNESS );
        MKT_ASSIMP_LOAD_UV_SET( properties.NormalTextureSet, material, aiTextureType_NORMALS );
        MKT_ASSIMP_LOAD_UV_SET( properties.OcclusionTextureSet, material, aiTextureType_LIGHTMAP );

        f32 glossiness{ 0.0f };
        if ( material->Get( AI_MATKEY_GLOSSINESS_FACTOR, glossiness ) == AI_SUCCESS ) {
            properties.GlossinessFactor = glossiness;
        }
    }

    auto MainImporter::LoadVertices(
        const aiMesh *mesh, MeshNodeDescription &meshNodeData ) -> void
    {
        meshNodeData.mVertices.resize( mesh->mNumVertices );
        for ( usize index{}; index < mesh->mNumVertices; index++ ) {
            auto &vertex{ meshNodeData.mVertices[index] };

            vertex.mPosition.x = mesh->mVertices[index].x;
            vertex.mPosition.y = mesh->mVertices[index].y;
            vertex.mPosition.z = mesh->mVertices[index].z;

            if ( mesh->HasNormals() ) {
                vertex.mNormals.x = mesh->mNormals[index].x;
                vertex.mNormals.y = mesh->mNormals[index].y;
                vertex.mNormals.z = mesh->mNormals[index].z;
            }

            if ( mesh->HasVertexColors( index ) ) {
                vertex.mColors.r = mesh->mColors[index]->r;
                vertex.mColors.g = mesh->mColors[index]->g;
                vertex.mColors.b = mesh->mColors[index]->b;
                vertex.mColors.a = mesh->mColors[index]->a;
            }

            if ( mesh->HasTextureCoords( 0 ) ) {
                vertex.mUv0.x = mesh->mTextureCoords[0][index].x;
                vertex.mUv0.y = mesh->mTextureCoords[0][index].y;
            }

            if ( mesh->HasTextureCoords( 1 ) ) {
                vertex.mUv1.x = mesh->mTextureCoords[1][index].x;
                vertex.mUv1.y = mesh->mTextureCoords[1][index].y;
            }
        }
    }

    auto MainImporter::LoadIndices( const aiMesh *mesh,
        MeshNodeDescription &meshNodeData ) -> void
    {
        for ( usize i{}; i < mesh->mNumFaces; i++ ) {
            const auto face{ mesh->mFaces[i] };

            for ( usize index{}; index < face.mNumIndices; index++ ) {
                meshNodeData.mIndices.emplace_back( face.mIndices[index] );
            }
        }
    }

    auto MainImporter::ConstructMeshNode( const Path& rootPath,
        const aiMesh *mesh, const aiScene *scene,
        MeshNodeDescription& meshNodeData,
        PhysicMaterialDescription& material ) -> void
    {
        meshNodeData.mName = mesh->mName.C_Str();

        LoadVertices( mesh, meshNodeData );
        LoadIndices( mesh, meshNodeData );

        LoadMaterial( scene->mMaterials[mesh->mMaterialIndex], material );
        LoadTextures( rootPath, mesh, scene, material );
    }

    auto MainImporter::LoadModelMeshes( const Path& rootPath,
        const aiNode *node, const aiScene *scene,
        const ModelLoadDescription& loadInfo,
        ModelDataDescription& modelData ) -> void
    {
        for (usize i{}; i < node->mNumMeshes; ++i) {
            auto& newMesh{ modelData.mMeshNodes.emplace_back() };
            auto& material{ modelData.mMaterials.emplace_back() };

            const aiMesh *meshNode{ scene->mMeshes[node->mMeshes[i]] };

            // Compute material index (since we inserted back, size increased by one last element is size() - 1)
            const u32 index{ as<u32>( modelData.mMaterials.size() ) - 1 };

            newMesh.MaterialIndex = index;
            ConstructMeshNode( rootPath, meshNode, scene, newMesh, material );

            if ( meshNode->HasBones() ) {
                //LoadBoneWeights( meshNode, newMesh, modelData.SceneSkeleton );
            }
        }

        // Recurse children
        for (usize i{}; i < node->mNumChildren; ++i) {
            LoadModelMeshes( rootPath, node->mChildren[i], scene, loadInfo, modelData );
        }
    }


    MainImporter::MainImporter( IGpuDevice *device )
        : ModelImporter{ device }
    {
        // Allocate max concurrent importers
        const usize concurrency{ threading::GetThreadConcurrency() };
        for ( i32 count{}; count < concurrency; ++count ) {
            mImporters.emplace_back( eastl::make_unique<ImporterInfo>() );
            mImporters.back()->mIndex = count;
        }

        // Prepare importers
        for (const auto &importerInfo: mImporters) {
            importerInfo->mCustomFileHandlingImpl = nullptr;
            importerInfo->mMeshImporter.SetIOHandler( importerInfo->mCustomFileHandlingImpl.get() );
        }

        // Custom Logging
        mLogImpl = eastl::make_unique<CustomLogStream>();
        Assimp::DefaultLogger::create( "DefaultLogger", Assimp::Logger::VERBOSE );
        Assimp::DefaultLogger::get()->attachStream( mLogImpl.get(), Assimp::Logger::VERBOSE );
    }

    auto MainImporter::Import( const ModelLoadDescription& description, ModelDataDescription& out ) -> void {
        auto iter{ mImporters.end() };
        do {
            iter = TryAcquireImporter();
        } while ( iter == mImporters.end() );

        MKT_CORE_LOGGER_DEBUG( "Using Assimp importer at index: {}", ( *iter )->mIndex );

        Import( *( *iter ), description, out );
        ( *iter )->mMeshImporter.FreeScene();
        ( *iter )->mIsFree.test_and_set( eastl::memory_order_release );
    }

    auto MainImporter::TryAcquireImporter() -> eastl::vector<eastl::unique_ptr<ImporterInfo>>::iterator {
        bool found{ false };
        auto it{ mImporters.begin() };

        while ( !found ) {
            if ( (*it)->mIsFree.test_and_set( eastl::memory_order_acquire ) ) {
                found = true;
            } else {
                ++it;
            }
        }

        return it;
    }


    auto MainImporter::Import( ImporterInfo &loaderData, const ModelLoadDescription &description, ModelDataDescription& modelData ) -> void {
        aiPostProcessSteps importerFlags{ as<aiPostProcessSteps>(
            aiProcess_Triangulate |
            aiProcess_GenSmoothNormals |
            aiProcess_GenUVCoords |
            aiProcess_TransformUVCoords |
            aiProcess_FlipUVs  ) };

        FileHandle file{ description.mFile };
        const Path path{ file->GetPath() };
        const aiScene *scene{ loaderData.mMeshImporter.ReadFile( path.GetAbsolute().c_str(), importerFlags ) };

        if (scene == nullptr || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || scene->mRootNode == nullptr) {
            MKT_CORE_LOGGER_ERROR( "Model load failed. Assimp error: '{}'", loaderData.mMeshImporter.GetErrorString() );
            return;
        }

        modelData.mName = scene->mName.C_Str();

        LoadModelMeshes( path.GetDirectory(), scene->mRootNode, scene, description, modelData );
    }

}
