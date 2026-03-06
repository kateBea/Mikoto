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

#include <string>
#include <string_view>
#include <vector>

#include <ankerl/unordered_dense.h>

#include <ozz/base/memory/unique_ptr.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>

#include <Common/Common.hh>
#include <Animation/Joint.hh>

namespace Mikoto {

    // Represents the hierarchy of a scene
    struct Node {
        Int32 JointID{ -1 };
        std::string Name{};

        // Bind pose transform
        Mat4F Transformation{};

        // The same info but split
        Vec3F Scale{ 1.0f };
        Vec3F Translation{ 0.0f };
        Quat Rotation{ 1, 0, 0, 0 };

        std::vector<Node> Children{};
    };

    // Hierarchy of nodes that are bones
    struct ArmatureNode {

    };

    // These 2 are vec4 because they need to match the bone influence
    // which is maximum bones influence a vertex ( from what we support now )
    inline constexpr UInt32 MAX_BONE_INFLUENCE{ 4 };
    inline constexpr UInt32 MAX_BONES_PER_MESH{ 128 };
    inline constexpr UInt32 MAX_SKINNED_MESHES{ 1000 };

    using JointsMap = ankerl::unordered_dense::map<std::string, Joint>;
    using JointsMapID = ankerl::unordered_dense::map<UInt32, std::string>;

    using JointsMapIterator = ankerl::unordered_dense::map<std::string, Joint>::iterator;
    using JointsMapConstIterator = ankerl::unordered_dense::map<std::string, Joint>::const_iterator;

    class Skeleton {
    public:
        explicit Skeleton() = default;

        auto RegisterJoint( const std::string& name, Int32 ID ) -> void;

        auto GetBoneMap() -> JointsMap&;
        MKT_NODISCARD auto GetBoneMap() const -> const JointsMap&;

        MKT_NODISCARD auto HasJoint( std::string_view name ) const -> bool;

        MKT_NODISCARD auto FindJoint( std::string_view name ) -> Joint*;
        MKT_NODISCARD auto FindJoint( std::string_view name ) const -> const Joint*;

        MKT_NODISCARD auto FindJointByID( UInt32 ID ) -> Joint*;
        MKT_NODISCARD auto FindJointByID( UInt32 ID ) const -> const Joint*;

        MKT_NODISCARD auto GetOzzSkeleton() -> ozz::animation::Skeleton*;
        MKT_NODISCARD auto GetOzzBondeIndex( UInt32 ID ) const -> Int32;
        MKT_NODISCARD auto GetOzzSkeleton() const -> const ozz::animation::Skeleton*;

        MKT_NODISCARD auto GetInverseBindMatrices() const -> const std::vector<Mat4F>&;
        MKT_NODISCARD auto GetHierarchy() const -> const Node&;
        MKT_NODISCARD auto GetBoneCount() const -> UInt32;

        MKT_NODISCARD auto begin() -> JointsMapIterator;
        MKT_NODISCARD auto end() -> JointsMapIterator;

        MKT_NODISCARD auto begin() const -> JointsMapConstIterator;
        MKT_NODISCARD auto end() const -> JointsMapConstIterator;

        MKT_NODISCARD auto cbegin() const -> JointsMapConstIterator;
        MKT_NODISCARD auto cend() const -> JointsMapConstIterator;

        auto PrintTreeView() -> void;
        auto PrintBoneInfo() const -> void;
        auto SetWeights( std::string_view meshName, std::string_view boneName, UInt64 vertex, float weight ) -> void;

        auto SetHierarchy( Node&& rootNode ) -> void;
        auto SetBoneMap( JointsMap&& boneMap ) -> void;
        auto SetInverseBindMatrices( std::vector<Mat4F>&& mats ) -> void;

        auto BuildOzzStructures() -> void;

    private:
        auto ConstructOzzHierarchy( Node& root, ozz::animation::offline::RawSkeleton::Joint& joint ) -> void;

    private:
        Node m_RootNode{};

        JointsMap m_Joints{};
        JointsMapID m_JointsByID{};

        std::vector<Mat4F> m_InverseBindMatrices{};

        // ID -> ozz joint index
        ankerl::unordered_dense::map<UInt32, Int32> m_JointOzzIndex{};

        // To construct the skeleton
        ozz::animation::offline::SkeletonBuilder m_Builder{};
        ozz::animation::offline::RawSkeleton m_RawSkeleton{};

        // Run time skeleton
        ozz::unique_ptr<ozz::animation::Skeleton> m_Skeleton{};
    };
}

#endif // MIKOTO_SKELETON_HH