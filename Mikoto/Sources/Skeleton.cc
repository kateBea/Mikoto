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

#include <utility>
#include <memory>
#include <ranges>

#include <Logging/Logger.hh>

#include <Common/String.hh>
#include <Animation/Skeleton.hh>
#include <Library/String/String.hh>

#include <ozz/animation/runtime/skeleton.h>

namespace Mikoto {

    Skeleton::Skeleton( SkeletonBuilder&& builder )
        {}

    auto Skeleton::RegisterJoint( const std::string &name, Int32 ID ) -> void {
        m_JointsByID.try_emplace( ID, name );
        m_Joints.try_emplace( name, name, ID );
    }

    auto Skeleton::GetBoneCount() const -> UInt32 {
        return m_Joints.size();
    }

    //auto Skeleton::SetBoneMap( JointsMap &&boneMap ) -> void {
    //    m_Joints = std::move( boneMap );
    //}

    //auto Skeleton::SetInverseBindMatrices(std::vector<Mat4F>&& mats) -> void {
    //    m_InverseBindMatrices = std::move( mats );
    // }

    auto Skeleton::ConstructOzzHierarchy( Node &node, ozz::animation::offline::RawSkeleton::Joint &joint ) -> void {
        // Setup root joints name.
        joint.name = node.Name;

        // Setup root joints rest pose transformation, in joint local-space.
        // This is the default skeleton posture (most of the time a T-pose). It's
        // used as a fallback when there's no animation for a joint.
        joint.transform.translation =
                ozz::math::Float3( node.Translation.x,
                                   node.Translation.y,
                                   node.Translation.z );

        joint.transform.rotation =
                ozz::math::Quaternion( node.Rotation.x,
                                       node.Rotation.y,
                                       node.Rotation.z,
                                       node.Rotation.w );
        
        joint.transform.scale =
                ozz::math::Float3( node.Scale.x,
                                   node.Scale.y,
                                   node.Scale.z );

        // Allocate children
        joint.children.resize( node.Children.size() );

        // Recursively build children
        for ( Size i{}; i < node.Children.size(); ++i ) {
            ConstructOzzHierarchy( node.Children[i], joint.children[i] );
        }
    }
    
    //auto Skeleton::BuildOzzStructures() -> void {
    //    // glTF skeletons usually have a single root
    //    m_RawSkeleton.roots.resize( 1 );
    //    auto &root{ m_RawSkeleton.roots[0] };

    //    ConstructOzzHierarchy( m_RootNode, root );

    //    if ( !m_RawSkeleton.Validate() ) {
    //        MKT_CORE_LOGGER_ERROR( "Skeleton is invalid" );
    //        return;
    //    }

    //    m_Skeleton = m_Builder( m_RawSkeleton );

    //    // Joint indices used later for animations
    //    for ( Size i{}; i < m_Skeleton->num_joints(); ++i ) {
    //        Joint *joint{ FindJoint( m_Skeleton->joint_names()[i] ) };
    //        MKT_ASSERT( joint != nullptr, "Joint cannot be null" );

    //        m_JointOzzIndex.try_emplace( joint->GetID(), i );
    //    }
    //}

    auto Skeleton::GetBoneMap() -> JointsMap & {
        return m_Joints;
    }

    auto Skeleton::GetBoneMap() const -> const JointsMap & {
        return m_Joints;
    }

    auto Skeleton::HasJoint( std::string_view name ) const -> bool {
        return m_Joints.contains( StringUtil::From( name ) );
    }

    auto Skeleton::FindJoint( std::string_view name ) -> Joint* {
        const std::string str{ StringUtil::From( name ) };
        const auto iter{ m_Joints.find( str ) };
        if ( iter != m_Joints.end() ) {
            return std::addressof( iter->second );
        }

        return nullptr;
    }

    auto Skeleton::FindJoint( std::string_view name ) const -> const Joint* {
        const std::string str{ StringUtil::From( name ) };
        const auto iter{ m_Joints.find( str ) };
        if ( iter != m_Joints.end() ) {
            return std::addressof( iter->second );
        }

        return nullptr;
    }

    /*auto Skeleton::SetHierarchy( Node&& rootNode ) -> void {
        m_RootNode = std::move( rootNode );
    }*/

    auto Skeleton::GetHierarchy() const -> const Node& {
        return m_RootNode;
    }

    auto Skeleton::GetInverseBindMatrices() const -> const std::vector<Mat4F> & {
        return m_InverseBindMatrices;
    }

    auto Skeleton::FindJointByID( UInt32 ID ) -> Joint * {
        const auto iter{ m_JointsByID.find( ID ) };
        if ( iter != m_JointsByID.end() ) {
            return std::addressof( m_Joints.at( iter->second ) );
        }

        return nullptr;
    }

    auto Skeleton::GetOzzSkeleton() -> ozz::animation::Skeleton* {
        return m_Skeleton.get();
    }

    auto Skeleton::GetOzzBondeIndex( UInt32 ID ) const -> Int32 {
        if (!m_JointOzzIndex.contains(ID)) {
            return -1;
        }

        return m_JointOzzIndex.at( ID );
    }

    auto Skeleton::GetOzzSkeleton() const -> const ozz::animation::Skeleton* {
        return m_Skeleton.get();
    }

    auto Skeleton::FindJointByID( UInt32 ID ) const -> const Joint * {
        const auto iter{ m_JointsByID.find( ID ) };
        if ( iter != m_JointsByID.end() ) {
            return std::addressof( m_Joints.at( iter->second ) );
        }

        return nullptr;
    }

    auto Skeleton::begin() -> JointsMapIterator {
        return m_Joints.begin();
    }

    auto Skeleton::end() -> JointsMapIterator {
        return m_Joints.end();
    }

    auto Skeleton::begin() const -> JointsMapConstIterator {
        return m_Joints.begin();
    }

    auto Skeleton::end() const -> JointsMapConstIterator {
        return m_Joints.end();
    }

    auto Skeleton::cbegin() const -> JointsMapConstIterator {
        return m_Joints.cbegin();
    }

    auto Skeleton::cend() const -> JointsMapConstIterator {
        return m_Joints.cend();
    }

    auto Skeleton::PrintBoneInfo() const -> void {
        for (const auto& joint : m_Joints | std::ranges::views::values) {
            joint.PrintBoneInfo();
        }
    }

    auto Skeleton::SetWeights( std::string_view meshName, std::string_view boneName, UInt64 vertex, float weight ) -> void {
        if (auto joint{ FindJoint( boneName ) }) {
            joint->SetWeights( meshName, vertex, weight );
        }
    }

    auto Skeleton::PrintTreeView() -> void {
        using ID = UInt32;

        const auto &boneMap{ m_Joints };

        // Build parent -> children adjacency
        std::unordered_map<ID, std::vector<const Joint *>> children{};
        std::vector<const Joint *> roots{};

        for ( const auto &[name, joint]: boneMap ) {
            const Int32 parentID{ joint.GetParentID() };

            if ( parentID == INVALID_JOINT_ID ) {
                roots.push_back( std::addressof( joint ) );
            } else {
                children[static_cast<ID>( parentID )].push_back( std::addressof( joint ) );
            }
        }

        fmt::print( "\n=== Skeleton Hierarchy ===\n" );

        constexpr std::string_view BRANCH{ "\u251C\u2500\u2500 " };
        constexpr std::string_view LAST{ "\u2514\u2500\u2500 " };
        constexpr std::string_view PIPE{ "\u2502   " };
        constexpr std::string_view SPACE{ "    " };

        auto printNode =
                [&]( const Joint *joint,
                     const std::string &prefix,
                     bool isLast,
                     auto &&self ) -> void {
            const std::string_view connector{ isLast ? LAST : BRANCH };

            MKT_COLOR_PRINT_FORMATTED_FLUSH( MKT_FMT_COLOR_ORANGE_RED, "{}{}{} ({})\n",
                                             prefix,
                                             connector,
                                             joint->GetBoneName(),
                                             joint->GetID() );

            const auto childIter{ children.find( joint->GetID() ) };
            if ( childIter == children.end() ) {
                return;
            }

            const auto &childList{ childIter->second };

            for ( Size i{}; i < childList.size(); ++i ) {
                const bool lastChild{ i == childList.size() - 1 };

                self(
                        childList[i],
                        prefix + StringUtil::From( isLast ? SPACE : PIPE ),
                        lastChild,
                        self );
            }
        };

        for ( Size i{}; i < roots.size(); ++i ) {
            const bool lastRoot{ i == roots.size() - 1 };
            printNode( roots[i], "", lastRoot, printNode );
        }

        MKT_COLOR_PRINT_FORMATTED_FLUSH(
                MKT_FMT_COLOR_BLUE_VIOLET,
                "=== End Skeleton ===\n\n" );
    }
}