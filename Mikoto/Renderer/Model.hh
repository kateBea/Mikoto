/**
 * Model.hh
 * Created by kate on 6/29/23.
 * */

#ifndef MIKOTO_MODEL_HH
#define MIKOTO_MODEL_HH

// C++ Standard Library
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Third Party Libraries
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <assimp/Importer.hpp>

// Project Libraries
#include <Common/Common.hh>
#include <Renderer/Mesh.hh>
#include <Renderer/Material/Texture2D.hh>

namespace Mikoto {
    struct ModelLoadInfo {
        Path_T ModelPath{};
        bool InvertedY{};// Y down (for vulkan)
        bool WantTextures{ true };
    };

    class Model {
    public:
        explicit Model() = default;

        /**
         * Loads an object model from the given path. If the path is not valid this function raises an exception
         * @param path path to the model to be loaded
         * @param wantLoadTextures tells whether we want to attempt to load this model's textures
         * @throws std::runtime_error if the file does not exist or the path is invalid
         * */
        explicit Model( const ModelLoadInfo &info );

        /**
         * Returns the list of meshes from this model
         * @returns list of meshes
         * */
        MKT_NODISCARD auto GetMeshes() const -> const std::vector<Mesh> & { return m_Meshes; }

        MKT_NODISCARD auto GetMeshes() -> std::vector<Mesh> & { return m_Meshes; }

        /**
         * Returns the absolute path to the directory where this model is located (doe not include the model's name)
         * @returns absolute path to this model's directory
         * */
        MKT_NODISCARD auto GetDirectory() const -> const Path_T & { return m_ModelDirectory; }

        /**
         * Returns the name of this model
         * @returns this model's name
         * */
        MKT_NODISCARD auto GetName() const -> const std::string & { return m_ModelName; }


        MKT_NODISCARD MKT_UNUSED_FUNC auto GetVertexCount() const -> UInt64_T { return m_TotalVertices; }
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

        /**
         * Adds the given mesh to the list of meshes of this model.
         * This function is mainly only for the sprite atm xd, will be deprecated some time soon and finally deleted. See assets manager loading prefabs
         * @returns mesh to be added
         * */
        auto AddMesh( MeshData &&meshData ) -> void { m_Meshes.emplace_back( std::move( meshData ) ); }

    public:
        DELETE_COPY_FOR( Model );

    private:
        /**
         * Helper function to load model resources from given path
         * @param path path to the model to be loaded
         * @param wantLoadTextures tells whether we want to attempt to load this model's textures
         * @throws std::runtime_error if the file does not exist or the path is invalid
         * */
        auto Load( const Path_T &path, bool wantLoadTextures = false ) -> void;

        /**
         * Retrieves each one of the meshes contained within the scene
         * into this Model. This process starts from the given node
         * traversing all of its children nodes
         * @param root contains components of the given scene
         * @param scene represents a complete scene, which contains aiNodes and the associated meshes, materials, etc.
         * @param wantLoadTextures tells whether we want to attempt to load this model's textures
         * */
        auto ProcessNode( aiNode *root, const aiScene *scene, const Path_T &modelDirectory, bool wantLoadTextures ) -> void;

        /**
         * Retrieves the components of the Mesh contained within
         * the given node from the given scene
         * @param mesh contains components of the Mesh
         * @param scene represents a complete scene, which contains aiNodes and the associated meshes, materials, etc.
         * @param wantLoadTextures tells whether we want to attempt to load this model's textures
         * @returns mesh containing the retrieved data
         * */
        auto ProcessMesh( aiMesh *node, const aiScene *scene, const Path_T &modelDirectory, bool wantLoadTextures ) -> Mesh;

        /**
         * Retrieves texture materials from the given aiMaterial
         * @param mat container of the materials
         * @param type type of texture to be processed
         * @param tType specifies the type of texture for the <b>kT::Texture</b> object
         * @param scene represents a complete scene, which contains aiNodes and the associated meshes, materials, etc.
         * @returns a list of textures from the given material of type <code>type</code>
         * */
        static auto LoadTextures( aiMaterial *mat, aiTextureType type, MapType tType, const aiScene *scene, const Path_T &modelDirectory ) -> std::vector<std::shared_ptr<Texture2D>>;

    protected:
        Path_T m_ModelDirectory{};
        std::vector<Mesh> m_Meshes{};
        std::string m_ModelName{};

        UInt64_T m_TotalVertices{};
        UInt64_T m_TotalIndices{};

        bool m_InvertedY{};// Y points Down (suits vulkan coordinate system)
    };
}

#endif// MIKOTO_MODEL_HH