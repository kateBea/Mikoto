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

#ifndef MIKOTO_MODEL_HH
#define MIKOTO_MODEL_HH

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Material/PBRMaterial.hh>
#include <Material/Texture2D.hh>
#include <Renderer/Core/Buffer.hh>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "Animation/SkinnedAnimation.hh"

namespace Mikoto {

    enum class VertexAttribute {
        POSITIONS,
        NORMALS,
        TANGENTS,
        BITANGENTS,
        UV0,
        UV1,
        COLORS,
        JOINTS,
        WEIGHTS,

        CUSTOM,
    };

    struct ModelLoadDescription {
        const File* ModelFile{};
        bool WantTextures{ true };

        GraphicsAPI TargetAPI{ GraphicsAPI::UNKNOWN };

        // Specifies the order we want attributes in
        // Default is order specified by DEFAULT_VERTEX_BUFFER_LAYOUT in Pipeline.hh file
        std::vector<VertexAttribute> Attributes{
            VertexAttribute::POSITIONS,
            VertexAttribute::NORMALS,
            VertexAttribute::COLORS,
            VertexAttribute::UV0,
            VertexAttribute::UV1,

            VertexAttribute::JOINTS, // Bone IDs
            VertexAttribute::WEIGHTS, // Weight IDs
        };

        auto LoadTextures( bool value ) -> ModelLoadDescription&;
        auto WithFilePath( const File* file ) -> ModelLoadDescription&;
    };

    class MeshNode final {
    public:
        explicit MeshNode( UInt32 index,
            BufferHandle vertices,
            BufferHandle indices, std::string_view name,
            MaterialProperties&& properties );

        MeshNode(MeshNode&& other) noexcept = default;

        MKT_NODISCARD auto GetName() -> const std::string& { return m_Name; }

        MKT_NODISCARD auto GetMeshIndex() const -> Size { return m_MeshIndex; }
        MKT_NODISCARD auto GetVertexBuffer() -> BufferHandle { return  m_Vertices; }
        MKT_NODISCARD auto GetIndexBuffer() -> BufferHandle { return m_Indices; }

        MKT_NODISCARD auto GetVertexBuffer() const -> BufferHandle { return m_Vertices; }
        MKT_NODISCARD auto GetIndexBuffer() const -> BufferHandle { return m_Indices; }

        MKT_NODISCARD auto GetProperties() const -> const MaterialProperties& { return m_Properties; }

        DISABLE_COPY_FOR( MeshNode );

    private:
        Size m_MeshIndex{};

        std::string m_Name{};

        BufferHandle m_Vertices{};
        BufferHandle m_Indices{};

        MaterialProperties m_Properties{};
    };

    using AnimationMap = ankerl::unordered_dense::map<std::string, SkinnedAnimation>;

    class Model final : public ReferenceCounted {
    public:
        MKT_NODISCARD auto GetMeshNodeCount() const -> Size { return m_Meshes.size(); }

        MKT_NODISCARD auto GetMeshNode(const Size index) -> MeshNode& { return m_Meshes.at(index); }

        MKT_NODISCARD auto GetMeshNode(const Size index) const -> const MeshNode& { return m_Meshes.at(index); }

        MKT_NODISCARD auto GetName() const -> const std::string& { return m_ModelName; }
        MKT_NODISCARD auto GetDirectory() const -> const Path& { return m_ModelAbsolutePath; }

        MKT_NODISCARD auto IsSkinned() const -> bool;
        MKT_NODISCARD auto HasAnimations() const -> bool;

        MKT_NODISCARD auto GetSkeleton() const -> const Skeleton&;

        MKT_NODISCARD auto FindAnimation( std::string_view name ) -> SkinnedAnimation*;
        MKT_NODISCARD auto FindAnimation( std::string_view name ) const -> const SkinnedAnimation*;

        MKT_NODISCARD auto GetAnimations() -> AnimationMap&;
        MKT_NODISCARD auto GetAnimations() const -> const AnimationMap&;

        template<typename... Args>
        auto PushMeshNode(UInt32 index, Args&&... args) -> void {
            m_Meshes.emplace(index, std::forward<Args>(args)...);
        }

        auto SetAnimations(AnimationMap&& animations ) -> void;

        ~Model() override = default;

    private:
        DISABLE_COPY_AND_MOVE_FOR( Model );

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

        Skeleton m_Skeleton{};
        AnimationMap m_Animations{};
    };

    using ModelHandle = Ref<Model>;

}

#endif// MIKOTO_MODEL_HH