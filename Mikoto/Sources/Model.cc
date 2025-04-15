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
#include <Library/Filesystem/PathBuilder.hh>
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
}// namespace Mikoto