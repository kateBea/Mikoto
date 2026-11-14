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

#include <EASTL/string.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Logging/Assert.hh>

#include <Animation/Joint.hh>

namespace mikoto::animation {

    Joint::Joint( const eastl::string &name, core::i32 ID )
        : mName{ name },
          mID{ ID } {
        MKT_BEGIN_PROFILER();
        MKT_ASSERT( mID != kInvalidJointID, "Not a valid Joint ID" );
    }

    auto Joint::SetParentID( core::i32 ID ) -> void {
        MKT_BEGIN_PROFILER();
        mParentID = ID;
    }

    auto Joint::GetID() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mID;
    }

    auto Joint::GetParentID() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mParentID;
    }

    auto Joint::GetBoneName() const -> const eastl::string & {
        MKT_BEGIN_PROFILER();
        return mName;
    }
}// namespace mikoto::animation