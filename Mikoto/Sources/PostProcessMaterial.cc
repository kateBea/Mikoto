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

#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Material/Material.hh>
#include <Renderer/Core/Rhi.hh>

#include <Material/PostProcessMaterial.hh>

namespace mikoto::material {

    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    PostProcessMaterial::PostProcessMaterial( eastl::string_view name )
        : Material{ name }
    {}

    PostProcessMaterial::PostProcessMaterial( const PostProcessMaterialDescription &desc )
        : Material{ desc.mName }
    {}

    PostProcessMaterial::~PostProcessMaterial() = default;

}// namespace Mikoto