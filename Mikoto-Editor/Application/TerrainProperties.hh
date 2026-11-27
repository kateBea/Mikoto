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

#ifndef MIKOTO_EDITOR_APPLICATION_TERRAIN_PROPERTIES_HH
#define MIKOTO_EDITOR_APPLICATION_TERRAIN_PROPERTIES_HH

#include <EASTL/array.h>

#include <Scene/Component.hh>

namespace mikoto::editor {

    /** Shared surface-only editor; both panels edit the same landscape and brush layer. */
    auto DrawTerrainSurfaceLayers( scene::LandscapeComponent &landscape, core::u32 &selectedLayer,
        eastl::array<asset::AssetHandle<asset::Texture>, 3> &previews ) -> void;

}// namespace mikoto::editor

#endif// MIKOTO_EDITOR_APPLICATION_TERRAIN_PROPERTIES_HH
