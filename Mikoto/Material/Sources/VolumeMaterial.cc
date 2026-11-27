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

#include <cmath>
#include <cstring>
#include <vector>

#include <yaml-cpp/yaml.h>

#include <Core/Exception.hh>

#include <Filesystem/FileService.hh>

#include <Material/VolumeMaterial.hh>

namespace mikoto::material {
    using namespace mikoto::core;

    static auto VolumeVector( const float3 &value ) -> YAML::Node {
        YAML::Node node{};
        for ( i32 axis{}; axis < 3; ++axis ) {
            node.push_back( value[axis] );
        }
        return node;
    }

    static auto VolumeGridHash( const void *data, usize size ) -> u64 {
        const auto *bytes{ static_cast<const ubyte *>( data ) };
        u64 result{ 1469598103934665603ull };
        for ( usize i{}; i < size; ++i ) {
            result = ( result ^ bytes[i] ) * 1099511628211ull;
        }
        return result;
    }

    static auto ReadVolumeVector( const YAML::Node &node, float3 fallback ) -> float3 {
        if ( !node ) {
            return fallback;
        }
        if ( !node.IsSequence() || node.size() != 3 ) {
            MKT_THROW_RUNTIME_ERROR( "Volume coefficient needs three components." );
        }
        return { node[0].as<f32>(), node[1].as<f32>(), node[2].as<f32>() };
    }

    // Lossless disk encoding keeps authored skies practical as project assets.
    // This is not GPU block compression: runtime still uploads ordinary voxels.
    // Version 2 CloudVoxelData: little-endian CLD1 magic, voxel count, zero/literal
    // runs of IEEE float32 profile/type/density triples, then an FNV32 checksum.
    // A 32-bit run token uses bit 31 for zero runs and the other bits for length;
    // literal runs carry 12 bytes per voxel. Decode validates size/count/runs/checksum
    // and trailing bytes. This compresses disk data only, not GPU representation.
    // Version 1 GridValues/scalar .mktvol Density and legacy CloudVoxels still load.
    // Version 2 .mktvol accepts Dimensions + CloudVoxelData/CloudVoxels at the root;
    // counts must match 1..256-axis dimensions and values must be finite. Failed
    // imports retain the previous material. Runtime grids are self-contained; recipes
    // need their source assets to rebake.
    static auto EncodeCloudVoxels( const eastl::vector<renderer::CloudVoxel> &values ) -> YAML::Binary {
        std::vector<unsigned char> bytes{};
        const auto write{ [&]( u32 value ) {
            for ( u32 shift{}; shift < 32; shift += 8 ) {
                bytes.push_back( static_cast<unsigned char>( value >> shift ) );
            }
        } };
        write( 0x31444c43u );// CLD1, little-endian IEEE float32 channels.
        write( static_cast<u32>( values.size() ) );
        const auto empty{ []( const renderer::CloudVoxel &v ) {
            return v.mProfile == 0 && v.mDetailType == 0 && v.mDensityScale == 0;
        } };
        for ( usize i{}; i < values.size(); ) {
            const bool zero{ empty( values[i] ) };
            const usize begin{ i++ };
            while ( i < values.size() && empty( values[i] ) == zero ) {
                ++i;
            }
            write( static_cast<u32>( i - begin ) | ( zero ? 0x80000000u : 0u ) );
            if ( zero ) {
                continue;
            }
            for ( usize index{ begin }; index < i; ++index ) {
                for ( const f32 value: { values[index].mProfile, values[index].mDetailType, values[index].mDensityScale } ) {
                    u32 bits{};
                    std::memcpy( &bits, &value, sizeof( bits ) );
                    write( bits );
                }
            }
        }
        u32 checksum{ 2166136261u };
        for ( const auto byte: bytes ) {
            checksum = ( checksum ^ byte ) * 16777619u;
        }
        write( checksum );
        YAML::Binary result{};
        result.swap( bytes );
        return result;
    }

    static auto DecodeCloudVoxels( const YAML::Node &node, u64 count ) -> eastl::vector<renderer::CloudVoxel> {
        if ( !node.IsScalar() || node.Scalar().size() > ( count * 16 + 16 ) * 2 ) {
            MKT_THROW_RUNTIME_ERROR( "Cloud voxel encoding exceeds its declared grid budget." );
        }
        const auto data{ node.as<YAML::Binary>() };
        const auto *bytes{ data.data() };
        const usize size{ data.size() };
        if ( size < 12 || size > count * 16 + 16 ) {
            MKT_THROW_RUNTIME_ERROR( "Invalid cloud voxel encoding size." );
        }
        usize cursor{};
        const auto read{ [&]() -> u32 {
            if ( cursor + 4 > size ) {
                MKT_THROW_RUNTIME_ERROR( "Truncated cloud voxel encoding." );
            }
            u32 value{};
            for ( u32 shift{}; shift < 32; shift += 8 ) {
                value |= u32( bytes[cursor++] ) << shift;
            }
            return value;
        } };
        if ( read() != 0x31444c43u || read() != count ) {
            MKT_THROW_RUNTIME_ERROR( "Cloud voxel encoding header does not match its grid." );
        }
        u32 checksum{ 2166136261u };
        for ( usize i{}; i < size - 4; ++i ) {
            checksum = ( checksum ^ bytes[i] ) * 16777619u;
        }
        eastl::vector<renderer::CloudVoxel> result( static_cast<usize>( count ) );
        usize written{};
        while ( written < count ) {
            const u32 token{ read() }, length{ token & 0x7fffffffu };
            if ( length == 0 || length > count - written ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid cloud voxel run length." );
            }
            if ( ( token & 0x80000000u ) != 0 ) {
                written += length;
                continue;
            }
            if ( u64( length ) * 12 > size - cursor - glm::min<usize>( size - cursor, 4 ) ) {
                MKT_THROW_RUNTIME_ERROR( "Truncated literal cloud voxel run." );
            }
            for ( u32 i{}; i < length; ++i ) {
                f32 channels[3]{};
                for ( auto &value: channels ) {
                    const auto bits{ read() };
                    std::memcpy( &value, &bits, sizeof( value ) );
                }
                result[written++] = { channels[0], channels[1], channels[2] };
            }
        }
        if ( cursor + 4 != size || read() != checksum ) {
            MKT_THROW_RUNTIME_ERROR( "Cloud voxel encoding checksum/trailing data is invalid." );
        }
        return result;
    }

    VolumeMaterial::VolumeMaterial( eastl::string_view name ) : mName{ name } {
    }

    VolumeMaterial::VolumeMaterial( const VolumeMaterial &other )
        : mName{ other.mName },
          mSettings{ other.mSettings },
          mCloudAuthoring{ other.mCloudAuthoring },
          mGridDimensions{ other.mGridDimensions },
          mGridHash{ other.mGridHash },
          mDensityGrid{ other.mDensityGrid },
          mCloudGrid{ other.mCloudGrid } {
    }

    auto VolumeMaterial::SetName( eastl::string_view name ) -> void {
        mName = name;
    }
    auto VolumeMaterial::GetName() const -> const eastl::string & {
        return mName;
    }
    auto VolumeMaterial::GetSourcePath() const -> const filesystem::Path & {
        return mSourcePath;
    }
    auto VolumeMaterial::GetSettings() const -> const VolumeSettings & {
        return mSettings;
    }
    auto VolumeMaterial::GetGridDimensions() const -> glm::uvec3 {
        return mGridDimensions;
    }
    auto VolumeMaterial::GetGridHash() const -> u64 {
        return mGridHash;
    }
    auto VolumeMaterial::GetDensityGrid() const -> const eastl::vector<f32> & {
        return mDensityGrid;
    }
    auto VolumeMaterial::GetCloudGrid() const -> const eastl::vector<renderer::CloudVoxel> & {
        return mCloudGrid;
    }

    auto VolumeMaterial::SetCloudAuthoring( const renderer::CloudAuthoringDescription &description ) -> void {
        mCloudAuthoring = description;
    }

    auto VolumeMaterial::GetCloudAuthoring() const -> const renderer::CloudAuthoringDescription & {
        return mCloudAuthoring;
    }

    auto VolumeMaterial::SetSettings( const VolumeSettings &settings ) -> void {


        if ( as<u32>( settings.mDensityModel ) > as<u32>( VolumeDensityModel::eDensityGrid ) ) {
            MKT_THROW_INVALID_ARGUMENT( "Unknown volume density model." );
        }

        if ( as<u32>( settings.mCloudForm ) > as<u32>( renderer::CloudForm::eCirrus ) ) {
            MKT_THROW_INVALID_ARGUMENT( "Unknown cloud form." );
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
        mSettings.mCloudDetailType = glm::clamp( settings.mCloudDetailType, 0.0f, 1.0f );
        mSettings.mOctaves = glm::clamp( settings.mOctaves, 1u, 8u );
        mSettings.mWeatherScale = glm::clamp( settings.mWeatherScale, 0.000001f, 1.0f );
        mSettings.mCoverageVariation = glm::clamp( settings.mCoverageVariation, 0.0f, 1.0f );
        mSettings.mCloudTypeVariation = glm::clamp( settings.mCloudTypeVariation, 0.0f, 1.0f );
        mSettings.mCloudHeightVariation = glm::clamp( settings.mCloudHeightVariation, 0.0f, 1.0f );
        mSettings.mWindShear = glm::clamp( settings.mWindShear, 0.0f, 2000.0f );
        mSettings.mCloudProfileDepth = glm::clamp( settings.mCloudProfileDepth, 0.001f, 1.0f );
        mSettings.mCloudFormationCount = glm::clamp( settings.mCloudFormationCount, 1u, 64u );
        mSettings.mCloudFormationScale = glm::clamp( settings.mCloudFormationScale, .2f, 3.0f );
        mSettings.mCloudFormationVariation = glm::clamp( settings.mCloudFormationVariation, 0.0f, 1.0f );
    }

    auto VolumeMaterial::SetDensityGrid( glm::uvec3 dimensions, const eastl::vector<f32> &values ) -> void {
        const u64 count{ u64( dimensions.x ) * dimensions.y * dimensions.z };
        if ( glm::any( glm::lessThan( dimensions, glm::uvec3{ 1 } ) ) ||
             glm::any( glm::greaterThan( dimensions, glm::uvec3{ 256 } ) ) || count != values.size() ) {
            MKT_THROW_INVALID_ARGUMENT( "Volume density grid dimensions must be 1..256 with matching voxel data." );
        }

        mGridDimensions = dimensions;
        mDensityGrid = values;
        mCloudGrid.clear();

        for ( auto &value: mDensityGrid ) {
            value = glm::clamp( value, 0.0f, 1.0f );
        }
        mGridHash = VolumeGridHash( mDensityGrid.data(), mDensityGrid.size() * sizeof( f32 ) );
    }

    auto VolumeMaterial::SetCloudGrid( glm::uvec3 dimensions, const eastl::vector<renderer::CloudVoxel> &values ) -> void {
        const u64 count{ u64( dimensions.x ) * dimensions.y * dimensions.z };
        if ( glm::any( glm::lessThan( dimensions, glm::uvec3{ 1 } ) ) ||
             glm::any( glm::greaterThan( dimensions, glm::uvec3{ 256 } ) ) || count != values.size() ) {
            MKT_THROW_INVALID_ARGUMENT( "Cloud grid dimensions must be 1..256 with matching voxel data." );
        }

        mGridDimensions = dimensions;
        mCloudGrid = values;
        mDensityGrid.resize( values.size() );
        for ( usize i{}; i < values.size(); ++i ) {
            auto &voxel{ mCloudGrid[i] };
            voxel.mProfile = glm::clamp( voxel.mProfile, 0.0f, 1.0f );
            voxel.mDetailType = glm::clamp( voxel.mDetailType, 0.0f, 1.0f );
            voxel.mDensityScale = glm::clamp( voxel.mDensityScale, 0.0f, 1.0f );
            // General fog/media consumers retain a bounded scalar fallback.
            mDensityGrid[i] = voxel.mProfile * voxel.mDensityScale;
        }
        mGridHash = VolumeGridHash( mCloudGrid.data(), mCloudGrid.size() * sizeof( renderer::CloudVoxel ) );
    }

    auto VolumeMaterial::ImportDensityGrid( const filesystem::Path &filename, const renderer::CloudVolumeImportSettings &options ) -> void {
        if ( filename.IsExtension( ".vdb" ) || filename.IsExtension( ".tga" ) ) {
            const auto imported{ filename.IsExtension( ".vdb" ) ? renderer::CloudVolumeIO::ImportVdb( filename, options ) : renderer::CloudVolumeIO::ImportSlices( filename, options ) };
            SetCloudGrid( imported.mGrid.mDimensions, imported.mGrid.mVoxels );
            auto settings{ GetSettings() };
            settings.mDensityModel = VolumeDensityModel::eDensityGrid;
            settings.mDensity = imported.mDensityMultiplier;
            SetSettings( settings );
            return;
        }
        ImportDensityGrid( filename );
    }

    auto VolumeMaterial::ImportDensityGrid( const filesystem::Path &filename ) -> void {
        if ( filename.IsExtension( ".vdb" ) || filename.IsExtension( ".tga" ) ) {
            ImportDensityGrid( filename, renderer::CloudVolumeImportSettings{} );
            return;
        }
        const auto file{ filesystem::FileHandle::New( filename ) };
        const auto root{ YAML::Load( file->GetContentsString().c_str() ) };
        const auto dimensions{ root["Dimensions"] };
        const auto version{ root["Version"].as<i32>( 0 ) };
        const bool encoded{ version == 2 && root["CloudVoxelData"].IsDefined() };
        const auto values{ version == 2 ? root["CloudVoxels"] : root["Density"] };
        if ( ( version != 1 && version != 2 ) || !dimensions.IsSequence() || dimensions.size() != 3 || ( !encoded && !values.IsSequence() ) ) {
            MKT_THROW_RUNTIME_ERROR( "Volume grid requires Dimensions and Version 1 Density or Version 2 CloudVoxels." );
        }
        const glm::uvec3 size{ dimensions[0].as<u32>(), dimensions[1].as<u32>(), dimensions[2].as<u32>() };
        if ( glm::any( glm::lessThan( size, glm::uvec3{ 1 } ) ) || glm::any( glm::greaterThan( size, glm::uvec3{ 256 } ) ) ||
             ( !encoded && u64( size.x ) * size.y * size.z != values.size() ) ) {
            MKT_THROW_RUNTIME_ERROR( "Volume grid dimensions must be 1..256 with matching voxel data." );
        }

        if ( encoded ) {
            SetCloudGrid( size, DecodeCloudVoxels( root["CloudVoxelData"], u64( size.x ) * size.y * size.z ) );
            return;
        }
        if ( version == 2 ) {
            eastl::vector<renderer::CloudVoxel> voxels{};
            voxels.reserve( values.size() );
            for ( const auto &value: values ) {
                if ( !value.IsSequence() || value.size() != 3 ) {
                    MKT_THROW_RUNTIME_ERROR( "Cloud voxel requires [profile, detail type, density scale]." );
                }
                voxels.push_back( { value[0].as<f32>(), value[1].as<f32>(), value[2].as<f32>() } );
            }
            SetCloudGrid( size, voxels );
        }
        else {
            eastl::vector<f32> density{};
            density.reserve( values.size() );
            for ( const auto &value: values ) {
                density.push_back( value.as<f32>() );
            }
            SetDensityGrid( size, density );
        }
    }

    auto VolumeMaterial::Serialize( const filesystem::Path &filename ) const -> void {
        const auto &s{ mSettings };
        YAML::Node root{};
        root["Version"] = mCloudGrid.empty() ? 1 : 2;
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
        node["CloudDetailType"] = s.mCloudDetailType;
        node["CloudSeed"] = s.mCloudSeed;
        node["CloudFormationCount"] = s.mCloudFormationCount;
        node["CloudFormationScale"] = s.mCloudFormationScale;
        node["CloudFormationVariation"] = s.mCloudFormationVariation;
        node["Octaves"] = s.mOctaves;
        node["CloudForm"] = as<u32>( s.mCloudForm );
        node["WeatherScale"] = s.mWeatherScale;
        node["CoverageVariation"] = s.mCoverageVariation;
        node["CloudTypeVariation"] = s.mCloudTypeVariation;
        node["CloudHeightVariation"] = s.mCloudHeightVariation;
        node["WindShear"] = s.mWindShear;
        node["CloudProfileDepth"] = s.mCloudProfileDepth;

        // Save the recipe as well as its baked result. Imported references are
        // relative to this asset so Save As and moving the project remain usable.
        auto authoring{ node["CloudAuthoring"] };
        authoring["Resolution"] = mCloudAuthoring.mResolution;
        if ( mCloudAuthoring.mDimensions != uint3{} ) {
            for ( u32 axis{}; axis < 3; ++axis ) {
                authoring["Dimensions"].push_back( mCloudAuthoring.mDimensions[axis] );
            }
        }
        authoring["DomainAspect"] = VolumeVector( mCloudAuthoring.mDomainAspect );
        authoring["ProfileDepth"] = mCloudAuthoring.mProfileDepth;
        for ( const auto &layer: mCloudAuthoring.mLayers ) {
            YAML::Node entry{};
            entry["Enabled"] = layer.mEnabled;
            entry["Source"] = as<u32>( layer.mSource );
            entry["Operation"] = as<u32>( layer.mOperation );
            entry["Offset"] = VolumeVector( layer.mOffset );
            entry["Scale"] = VolumeVector( layer.mScale );
            entry["Rotation"] = VolumeVector( layer.mRotation );
            entry["Opacity"] = layer.mOpacity;
            entry["Erosion"] = layer.mErosion;
            entry["Softness"] = layer.mSoftness;
            entry["PointRadius"] = layer.mPointRadius;
            auto import{ entry["Import"] };
            import["ProfileGrid"] = layer.mImport.mProfileGrid.c_str();
            import["DetailGrid"] = layer.mImport.mDetailGrid.c_str();
            import["DensityGrid"] = layer.mImport.mDensityGrid.c_str();
            import["Resolution"] = layer.mImport.mMaxResolution;
            import["DetailType"] = layer.mImport.mDetailType;
            import["ProfileDepth"] = layer.mImport.mProfileDepth;
            import["SliceLayout"] = as<u32>( layer.mImport.mSliceLayout );
            if ( !layer.mAssetPath.empty() ) {
                const auto source{ filesystem::Path{ layer.mAssetPath }.GetPathTyped<std::filesystem::path>() };
                const auto relative{ source.lexically_relative( filename.ToAbsolute().GetDirectory().GetPathTyped<std::filesystem::path>() ) };
                const auto text{ ( relative.empty() ? source : relative ).generic_u8string() };
                entry["Asset"] = std::string{ reinterpret_cast<const char *>( text.data() ), text.size() };
            }
            auto fluid{ entry["Fluid"] };
            fluid["CloudForm"] = as<u32>( layer.mFluid.mCloudForm );
            fluid["Resolution"] = layer.mFluid.mResolution;
            fluid["Steps"] = layer.mFluid.mSteps;
            fluid["Seed"] = layer.mFluid.mSeed;
            fluid["Buoyancy"] = layer.mFluid.mBuoyancy;
            fluid["Vorticity"] = layer.mFluid.mVorticity;
            fluid["Humidity"] = layer.mFluid.mHumidity;
            fluid["Cooling"] = layer.mFluid.mCooling;
            fluid["Condensation"] = layer.mFluid.mCondensation;
            fluid["Evaporation"] = layer.mFluid.mEvaporation;
            fluid["WindShear"] = layer.mFluid.mWindShear;
            fluid["PressureIterations"] = layer.mFluid.mPressureIterations;
            fluid["DetailType"] = layer.mFluid.mDetailType;
            fluid["ProfileDepth"] = layer.mFluid.mProfileDepth;
            authoring["Layers"].push_back( entry );
        }

        if ( !mDensityGrid.empty() ) {
            for ( i32 axis{}; axis < 3; ++axis ) {
                node["GridDimensions"].push_back( mGridDimensions[axis] );
            }
            if ( !mCloudGrid.empty() ) {
                node["CloudVoxelData"] = EncodeCloudVoxels( mCloudGrid );
            }
            else {
                for ( const auto value: mDensityGrid ) {
                    node["GridValues"].push_back( value );
                }
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

    auto VolumeMaterial::Deserialize( const filesystem::Path &filename ) -> void {
        Deserialize( filesystem::FileHandle::New( filename ) );
    }

    auto VolumeMaterial::Deserialize( filesystem::FileHandle file ) -> void {
        if ( !file ) {
            MKT_THROW_INVALID_ARGUMENT( "Volume material file is null." );
        }
        const auto root{ YAML::Load( file->GetContentsString().c_str() ) };
        const auto node{ root["Material"] };
        const auto version{ root["Version"].as<i32>() };
        if ( root["Type"].as<std::string>() != "VolumeMaterial" || ( version != 1 && version != 2 ) || !node.IsMap() ) {
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
        s.mCloudDetailType = node["CloudDetailType"].as<f32>( s.mCloudDetailType );
        s.mCloudSeed = node["CloudSeed"].as<u32>( 0 );
        s.mCloudFormationCount = node["CloudFormationCount"].as<u32>( s.mCloudFormationCount );
        s.mCloudFormationScale = node["CloudFormationScale"].as<f32>( s.mCloudFormationScale );
        s.mCloudFormationVariation = node["CloudFormationVariation"].as<f32>( s.mCloudFormationVariation );
        s.mOctaves = node["Octaves"].as<u32>( s.mOctaves );
        s.mCloudForm = as<renderer::CloudForm>( node["CloudForm"].as<u32>( as<u32>( s.mCloudForm ) ) );
        s.mWeatherScale = node["WeatherScale"].as<f32>( s.mWeatherScale );
        s.mCoverageVariation = node["CoverageVariation"].as<f32>( s.mCoverageVariation );
        s.mCloudTypeVariation = node["CloudTypeVariation"].as<f32>( s.mCloudTypeVariation );
        s.mCloudHeightVariation = node["CloudHeightVariation"].as<f32>( s.mCloudHeightVariation );
        s.mWindShear = node["WindShear"].as<f32>( s.mWindShear );
        s.mCloudProfileDepth = node["CloudProfileDepth"].as<f32>( s.mCloudProfileDepth );
        candidate->SetSettings( s );

        if ( const auto authoring{ node["CloudAuthoring"] } ) {
            renderer::CloudAuthoringDescription recipe{};
            recipe.mResolution = authoring["Resolution"].as<u32>( recipe.mResolution );
            if ( const auto size{ authoring["Dimensions"] } ) {
                if ( !size.IsSequence() || size.size() != 3 ) {
                    MKT_THROW_RUNTIME_ERROR( "Cloud authoring dimensions require three axes." );
                }
                recipe.mDimensions = { size[0].as<u32>(), size[1].as<u32>(), size[2].as<u32>() };
            }
            recipe.mDomainAspect = ReadVolumeVector( authoring["DomainAspect"], recipe.mDomainAspect );
            recipe.mProfileDepth = authoring["ProfileDepth"].as<f32>( recipe.mProfileDepth );
            const auto layers{ authoring["Layers"] };
            if ( layers && ( !layers.IsSequence() || layers.size() > 32 ) ) {
                MKT_THROW_RUNTIME_ERROR( "Cloud authoring layer stack must contain at most 32 entries." );
            }
            for ( const auto &entry: layers ) {
                renderer::CloudAuthoringLayer layer{};
                layer.mEnabled = entry["Enabled"].as<bool>( true );
                layer.mSource = as<renderer::CloudLayerSource>( entry["Source"].as<u32>( 0 ) );
                layer.mOperation = as<renderer::CloudLayerOperation>( entry["Operation"].as<u32>( 0 ) );
                layer.mOffset = ReadVolumeVector( entry["Offset"], layer.mOffset );
                layer.mScale = ReadVolumeVector( entry["Scale"], layer.mScale );
                layer.mRotation = ReadVolumeVector( entry["Rotation"], layer.mRotation );
                layer.mOpacity = entry["Opacity"].as<f32>( layer.mOpacity );
                layer.mErosion = entry["Erosion"].as<f32>( layer.mErosion );
                layer.mSoftness = entry["Softness"].as<f32>( layer.mSoftness );
                layer.mPointRadius = entry["PointRadius"].as<f32>( layer.mPointRadius );
                if ( const auto import{ entry["Import"] } ) {
                    layer.mImport.mProfileGrid = import["ProfileGrid"].as<std::string>( "" ).c_str();
                    layer.mImport.mDetailGrid = import["DetailGrid"].as<std::string>( "" ).c_str();
                    layer.mImport.mDensityGrid = import["DensityGrid"].as<std::string>( "" ).c_str();
                    layer.mImport.mMaxResolution = import["Resolution"].as<u32>( 256 );
                    layer.mImport.mDetailType = import["DetailType"].as<f32>( .75f );
                    layer.mImport.mProfileDepth = import["ProfileDepth"].as<f32>( .08f );
                    layer.mImport.mSliceLayout = as<renderer::CloudSliceLayout>( import["SliceLayout"].as<u32>( 0 ) );
                }
                const auto asset{ entry["Asset"].as<std::string>( "" ) };
                if ( !asset.empty() ) {
                    const auto resolved{ ( file->GetPath().ToAbsolute().GetDirectory().GetPathTyped<std::filesystem::path>() /
                                           std::filesystem::path{ asset } )
                            .lexically_normal() };
                    layer.mAssetPath = filesystem::Path{ resolved }.GetC_Str();
                }
                if ( const auto fluid{ entry["Fluid"] } ) {
                    layer.mFluid.mCloudForm = as<renderer::CloudForm>( fluid["CloudForm"].as<u32>( as<u32>( layer.mFluid.mCloudForm ) ) );
                    layer.mFluid.mResolution = fluid["Resolution"].as<u32>( layer.mFluid.mResolution );
                    layer.mFluid.mSteps = fluid["Steps"].as<u32>( layer.mFluid.mSteps );
                    layer.mFluid.mSeed = fluid["Seed"].as<u32>( layer.mFluid.mSeed );
                    layer.mFluid.mBuoyancy = fluid["Buoyancy"].as<f32>( layer.mFluid.mBuoyancy );
                    layer.mFluid.mVorticity = fluid["Vorticity"].as<f32>( layer.mFluid.mVorticity );
                    layer.mFluid.mHumidity = fluid["Humidity"].as<f32>( layer.mFluid.mHumidity );
                    layer.mFluid.mCooling = fluid["Cooling"].as<f32>( layer.mFluid.mCooling );
                    layer.mFluid.mCondensation = fluid["Condensation"].as<f32>( layer.mFluid.mCondensation );
                    layer.mFluid.mEvaporation = fluid["Evaporation"].as<f32>( layer.mFluid.mEvaporation );
                    layer.mFluid.mWindShear = fluid["WindShear"].as<f32>( layer.mFluid.mWindShear );
                    layer.mFluid.mPressureIterations = fluid["PressureIterations"].as<u32>( layer.mFluid.mPressureIterations );
                    layer.mFluid.mDetailType = fluid["DetailType"].as<f32>( layer.mFluid.mDetailType );
                    layer.mFluid.mProfileDepth = fluid["ProfileDepth"].as<f32>( layer.mFluid.mProfileDepth );
                }
                recipe.mLayers.push_back( eastl::move( layer ) );
            }
            candidate->SetCloudAuthoring( recipe );
        }

        if ( node["GridDimensions"] ) {
            const auto d{ node["GridDimensions"] };
            const bool encoded{ version == 2 && node["CloudVoxelData"].IsDefined() };
            const bool cloudGrid{ version == 2 && ( encoded || node["CloudVoxels"].IsDefined() ) };
            const auto values{ cloudGrid ? node["CloudVoxels"] : node["GridValues"] };
            if ( !d.IsSequence() || d.size() != 3 || ( !encoded && !values.IsSequence() ) ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid volume grid." );
            }
            const uint3 size{ d[0].as<u32>(), d[1].as<u32>(), d[2].as<u32>() };
            if ( glm::any( glm::lessThan( size, uint3{ 1 } ) ) || glm::any( glm::greaterThan( size, uint3{ 256 } ) ) ||
                 ( !encoded && u64( size.x ) * size.y * size.z != values.size() ) ) {
                MKT_THROW_RUNTIME_ERROR( "Volume grid dimensions do not match its voxel data." );
            }
            if ( encoded ) {
                candidate->SetCloudGrid( size, DecodeCloudVoxels( node["CloudVoxelData"], u64( size.x ) * size.y * size.z ) );
            }
            else if ( cloudGrid ) {
                eastl::vector<renderer::CloudVoxel> voxels{};
                voxels.reserve( values.size() );
                for ( const auto &value: values ) {
                    if ( !value.IsSequence() || value.size() != 3 ) {
                        MKT_THROW_RUNTIME_ERROR( "Cloud voxel requires [profile, detail type, density scale]." );
                    }
                    voxels.push_back( { value[0].as<f32>(), value[1].as<f32>(), value[2].as<f32>() } );
                }
                candidate->SetCloudGrid( size, voxels );
            }
            else {
                eastl::vector<f32> density{};
                density.reserve( values.size() );
                for ( const auto &value: values ) {
                    density.push_back( value.as<f32>() );
                }
                candidate->SetDensityGrid( size, density );
            }
        }
        else if ( node["GridValues"] || node["CloudVoxels"] || node["CloudVoxelData"] ) {
            MKT_THROW_RUNTIME_ERROR( "Volume voxel data requires GridDimensions." );
        }

        mName = eastl::move( candidate->mName );
        mSettings = candidate->mSettings;
        mCloudAuthoring = eastl::move( candidate->mCloudAuthoring );
        mGridDimensions = candidate->mGridDimensions;
        mGridHash = candidate->mGridHash;
        mDensityGrid = eastl::move( candidate->mDensityGrid );
        mCloudGrid = eastl::move( candidate->mCloudGrid );
        mSourcePath = file->GetPath().ToAbsolute();
    }
}// namespace mikoto::material
