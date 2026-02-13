//    Copyright 2026 ケイト
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

#include <filesystem>
#include <vector>

#include <Assets/Model.hh>
#include <Library/String/String.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    auto ModelLoadDescription::WithFilePath( const File* file ) -> ModelLoadDescription & {
        this->ModelFile = file;

        return *this;
    }

    auto ModelLoadDescription::LoadTextures( bool value ) -> ModelLoadDescription & {
        this->WantTextures = value;

        return *this;
    }

    MeshNode::MeshNode( UInt32 index, BufferHandle vertices, BufferHandle indices, std::string_view name, MaterialProperties&& properties )
        : m_MeshIndex{ index }, m_Name{ name }, m_Vertices{ vertices }, m_Indices{ indices }, m_Properties{ std::move(properties) }
    {}
}