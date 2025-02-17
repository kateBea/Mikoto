/**
 * Mesh.hh
 * Created by kate on 6/29/23.
 * */

#ifndef MIKOTO_MESH_HH
#define MIKOTO_MESH_HH

// C++ Standard Library
#include <algorithm>
#include <span>
#include <utility>
#include <vector>

// Project Libraries
#include <Common/Common.hh>
#include <Material/Core/Material.hh>
#include <Material/Texture/Texture2D.hh>
#include <Renderer/Buffer/IndexBuffer.hh>
#include <Renderer/Buffer/VertexBuffer.hh>

namespace Mikoto {

    class Mesh final {
    public:
        /**
        * Default constructs this Mesh
        * */
        explicit Mesh() = default;

        /**
        * Initializes this mesh from the parameter data.
        * @param data contains the required data to initialize this mesh
        * */
        explicit Mesh( std::string_view name, Scope_T<VertexBuffer>&& vertices, Scope_T<IndexBuffer>&& indices, std::vector<Scope_T<Texture2D>>&& textures, const Path_T& path );


        /**
        * Constructs and initializes this mesh with the contents of the other
        * mesh using move semantics, the other mesh is put into an invalid state after this operation
        * @param other move from Mesh
        * */
        Mesh( Mesh&& other ) noexcept = default;

        /**
        * Assigns to this mesh with the contents of the other
        * mesh using move semantics, the other mesh is invalid after this operation
        * @param other move from Mesh
        * @returns </code>*this<code>
        * */
        auto operator=( Mesh&& other ) noexcept -> Mesh& = default;

      /**
         * Returns the absolute path to the directory where this model is located (does not include the model's name)
         * @returns absolute path to this model's directory
         * */
      MKT_NODISCARD auto GetDirectory() const -> const Path_T & { return m_ModelAbsolutePath; }


        /**
         * Returns the vertex buffer object from this mesh
         * @returns vertex buffer
         * */
        MKT_NODISCARD auto GetVertexBuffer() const -> const VertexBuffer* { return m_Vertices.get(); }

        /**
         * Returns the index buffer object from this mesh
         * @returns index buffer
         * */
        MKT_NODISCARD auto GetIndexBuffer() const -> const IndexBuffer* { return m_Indices.get(); }


        /**
         * Returns the set of textures of this mesh
         * @returns set of textures
         * */
        MKT_NODISCARD auto GetTextures() const -> const std::vector<Scope_T<Texture2D>>& { return m_Textures; }

        /**
         * Returns the name of this mesh
         * @returns Mesh name
         * */
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

        /**
         * Default destructor
         * */
        ~Mesh() = default;

    private:
        Path_T m_ModelAbsolutePath{};

        std::string m_Name{};
        Scope_T<VertexBuffer> m_Vertices{};
        Scope_T<IndexBuffer> m_Indices{};
        std::vector<Scope_T<Texture2D>> m_Textures{};
    };
}// namespace Mikoto

#endif// MIKOTO_MESH_HH
