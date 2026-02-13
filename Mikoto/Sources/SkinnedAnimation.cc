//    Copyright 2025 ケイト
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

#include <Animation/SkinnedAnimation.hh>

namespace Mikoto {
    SkinnedAnimation::SkinnedAnimation( NodeHierarchy&& hierarchy, float duration, UInt32 ticksPerSecond )
        : m_Duration{ duration }, m_TicksPerSecond{ ticksPerSecond }, m_RootNode{ std::move( hierarchy ) } {
    }
}
