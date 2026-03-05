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

#include <ranges>
#include <algorithm>

#include <Library/Math/Math.hh>

#include <tiny_gltf.h>

#include <Logging/Logger.hh>
#include <Common/String.hh>

#include <Assets/GltfImporter.hh>

#include <Filesystem/FileService.hh>
#include <Filesystem/FileSystem.hh>

#include <Material/PBRMaterial.hh>

#include <Threading/ThreadUtility.hh>

#include <Renderer/Core/RenderUtility.hh>

#include <Assets/AssetsService.hh>

namespace Mikoto {

    static auto ComponentSize( Int32 componentType ) -> Size {
        switch ( componentType ) {
            case TINYGLTF_COMPONENT_TYPE_FLOAT:
                return sizeof( float );
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                return sizeof( UInt32 );
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                return sizeof( UInt16 );
            case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
                return sizeof( UInt8 );
            default:
                return 0;
        }
    }

    static auto TypeCount( Int32 type ) -> Size {
        switch ( type ) {
            case TINYGLTF_TYPE_SCALAR:
                return 1;
            case TINYGLTF_TYPE_VEC2:
                return 2;
            case TINYGLTF_TYPE_VEC3:
                return 3;
            case TINYGLTF_TYPE_VEC4:
                return 4;
            default:
                return 1;
        }
    }

    static auto GetMikotoWrapMode( Int32 wrapMode ) -> SamplerWrapMode {
        switch ( wrapMode ) {
            case -1:
            case 10497:
                return SamplerWrapMode::WRAP_REPEAT;
            case 33071:
                return SamplerWrapMode::WRAP_CLAMP_TO_EDGE;
            case 33648:
                return SamplerWrapMode::MIRRORED_REPEAT;
        }

        return SamplerWrapMode::WRAP_REPEAT;
    }

    static auto GetMikotoFilterMode( Int32 filterMode ) -> SamplerFilter {
        switch ( filterMode ) {
            case -1:
            case 9728:
                return SamplerFilter::FILTER_NEAREST;
            case 9729:
                return SamplerFilter::FILTER_LINEAR;
            case 9984:
                return SamplerFilter::FILTER_NEAREST;
            case 9985:
                return SamplerFilter::FILTER_NEAREST;
            case 9986:
                return SamplerFilter::FILTER_LINEAR;
            case 9987:
                return SamplerFilter::FILTER_LINEAR;
        }

        return SamplerFilter::FILTER_NEAREST;
    }

    static auto ToMat4F( const tinygltf::Node& node ) -> Mat4F {
        glm::mat4 transform{ 1.0f };

        if ( !node.matrix.empty() ) {
            const double* m = node.matrix.data();

            transform = glm::mat4(
                    ( float )m[0], ( float )m[1], ( float )m[2], ( float )m[3],
                    ( float )m[4], ( float )m[5], ( float )m[6], ( float )m[7],
                    ( float )m[8], ( float )m[9], ( float )m[10], ( float )m[11],
                    ( float )m[12], ( float )m[13], ( float )m[14], ( float )m[15] );
        }

        return transform;
    }

    static Mat4F ComputeNodeTransform( const tinygltf::Node& node ) {
        Mat4F transform{ 1.0f };

        if ( !node.matrix.empty() ) {
            transform = ToMat4F( node );
        } else {
            Vec3F translation{ 0.0f };
            Vec3F scale{ 1.0f };
            Quat rotation{ 1, 0, 0, 0 };

            if ( !node.translation.empty() )
                translation = Vec3F(
                        node.translation[0],
                        node.translation[1],
                        node.translation[2] );

            if ( !node.scale.empty() )
                scale = Vec3F(
                        node.scale[0],
                        node.scale[1],
                        node.scale[2] );

            if ( !node.rotation.empty() )
                rotation = Quat(
                        node.rotation[3],
                        node.rotation[0],
                        node.rotation[1],
                        node.rotation[2] );

            transform =
                    glm::translate( Mat4F{ 1.0f }, translation ) *
                    glm::mat4_cast( rotation ) *
                    glm::scale( Mat4F{ 1.0f }, scale );
        }

        return transform;
    }

    static auto LoadNode( const tinygltf::Model& model, Skeleton& skeleton, Int32 nodeIndex ) -> Node {
        const tinygltf::Node& gltfNode = model.nodes[nodeIndex];

        Node node{};

        // Node name
        node.Name = gltfNode.name.empty()
                            ? std::to_string( nodeIndex )
                            : gltfNode.name;

        // Local transform
        node.Transformation = ComputeNodeTransform( gltfNode );

        
        // -------------------------
        // Extract TRS
        // -------------------------

        // Translation
        if ( !gltfNode.translation.empty() ) {
            node.Translation = Vec3F{
                static_cast<float>( gltfNode.translation[0] ),
                static_cast<float>( gltfNode.translation[1] ),
                static_cast<float>( gltfNode.translation[2] )
            };
        }

        // Scale
        if ( !gltfNode.scale.empty() ) {
            node.Scale = Vec3F{
                static_cast<float>( gltfNode.scale[0] ),
                static_cast<float>( gltfNode.scale[1] ),
                static_cast<float>( gltfNode.scale[2] )
            };
        }

        // Rotation (glTF stores quaternion as x,y,z,w)
        if ( !gltfNode.rotation.empty() ) {
            node.Rotation = Quat{
                static_cast<float>( gltfNode.rotation[3] ),// w
                static_cast<float>( gltfNode.rotation[0] ),// x
                static_cast<float>( gltfNode.rotation[1] ),// y
                static_cast<float>( gltfNode.rotation[2] ) // z
            };
        }

        // If TRS wasn't provided explicitly, extract it from the matrix
        if ( gltfNode.translation.empty() &&
             gltfNode.rotation.empty() &&
             gltfNode.scale.empty() ) {
            glm::vec3 scale{};
            glm::quat rotation{};
            glm::vec3 translation{};
            glm::vec3 skew{};
            glm::vec4 perspective{};

            glm::decompose( node.Transformation,
                            scale,
                            rotation,
                            translation,
                            skew,
                            perspective );

            node.Translation = translation;
            node.Rotation = rotation;
            node.Scale = scale;
        }

        // Check if this node corresponds to a joint
        if ( skeleton.HasJoint( node.Name ) ) {
            if ( auto* joint = skeleton.FindJoint( node.Name ) ) {
                node.JointID = joint->GetID();
            }
        }

        // Recursively process children
        for ( Int32 childIndex: gltfNode.children ) {
            auto& child{ node.Children.emplace_back( LoadNode( model, skeleton, childIndex ) ) };

            // Register parent ID
            auto childName = model.nodes[childIndex].name.empty()
                                     ? std::to_string( childIndex )
                                     : model.nodes[childIndex].name;

            auto childJoint{ skeleton.FindJointByID( childIndex ) };
            if ( childJoint ) {
                childJoint->SetParentID( nodeIndex );
            }
        }

        return node;
    }

    static auto ReadAccessorAsFloat(
            const tinygltf::Model& model,
            const tinygltf::Accessor& accessor ) -> std::vector<float> {
        const auto& view{ model.bufferViews[accessor.bufferView] };
        const auto& buffer{ model.buffers[view.buffer] };

        const auto compSize{ ComponentSize( accessor.componentType ) };
        const auto elemSize{ TypeCount( accessor.type ) };
        const auto stride{ accessor.ByteStride( view ) };

        const auto* dataPtr{ buffer.data.data() + view.byteOffset + accessor.byteOffset };

        std::vector<float> result{};
        result.resize( accessor.count * elemSize );

        for ( Size i{}; i < accessor.count; ++i ) {
            const auto* element = dataPtr + i * ( stride ? stride : compSize * elemSize );

            for ( Size c{}; c < elemSize; ++c ) {
                const auto* compPtr = element + c * compSize;
                float value{};

                switch ( accessor.componentType ) {
                    case TINYGLTF_COMPONENT_TYPE_FLOAT:
                        value = *reinterpret_cast<const float*>( compPtr );
                        break;

                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                        auto v = *reinterpret_cast<const UInt8*>( compPtr );
                        value = accessor.normalized ? v / 255.f : static_cast<float>( v );
                        break;
                    }

                    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                        auto v = *reinterpret_cast<const UInt16*>( compPtr );
                        value = accessor.normalized ? v / 65535.f : static_cast<float>( v );
                        break;
                    }

                    default:
                        value = 0.f;
                        break;
                }

                result[i * elemSize + c] = value;
            }
        }

        return result;
    }

    template<typename TVec>
    static auto LoadVertexAttribute(
            const tinygltf::Model& model,
            const tinygltf::Primitive& primitive,
            const std::string& attributeName,
            std::vector<VertexData>& vertices,
            TVec VertexData::* member,
            Size componentCount ) -> void {

        if ( !primitive.attributes.contains( attributeName ) ) {
            return;
        }

        const auto& accessor{ model.accessors[primitive.attributes.at( attributeName )] };
        auto data{ ReadAccessorAsFloat( model, accessor ) };

        const Size vertexCount{ vertices.size() };

        for ( Size i{}; i < vertexCount; ++i ) {
            TVec value{};

            if constexpr ( std::is_same_v<TVec, Vec2F> ) {
                value = {
                    data[i * componentCount + 0],
                    data[i * componentCount + 1]
                };
            } else if constexpr ( std::is_same_v<TVec, Vec3F> ) {
                value = {
                    data[i * componentCount + 0],
                    data[i * componentCount + 1],
                    data[i * componentCount + 2]
                };
            } else if constexpr ( std::is_same_v<TVec, Vec4F> ) {
                value = {
                    data[i * componentCount + 0],
                    data[i * componentCount + 1],
                    data[i * componentCount + 2],
                    data[i * componentCount + 3]
                };
            }

            vertices[i].*member = value;
        }
    }

    GLTFImporter::GLTFImporter( GpuDevice* device )
        : ModelImporter{ device } {
        for ( Int32 count{}; count < ThreadUtils::InferConcurrentThreads(); ++count ) {
            m_Importers.emplace_back( CreateScope<LoaderData>( count ) );
        }
    }

    auto GLTFImporter::Import( const ModelLoadDescription& description, ModelData& out ) -> void {
        auto iter{ m_Importers.end() };
        do {
            iter = TryAcquireImporter();
        } while ( iter == m_Importers.end() );

        MKT_CORE_LOGGER_DEBUG( "Using GLTF importer {}", ( *iter )->Index );

        Import( *( *iter ), description, out );
        ( *iter )->IsFree.store( true, std::memory_order_release );
    }

    auto GLTFImporter::LoadPrimitives( tinygltf::Model& model, ModelData& modelData ) -> void {
        for ( const auto& mesh: model.meshes ) {
            for ( const auto& primitive: mesh.primitives ) {
                MeshNodeData node{};
                node.Name = mesh.name;
                node.MaterialIndex = primitive.material;

                const auto& posAccessor{ model.accessors[primitive.attributes.at( "POSITION" )] };

                const Size vertexCount{ posAccessor.count };
                node.Vertices.resize( vertexCount );

                // POSITION (required)
                LoadVertexAttribute(
                        model,
                        primitive,
                        "POSITION",
                        node.Vertices,
                        &VertexData::Position,
                        3 );

                // NORMAL
                LoadVertexAttribute(
                        model,
                        primitive,
                        "NORMAL",
                        node.Vertices,
                        &VertexData::Normals,
                        3 );

                // COLOR_0 (can be VEC3 or VEC4)
                if ( primitive.attributes.contains( "COLOR_0" ) ) {
                    const auto& accessor =
                            model.accessors[primitive.attributes.at( "COLOR_0" )];

                    const auto compCount = TypeCount( accessor.type );

                    LoadVertexAttribute(
                            model,
                            primitive,
                            "COLOR_0",
                            node.Vertices,
                            &VertexData::Colors,
                            compCount == 4 ? 4 : 3 );
                }

                // TEXCOORD_0
                LoadVertexAttribute(
                        model,
                        primitive,
                        "TEXCOORD_0",
                        node.Vertices,
                        &VertexData::UV_0,
                        2 );

                // TEXCOORD_1
                LoadVertexAttribute(
                        model,
                        primitive,
                        "TEXCOORD_1",
                        node.Vertices,
                        &VertexData::UV_1,
                        2 );

                // JOINTS_0
                LoadVertexAttribute(
                        model,
                        primitive,
                        "JOINTS_0",
                        node.Vertices,
                        &VertexData::Joints,
                        4 );

                // WEIGHTS_0
                LoadVertexAttribute(
                        model,
                        primitive,
                        "WEIGHTS_0",
                        node.Vertices,
                        &VertexData::Weights,
                        4 );

                if ( primitive.indices >= 0 ) {
                    const auto& accessor{ model.accessors[primitive.indices] };
                    const auto& view{ model.bufferViews[accessor.bufferView] };
                    const auto& buffer{ model.buffers[view.buffer] };
                    const auto* dataPtr{ buffer.data.data() + view.byteOffset + accessor.byteOffset };

                    node.Indices.resize( accessor.count );

                    for ( Size i{}; i < accessor.count; ++i ) {
                        if ( accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ) {
                            node.Indices[i] = reinterpret_cast<const UInt16*>( dataPtr )[i];
                        } else if ( accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT ) {
                            node.Indices[i] = reinterpret_cast<const UInt32*>( dataPtr )[i];
                        }
                    }
                }

                modelData.MeshNodes.push_back( std::move( node ) );
            }
        }
    }

    auto GLTFImporter::LoadMaterials( tinygltf::Model& model, ModelData& modelData, const std::string& rootPath ) -> void {
        modelData.Materials.reserve( model.materials.size() );

        TextureLoadDescription loadInfo{};
        loadInfo.WithType( TextureType::TEXTURE_2D );

        for ( const auto& gltfMaterial: model.materials ) {
            MaterialProperties props{};
            props.Name = gltfMaterial.name;
            props.IsDoubleSided = gltfMaterial.doubleSided;

            const auto& pbr{ gltfMaterial.pbrMetallicRoughness };

            props.BaseColorFactor = {
                static_cast<float>( pbr.baseColorFactor[0] ),
                static_cast<float>( pbr.baseColorFactor[1] ),
                static_cast<float>( pbr.baseColorFactor[2] ),
                static_cast<float>( pbr.baseColorFactor[3] )
            };

            props.MetallicFactor = static_cast<float>( pbr.metallicFactor );
            props.RoughnessFactor = static_cast<float>( pbr.roughnessFactor );

            // Base color
            if ( pbr.baseColorTexture.index >= 0 ) {
                const auto& tex{ model.textures[pbr.baseColorTexture.index] };
                props.BaseColorTextureSet = pbr.baseColorTexture.texCoord;
                
                loadInfo.WithMapType( MapType::BASE_COLOR_TEXTURE );
                loadInfo.WithFile( FileService::Get()->LoadFile( Path{ PathBuilder()
                           .WithPath( rootPath )
                           .WithPath( model.images[tex.source].uri )
                           .Build() } ) );

                TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadInfo ) };
                if (!texture.IsEmpty()) {
                    props.TexturesByUri[loadInfo.TextureFile->GetPath()] = texture;
                }
            }

            // Metall roughness
            if ( pbr.metallicRoughnessTexture.index >= 0 ) {
                const auto& tex{ model.textures[pbr.metallicRoughnessTexture.index] };
                props.MetallicRoughnessTextureSet = pbr.metallicRoughnessTexture.texCoord;

                loadInfo.WithMapType( MapType::METALLIC_ROUGHNESS_TEXTURE );
                loadInfo.WithFile( FileService::Get()->LoadFile( Path{ PathBuilder()
                           .WithPath( rootPath )
                           .WithPath( model.images[tex.source].uri )
                           .Build() } ) );

                TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadInfo ) };
                if (!texture.IsEmpty()) {
                    props.TexturesByUri[loadInfo.TextureFile->GetPath()] = texture;
                }
            }

            // Normal
            if ( gltfMaterial.normalTexture.index >= 0 ) {
                const auto& tex{ model.textures[gltfMaterial.normalTexture.index] };
                props.NormalTextureSet = gltfMaterial.normalTexture.texCoord;
                props.NormalScale = static_cast<float>( gltfMaterial.normalTexture.scale );

                loadInfo.WithMapType( MapType::NORMAL_TEXTURE );
                loadInfo.WithFile( FileService::Get()->LoadFile( Path{ PathBuilder()
                           .WithPath( rootPath )
                           .WithPath( model.images[tex.source].uri )
                           .Build() } ) );

                TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadInfo ) };
                if (!texture.IsEmpty()) {
                    props.TexturesByUri[loadInfo.TextureFile->GetPath()] = texture;
                }
            }

            // Occlusion
            if ( gltfMaterial.occlusionTexture.index >= 0 ) {
                const auto& tex{ model.textures[gltfMaterial.occlusionTexture.index] };
                props.OcclusionStrength = gltfMaterial.occlusionTexture.texCoord;
                props.OcclusionStrength =
                        static_cast<float>( gltfMaterial.occlusionTexture.strength );

                loadInfo.WithMapType( MapType::AMBIENT_OCCLUSION_TEXTURE );
                loadInfo.WithFile( FileService::Get()->LoadFile( Path{ PathBuilder()
                           .WithPath( rootPath )
                           .WithPath( model.images[tex.source].uri )
                           .Build() } ) );

                TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadInfo ) };
                if (!texture.IsEmpty()) {
                    props.TexturesByUri[loadInfo.TextureFile->GetPath()] = texture;
                }
            }

            // Emissive
            if ( gltfMaterial.emissiveTexture.index >= 0 ) {
                const auto& tex{ model.textures[gltfMaterial.emissiveTexture.index] };
                props.EmissiveTextureSet = gltfMaterial.emissiveTexture.texCoord;

                loadInfo.WithMapType( MapType::EMISSIVE_TEXTURE );
                loadInfo.WithFile( FileService::Get()->LoadFile( Path{ PathBuilder()
                           .WithPath( rootPath )
                           .WithPath( model.images[tex.source].uri )
                           .Build() } ) );

                TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadInfo ) };
                if (!texture.IsEmpty()) {
                    props.TexturesByUri[loadInfo.TextureFile->GetPath()] = texture;
                }
            }

            props.EmissiveFactor = {
                static_cast<float>( gltfMaterial.emissiveFactor[0] ),
                static_cast<float>( gltfMaterial.emissiveFactor[1] ),
                static_cast<float>( gltfMaterial.emissiveFactor[2] )
            };

            // Alpha (Default is Opaque unless otherwise specified)
            if (gltfMaterial.alphaMode == "BLEND") {
                props.AlphaMask = PBR_AlphaMode::Blend;
            } else if ( gltfMaterial.alphaMode == "MASK" ) {
                props.AlphaMask = PBR_AlphaMode::Mask;
            }

            props.AlphaMaskCutoff = static_cast<float>( gltfMaterial.alphaCutoff );

            // Extensions
            auto ext{ gltfMaterial.extensions.find( KHR_PBR_SpecularGlossiness.data() ) };
            if ( gltfMaterial.extensions.find( KHR_PBR_SpecularGlossiness.data() ) != gltfMaterial.extensions.end() ) {

                if ( ext->second.Has( "specularGlossinessTexture" ) ) {
                    auto index{ ext->second.Get( "specularGlossinessTexture" ).Get( "index" ) };
                    
                    auto texIndex = index.Get<int>();
                    auto texCoordSet = ext->second.Get( "specularGlossinessTexture" ).Get( "texCoord" ).Get<int>();

                    loadInfo.WithMapType( MapType::SPECULAR_GLOSSINESS );
                    loadInfo.WithFile( FileService::Get()->LoadFile( 
                        Path{ PathBuilder()
                            .WithPath( rootPath )
                            .WithPath( model.images[texIndex].uri )
                            .Build() } ) );

                    TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadInfo ) };
                    if ( !texture.IsEmpty() ) {
                        props.TexturesByUri[loadInfo.TextureFile->GetPath()] = texture;
                    }

                    props.SpecularGlossinessSet = texCoordSet;
                    props.Workflow = PBR_Workflow::SpecularGlossiness;
                }
                
                if ( ext->second.Has( "diffuseTexture" ) ) {
                    auto index{ ext->second.Get( "diffuseTexture" ).Get( "index" ) };
                    loadInfo.WithMapType( MapType::DIFFUSE_TEXTURE );
                    loadInfo.WithFile( FileService::Get()->LoadFile( Path{ PathBuilder()
                            .WithPath( rootPath )
                            .WithPath( model.images[index.Get<int>()].uri )
                            .Build() } ) );

                    TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadInfo ) };
                    if ( !texture.IsEmpty() ) {
                        props.TexturesByUri[loadInfo.TextureFile->GetPath()] = texture;
                    }
                }

                if ( ext->second.Has( "diffuseFactor" ) ) {
                    auto factor{ ext->second.Get( "diffuseFactor" ) };
                    for ( UInt32 i{}; i < factor.ArrayLen(); i++ ) {
                        auto val{ factor.Get( i ) };
                        //material.extension.diffuseFactor[i] = val.IsNumber() ? ( float )val.Get<double>() : ( float )val.Get<int>();
                    }
                }

                if ( ext->second.Has( "specularFactor" ) ) {
                    auto factor{ ext->second.Get( "specularFactor" ) };
                    for ( UInt32 i{}; i < factor.ArrayLen(); i++ ) {
                        auto val{ factor.Get( i ) };
                        //material.extension.specularFactor[i] = val.IsNumber() ? ( float )val.Get<double>() : ( float )val.Get<int>();
                    }
                }
            }

            if ( gltfMaterial.extensions.find( KHR_PBR_Unlit.data() ) != gltfMaterial.extensions.end() ) {
                props.Unlit = true;
            }

            if ( gltfMaterial.extensions.find( KHR_Emissive_Strength.data() ) != gltfMaterial.extensions.end() ) {
                auto ext = gltfMaterial.extensions.find( KHR_Emissive_Strength.data() );
                if ( ext->second.Has( "emissiveStrength" ) ) {
                    auto value{ ext->second.Get( "emissiveStrength" ) };
                    props.EmissiveStrength = ( float )value.Get<double>();
                }
            }

            modelData.Materials.push_back( std::move( props ) );
        }
    }

    auto GLTFImporter::LoadSkeleton( tinygltf::Model& model, ModelData& modelData ) -> void {
        if (model.skins.empty()) {
            return;
        }

        const tinygltf::Skin& skin{ model.skins[0] };

        Skeleton& skeleton{ modelData.SceneSkeleton };

        // Load inverse bind matrices
        std::vector<glm::mat4> inverseBindMatrices{};
        if ( skin.inverseBindMatrices >= 0 ) {
            const tinygltf::Accessor& accessor = model.accessors[skin.inverseBindMatrices];
            const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
            const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

            inverseBindMatrices.resize( accessor.count );
            std::memcpy( inverseBindMatrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof( Mat4F ) );

            skeleton.SetInverseBindMatrices( std::move( inverseBindMatrices ) );
        }

        // Register joints
        for ( Int32 jointNodeIndex: skin.joints ) {
            const tinygltf::Node& node{ model.nodes[jointNodeIndex] };

            const std::string name {
                    node.name.empty()
                            ? std::to_string( jointNodeIndex )
                            : node.name };

            skeleton.RegisterJoint( name, jointNodeIndex );
        }

        // Determine root node
        Int32 rootNodeIndex {
                skin.skeleton != -1
                        ? skin.skeleton
                        : skin.joints[0] };

        Node root{ LoadNode( model, skeleton, rootNodeIndex ) };

        skeleton.SetHierarchy( std::move( root ) );

#if !defined( NDEBUG )
        MKT_COLOR_PRINT_FORMATTED_FLUSH(
                MKT_FMT_COLOR_BLUE_VIOLET,
                "Printing skeleton hierarchy\n" );

        modelData.SceneSkeleton.PrintTreeView();
#endif

        skeleton.BuildOzzStructures();
    }

    auto GLTFImporter::LoadAnimations( tinygltf::Model& model, ModelData& modelData ) -> void {
        for ( tinygltf::Animation& anim: model.animations ) {
            AnimationDescription animationDescription{};
            animationDescription.Name = anim.name;

            // Some animations do not have names
            if ( anim.name.empty() ) {
                animationDescription.Name = StringUtil::Format( "Unnamed animation" );
            }

            // Samplers
            for ( auto& samp: anim.samplers ) {
                AnimationSampler sampler{};

                if ( samp.interpolation == "LINEAR" ) {
                    sampler.Interpolation = InterpolationType::LINEAR;
                }
                if ( samp.interpolation == "STEP" ) {
                    sampler.Interpolation = InterpolationType::STEP;
                }
                if ( samp.interpolation == "CUBICSPLINE" ) {
                    sampler.Interpolation = InterpolationType::CUBICSPLINE;
                }

                // Read sampler input time values
                {
                    const tinygltf::Accessor& accessor = model.accessors[samp.input];
                    const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
                    const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

                    MKT_ASSERT( accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "Unexpected component type" );

                    const void* dataPtr{ MKT_ADDRESSOF( buffer.data[accessor.byteOffset + bufferView.byteOffset] ) };
                    const float* buf{ static_cast<const float*>( dataPtr ) };

                    for ( Size index{}; index < accessor.count; index++ ) {
                        sampler.TimeStamps.push_back( buf[index] );
                    }

                    // Compute bounds
                    for ( auto input: sampler.TimeStamps ) {
                        if ( input < animationDescription.Start ) {
                            animationDescription.Start = input;
                        }

                        if ( input > animationDescription.End ) {
                            animationDescription.End = input;
                        }
                    }
                }

                // Read sampler output T/R/S values
                {
                    const tinygltf::Accessor& accessor{ model.accessors[samp.output] };
                    const tinygltf::BufferView& bufferView{ model.bufferViews[accessor.bufferView] };
                    const tinygltf::Buffer& buffer{ model.buffers[bufferView.buffer] };

                    MKT_ASSERT( accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT, "Unexpected component type" );

                    const void* dataPtr{ MKT_ADDRESSOF( buffer.data[accessor.byteOffset + bufferView.byteOffset] ) };

                    switch ( accessor.type ) {
                        case TINYGLTF_TYPE_VEC3: {
                            const Vec3F* buf{ static_cast<const Vec3F*>( dataPtr ) };

                            for ( Size index{}; index < accessor.count; index++ ) {
                                sampler.OutputsVec4.emplace_back( glm::vec4( buf[index], 0.0f ) );
                                sampler.Outputs.emplace_back( buf[index][0] );
                                sampler.Outputs.emplace_back( buf[index][1] );
                                sampler.Outputs.emplace_back( buf[index][2] );
                            }
                            break;
                        }
                        case TINYGLTF_TYPE_VEC4: {
                            const Vec4F* buf{ static_cast<const Vec4F*>( dataPtr ) };

                            for ( Size index{}; index < accessor.count; index++ ) {
                                sampler.OutputsVec4.emplace_back( buf[index] );

                                sampler.Outputs.emplace_back( buf[index][0] );
                                sampler.Outputs.emplace_back( buf[index][1] );
                                sampler.Outputs.emplace_back( buf[index][2] );
                                sampler.Outputs.emplace_back( buf[index][3] );
                            }
                            break;
                        }
                        default: {
                            MKT_CORE_LOGGER_WARN( "Unknown sampler type" );
                            break;
                        }
                    }
                }

                animationDescription.Samplers.emplace_back( sampler );
            }

            // Channels
            for ( auto& source: anim.channels ) {
                AnimationChannel channel{};

                if ( source.target_path == "rotation" ) {
                    channel.Path = PathType::ROTATION;
                }
                if ( source.target_path == "translation" ) {
                    channel.Path = PathType::TRANSLATION;
                }
                if ( source.target_path == "scale" ) {
                    channel.Path = PathType::SCALE;
                }
                if ( source.target_path == "weights" ) {
                    MKT_CORE_LOGGER_WARN( "weights not yet supported, skipping channel" );
                    continue;
                }

                channel.SamplerIndex = source.sampler;
                channel.JointIndex = source.target_node;

                if ( Joint * joint{ modelData.SceneSkeleton.FindJointByID( channel.JointIndex ) } ) {
                    channel.NodeName = joint->GetBoneName();
                }

                if (channel.JointIndex == 13) {
                    MKT_CORE_LOGGER_DEBUG( "YOO" );
                }

                animationDescription.Channels.emplace_back( channel );
            }

            // Validations
            for (const auto& sampler : animationDescription.Samplers) {
                MKT_ASSERT( std::ranges::is_sorted( animationDescription.Samplers[0].TimeStamps ), "Keyframes not sorted in increasing order." );
            }

            const auto [it, success]{ 
                modelData.Animations.try_emplace( animationDescription.Name, std::move( animationDescription ) ) 
            };

            // Build Ozz structures
            if (success) {
                it->second.BuildOzzStructures( modelData.SceneSkeleton );
            }
        }
    }

    auto GLTFImporter::TryAcquireImporter() -> std::vector<Unique<LoaderData>>::iterator {
        return std::ranges::find_if( m_Importers, []( const auto& importer ) -> bool {
            bool expected{ true };
            if ( importer->IsFree.compare_exchange_strong( expected, false, std::memory_order_acquire ) ) {
                return true;
            }

            return false;
        } );
    }


    auto GLTFImporter::Import( LoaderData& loaderData, const ModelLoadDescription& description, ModelData& out ) -> void {
        tinygltf::Model model{};

        bool res{ loaderData.Loader.LoadASCIIFromFile( &model, &loaderData.Err, &loaderData.Warn, description.ModelFile->GetPath() ) };
        if ( !loaderData.Warn.empty() ) {
            MKT_CORE_LOGGER_WARN( "GLTF Loader WARN: {}", loaderData.Warn );
        }

        if ( !loaderData.Err.empty() ) {
            MKT_CORE_LOGGER_ERROR( "GLTF Loader ERROR: {}", loaderData.Err );
        }

        if ( !res ) {
            MKT_CORE_LOGGER_ERROR( "Failed to load glTF: {}", description.ModelFile->GetPath() );
        } else {
            MKT_CORE_LOGGER_DEBUG( "Loaded glTF: {}", description.ModelFile->GetPath() );

            // Reference root path for loading textures
            const std::string rootPath{ Filesystem::StripFileName( description.ModelFile->GetPath() ) };

            LoadPrimitives( model, out );
            LoadMaterials( model, out, rootPath );

            LoadSkeleton( model, out );

            // Animation requires skeleton ready
            LoadAnimations( model, out );
        }
    }
}