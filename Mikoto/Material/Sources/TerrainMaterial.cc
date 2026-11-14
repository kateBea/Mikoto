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

#include <Logging/Assert.hh>

#include <Material/TerrainMaterial.hh>

namespace mikoto::material {

    TerrainMaterial::TerrainMaterial( const TerrainMaterial &other ) {

    }
    
    auto TerrainMaterial::Serialize( const filesystem::Path &filename ) const -> void {
    }

    auto TerrainMaterial::Deserialize( const filesystem::Path &filename ) -> void {
    }

    auto TerrainMaterial::Serialize( filesystem::FileHandle file ) const -> void {
    }

    auto TerrainMaterial::Deserialize( filesystem::FileHandle file ) -> void {
    }

    auto TerrainMaterial::SetName( eastl::string_view newName ) -> void {
    }

    auto TerrainMaterial::RemoveLayer( core::usize index ) -> void {
        MKT_ASSERT( index < mLayers.size(), "Index to layer list is out of bounds." );
    }

    auto TerrainMaterial::AddLayer() -> renderer::TerrainLayerHandle {
        return renderer::TerrainLayerHandle::CreateEmpty();
    }

    auto TerrainMaterial::GetLayer( core::usize index ) const -> renderer::TerrainLayerHandle {
        return renderer::TerrainLayerHandle::CreateEmpty();
    }

    auto TerrainMaterial::GetName() const -> const eastl::string & {
        return mName;
    }
}// namespace mikoto::material
