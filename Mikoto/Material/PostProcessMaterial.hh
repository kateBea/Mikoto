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

#ifndef MIKOTO_POST_PROCESS_MATERIAL_HH
#define MIKOTO_POST_PROCESS_MATERIAL_HH

#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Material/Material.hh>
#include <Renderer/Core/Rhi.hh>

namespace mikoto::material {

    using namespace mikoto::renderer;

    // This will be used for effects applied on top of an image, usually the final image but we can specify the target image this is applied on
    // a good example is the chromatic aberration effect, takes as input the final HDR shading image
    class PostProcessMaterial final : public Material {
    public:

        explicit PostProcessMaterial( eastl::string_view name = "PostProcessMaterial" );

        auto SetTargetImage(rhi::TextureHandle handle) -> void;

        MKT_NODISCARD auto GetTargetImage() -> rhi::TextureHandle;

        ~PostProcessMaterial() override;

    private:
        // Post process is applied on top of an image
        rhi::TextureHandle mTargetTexture{};
    };

}// namespace Mikoto

#endif// MIKOTO_POST_PROCESS_MATERIAL_HH
