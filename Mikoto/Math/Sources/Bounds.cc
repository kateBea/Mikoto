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

#include <glm/glm.hpp>

#include <Math/Bounds.hh>

namespace mikoto::math {
    auto Bounds::Include( const core::float3& point ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mIsValid ) {
            mMinimum = point;
            mMaximum = point;
            mIsValid = true;
        }
        else {
            mMinimum = glm::min( mMinimum, point );
            mMaximum = glm::max( mMaximum, point );
        }
    }

    auto Bounds::HasExtent() const -> bool {
        MKT_BEGIN_PROFILER();

        return mIsValid && glm::any( glm::greaterThan( mMaximum, mMinimum ) );
    }
}
