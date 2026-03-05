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

#ifndef MIKOTO_BONE_HH
#define MIKOTO_BONE_HH

#include <vector>
#include <string>

#include <ankerl/unordered_dense.h>

#include <Library/Utility/Types.hh>

namespace Mikoto {

    // Maps vertex and the weight contribution of the joint to that vertex
    using JointVertexMap = 
        ankerl::unordered_dense::map<std::string, ankerl::unordered_dense::map<UInt64, float>>;

    inline constexpr Int32 INVALID_JOINT_ID{ -1 };

    class Joint final {
    public:
        Joint( const std::string& name, Int32 ID );

        auto SetParentID( Int32 ID ) -> void;

        MKT_NODISCARD auto GetID() const -> Int32;
        MKT_NODISCARD auto GetParentID() const -> Int32;
        MKT_NODISCARD auto GetBoneName() const -> const std::string&;

        // [DEBUG]
        auto PrintBoneInfo() const -> void;
        auto SetWeights( std::string_view meshName, UInt64 vertex, float weight ) -> void;

    private:
        std::string m_Name{};

        Int32 m_ID{ INVALID_JOINT_ID };
        Int32 m_ParentID{ INVALID_JOINT_ID };

        JointVertexMap m_VertexWeights{};
    };
}

#endif//MIKOTO_BONE_HH