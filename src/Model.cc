// C++ Standard Library
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

// Third Party Libraries
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// Project Headers
#include <Utility/Common.hh>

#include <Renderer/Mesh.hh>
#include <Renderer/Model.hh>
#include <Renderer/Buffers/VertexBuffer.hh>
#include <Renderer/Buffers/IndexBuffer.hh>
#include <Renderer/Material/Texture.hh>

namespace kaTe {
    Model::Model(const Path_T &path, bool wantLoadTextures)
        :   m_ModelDirectory{ path.string().substr(0,  path.string().find_last_of('/')) }
        ,   m_ModelName{ path.string().substr(path.string().find_last_of('/'),  path.string().size() - 1) }
    {
        Load(path, wantLoadTextures);
    }

    auto Model::LoadFromFile(const Path_T &path, bool wantLoadTextures) -> void {
        m_ModelDirectory = path.string().substr(0,  path.string().find_last_of('/'));
        m_ModelName = path.string().substr(path.string().find_last_of('/'),  path.string().size() - 1);
        Load(path, wantLoadTextures);
    }

    auto Model::Load(const Path_T& path, bool wantLoadTextures) -> void {
        if (!path.has_filename())
            throw std::runtime_error("Not valid path for model object");

        auto fileDir{ GetByteChar(path) };

        Assimp::Importer importer{};

        // See more postprocessing options: https://assimp.sourceforge.net/lib_html/postprocess_8h.html
        unsigned int importerFlags{ aiProcess_Triangulate |
                                   aiProcess_FlipUVs |
                                   aiProcess_GenSmoothNormals |
                                   aiProcess_JoinIdenticalVertices };

        auto scene = importer.ReadFile(fileDir.data(), importerFlags);

        if((scene == nullptr) || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || (scene->mRootNode == nullptr))
            throw std::runtime_error(importer.GetErrorString());

        m_Meshes.reserve(scene->mRootNode->mNumMeshes);

        // Contains the model's directory. Since substr will not engine the ast character in the range,
        // this variable does not engine the last slash of the path string
        const Path_T modelDirectory{ path.string().substr(0,  path.string().find_last_of('/')) };
        ProcessNode(scene->mRootNode, scene, modelDirectory, wantLoadTextures);
    }

    auto Model::ProcessNode(aiNode *root, const aiScene *scene, const Path_T &modelDirectory, bool wantLoadTextures) -> void {
        // Process all the meshes from this node
        for(std::size_t i{}; i < root->mNumMeshes; i++)
            m_Meshes.emplace_back(ProcessMesh(scene->mMeshes[root->mMeshes[i]], scene, modelDirectory, false));

        // then do the same for each of its children
        for(std::size_t i {}; i < root->mNumChildren; i++)
            ProcessNode(root->mChildren[i], scene, modelDirectory, false);
    }

    auto Model::ProcessMesh(aiMesh *mesh, const aiScene *scene, const Path_T &modelDirectory, bool wantLoadTextures) -> Mesh {
        std::vector<float> vertices{};
        std::vector<UInt32_T> indices{};
        std::vector<std::shared_ptr<Texture>> textures{};

        for(UInt64_T index{}; index < mesh->mNumVertices; index++) {
            vertices.push_back(mesh->mVertices[index].x);
            vertices.push_back(mesh->mVertices[index].y);
            vertices.push_back(mesh->mVertices[index].z);

            if (mesh->HasNormals()) {
                vertices.push_back(mesh->mNormals[index].x);
                vertices.push_back(mesh->mNormals[index].y);
                vertices.push_back(mesh->mNormals[index].z);
            }

            if (mesh->mTextureCoords[0] != nullptr) {
                vertices.push_back(mesh->mTextureCoords[0][index].x);
                vertices.push_back(mesh->mTextureCoords[0][index].y);
            }
        }

        // Retrieve mesh indices
        for(UInt64_T i{}; i < mesh->mNumFaces; i++) {
            auto face{ mesh->mFaces[i] };

            for(UInt64_T index{}; index < face.mNumIndices; index++)
                indices.emplace_back(face.mIndices[index]);
        }

        // process material
        if(mesh->mMaterialIndex >= 0) {
            auto material { scene->mMaterials[mesh->mMaterialIndex] };

            if (wantLoadTextures) {
                auto diffuseMaps { LoadTextures(material, aiTextureType_DIFFUSE, Texture2D::Type::DIFFUSE, scene, modelDirectory) };
                auto specularMaps { LoadTextures(material, aiTextureType_SPECULAR, Texture2D::Type::SPECULAR, scene, modelDirectory) };
                auto normalMaps { LoadTextures(material, aiTextureType_NORMALS, Texture2D::Type::NORMAL, scene, modelDirectory) };

                for (auto& item : diffuseMaps)
                    textures.push_back(std::move(item));

                for (auto& item : specularMaps)
                    textures.push_back(std::move(item));

                for (auto& item : normalMaps)
                    textures.push_back(std::move(item));
            }
        }

        MeshData meshData{};

        // Setup mesh data structure from previously retrieved data
        meshData.SetVertices(VertexBuffer::CreateBuffer(vertices));
        meshData.SetIndices(IndexBuffer::CreateBuffer(indices));

        meshData.SetTextures(std::move(textures));
        return Mesh{ meshData };
    }

    auto Model::LoadTextures(aiMaterial *mat, aiTextureType type, Texture2D::Type tType, const aiScene *scene, const Path_T& modelDirectory) -> std::vector<std::shared_ptr<Texture>> {
        std::vector<std::shared_ptr<Texture>> textures{};
        for(std::uint32_t i{}; i < mat->GetTextureCount(type); i++) {
            aiString str{};
            if (mat->GetTexture(type, i, &str) == AI_SUCCESS)
                // TODO: temporary, assumes the textures are in the same directory as the model file
                textures.push_back(Texture2D::LoadFromFile(modelDirectory.string() + '/' + str.C_Str(), tType));
        }

        return textures;
    }

    Model::Model(Model &&other) noexcept
        :   m_Meshes{ std::move(other.m_Meshes) }
        ,   m_ModelDirectory{ std::move(other.m_ModelDirectory) }
        ,   m_ModelName{ std::move(other.m_ModelName) }
    {

    }

    auto Model::operator=(Model&& other) noexcept -> Model& {
        m_Meshes = std::move(other.m_Meshes);
        m_ModelDirectory = std::move(other.m_ModelDirectory);
        m_ModelName = std::move(other.m_ModelName);

        return *this;
    }
}