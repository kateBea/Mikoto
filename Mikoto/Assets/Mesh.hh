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
#include <Models/MeshData.hh>
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
        explicit Mesh( MeshData &&data );


        /**
   * Constructs and initializes this mesh with the contents of the other
   * mesh using move semantics, the other mesh is put into an invalid state after this operation
   * @param other move from Mesh
   * */
        Mesh( Mesh &&other ) noexcept = default;

        /**
   * Assigns to this mesh with the contents of the other
   * mesh using move semantics, the other mesh is invalid after this operation
   * @param other move from Mesh
   * @returns </code>*this<code>
   * */
        auto operator=( Mesh &&other ) noexcept -> Mesh & = default;


        /**
   * Returns the vertex buffer object from this mesh
   * @returns vertex buffer
   * */
        MKT_NODISCARD auto GetVertexBuffer() const -> const std::shared_ptr<VertexBuffer> & { return m_Data.GetVertices(); }

        /**
   * Returns the index buffer object from this mesh
   * @returns index buffer
   * */
        MKT_NODISCARD auto GetIndexBuffer() const -> const std::shared_ptr<IndexBuffer> & { return m_Data.GetIndices(); }


        /**
   * Returns the set of textures of this mesh
   * @returns set of textures
   * */
        MKT_NODISCARD auto GetTextures() const -> const std::vector<std::shared_ptr<Texture2D> > & {
            return ( m_Data.GetTextures() );
        }

        /**
   * Returns the name of this mesh
   * @returns Mesh name
   * */
        MKT_NODISCARD auto GetName() const -> const std::string & { return ( m_Data.GetName() ); }

        /**
   * Default destructor
   * */
        ~Mesh() = default;

    private:
        MeshData m_Data{};
    };
}// namespace Mikoto

#endif// MIKOTO_MESH_HH
