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

#include <Core/Exception.hh>

#include <Material/Utility.hh>
#include <Material/TextMaterial.hh>
#include <Material/VolumeMaterial.hh>
#include <Material/SkyboxMaterial.hh>
#include <Material/ParticleMaterial.hh>
#include <Material/PostProcessMaterial.hh>

namespace mikoto::material {
    using namespace core;
    using namespace filesystem;
    using namespace renderer::rhi;

    auto LoadMaterial( filesystem::FileHandle file ) -> MaterialHandle {
        MKT_BEGIN_PROFILER();

        if ( !file ) {
            MKT_THROW_INVALID_ARGUMENT( "Material file is null." );
        }

        // The factory chooses the concrete type; that object owns deserialization.
        const auto root{ YAML::Load( file->GetContentsString().c_str() ) };
        const auto type{ root["Type"].as<std::string>() };
        MaterialHandle material{};

        if ( type == "SurfaceMaterial" || type == "PhysicalMaterial" ) {
            material = core::Ref<SurfaceMaterial>::New();
        }
        else if ( type == "VolumeMaterial" ) {
            material = core::Ref<VolumeMaterial>::New();
        }
        else if ( type == "ParticleMaterial" ) {
            material = core::Ref<ParticleMaterial>::New();
        }
        else if ( type == "TextMaterial" ) {
            material = core::Ref<TextMaterial>::New();
        }
        else if ( type == "SkyboxMaterial" ) {
            material = core::Ref<SkyboxMaterial>::New();
        }
        else if ( type == "PostProcessMaterial" ) {
            material = core::Ref<PostProcessMaterial>::New();
        }
        else {
            MKT_THROW_RUNTIME_ERROR( "Unsupported material type." );
        }

        material->Deserialize( file );
        return material;
    }

    auto GetMaterialSourcePath( const IMaterial& material ) -> const filesystem::Path& {
        if ( const auto* value{ dynamic_cast<const VolumeMaterial*>( &material ) } ) {
            return value->GetSourcePath();
        }
        if ( const auto* value{ dynamic_cast<const SurfaceMaterial*>( &material ) } ) {
            return value->GetSourcePath();
        }

        if ( const auto* value{ dynamic_cast<const ParticleMaterial*>( &material ) } ) {
            return value->GetSourcePath();
        }

        if ( const auto* value{ dynamic_cast<const TextMaterial*>( &material ) } ) {
            return value->GetSourcePath();
        }

        if ( const auto* value{ dynamic_cast<const SkyboxMaterial*>( &material ) } ) {
            return value->GetSourcePath();
        }

        if ( const auto* value{ dynamic_cast<const PostProcessMaterial*>( &material ) } ) {
            return value->GetSourcePath();
        }

        MKT_THROW_RUNTIME_ERROR( "Unsupported material type." );
    }

    auto CloneMaterial( const IMaterial& material ) -> MaterialHandle {
        if ( const auto* value{ dynamic_cast<const VolumeMaterial*>( &material ) } ) {
            return VolumeMaterialHandle::New( *value );
        }

        if ( const auto* value{ dynamic_cast<const SurfaceMaterial*>( &material ) } ) {
            return SurfaceMaterialHandle::New( *value );
        }

        if ( const auto* value{ dynamic_cast<const ParticleMaterial*>( &material ) } ) {
            return ParticleMaterialHandle::New( *value );
        }

        if ( const auto* value{ dynamic_cast<const TextMaterial*>( &material ) } ) {
            return core::Ref<TextMaterial>::New( *value );
        }

        if ( const auto* value{ dynamic_cast<const SkyboxMaterial*>( &material ) } ) {
            return core::Ref<SkyboxMaterial>::New( *value );
        }

        if ( const auto* value{ dynamic_cast<const PostProcessMaterial*>( &material ) } ) {
            return core::Ref<PostProcessMaterial>::New( *value );
        }

        MKT_THROW_RUNTIME_ERROR( "Unsupported material type." );
    }
}// namespace mikoto::material
