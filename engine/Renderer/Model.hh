/**
* Model.hh
* Created by kate on 6/29/23.
* */

#ifndef KATE_ENGINE_MODEL_HH
#define KATE_ENGINE_MODEL_HH

// C++ Standard Library
#include <filesystem>
#include <string_view>
#include <stdexcept>
#include <cstdint>
#include <string>
#include <vector>

// Third Party Libraries
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Project Libraries
#include <Utility/Common.hh>

#include <Renderer/Material/Texture.hh>
#include <Renderer/Material/Texture2D.hh>
#include <Renderer/Mesh.hh>

namespace Mikoto {
    class Model {
    public:
        explicit Model() = default;

        /**
         * Loads an object model from the given path. If the path is not valid
         * this function raises an exception
         * @param path path to the model to be loaded
         * @param wantLoadTextures tells whether we want to attempt to load this model's textures
         * @throws std::runtime_error if the file does not exist or the path is invalid
         * */
        explicit Model(const Path_T &path, bool wantLoadTextures = false);

        /**
         * Copy constructor disabled. Use the default constructor
         * to load a new Model
         * */
        Model(const Model& other) = delete;

        MKT_NODISCARD auto GetMeshes() const -> const std::vector<Mesh>& { return m_Meshes; }
        MKT_NODISCARD auto GetDirectory() const -> const Path_T& { return m_ModelDirectory; }
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_ModelName; }

        auto LoadFromFile(const Path_T& path, bool wantLoadTextures = false) -> void;

        /**
         * Copy assigment disabled. Use the default constructor
         * to load a new Model
         * */
        auto operator=(const Model& other) -> Model& = delete;

        /**
         * Moves <code>other</code> model to the implicit parameter
         * @param other moved from Model
         * */
        Model(Model&& other) noexcept;

        /**
         * Moves other model to the implicit parameter
         * @param other moved from Model
         * @returns *this
         * */
        auto operator=(Model&& other) noexcept -> Model&;

    private:
        /**
         * Helper function to load model resources from given path
         * @param path path to the model to be loaded
         * @param wantLoadTextures tells whether we want to attempt to load this model's textures
         * @throws std::runtime_error if the file does not exist or the path is invalid
         * */
        auto Load(const Path_T &path, bool wantLoadTextures = false) -> void;


        // ASSIMP INTERFACE HELPER FUNCTIONS

        /**
         * Retrieves each one of the meshes contained within the scene
         * into this Model. This process starts from the given node
         * traversing all of its children nodes
         * @param root contains components of the given scene
         * @param scene represents a complete scene, which contains aiNodes and the associated meshes, materials, etc.
         * @param wantLoadTextures tells whether we want to attempt to load this model's textures
         * */
        auto ProcessNode(aiNode *root, const aiScene *scene, const Path_T &modelDirectory, bool wantLoadTextures) -> void;

        /**
         * Retrieves the components of the Mesh contained within
         * the given node from the given scene
         * @param mesh contains components of the Mesh
         * @param scene represents a complete scene, which contains aiNodes and the associated meshes, materials, etc.
         * @param wantLoadTextures tells whether we want to attempt to load this model's textures
         * @returns mesh containing the retrieved data
         * */
        static auto ProcessMesh(aiMesh *node, const aiScene *scene, const Path_T &modelDirectory, bool wantLoadTextures) -> Mesh;

        /**
         * Retrieves texture materials from the given aiMaterial
         * @param mat container of the materials
         * @param type type of texture to be processed
         * @param tType specifies the type of texture for the <b>kT::Texture</b> object
         * @param scene represents a complete scene, which contains aiNodes and the associated meshes, materials, etc.
         * @returns a list of textures from the given material of type <code>type</code>
         * */
        static auto LoadTextures(aiMaterial *mat, aiTextureType type, Type tType, const aiScene *scene, const Path_T& modelDirectory) -> std::vector<std::shared_ptr<Texture2D>>;

    private:
        static inline BufferLayout s_Layout{
                { ShaderDataType::FLOAT3_TYPE, "a_Position" },
                { ShaderDataType::FLOAT3_TYPE, "a_Normal" },
                { ShaderDataType::FLOAT3_TYPE, "a_Color" },
                { ShaderDataType::FLOAT2_TYPE, "a_TextureCoordinates" }
        };

        MKT_NODISCARD static auto GetDefaultBufferLayout() -> const BufferLayout& { return s_Layout; }
        static auto SetDefaultBufferLayout(const BufferLayout& layout) -> void { s_Layout = layout; }

    private:
        Path_T m_ModelDirectory{};
        std::vector<Mesh> m_Meshes{};
        std::string m_ModelName{};
    };

}

#endif // KATE_ENGINE_MODEL_HH