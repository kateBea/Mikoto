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

#ifndef MIKOTO_MATERIAL_TERRAIN_MATERIAL_HH
#define MIKOTO_MATERIAL_TERRAIN_MATERIAL_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>

#include <Material/Material.hh>

#include <Renderer/Core/TerrainGeneration.hh>

namespace mikoto::material {

    class TerrainMaterial final : public IMaterial {
    public:
        TerrainMaterial( const TerrainMaterial& other);

        auto Deserialize( const filesystem::Path &filename ) -> void override;
        auto Serialize( const filesystem::Path &filename ) const -> void override;

        auto Deserialize( filesystem::FileHandle file ) -> void override;
        auto Serialize( filesystem::FileHandle file ) const -> void override;

        auto SetName( eastl::string_view newName ) -> void override;

        auto RemoveLayer( core::usize index ) -> void;

        MKT_NODISCARD auto AddLayer( /**/ ) -> renderer::TerrainLayerHandle;
        MKT_NODISCARD auto GetLayer( core::usize index ) const -> renderer::TerrainLayerHandle;

        MKT_NODISCARD auto GetName() const -> const eastl::string & override;

    private:
        eastl::string mName{};
        eastl::fixed_vector<renderer::TerrainLayerHandle, 5> mLayers{};
    };
}
#endif//MIKOTO_MATERIAL_TERRAIN_MATERIAL_HH
