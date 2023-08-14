/**
* Mesh.cc
* Created by kate on 6/29/23.
* */

// C++ Standard Library
#include <utility>

// Project Headers
#include <Renderer/Mesh.hh>

namespace Mikoto {
    Mesh::Mesh(MeshData data)
        : m_Data{ std::move( data ) } {}

    Mesh::Mesh(Mesh&& other) noexcept = default;
    auto Mesh::operator=(Mesh&& other) noexcept -> Mesh& = default;
}