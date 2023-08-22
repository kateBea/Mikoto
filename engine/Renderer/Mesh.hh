/**
 * Mesh.hh
 * Created by kate on 6/29/23.
 * */

#ifndef MIKOTO_MESH_HH
#define MIKOTO_MESH_HH

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
#include <Renderer/Material/Texture2D.hh>

namespace Mikoto {
    class MeshData {
    public:
        MeshData() = default;

        MeshData(const MeshData& other) = default;
        MeshData(MeshData&& other)  noexcept
            :   m_Vertices{ std::move(other.m_Vertices) }, m_Indices{ std::move(other.m_Indices) }, m_Textures{ std::move(other.m_Textures) }
        {}

        MKT_NODISCARD auto GetVertices() const -> const std::shared_ptr<VertexBuffer>& { return m_Vertices; }
        MKT_NODISCARD auto GetIndices() const -> const std::shared_ptr<IndexBuffer>& { return m_Indices; }
        MKT_NODISCARD auto GetTextures() const -> const std::vector<std::shared_ptr<Texture2D>>& { return m_Textures; }

        auto SetVertices(const std::shared_ptr<VertexBuffer>& vertices) -> void { m_Vertices = vertices; }
        auto SetIndices(const std::shared_ptr<IndexBuffer>& indices) -> void { m_Indices = indices; }
        auto SetTextures(std::vector<std::shared_ptr<Texture2D>>&& textures) -> void { m_Textures = std::move(textures); }

        auto operator=(MeshData&& other) noexcept -> MeshData& = default;
        auto operator=(const MeshData& other) -> MeshData& = default;
    private:
        // Specifies the default vertex buffer layout for Meshes
        inline static const BufferLayout MeshLayout{
                { ShaderDataType::FLOAT3_TYPE, "a_Position" },
                { ShaderDataType::FLOAT3_TYPE, "a_Normal" },
                { ShaderDataType::FLOAT2_TYPE, "a_TextureCoordinates" },
                { ShaderDataType::FLOAT4_TYPE, "a_Color" }
        };

        std::shared_ptr<VertexBuffer> m_Vertices{};
        std::shared_ptr<IndexBuffer> m_Indices{};
        std::vector<std::shared_ptr<Texture2D>> m_Textures{};
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

        MKT_NODISCARD auto GetVertexBuffer() const -> const std::shared_ptr<VertexBuffer>& { return m_Data.GetVertices(); }
        MKT_NODISCARD auto GetIndexBuffer() const -> const std::shared_ptr<IndexBuffer>& { return m_Data.GetIndices(); }
        MKT_NODISCARD [[maybe_unused]] auto GetTextures() const -> const std::vector<std::shared_ptr<Texture2D>>& { return (m_Data.GetTextures()); }

        ~Mesh() = default;
        
    private:
        MeshData m_Data{};
    };
}

#endif // MIKOTO_MESH_HH