/**
* Mesh.cc
* Created by kate on 6/29/23.
* */

// C++ Standard Library

// Project Headers
#include <Assets//Mesh.hh>
#include <utility>

namespace Mikoto {
    Mesh::Mesh( MeshData&& data )
        :   m_Data{ std::move( data ) }
    {

    }
}