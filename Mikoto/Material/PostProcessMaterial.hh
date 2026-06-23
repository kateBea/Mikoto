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

    struct PostProcessMaterialDescription {
        eastl::string_view mName{};
    };

    // This will be used for effects applied on top of an image
    // a good example is the chromatic aberration effect,
    // takes as input the final HDR shading image
    // the idea is that an effect like this is achieved only by tweaking
    // parameters from this material and apply those properties on the target image
    // See the PostProcessModule in the frame graph
    class PostProcessMaterial final : public Material {
    public:

        explicit PostProcessMaterial( eastl::string_view name = "PostProcessMaterial" );
        explicit PostProcessMaterial( const PostProcessMaterialDescription& desc );


        ~PostProcessMaterial() override;

    private:

        float mContrast{};
        float mSaturation{};
        rhi::Color mTintColor{};
    };

}// namespace Mikoto

#endif// MIKOTO_POST_PROCESS_MATERIAL_HH
