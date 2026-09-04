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

#ifndef MIKOTO_MAIN_IMPORTER_HH
#define MIKOTO_MAIN_IMPORTER_HH

#include <EASTL/atomic.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>
#include <EASTL/unique_ptr.h>

#include <assimp/mesh.h>
#include <assimp/scene.h>
#include <assimp/IOStream.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/Importer.hpp>
#include <assimp/LogStream.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Assets/Model.hh>
#include <Assets/Importer.hh>

#include <Animation/SkinningBuilder.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::asset {

    class MainImporter final : public ModelImporter {
    public:
        explicit MainImporter( renderer::rhi::IGpuDevice* device );

        auto Import( const ModelLoadDescription& description, ModelDataDescription& out ) -> void override;

    private:
        struct ImporterInfo {
            core::i32 mIndex{ -1 };
            Assimp::Importer mMeshImporter{};
            eastl::atomic_flag mIsFree{ true };

            eastl::unique_ptr<Assimp::IOSystem> mCustomFileHandlingImpl{};

            ImporterInfo() = default;
            ~ImporterInfo() = default;

            // Assimp::Importer copy is forbidden
            ImporterInfo( const ImporterInfo& ) = delete;
            ImporterInfo& operator=( const ImporterInfo& ) = delete;
        };

        struct LoadTextureDescription {
            filesystem::Path mPath{};
            renderer::rhi::TextureHandle mTexture{};
        };

    private:
        auto Import(
            ImporterInfo& loaderData,
            const ModelLoadDescription& description,
            ModelDataDescription& modelData ) -> void;

        auto LoadTextures(
            const filesystem::Path& modelRootPath,
            const aiMesh *mesh,
            const aiScene *scene,
            material::PhysicMaterialDescription& properties ) -> void;

        auto LoadMaterial( aiMaterial const *material,
            material::PhysicMaterialDescription &properties ) -> void;

        auto LoadModelMeshes( const Path& rootPath,
            const aiNode *node, const aiScene *scene,
            const ModelLoadDescription& loadInfo,
            ModelDataDescription& modelData ) -> void;

        auto ConstructMeshNode( const Path& rootPath,
            const aiMesh *mesh, const aiScene *scene,
            MeshNodeDescription& meshNodeData,
            material::PhysicMaterialDescription& material ) -> void;

        auto LoadIndices( const aiMesh *mesh,
            MeshNodeDescription &meshNodeData ) -> void;

        auto LoadVertices( const aiMesh *mesh,
            MeshNodeDescription &meshNodeData ) -> void;

        MKT_NODISCARD auto TryAcquireImporter() ->
            eastl::vector<eastl::unique_ptr<ImporterInfo>>::iterator;

        MKT_NODISCARD auto LoadTexture(
            const Path& modelRootPath,
            const aiMaterial *material, aiTextureType type,
            const aiScene *scene ) -> LoadTextureDescription;

    private:
        eastl::unique_ptr<Assimp::LogStream> mLogImpl{};
        eastl::vector<eastl::unique_ptr<ImporterInfo>> mImporters{};
    };
}// namespace mikoto::asset

#endif // MIKOTO_MAIN_IMPORTER_HH
