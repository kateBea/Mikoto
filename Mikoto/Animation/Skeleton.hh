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

#ifndef MIKOTO_SKELETON_HH
#define MIKOTO_SKELETON_HH

#include <string_view>

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Animation/Joint.hh>

namespace Mikoto {

    using JointsMap = ankerl::unordered_dense::map<std::string, Joint>;
    using JointsMapID = ankerl::unordered_dense::map<UInt32, std::string>;
    class Skeleton {
    public:
        explicit Skeleton() = default;

        auto RegisterJoint( const std::string& name, Int32 ID, Mat4F ModelToBoneTransform ) -> void;

        auto GetBoneMap() -> JointsMap&;
        MKT_NODISCARD auto GetBoneMap() const -> const JointsMap&;

        MKT_NODISCARD auto HasJoint( std::string_view name ) const -> bool;
        MKT_NODISCARD auto FindJoint( std::string_view name ) -> Joint*;
        MKT_NODISCARD auto FindJointByID( UInt32 ID ) -> Joint*;

        auto GetBoneCount() const -> UInt32;

        auto SetBoneMap(JointsMap&& boneMap ) -> void;

    private:
        JointsMap m_Joints{};
        JointsMapID m_JointsByID{};
    };
}

#endif // MIKOTO_SKELETON_HH
