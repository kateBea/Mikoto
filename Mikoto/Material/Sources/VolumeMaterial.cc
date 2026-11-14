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

#include <Core/Exception.hh>

#include <Filesystem/FileService.hh>

#include <Material/VolumeMaterial.hh>

namespace mikoto::material {
    using namespace mikoto::core;

    static auto VolumeVector( const float3& value ) -> YAML::Node {
        YAML::Node node{};
        for ( i32 axis{}; axis < 3; ++axis ) {
            node.push_back( value[axis] );
        }
        return node;
    }

    static auto ReadVolumeVector( const YAML::Node& node, float3 fallback ) -> float3 {
        if ( !node ) {
            return fallback;
        }
        if ( !node.IsSequence() || node.size() != 3 ) {
            MKT_THROW_RUNTIME_ERROR( "Volume coefficient needs three components." );
        }
        return { node[0].as<f32>(), node[1].as<f32>(), node[2].as<f32>() };
    }

    VolumeMaterial::VolumeMaterial( eastl::string_view name ) : mName{ name } {}

    VolumeMaterial::VolumeMaterial( const VolumeMaterial& other )
        : mName{ other.mName },
          mSettings{ other.mSettings },
          mGridDimensions{ other.mGridDimensions },
          mDensityGrid{ other.mDensityGrid } {
    }

    auto VolumeMaterial::SetName( eastl::string_view name ) -> void { mName = name; }
    auto VolumeMaterial::GetName() const -> const eastl::string& { return mName; }
    auto VolumeMaterial::GetSourcePath() const -> const filesystem::Path& { return mSourcePath; }
    auto VolumeMaterial::GetSettings() const -> const VolumeSettings& { return mSettings; }
    auto VolumeMaterial::GetGridDimensions() const -> glm::uvec3 { return mGridDimensions; }
    auto VolumeMaterial::GetDensityGrid() const -> const eastl::vector<f32>& { return mDensityGrid; }

    auto VolumeMaterial::SetSettings( const VolumeSettings& settings ) -> void {
        const float3 values[]{ settings.mAbsorption, settings.mScattering,
            float3{ settings.mAnisotropy, settings.mDensity, settings.mHeightFalloff },
            float3{ settings.mNoiseScale, settings.mErosionScale, settings.mErosion },
            float3{ settings.mCoverage } };

        for ( const auto& value: values ) {
            if ( glm::any( glm::isnan( value ) ) || glm::any( glm::isinf( value ) ) ) {
                MKT_THROW_INVALID_ARGUMENT( "Volume coefficients must be finite." );
            }
        }

        if ( as<u32>( settings.mDensityModel ) > as<u32>( VolumeDensityModel::eDensityGrid ) ) {
            MKT_THROW_INVALID_ARGUMENT( "Unknown volume density model." );
        }

        mSettings = settings;
        mSettings.mAbsorption = glm::max( settings.mAbsorption, float3{ 0.0f } );
        mSettings.mScattering = glm::max( settings.mScattering, float3{ 0.0f } );
        mSettings.mAnisotropy = glm::clamp( settings.mAnisotropy, -0.99f, 0.99f );

        mSettings.mDensity = glm::max( settings.mDensity, 0.0f );
        mSettings.mHeightFalloff = glm::max( settings.mHeightFalloff, 0.0f );

        mSettings.mNoiseScale = glm::max( settings.mNoiseScale, 0.000001f );
        mSettings.mErosionScale = glm::max( settings.mErosionScale, 0.000001f );
        mSettings.mErosion = glm::clamp( settings.mErosion, 0.0f, 1.0f );
        mSettings.mCoverage = glm::clamp( settings.mCoverage, 0.0f, 1.0f );
        mSettings.mOctaves = glm::clamp( settings.mOctaves, 1u, 8u );
    }

    auto VolumeMaterial::SetDensityGrid( glm::uvec3 dimensions, const eastl::vector<f32>& values ) -> void {
        const u64 count{ u64( dimensions.x ) * dimensions.y * dimensions.z };
        if ( glm::any( glm::lessThan( dimensions, glm::uvec3{ 1 } ) ) ||
             glm::any( glm::greaterThan( dimensions, glm::uvec3{ 256 } ) ) || count != values.size() ) {
            MKT_THROW_INVALID_ARGUMENT( "Volume density grid dimensions must be 1..256 with matching voxel data." );
        }
        for ( const auto value: values ) {
            if ( glm::isnan( value ) || glm::isinf( value ) ) {
                MKT_THROW_INVALID_ARGUMENT( "Volume density voxels must be finite." );
            }
        }

        mGridDimensions = dimensions;
        mDensityGrid = values;

        for ( auto& value: mDensityGrid ) {
            value = glm::clamp( value, 0.0f, 1.0f );
        }
    }

    auto VolumeMaterial::ImportDensityGrid( const filesystem::Path& filename ) -> void {
        const auto file{ filesystem::FileHandle::New( filename ) };
        const auto root{ YAML::Load( file->GetContentsString().c_str() ) };
        const auto dimensions{ root["Dimensions"] };
        const auto density{ root["Density"] };
        if ( root["Version"].as<i32>( 0 ) != 1 || !dimensions.IsSequence() || dimensions.size() != 3 || !density.IsSequence() ) {
            MKT_THROW_RUNTIME_ERROR( "Density grid requires Version 1, Dimensions [x,y,z] and Density voxels." );
        }
        const glm::uvec3 size{ dimensions[0].as<u32>(), dimensions[1].as<u32>(), dimensions[2].as<u32>() };
        if ( glm::any( glm::lessThan( size, glm::uvec3{ 1 } ) ) || glm::any( glm::greaterThan( size, glm::uvec3{ 256 } ) ) ||
             u64( size.x ) * size.y * size.z != density.size() ) {
            MKT_THROW_RUNTIME_ERROR( "Density grid dimensions must be 1..256 with matching voxel data." );
        }
        eastl::vector<f32> values{};
        values.reserve( density.size() );
        for ( const auto& voxel: density ) {
            values.push_back( voxel.as<f32>() );
        }
        SetDensityGrid( size, values );
    }


    auto VolumeMaterial::Serialize( const filesystem::Path& filename ) const -> void {
        const auto& s{ mSettings };
        YAML::Node root{};
        root["Version"] = 1;
        root["Type"] = "VolumeMaterial";
        root["Name"] = mName.c_str();
        auto node{ root["Material"] };
        node["DensityModel"] = as<u32>( s.mDensityModel );
        node["Absorption"] = VolumeVector( s.mAbsorption );
        node["Scattering"] = VolumeVector( s.mScattering );
        node["Anisotropy"] = s.mAnisotropy;
        node["Density"] = s.mDensity;
        node["HeightFalloff"] = s.mHeightFalloff;
        node["NoiseScale"] = s.mNoiseScale;
        node["ErosionScale"] = s.mErosionScale;
        node["Erosion"] = s.mErosion;
        node["Coverage"] = s.mCoverage;
        node["Octaves"] = s.mOctaves;

        if ( !mDensityGrid.empty() ) {
            for ( i32 axis{}; axis < 3; ++axis ) {
                node["GridDimensions"].push_back( mGridDimensions[axis] );
            }
            for ( const auto value: mDensityGrid ) {
                node["GridValues"].push_back( value );
            }
        }

        YAML::Emitter emitter{};
        emitter << root;
        if ( !emitter.good() ) {
            MKT_THROW_RUNTIME_ERROR( "Could not encode volume material." );
        }
        auto file{ filesystem::FileService::Get()->CreateNewFile( filename ) };
        if ( !file ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot open volume material for writing." );
        }
        file->Write( emitter.c_str() );
        mSourcePath = filename.ToAbsolute();
    }

    auto VolumeMaterial::Serialize( filesystem::FileHandle file ) const -> void {
        if ( !file ) {
            MKT_THROW_INVALID_ARGUMENT( "Volume material file is null." );
        }
        Serialize( file->GetPath() );
        file->UpdateContentsFromDisk();
    }

    auto VolumeMaterial::Deserialize( const filesystem::Path& filename ) -> void {
        Deserialize( filesystem::FileHandle::New( filename ) );
    }

    auto VolumeMaterial::Deserialize( filesystem::FileHandle file ) -> void {
        if ( !file ) {
            MKT_THROW_INVALID_ARGUMENT( "Volume material file is null." );
        }
        const auto root{ YAML::Load( file->GetContentsString().c_str() ) };
        const auto node{ root["Material"] };
        if ( root["Type"].as<std::string>() != "VolumeMaterial" || root["Version"].as<i32>() != 1 || !node.IsMap() ) {
            MKT_THROW_RUNTIME_ERROR( "Unsupported volume material asset." );
        }

        // Restore into a candidate; a malformed grid must not partially replace an asset.
        auto candidate{ Ref<VolumeMaterial>::New( root["Name"].as<std::string>( "Volume Material" ).c_str() ) };
        auto s{ candidate->mSettings };
        s.mDensityModel = as<VolumeDensityModel>( node["DensityModel"].as<u32>( 0 ) );
        s.mAbsorption = ReadVolumeVector( node["Absorption"], s.mAbsorption );
        s.mScattering = ReadVolumeVector( node["Scattering"], s.mScattering );
        s.mAnisotropy = node["Anisotropy"].as<f32>( s.mAnisotropy );
        s.mDensity = node["Density"].as<f32>( s.mDensity );
        s.mHeightFalloff = node["HeightFalloff"].as<f32>( s.mHeightFalloff );
        s.mNoiseScale = node["NoiseScale"].as<f32>( s.mNoiseScale );
        s.mErosionScale = node["ErosionScale"].as<f32>( s.mErosionScale );
        s.mErosion = node["Erosion"].as<f32>( s.mErosion );
        s.mCoverage = node["Coverage"].as<f32>( s.mCoverage );
        s.mOctaves = node["Octaves"].as<u32>( s.mOctaves );
        candidate->SetSettings( s );

        if ( node["GridDimensions"] ) {
            const auto d{ node["GridDimensions"] };
            if ( !d.IsSequence() || d.size() != 3 || !node["GridValues"].IsSequence() ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid volume density grid." );
            }
            eastl::vector<f32> values{};
            for ( const auto& value: node["GridValues"] ) {
                values.push_back( value.as<f32>() );
            }
            candidate->SetDensityGrid( { d[0].as<u32>(), d[1].as<u32>(), d[2].as<u32>() }, values );
        }

        mName = eastl::move( candidate->mName );
        mSettings = candidate->mSettings;
        mGridDimensions = candidate->mGridDimensions;
        mDensityGrid = eastl::move( candidate->mDensityGrid );
        mSourcePath = file->GetPath().ToAbsolute();
    }
}
