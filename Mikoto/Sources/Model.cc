/**
 * Model.cc
 * Created by kate on 6/29/23.
 * */

// C++ Standard Library
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

// Third Party Libraries
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <assimp/Importer.hpp>

// Project Headers
#include <Assets/Model.hh>
#include <Common/Common.hh>
#include <Library/String/String.hh>
#include <Library/Utility/Types.hh>

#include <Assets/MeshFactory.hh>

namespace Mikoto {

    auto ModelLoadDescription::WithFilePath( const File* file ) -> ModelLoadDescription & {
        this->ModelFile = file;

        return *this;
    }

    auto ModelLoadDescription::LoadTextures( bool value ) -> ModelLoadDescription & {
        this->WantTextures = value;

        return *this;
    }

    MeshNode::MeshNode( Size index, BufferHandle vertices, BufferHandle indices, std::vector<TextureHandle>&& textures, std::string_view name )
        : m_MeshIndex{ index }, m_Vertices{ ( vertices ) }, m_Indices{ ( indices ) }, m_OriginalTextures{ std::move( textures ) }, m_Name{ name }
    {}

    MeshNode::MeshNode( MeshNode &&other ) noexcept {
        if (std::addressof( other ) == this) {
            return;
        }

        m_Name = std::move( other.m_Name );
        m_Indices = other.m_Indices;
        m_Vertices = other.m_Vertices;
        m_MeshIndex = other.m_MeshIndex;
        m_OriginalTextures = std::move( other.m_OriginalTextures );
    }
}// namespace Mikoto