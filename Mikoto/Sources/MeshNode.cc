/**
* MeshInstance.cc
* Created by kate on 6/29/23.
* */

// C++ Standard Library
#include <utility>

// Project Headers
#include <Assets/MeshNode.hh>

namespace Mikoto {

    MeshNode::MeshNode( Size_T index, Ref<Buffer> vertices, Ref<Buffer> indices, std::vector<Ref<Texture>> &&textures )
        : m_MeshIndex{ index }, m_Vertices{ vertices }, m_Indices{ indices }, m_OriginalTextures{ std::move( textures ) }
    {}

    MeshNode::MeshNode( MeshNode &&other ) noexcept {
        if (std::addressof( other ) == this) {
            return;
        }

        m_Indices = other.m_Indices;
        m_Vertices = other.m_Vertices;
        m_MeshIndex = other.m_MeshIndex;
        m_OriginalTextures = std::move( other.m_OriginalTextures );
    }
}// namespace Mikoto