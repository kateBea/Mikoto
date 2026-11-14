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

#include <EASTL/utility.h>
#include <EASTL/string.h>

#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Animation/SkinnedAnimation.hh>

namespace mikoto::animation {
    SkinnedAnimation::SkinnedAnimation( ozz::unique_ptr<ozz::animation::Animation>&& data )
        : mName{ data ? data->name() : "" },
          mDuration{ data ? data->duration() : 0.0f },
          mAnimation{ eastl::move( data ) } {
    }

    auto SkinnedAnimation::GetDuration() const -> float {
        MKT_BEGIN_PROFILER();
        return mDuration;
    }

    auto SkinnedAnimation::GetName() const -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mName;
    }

    auto SkinnedAnimation::GetOzzAnimation() -> ozz::animation::Animation* {
        MKT_BEGIN_PROFILER();
        return mAnimation.get();
    }
}// namespace mikoto::animation
