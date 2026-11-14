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

#include <yaml-cpp/yaml.h>

#include <Core/Core.hh>
#include <Core/Exception.hh>

#include <Filesystem/File.hh>
#include <Filesystem/FileService.hh>

#include <Assets/Importer.hh>
#include <Assets/AssetModel.hh>

namespace mikoto::asset {
    auto ImportConfiguration::Load( const filesystem::Path& model ) -> ImportConfiguration {
        const filesystem::Path path{ string::Format( "{}.mktimport", model.GetC_Str() ) };
        ImportConfiguration settings{};

        if ( !path.Exists() ) {
            return settings;
        }

        const auto node{ YAML::LoadFile( path.GetC_Str() ) };
        settings.mImportMesh = node["Mesh"].as<bool>( true );
        settings.mImportSkeletons = node["Skeletons"].as<bool>( true );
        settings.mImportAnimations = node["Animations"].as<bool>( true );
        settings.mImportTranslation = node["Translation"].as<bool>( true );
        settings.mImportRotation = node["Rotation"].as<bool>( true );
        settings.mImportScale = node["Scale"].as<bool>( true );
        settings.mImportTextures = node["Textures"].as<bool>( true );
        settings.mImportMaterials = node["Materials"].as<bool>( true );
        return settings;
    }

    auto ImportConfiguration::Save( const filesystem::Path& model ) const -> void {
        YAML::Node node{};
        node["Mesh"] = mImportMesh;
        node["Skeletons"] = mImportSkeletons;
        node["Animations"] = mImportAnimations;
        node["Translation"] = mImportTranslation;
        node["Rotation"] = mImportRotation;
        node["Scale"] = mImportScale;
        node["Textures"] = mImportTextures;
        node["Materials"] = mImportMaterials;
        YAML::Emitter output{};
        output << node;
        filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( filesystem::Path{ string::Format( "{}.mktimport", model.GetC_Str() ) } ) };

        if ( file.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot open model import settings for writing." );
        }

        file->Write( output.c_str() );
    }

    ModelImporter::ModelImporter( renderer::rhi::IGpuDevice* device )
        : mDevice{ device } {
    }
}
