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

#ifndef MIKOTO_MATERIAL_UTILITY_HH
#define MIKOTO_MATERIAL_UTILITY_HH

#include <yaml-cpp/yaml.h>

#include <Filesystem/Path.hh>
#include <Filesystem/File.hh>

#include <Material/Material.hh>
#include <Material/SurfaceMaterial.hh>

namespace mikoto::material {

    /**
     * @brief Creates the file's material type and loads it through ISerializable.
     * @param file Material file whose cached contents should be read.
     * @return The deserialized material.
     */
    auto LoadMaterial( filesystem::FileHandle file ) -> MaterialHandle;

    /**
     * @brief Returns the material's last successfully saved or loaded file.
     * @param material Material whose source is queried.
     * @return An empty path for a new or cloned material.
     */
    auto GetMaterialSourcePath( const IMaterial& material ) -> const filesystem::Path&;

    /**
     * @brief Allocates a material through its concrete copy constructor without retaining the source file.
     * @param material Material to copy.
     * @return An independent material with no assigned file path.
     */
    auto CloneMaterial( const IMaterial& material ) -> MaterialHandle;

}// namespace mikoto::material

#endif// MIKOTO_MATERIAL_UTILITY_HH
