//
// Created by kate on 1/3/25.
//

#ifndef GAMEOBJECTDATA_HH
#define GAMEOBJECTDATA_HH

#include <Material/Core/Material.hh>
#include <Assets/Mesh.hh>
#include <Assets/Model.hh>
#include <Models/TransformData.hh>

namespace Mikoto {
    struct MaterialInfo {
        glm::vec4 Color{ };
        std::shared_ptr<Material> MeshMat{ nullptr };
    };

    struct MeshInfo {
        const Mesh* Data;
        const Model* ModelSource;
    };

    struct GameObject {
        Path_T ModelPath{};
        std::string ModelName{};
        TransformData Transform{};
        MeshInfo MeshData{};
    };
}
#endif //GAMEOBJECTDATA_HH
