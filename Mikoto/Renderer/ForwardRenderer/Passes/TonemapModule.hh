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

#ifndef MIKOTO_RENDERER_FORWARD_RENDERER_TONEMAP_MODULE_HH
#define MIKOTO_RENDERER_FORWARD_RENDERER_TONEMAP_MODULE_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Core/FrameGraph.hh>

namespace mikoto::renderer {

    enum class ToneMapper : core::i32 {
        eAces,
        eLinear,
        eReinhard,
        eUncharted2,
        eKhronos_Neutral,
        eCount,
    };

    class TonemapModule {
    public:
        explicit TonemapModule( rhi::RenderResolution resolution );

        auto RegisterColorGradientPass( FrameGraph& graph ) -> void;
        auto RegisterTonemapPass( FrameGraph& graph ) -> void;

        auto SetToneMapping( ToneMapper type ) -> void;
        auto SetExposure( core::f32 exposure ) -> void;
        auto SetGamma( core::f32 gamma ) -> void;

    private:
        ToneMapper mToneMapType{ ToneMapper::eAces };
        core::f32 mExposure{ 1.0f };
        core::f32 mGamma{ 1.0f };
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_FORWARD_RENDERER_TONEMAP_MODULE_HH

