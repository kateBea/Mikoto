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

#ifndef MIKOTO_JOINT_SKELETON_HH
#define MIKOTO_JOINT_SKELETON_HH

#include <EASTL/string.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::animation {

	inline constexpr core::i32 kInvalidJointID{ -1 };

    class Joint final {
    public:
        Joint( const eastl::string &name, core::i32 ID );

        auto SetParentID( core::i32 ID ) -> void;

        MKT_NODISCARD auto GetID() const -> core::i32;
        MKT_NODISCARD auto GetParentID() const -> core::i32;
        MKT_NODISCARD auto GetBoneName() const -> const eastl::string&;

    private:
        eastl::string mName{};

        core::i32 mID{ kInvalidJointID };
        core::i32 mParentID{ kInvalidJointID };
    };
}

#endif