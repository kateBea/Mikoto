/**
 * MeshNode.hh
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
#include <Material/Material.hh>
#include <Material/Texture2D.hh>
#include <Renderer/Buffer.hh>

namespace Mikoto {


    /**
     * @class MeshNode
     * @brief Represents a single mesh node within a 3D model.
     *
     * The `MeshNode` class stores vertex and index buffer references, along with associated textures.
     */
    class MeshNode final {
    public:
        /**
         * @brief Constructs a MeshNode with the given parameters.
         * @param index Index of the mesh within the model.
         * @param vertices Handle to the vertex buffer.
         * @param indices Handle to the index buffer.
         * @param textures Vector of texture Handles associated with the mesh.
         */
        explicit MeshNode( Size_T index, BufferHandle vertices, BufferHandle indices, std::vector<TextureHandle>&& textures );

        MeshNode(MeshNode&& other) noexcept;

        /**
         * @brief Retrieves the vertex buffer of the mesh.
         * @return A pointer to the vertex buffer.
         */
        MKT_NODISCARD auto GetVertexBuffer() const -> BufferHandle { return m_Vertices; }

        /**
         * @brief Retrieves the index buffer of the mesh.
         * @return A pointer to the index buffer.
         */
        MKT_NODISCARD auto GetIndexBuffer() const -> BufferHandle { return m_Indices; }

        /**
         * @brief Retrieves the index of the mesh into its corresponding model.
         * @return The mesh index for this mesh.
         */
        MKT_NODISCARD auto GetMeshIndex() const -> Size_T { return m_MeshIndex; }

        /**
         * @brief Retrieves the textures associated with the mesh.
         * @return A constant reference to the vector of textures.
         */
        MKT_NODISCARD auto GetTextures() const -> const std::vector<TextureHandle>& { return m_OriginalTextures; }

        DISABLE_COPY_FOR( MeshNode );

    private:
        Size_T m_MeshIndex{};

        BufferHandle m_Vertices{};
        BufferHandle m_Indices{};

        std::vector<TextureHandle> m_OriginalTextures{};
    };
}

#endif// MIKOTO_MESH_HH
