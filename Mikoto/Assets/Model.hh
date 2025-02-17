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
#include <assimp/scene.h>

#include <assimp/Importer.hpp>

// Project Libraries
#include <Assets/Mesh.hh>
#include <Common/Common.hh>
#include <Material/Texture/Texture2D.hh>

namespace Mikoto {
    struct ModelLoadInfo {
        Path_T Path{};
        bool InvertedY{};// Y down (for vulkan)
        bool WantTextures{ true };
    };

    class Model final {
    public:
        /**
         * Default constructor for models.
         * */
        explicit Model() = default;


        /**
         * Loads an object model from the given path. If the path is not valid this function raises an exception
         * @param info Information to load model, see definition of ModelLoadInfo
         * @throws std::runtime_error if the file does not exist or the path is invalid
         * */
        explicit Model( const ModelLoadInfo &info );


        /**
         * Returns the list of meshes from this model
         * @returns non-mutable list of meshes
         * */
        MKT_NODISCARD auto GetMeshes() const -> const std::vector<Scope_T<Mesh>> & { return m_Meshes; }


        /**
         * Returns the list of meshes from this model
         * @returns mutable list of meshes
         * */
        MKT_NODISCARD auto GetMeshes() -> std::vector<Scope_T<Mesh>> & { return m_Meshes; }


        /**
         * Returns the absolute path to the directory where this model is located (does not include the model's name)
         * @returns absolute path to this model's directory
         * */
        MKT_NODISCARD auto GetDirectory() const -> const Path_T & { return m_ModelAbsolutePath; }


        /**
         * Returns the name of this model
         * @returns this model's name
         * */
        MKT_NODISCARD auto GetName() const -> const std::string & { return m_ModelName; }


        /**
         * Returns the name of this model
         * @returns this model's name
         * */
        MKT_NODISCARD MKT_UNUSED_FUNC auto GetVertexCount() const -> UInt64_T { return m_TotalVertices; }


        /**
         * Returns the name of this model
         * @returns this model's name
         * */
        MKT_NODISCARD MKT_UNUSED_FUNC auto GetIndexCount() const -> UInt64_T { return m_TotalIndices; }


        /**
         * Moves <code>other</code> model to the implicit parameter
         * @param other moved from Model
         * */
        Model( Model &&other ) noexcept;


        /**
         * Moves other model to the implicit parameter
         * @param other moved from Model
         * @returns *this
         * */
        auto operator=( Model &&other ) noexcept -> Model &;


    public:
        DELETE_COPY_FOR( Model );

    private:
        /**
         * Helper function to load model resources from given path
         * @param wantLoadTextures tells whether we want to attempt to load this model's textures
         * @throws std::runtime_error if the file does not exist or the path is invalid
         * */
        auto Load( bool wantLoadTextures = false ) -> void;


        /**
         * Retrieves each one of the meshes contained within the scene
         * into this Model. This process starts from the given node
         * traversing all of its children nodes
         * @param root contains components of the given scene
         * @param scene represents a complete scene, which contains aiNodes and the associated meshes, materials, etc.
         * @param modelDirectory The model's directory
         * @param wantLoadTextures tells whether we want to attempt to load this model's textures
         * */
        auto ProcessNode( const aiNode *root, const aiScene *scene, const Path_T &modelDirectory, bool wantLoadTextures ) -> void;


        /**
         * Retrieves the components of the Mesh contained within
         * the given node from the given scene
         * @param node A mesh node (for usage with Assimp)
         * @param scene Represents a complete scene, which contains aiNodes and the associated meshes, materials, etc.
         * @param modelDirectory The model's directory
         * @param wantLoadTextures Tells whether we want to attempt to load this model's textures
         * @returns mesh containing the retrieved data
         * */
        auto ProcessMesh( const aiMesh *node, const aiScene *scene, const Path_T &modelDirectory, bool wantLoadTextures ) const -> Scope_T<Mesh>;


        /**
         * Retrieves texture materials from the given aiMaterial
         * @param mat Container of the materials
         * @param type Type of texture to be processed
         * @param tType Specifies the type of texture for the <b>kT::Texture</b> object
         * @param scene Represents a complete scene, which contains aiNodes and the associated meshes, materials, etc.
         * @param modelDirectory The model's directory
         * @returns A list of textures from the given material of type
         * */
        static auto LoadTextures( const aiMaterial *mat, aiTextureType type, MapType tType, const aiScene *scene, const Path_T &modelDirectory ) -> std::vector<Scope_T<Texture2D>>;

    protected:
        Path_T m_ModelAbsolutePath{};
        std::vector<Scope_T<Mesh>> m_Meshes{};
        std::string m_ModelName{};

        UInt64_T m_TotalVertices{};
        UInt64_T m_TotalIndices{};

        /** Y points Down (suits vulkan coordinate system) */
        bool m_InvertedY{};
    };
}// namespace Mikoto

#endif// MIKOTO_MODEL_HH