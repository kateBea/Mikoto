/**
 * Mesh.hh
 * Created by kate on 6/29/23.
 * */

#ifndef KATE_ENGINE_MESH_HH
#define KATE_ENGINE_MESH_HH

// C++ Standard Library
#include <vector>
#include <cstdint>
#include <span>
#include <algorithm>
#include <utility>

// Project Libraries
#include <Utility/Common.hh>

#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Material/Texture.hh>

namespace kaTe {
    class MeshData {
    public:
        MeshData() = default;

        MeshData(const MeshData& other) = default;
        MeshData(MeshData&& other)  noexcept
            :   m_Vertices{ std::move(other.m_Vertices) }, m_Indices{ std::move(other.m_Indices) }, m_Textures{ std::move(other.m_Textures) }
        {}

        KT_NODISCARD auto GetVertices() -> std::shared_ptr<VertexBuffer>& { return m_Vertices; }
        KT_NODISCARD auto GetIndices() -> std::shared_ptr<IndexBuffer>& { return m_Indices; }
        KT_NODISCARD auto GetTextures() -> std::vector<std::shared_ptr<Texture>>& { return m_Textures; }

        KT_NODISCARD auto GetVertices() const -> const std::shared_ptr<VertexBuffer>& { return m_Vertices; }
        KT_NODISCARD auto GetIndices() const -> const std::shared_ptr<IndexBuffer>& { return m_Indices; }
        KT_NODISCARD auto GetTextures() const -> const std::vector<std::shared_ptr<Texture>>& { return m_Textures; }

        auto SetVertices(const std::shared_ptr<VertexBuffer>& vertices) -> void { m_Vertices = vertices; }
        auto SetIndices(const std::shared_ptr<IndexBuffer>& indices) -> void { m_Indices = indices; }
        auto SetTextures(std::vector<std::shared_ptr<Texture>>&& textures) -> void { m_Textures = std::move(textures); }

        auto operator=(MeshData&& other) noexcept -> MeshData& {
            m_Vertices = std::move(other.m_Vertices);
            m_Indices = std::move(other.m_Indices);
            m_Textures = std::move(other.m_Textures);

            return *this;
        }

        auto operator=(const MeshData& other) -> MeshData& {
            m_Vertices = other.m_Vertices;
            m_Indices = other.m_Indices;
            m_Textures = other.m_Textures;
            return *this;
        }
    private:
        inline static const BufferLayout MeshLayout{
                { ShaderDataType::FLOAT3_TYPE, "a_Position" },
                { ShaderDataType::FLOAT3_TYPE, "a_Normal" },
                { ShaderDataType::FLOAT2_TYPE, "a_TextureCoordinates" },
                { ShaderDataType::FLOAT4_TYPE, "a_Color" }
        };

        std::shared_ptr<VertexBuffer> m_Vertices{};
        std::shared_ptr<IndexBuffer> m_Indices{};

        // TODO: make part of the material
        std::vector<std::shared_ptr<Texture>> m_Textures{};

        // should probably have a material or set of material alongside with the set of indices/vertices
        // std::unordered_map<material_id, Material>
    };

    class Mesh {
    public:
        /**
         * Default constructs this Mesh
         * */
        explicit Mesh() = default;

        /**
         * Initializes this Mesh' vertex, index and texture objects with the
         * data we pass ass parameters. The parameters data are moved
         * @param vertices contains the vertex data for this mesh, like positions, texture coordinates, etc.
         * @param indices contains the indices for indexed drawing
         * @param textures contains the texture data for this mesh, see kT::Texture for more
         * */
        explicit Mesh(MeshData data);

        /**
         * Constructs and initializes this mesh with the contents of the other
         * mesh using move semantics, the other mesh is invalid after this operation
         * @param other moved from Mesh
         * */
        Mesh(Mesh&& other) noexcept;

        /**
         * Assigns to this mesh with the contents of the other
         * mesh using move semantics, the other mesh is invalid after this operation
         * @param other moved from Mesh
         * @returns *this
         * */
        auto operator=(Mesh&& other) noexcept -> Mesh&;

        KT_NODISCARD auto GetVertexBuffer() const -> const std::shared_ptr<VertexBuffer>& { return m_Data.GetVertices(); }
        KT_NODISCARD auto GetIndexBuffer() const -> const std::shared_ptr<IndexBuffer>& { return m_Data.GetIndices(); }
        KT_NODISCARD auto GetTextures() const -> const std::vector<std::shared_ptr<Texture>>& { return (m_Data.GetTextures()); }

        ~Mesh() = default;
        
    private:
        MeshData m_Data{};
    };
}

#endif