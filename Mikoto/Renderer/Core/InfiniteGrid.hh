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

#ifndef MIKOTO_RENDERER_CORE_INFINITE_GRID_HH
#define MIKOTO_RENDERER_CORE_INFINITE_GRID_HH

#include <Core/Types.hh>

namespace mikoto::renderer {

    struct InfiniteGridSettings {
        core::f32 mCellSize{ 1.0f };
        core::f32 mOpacity{ 1.0f };
        core::f32 mLineWidth{ 1.0f };
        core::f32 mAxisWidth{ 1.5f };

        bool mShowXAxis{ true };
        bool mShowYAxis{};
        bool mShowZAxis{ true };

        auto operator==( const InfiniteGridSettings & ) const -> bool = default;
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CORE_INFINITE_GRID_HH
