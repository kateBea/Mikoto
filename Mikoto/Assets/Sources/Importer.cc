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
