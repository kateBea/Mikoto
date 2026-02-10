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

#ifndef MIKOTO_MODEL_HH
#define MIKOTO_MODEL_HH

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Material/Texture2D.hh>
#include <Renderer/Core/Buffer.hh>

namespace Mikoto {

    enum class VertexAttribute {
        POSITIONS,
        NORMALS,
        TANGENTS,
        BITANGENTS,
        UV0,
        UV1,
        COLORS,
        JOINTS,
        WEIGHTS,

        CUSTOM,
    };

    struct ModelLoadDescription {
        const File* ModelFile{};
        bool WantTextures{ true };

        // Specifies the order we want attributes in
        // Default is order specified by DEFAULT_VERTEX_BUFFER_LAYOUT in Pipeline.hh file
        std::vector<VertexAttribute> Attributes{
            VertexAttribute::POSITIONS,
            VertexAttribute::NORMALS,
            VertexAttribute::COLORS,
            VertexAttribute::UV0,
            VertexAttribute::UV1,

            VertexAttribute::JOINTS, // Bone IDs
            VertexAttribute::WEIGHTS, // Weight IDs
        };

        auto LoadTextures( bool value ) -> ModelLoadDescription&;
        auto WithFilePath( const File* file ) -> ModelLoadDescription&;
    };

    struct MaterialProperties {
        std::string Name;

        enum class Workflow {
            MetallicRoughness,
            SpecularGlossiness,
            Unlit
        };

        Workflow Workflow{ Workflow::MetallicRoughness };

        // Base color/Albedo
        Vec4F BaseColorFactor{1.f, 1.f, 1.f, 1.f};
        std::string BaseColorTexture{};

        // Metallic-Roughness workflow
        float MetallicFactor{ 1.f };
        float RoughnessFactor{ 1.f };
        std::string MetallicRoughnessTexture{};

        // Specular-Glossiness workflow (FBX/OBJ/glTF extension)
        Vec3F DiffuseFactor{1.f, 1.f, 1.f};
        std::string DiffuseTexture{};
        Vec3F SpecularFactor{1.f, 1.f, 1.f};

        std::string SpecularGlossinessTexture{};
        float GlossinessFactor{ 1.f };

        // Normal mapping
        std::string NormalTexture{};
        float NormalScale{ 1.f };

        // Occlusion
        std::string OcclusionTexture{};
        float OcclusionStrength{ 1.f };

        // Emissive
        Vec3F EmissiveFactor{0.f, 0.f, 0.f};
        float EmissiveStrength{ 1.f };
        std::string EmissiveTexture{};

        // Alpha
        enum class AlphaMode { Opaque, Mask, Blend };
        AlphaMode alphaMode{ AlphaMode::Opaque };
        float AlphaCutoff{ 0.5f };

        // UV sets
        Int32 BaseColorTexCoord{};
        Int32 MetallicRoughnessTexCoord{};
        Int32 NormalTexCoord{};
        Int32 OcclusionTexCoord{};
        Int32 EmissiveTexCoord{};
    };

    class MeshNode final {
    public:
        explicit MeshNode( Size index,
            BufferHandle vertices,
            BufferHandle indices,
            std::vector<TextureHandle>&& textures,
            std::string_view name, MaterialProperties&&
            properties );

        MeshNode(MeshNode&& other) noexcept = default;

        MKT_NODISCARD auto GetName() -> const std::string& { return m_Name; }

        MKT_NODISCARD auto GetMeshIndex() const -> Size { return m_MeshIndex; }
        MKT_NODISCARD auto GetVertexBuffer() -> BufferHandle { return  m_Vertices; }
        MKT_NODISCARD auto GetIndexBuffer() -> BufferHandle { return m_Indices; }

        MKT_NODISCARD auto GetProperties() const -> const MaterialProperties& { return m_Properties; }
        MKT_NODISCARD auto GetTextures() const -> const std::vector<TextureHandle>& { return m_OriginalTextures; }

        DISABLE_COPY_FOR( MeshNode );

    private:
        Size m_MeshIndex{};

        std::string m_Name{};

        BufferHandle m_Vertices{};
        BufferHandle m_Indices{};

        MaterialProperties m_Properties{};

        std::vector<TextureHandle> m_OriginalTextures{};
    };

    /**
    * @class Model
    * @brief Represents a 3D model composed of multiple mesh nodes.
    *
    * The `Model` class encapsulates a 3D object, including its mesh data,
    * directory path, name, and vertex/index counts.
    * It provides access to mesh data and metadata about the model.
    */
    class Model final : public ReferenceCounted {
    public:
        /**
        * @brief Retrieves the meshes of the model.
        * @return The count of mesh nodes.
        */
        MKT_NODISCARD auto GetMeshNodeCount() const -> Size { return m_Meshes.size(); }

        /**
        * @brief Retrieves the mesh of the model by index.
        * @return A reference to a mesh.
        * @throws
        */
        MKT_NODISCARD auto GetMeshNode(const Size index) -> MeshNode& { return m_Meshes.at(index); }

        /**
        * @brief Retrieves the mesh of the model by index.
        * @return A constant reference to a mesh.
        * @throws
        */
        MKT_NODISCARD auto GetMeshNode(const Size index) const -> const MeshNode& { return m_Meshes.at(index); }

        /**
        * @brief Gets the absolute directory path where the model is stored.
        * @return A constant reference to the model's directory path.
        */
        MKT_NODISCARD auto GetDirectory() const -> const Path& { return m_ModelAbsolutePath; }

        /**
        * @brief Retrieves the name of the model.
        * @return A constant reference to the model's name.
        */
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_ModelName; }

        /**
        * @brief Gets the total number of vertices in the model.
        * @return The vertex count.
        */
        MKT_NODISCARD auto GetVertexCount() const -> UInt64 { return m_TotalVertices; }

        /**
         * @brief Gets the total number of indices in the model.
         * @return The index count.
         */
        MKT_NODISCARD auto GetIndexCount() const -> UInt64 { return m_TotalIndices; }

        /**
        * @brief Adds a new mesh node to the collection.
        * @tparam Args Variadic template parameters for forwarding constructor arguments.
        * @param index The index at which to insert the mesh node.
        * @param args Arguments to be forwarded to the mesh node constructor.
        *
        * This function inserts a new mesh node into the `m_Meshes` collection at the given index.
        */
        template<typename... Args>
        auto PushMeshNode(UInt32 index, Args&&... args) -> void {
            m_Meshes.emplace(index, std::forward<Args>(args)...);
        }

    ~Model() override = default;

    private:
        DISABLE_COPY_AND_MOVE_FOR( Model );

        /**
         * @brief Constructs a Model with the provided parameters.
         * @param modelName Name of the model.
         * @param modelPath Absolute path to the model file.
         */
        explicit Model( std::string modelName, Path modelPath)
            : m_ModelName{ std::move( modelName ) },
              m_ModelAbsolutePath{ std::move( modelPath ) }
        {}

    private:

        // Only the factory can construct models
        friend class MeshFactory;

    protected:
        std::string m_ModelName{};
        Path m_ModelAbsolutePath{};

        // ( Mesh index, mesh node )
        ankerl::unordered_dense::map<UInt32, MeshNode> m_Meshes{};

        UInt64 m_TotalVertices{};
        UInt64 m_TotalIndices{};
    };

    using ModelHandle = Ref<Model>;

}

#endif// MIKOTO_MODEL_HH