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

#include <algorithm>
#include <ranges>

#include <tiny_gltf.h>

#include <Assets/GltfImporter.hh>
#include <Filesystem/FileService.hh>
#include <Filesystem/FileSystem.hh>
#include <Logging/Logger.hh>
#include <Material/PBRMaterial.hh>
#include <Threading/ThreadUtility.hh>

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

    static auto ReadAccessorAsFloat(
            const tinygltf::Model& model,
            const tinygltf::Accessor& accessor ) -> std::vector<float> {
        const auto& view = model.bufferViews[accessor.bufferView];
        const auto& buffer = model.buffers[view.buffer];

        const auto compSize = ComponentSize( accessor.componentType );
        const auto elemSize = TypeCount( accessor.type );
        const auto stride = accessor.ByteStride( view );

        const auto* dataPtr = buffer.data.data() + view.byteOffset + accessor.byteOffset;

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

        const auto& accessor = model.accessors[primitive.attributes.at( attributeName )];

        auto data = ReadAccessorAsFloat( model, accessor );

        const Size vertexCount = vertices.size();

        for ( size_t i{}; i < vertexCount; ++i ) {
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

                const auto& posAccessor = model.accessors[primitive.attributes.at( "POSITION" )];

                const Size vertexCount = posAccessor.count;
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
                    const auto& accessor = model.accessors[primitive.indices];
                    const auto& view = model.bufferViews[accessor.bufferView];
                    const auto& buffer = model.buffers[view.buffer];
                    const auto* dataPtr = buffer.data.data() + view.byteOffset + accessor.byteOffset;

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
        loadInfo.WithType( TextureType::TEXTURE_2D )
                .WithMapType( MapType::ALBEDO_TEXTURE );

        for ( const auto& mat: model.materials ) {
            MaterialProperties props{};
            props.Name = mat.name;

            const auto& pbr = mat.pbrMetallicRoughness;

            props.BaseColorFactor = {
                static_cast<float>( pbr.baseColorFactor[0] ),
                static_cast<float>( pbr.baseColorFactor[1] ),
                static_cast<float>( pbr.baseColorFactor[2] ),
                static_cast<float>( pbr.baseColorFactor[3] )
            };

            props.MetallicFactor = static_cast<float>( pbr.metallicFactor );
            props.RoughnessFactor = static_cast<float>( pbr.roughnessFactor );

            if ( pbr.baseColorTexture.index >= 0 ) {
                const auto& tex = model.textures[pbr.baseColorTexture.index];
                props.BaseColorTextureSet = pbr.baseColorTexture.texCoord;

                loadInfo.WithMapType( MapType::ALBEDO_TEXTURE );
                loadInfo.WithFile( FileService::Get()->LoadFile( Path{ PathBuilder()
                           .WithPath( rootPath )
                           .WithPath( model.images[tex.source].uri )
                           .Build() } ) );

                TextureHandle texture{ AssetsService::Get()->LoadAsset<Texture>( loadInfo ) };
                if (!texture.IsEmpty()) {
                    props.TexturesByUri[loadInfo.TextureFile->GetPath()] = texture;
                }
            }

            if ( pbr.metallicRoughnessTexture.index >= 0 ) {
                const auto& tex = model.textures[pbr.metallicRoughnessTexture.index];
                props.BaseColorTextureSet =
                        pbr.metallicRoughnessTexture.texCoord;

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
            if ( mat.normalTexture.index >= 0 ) {
                const auto& tex = model.textures[mat.normalTexture.index];
                props.NormalTextureSet = mat.normalTexture.texCoord;
                props.NormalScale =
                        static_cast<float>( mat.normalTexture.scale );

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
            if ( mat.occlusionTexture.index >= 0 ) {
                const auto& tex = model.textures[mat.occlusionTexture.index];
                props.OcclusionStrength = mat.occlusionTexture.texCoord;
                props.OcclusionStrength =
                        static_cast<float>( mat.occlusionTexture.strength );

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
            if ( mat.emissiveTexture.index >= 0 ) {
                const auto& tex = model.textures[mat.emissiveTexture.index];
                props.EmissiveTextureSet = mat.emissiveTexture.texCoord;

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
                static_cast<float>( mat.emissiveFactor[0] ),
                static_cast<float>( mat.emissiveFactor[1] ),
                static_cast<float>( mat.emissiveFactor[2] )
            };

            // Alpha
            if ( mat.alphaMode == "BLEND" )
                props.AlphaMask = PBR_AlphaMode::Blend;
            else if ( mat.alphaMode == "MASK" )
                props.AlphaMask = PBR_AlphaMode::Mask;

            props.AlphaMaskCutoff = static_cast<float>( mat.alphaCutoff );

            modelData.Materials.push_back( std::move( props ) );
        }
    }

    auto GLTFImporter::LoadAnimations( tinygltf::Model& model, ModelData& modelData ) -> void {
    }

    auto GLTFImporter::LoadTextures( tinygltf::Model& model, ModelData& modelData ) -> void {

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

            // Load textures
            const std::string root{ Filesystem::StripFileName( description.ModelFile->GetPath() ) };

            LoadMaterials( model, out, root );
            LoadPrimitives( model, out );
            LoadAnimations( model, out );

        }
    }
}