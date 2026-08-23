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

#ifndef MIKOTOROOT_TONEMAP_MODULE_HH
#define MIKOTOROOT_TONEMAP_MODULE_HH

#include <EASTL/fixed_vector.h>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Renderer/Core/FrameGraph.hh>
#include <Renderer/Core/CommandContext.hh>

namespace mikoto::renderer {

    // Index order matches shader
    // See base/Tonemap_Helpers.slang
    enum class ToneMappingType {
        Aces,
        Linear,
        Reinhard,
        Uncharted2,
        Khronos_Neutral,
        Max_Count,
    };

    class TonemapModule {
    public:
        explicit TonemapModule( rhi::RenderResolution resolution );

        auto RegisterPasses( FrameGraph& graph ) -> void;

        // Tonemap
        auto SetToneMapping( ToneMappingType type ) -> void;

    private:
        auto RegisterTonemapPass( FrameGraph& graph ) -> void;
        auto RegisterColorGradientPass( FrameGraph& graph ) -> void;

    private:
        rhi::RenderResolution mResolution{ rhi::RenderResolution::e1080P };
        ToneMappingType mToneMapType{ ToneMappingType::Aces };
    };

}// namespace mikoto

#endif//MIKOTOROOT_TONEMAP_MODULE_HH
