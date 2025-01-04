//
// Created by kate on 1/4/25.
//

#ifndef MESHDATA_HH
#define MESHDATA_HH

#include <Common/Common.hh>
#include <Material/Texture/Texture2D.hh>
#include <Renderer/Buffer/IndexBuffer.hh>
#include <Renderer/Buffer/VertexBuffer.hh>
#include <utility>

namespace Mikoto {
    class MeshData {
    public:
        explicit MeshData() = default;

        MeshData( const MeshData& other ) = default;
        MeshData( MeshData&& other ) noexcept
            :   m_Name{ std::move(other.m_Name) },
                m_Vertices{ std::move( other.m_Vertices ) },
                m_Indices{ std::move( other.m_Indices ) },
                m_Textures{ std::move( other.m_Textures ) } {}

        MKT_NODISCARD auto GetVertices() const -> const std::shared_ptr<VertexBuffer>& { return m_Vertices; }
        MKT_NODISCARD auto GetIndices() const -> const std::shared_ptr<IndexBuffer>& { return m_Indices; }
        MKT_NODISCARD auto GetTextures() const -> const std::vector<std::shared_ptr<Texture2D>>& { return m_Textures; }
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

        /**
         * Returns the standard vertex buffer layout for all meshes
         * @returns default vertex buffer layout
         * */
        MKT_NODISCARD MKT_UNUSED_FUNC static auto GetDefaultBufferLayout() -> const BufferLayout& { return s_Layout; }

        auto SetVertices( const std::shared_ptr<VertexBuffer>& vertices ) -> void { m_Vertices = vertices; }
        auto SetIndices( const std::shared_ptr<IndexBuffer>& indices ) -> void { m_Indices = indices; }
        auto SetTextures( std::vector<std::shared_ptr<Texture2D>>&& textures ) -> void { m_Textures = std::move( textures ); }
        auto SetName(const std::string& name) -> void { m_Name = name; }

        MKT_UNUSED_FUNC static auto SetDefaultBufferLayout( const BufferLayout& layout ) -> void { s_Layout = layout; }

        auto operator=( MeshData&& other ) noexcept -> MeshData& = default;
        auto operator=( const MeshData& other ) -> MeshData& = default;

    private:
        // Specifies the default vertex buffer layout for Meshes
        static inline BufferLayout s_Layout{
            { ShaderDataType::FLOAT3_TYPE, "a_Position" },
            { ShaderDataType::FLOAT3_TYPE, "a_Normal" },
            { ShaderDataType::FLOAT3_TYPE, "a_Color" },
            { ShaderDataType::FLOAT2_TYPE, "a_TextureCoordinates" }
        };

        std::string m_Name{};
        std::shared_ptr<VertexBuffer> m_Vertices{};
        std::shared_ptr<IndexBuffer> m_Indices{};
        std::vector<std::shared_ptr<Texture2D>> m_Textures{};
    };
}
#endif //MESHDATA_HH
