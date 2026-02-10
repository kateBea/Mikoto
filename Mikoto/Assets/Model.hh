/**
 * Model.hh
 * Created by kate on 6/29/23.
 * */

#ifndef MIKOTO_MODEL_HH
#define MIKOTO_MODEL_HH

// C++ Standard Library
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

// Third Party Libraries
#include <ankerl/unordered_dense.h>

// Project Libraries
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

        CUSTOM,
    };

    /**
    * @struct ModelLoadDescription
    * @brief Contains parameters for loading a 3D model.
    *
    * The `ModelLoadInfo` structure specifies the path of the model to be loaded
    * and whether textures should be included in the loading process.
    */
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
            VertexAttribute::TANGENTS,
            VertexAttribute::BITANGENTS,

            VertexAttribute::CUSTOM, // Bone IDs
            VertexAttribute::CUSTOM, // Weight IDs
        };

        /**
        * @brief Sets the path of the model.
        * @param file The absolute or relative path to the model file.
        * @return Reference to the modified ModelLoadInfo.
        */
        auto WithFilePath( const File* file ) -> ModelLoadDescription&;

        /**
        * @brief Specifies whether to load textures for the model.
        * @param value True to load textures, false otherwise.
        * @return Reference to the modified ModelLoadInfo.
        */
        auto LoadTextures( bool value ) -> ModelLoadDescription&;
    };

    struct MaterialProperties {
        std::string Name{};
        Vec3F DiffuseColor{0.f, 0.f, 0.f};
        float Metallic{0.f};
        float Roughness{0.f};
        float Shininess{0.f};
    };

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
        explicit MeshNode( Size index, BufferHandle vertices, BufferHandle indices, std::vector<TextureHandle>&& textures, std::string_view name, MaterialProperties&& properties );

        MeshNode(MeshNode&& other) noexcept;

        /**
         * @brief Retrieves the vertex buffer of the mesh.
         * @return A pointer to the vertex buffer.
         */
        MKT_NODISCARD auto GetVertexBuffer() -> BufferHandle { return  m_Vertices; }

        /**
         * @brief Retrieves the index buffer of the mesh.
         * @return A pointer to the index buffer.
         */
        MKT_NODISCARD auto GetIndexBuffer() -> BufferHandle { return m_Indices; }

        MKT_NODISCARD auto GetName() -> const std::string& { return m_Name; }

        MKT_NODISCARD auto GetProperties() const -> const MaterialProperties& { return m_Properties; }

        /**
         * @brief Retrieves the index of the mesh into its corresponding model.
         * @return The mesh index for this mesh.
         */
        MKT_NODISCARD auto GetMeshIndex() const -> Size { return m_MeshIndex; }

        /**
         * @brief Retrieves the textures associated with the mesh.
         * @return A constant reference to the vector of textures.
         */
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
        auto AddMeshNode(UInt32 index, Args&&... args) -> void {
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

}// namespace Mikoto

#endif// MIKOTO_MODEL_HH