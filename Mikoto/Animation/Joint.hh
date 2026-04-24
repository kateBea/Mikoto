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

#ifndef MIKOTO_JOINT_HH
#define MIKOTO_JOINT_HH

#include <string>

#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

namespace mikoto::animation {

    using namespace mikoto::core;

    // Maps vertex and the weight contribution of the joint to that vertex
    using JointVertexMap = 
        ankerl::unordered_dense::map<std::string, ankerl::unordered_dense::map<u64, float>>;

    inline constexpr i32 kInvalidJointID{ -1 };

    class Joint final {
    public:
        Joint( const eastl::string& name, i32 ID );

        auto SetParentID( i32 ID ) -> void;

        MKT_NODISCARD auto GetID() const -> i32;
        MKT_NODISCARD auto GetParentID() const -> i32;
        MKT_NODISCARD auto GetBoneName() const -> const eastl::string&;

        // [DEBUG]
        auto PrintBoneInfo() const -> void;
        auto SetWeights( eastl::string_view meshName, u64 vertex, float weight ) -> void;

    private:
        eastl::string mName{};

        i32 mID{ kInvalidJointID };
        i32 mParentID{ kInvalidJointID };

        JointVertexMap mVertexWeights{};
    };
}

#endif//MIKOTO_JOINT_HH