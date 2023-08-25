/**
 * Mesh.hh
 * Created by kate on 6/29/23.
 * */

#ifndef MIKOTO_MESH_HH
#define MIKOTO_MESH_HH

// C++ Standard Library
#include <span>
#include <vector>
#include <utility>
#include <algorithm>

// Project Libraries
#include <Utility/Common.hh>
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Material/Texture2D.hh>

namespace Mikoto {
    class MeshData {
    public:
        explicit MeshData() = default;

        MeshData(const MeshData& other) = default;
        MeshData(MeshData&& other)  noexcept
            :   m_Vertices{ std::move(other.m_Vertices) }, m_Indices{ std::move(other.m_Indices) }, m_Textures{ std::move(other.m_Textures) }
        {}

        MKT_NODISCARD auto GetVertices() const -> const std::shared_ptr<VertexBuffer>& { return m_Vertices; }
        MKT_NODISCARD auto GetIndices() const -> const std::shared_ptr<IndexBuffer>& { return m_Indices; }
        MKT_NODISCARD auto GetTextures() const -> const std::vector<std::shared_ptr<Texture2D>>& { return m_Textures; }
        /**
         * Returns the standard vertex buffer layout for all meshes
         * @returns default vertex buffer layout
         * */
        MKT_NODISCARD static auto GetDefaultBufferLayout() -> const BufferLayout& { return s_Layout; }

        auto SetVertices(const std::shared_ptr<VertexBuffer>& vertices) -> void { m_Vertices = vertices; }
        auto SetIndices(const std::shared_ptr<IndexBuffer>& indices) -> void { m_Indices = indices; }
        auto SetTextures(std::vector<std::shared_ptr<Texture2D>>&& textures) -> void { m_Textures = std::move(textures); }
        static auto SetDefaultBufferLayout(const BufferLayout& layout) -> void { s_Layout = layout; }

        auto operator=(MeshData&& other) noexcept -> MeshData& = default;
        auto operator=(const MeshData& other) -> MeshData& = default;

    private:
        // Specifies the default vertex buffer layout for Meshes
        static inline BufferLayout s_Layout{
                { ShaderDataType::FLOAT3_TYPE, "a_Position" },
                { ShaderDataType::FLOAT3_TYPE, "a_Normal" },
                { ShaderDataType::FLOAT3_TYPE, "a_Color" },
                { ShaderDataType::FLOAT2_TYPE, "a_TextureCoordinates" }
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
         * Initializes this mesh from the parameter data.
         * @param data contains the required data to initialize this mesh
         * */
        explicit Mesh(const MeshData& data);

        /**
         * Constructs and initializes this mesh with the contents of the other
         * mesh using move semantics, the other mesh is put into an invalid state after this operation
         * @param other move from Mesh
         * */
        Mesh(Mesh&& other) noexcept;

        /**
         * Assigns to this mesh with the contents of the other
         * mesh using move semantics, the other mesh is invalid after this operation
         * @param other move from Mesh
         * @returns </code>*this<code>
         * */
        auto operator=(Mesh&& other) noexcept -> Mesh&;

        /**
         * Returns the vertex buffer object from this mesh
         * @returns vertex buffer
         * */
        MKT_NODISCARD auto GetVertexBuffer() const -> const std::shared_ptr<VertexBuffer>& { return m_Data.GetVertices(); }
        /**
         * Returns the index buffer object from this mesh
         * @returns index buffer
         * */
        MKT_NODISCARD auto GetIndexBuffer() const -> const std::shared_ptr<IndexBuffer>& { return m_Data.GetIndices(); }
        /**
         * Returns the set of textures of this mesh
         * @returns set of textures
         * */
        MKT_NODISCARD auto GetTextures() const -> const std::vector<std::shared_ptr<Texture2D>>& { return (m_Data.GetTextures()); }

        /**
         * Default destructor
         * */
        ~Mesh() = default;
        
    private:
        /*************************************************************
        * MEMBER VARIABLES
        * ***********************************************************/
        MeshData m_Data{};
    };
}

#endif // MIKOTO_MESH_HH