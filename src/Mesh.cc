/**
* Mesh.cc
* Created by kate on 6/29/23.
* */

// C++ Standard Library
#include <utility>

// Project Headers
#include <Renderer/Mesh.hh>

namespace kaTe {
    Mesh::Mesh(MeshData data)
        : m_Data{ std::move( data ) } {}

    Mesh::Mesh(Mesh&& other) noexcept
        : m_Data{ std::move(other.m_Data) } {}

    auto Mesh::operator=(Mesh&& other) noexcept -> Mesh& {
        m_Data = std::move(other.m_Data);
        return *this;
    }
}