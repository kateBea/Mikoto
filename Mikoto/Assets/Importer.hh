//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MIKOTO_IMPORTER_HH
#define MIKOTO_IMPORTER_HH

#include <string>
#include <vector>

#include <Animation/SkinnedAnimation.hh>
#include <Assets/Model.hh>
#include <Library/Utility/Types.hh>
#include <Renderer/Core/GpuDevice.hh>

namespace Mikoto {

    struct VertexData {
        Vec3F Position{};
        Vec3F Normals{};
        Vec3F Colors{};

        Vec2F UV_0{};
        Vec2F UV_1{};

        Vec4F Joints{};
        Vec4F Weights{};
    };

    struct MeshNodeData {
        std::string Name{};

        std::vector<VertexData> Vertices{};
        std::vector<UInt32> Indices{};

        // Unsigned because it needs at least one material
        UInt32 MaterialIndex{};
    };

    struct ModelData {
        std::string Name{};

        std::vector<MeshNodeData> MeshNodes{};
        std::vector<SkinnedAnimation> Animations{};
        std::vector<MaterialProperties> Materials{};

        // Texture URI the same way is stored in the materials
        std::unordered_map<std::string, TextureHandle> Textures{};
    };

    class ModelImporter {
    public:
        explicit ModelImporter(GpuDevice* device)
            : m_Device{ device } {}

        virtual auto Import(const ModelLoadDescription& description, ModelData& out) -> void = 0;

        virtual ~ModelImporter() = default;

    protected:
        GpuDevice* m_Device{ nullptr };
    };

}

#endif//MIKOTO_IMPORTER_HH
