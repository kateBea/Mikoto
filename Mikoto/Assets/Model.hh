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
#include <Assets/MeshNode.hh>
#include <Common/Common.hh>
#include <Material/Texture2D.hh>

namespace Mikoto {

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

    /**
    * @class Model
    * @brief Represents a 3D model composed of multiple mesh nodes.
    *
    * The `Model` class encapsulates a 3D object, including its mesh data,
    * directory path, name, and vertex/index counts.
    * It provides access to mesh data and metadata about the model.
    */
    class Model final : ReferenceCounted {
    public:
        /**
        * @brief Retrieves the meshes of the model.
        * @return A constant reference to a vector containing the model's meshes.
        */
        MKT_NODISCARD auto GetMeshes() const -> decltype( auto ) { return (m_Meshes); }

        /**
        * @brief Retrieves the meshes of the model.
        * @return A reference to a vector containing the model's meshes.
        */
        MKT_NODISCARD auto GetMeshes() -> decltype( auto ) { return (m_Meshes); }

        /**
        * @brief Retrieves the mesh of the model by index.
        * @return A reference to a mesh.
        */
        MKT_NODISCARD auto GetMeshes(const Size_T index) -> MeshNode& { return m_Meshes.at(index); }

        /**
        * @brief Retrieves the mesh of the model by index.
        * @return A constant reference to a mesh.
        */
        MKT_NODISCARD auto GetMeshes(const Size_T index) const -> const MeshNode& { return m_Meshes.at(index); }

        /**
        * @brief Gets the absolute directory path where the model is stored.
        * @return A constant reference to the model's directory path.
        */
        MKT_NODISCARD auto GetDirectory() const -> const Path_T& { return m_ModelAbsolutePath; }

        /**
        * @brief Retrieves the name of the model.
        * @return A constant reference to the model's name.
        */
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_ModelName; }

        /**
        * @brief Gets the total number of vertices in the model.
        * @return The vertex count.
        */
        MKT_NODISCARD auto GetVertexCount() const -> UInt64_T { return m_TotalVertices; }

        /**
         * @brief Gets the total number of indices in the model.
         * @return The index count.
         */
        MKT_NODISCARD auto GetIndexCount() const -> UInt64_T { return m_TotalIndices; }

        /**
        * @brief Adds a new mesh node to the collection.
        * @tparam Args Variadic template parameters for forwarding constructor arguments.
        * @param index The index at which to insert the mesh node.
        * @param args Arguments to be forwarded to the mesh node constructor.
        *
        * This function inserts a new mesh node into the `m_Meshes` collection at the given index.
        */
        template<typename... Args>
        auto AddMeshNode(UInt32_T index, Args&&... args) -> void {
            m_Meshes.emplace(index, std::forward<Args>(args)...);
        }

    public:
        DISABLE_COPY_AND_MOVE_FOR( Model );

        /**
         * @brief Constructs a Model with the provided parameters.
         * @param modelName Name of the model.
         * @param modelPath Absolute path to the model file.
         */
        explicit Model( std::string modelName, Path_T modelPath)
            : m_ModelName{ std::move( modelName ) },
              m_ModelAbsolutePath{ std::move( modelPath ) }
        {}

    private:

        // Only the factory can construct models
        friend class MeshFactory;

    private:
        auto FreeObject() -> void {

        }

    protected:
        std::string m_ModelName{};
        Path_T m_ModelAbsolutePath{};

        // ( Mesh index, mesh node )
        ankerl::unordered_dense::map<UInt32_T, MeshNode> m_Meshes{};

        UInt64_T m_TotalVertices{};
        UInt64_T m_TotalIndices{};
    };

}// namespace Mikoto

#endif// MIKOTO_MODEL_HH