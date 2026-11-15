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

#ifndef MIKOTO_RENDERER_POSTPROCESS_BLOOM_HH
#define MIKOTO_RENDERER_POSTPROCESS_BLOOM_HH

#include <Core/Types.hh>

namespace mikoto::renderer {

    struct BloomSettings {
        core::f32 mThreshold{ 1.0f };
        core::f32 mIntensity{ 0.25f };
        core::f32 mScatter{ 0.85f };
        core::f32 mRadius{ 1.0f };

        bool mEnabled{ true };
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_POSTPROCESS_BLOOM_HH
