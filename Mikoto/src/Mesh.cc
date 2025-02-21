/**
* Mesh.cc
* Created by kate on 6/29/23.
* */

// C++ Standard Library

// Project Headers
#include <Assets//Mesh.hh>
#include <utility>

namespace Mikoto {
    Mesh::Mesh( const std::string_view name, Scope_T<VertexBuffer>&& vertices, Scope_T<IndexBuffer>&& indices, std::vector<Texture2D*>&& textures, const Path_T& path )
        : m_Name{ name }, m_Vertices{ std::move( vertices ) }, m_Indices{ std::move( indices ) }, m_Textures{ std::move( textures ) }, m_ModelAbsolutePath{ path } {}

    Mesh::~Mesh() {
        m_Textures.clear();

        m_Indices = nullptr;

        m_Vertices = nullptr;
    }
}// namespace Mikoto