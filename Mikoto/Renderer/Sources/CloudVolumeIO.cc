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

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <memory>
#include <regex>
#include <vector>

#include <stb_image.h>

#include <Core/Exception.hh>

#include <Renderer/Clouds/CloudVolumeIO.hh>

#if MIKOTO_ENABLE_OPENVDB
#include <openvdb/io/File.h>
#include <openvdb/openvdb.h>
#include <openvdb/tools/Interpolation.h>
#endif

namespace mikoto::renderer {

    using namespace core;

#if MIKOTO_ENABLE_OPENVDB
    // All channels are sampled through their own transforms, rather than assuming
    // equal active-coordinate bounds or matching sparse iteration order.
    struct CloudVdbChannel {
        openvdb::FloatGrid::Ptr mFloat{};
        openvdb::DoubleGrid::Ptr mDouble{};
        // One accessor per import worker: adjacent resampling queries reuse the
        // sparse-tree cache without sharing mutable accessor state across threads.
        std::unique_ptr<openvdb::FloatGrid::ConstAccessor> mFloatAccessor{};
        std::unique_ptr<openvdb::DoubleGrid::ConstAccessor> mDoubleAccessor{};
        MKT_NODISCARD auto Sample( const openvdb::Vec3d &world ) const -> f32 {
            if ( mFloat ) {
                const openvdb::tools::GridSampler<openvdb::FloatGrid::ConstAccessor, openvdb::tools::BoxSampler> sampler{ *mFloatAccessor, mFloat->transform() };
                return sampler.wsSample( world );
            }
            const openvdb::tools::GridSampler<openvdb::DoubleGrid::ConstAccessor, openvdb::tools::BoxSampler> sampler{ *mDoubleAccessor, mDouble->transform() };
            return static_cast<f32>( sampler.wsSample( world ) );
        }
        MKT_NODISCARD auto Base() const -> const openvdb::GridBase & {
            return mFloat ? static_cast<const openvdb::GridBase &>( *mFloat ) : *mDouble;
        }
        MKT_NODISCARD auto Valid() const -> bool {
            return mFloat || mDouble;
        }
    };

    static auto ReadVdbChannel( openvdb::io::File &file, const eastl::string &requested,
        const eastl::vector<eastl::string> &names, std::initializer_list<const char *> aliases, bool required ) -> CloudVdbChannel {
        eastl::string selected{ requested };
        if ( selected.empty() ) {
            for ( const auto *alias: aliases ) {
                for ( const auto &name: names ) {
                    if ( name == alias ) {
                        selected = name;
                        break;
                    }
                }
                if ( !selected.empty() ) {
                    break;
                }
            }
        }
        if ( selected.empty() ) {
            if ( required ) {
                MKT_THROW_RUNTIME_ERROR( "VDB has no recognized cloud density/profile channel; select its grid name explicitly." );
            }
            return {};
        }
        const auto base{ file.readGrid( selected.c_str() ) };
        CloudVdbChannel result{};
        if ( base->isType<openvdb::FloatGrid>() ) {
            result.mFloat = openvdb::gridPtrCast<openvdb::FloatGrid>( base );
        }
        else if ( base->isType<openvdb::DoubleGrid>() ) {
            result.mDouble = openvdb::gridPtrCast<openvdb::DoubleGrid>( base );
        }
        else {
            MKT_THROW_RUNTIME_ERROR( "Cloud VDB channels must be scalar float or double grids." );
        }
        if ( base->getGridClass() == openvdb::GRID_LEVEL_SET || !base->transform().isLinear() ) {
            MKT_THROW_RUNTIME_ERROR( "Cloud import expects fog/profile values with an affine transform, not a level set or frustum grid." );
        }
        if ( result.mFloat ) {
            result.mFloatAccessor = std::make_unique<openvdb::FloatGrid::ConstAccessor>( result.mFloat->tree() );
        }
        if ( result.mDouble ) {
            result.mDoubleAccessor = std::make_unique<openvdb::DoubleGrid::ConstAccessor>( result.mDouble->tree() );
        }
        return result;
    }
#endif

    auto CloudVolumeIO::ListVdbGrids( const filesystem::Path &path ) -> eastl::vector<eastl::string> {
#if MIKOTO_ENABLE_OPENVDB
        openvdb::initialize();
        openvdb::io::File file{ path.GetC_Str() };
        file.open();
        eastl::vector<eastl::string> names{};
        for ( auto it{ file.beginName() }; it != file.endName(); ++it ) {
            names.emplace_back( it.gridName().c_str() );
        }
        file.close();
        return names;
#else
        MKT_THROW_RUNTIME_ERROR( "Native VDB import requires MIKOTO_ENABLE_OPENVDB at configuration time." );
#endif
    }

    auto CloudVolumeIO::ImportVdb( const filesystem::Path &path, const CloudVolumeImportSettings &settings ) -> CloudImportedVolume {
#if MIKOTO_ENABLE_OPENVDB
        openvdb::initialize();
        const auto names{ ListVdbGrids( path ) };
        openvdb::io::File file{ path.GetC_Str() };
        file.open();
        const auto profile{ ReadVdbChannel( file, settings.mProfileGrid, names, { "dimensional_profile", "profile" }, false ) };
        const auto density{ ReadVdbChannel( file, settings.mDensityGrid, names, { "density_scale", "density", "densityScale" }, !profile.Valid() ) };
        const auto detail{ ReadVdbChannel( file, settings.mDetailGrid, names, { "detail_type", "detail", "detailType" }, false ) };
        file.close();
        const auto &domain{ profile.Valid() ? profile.Base() : density.Base() };
        const auto box{ domain.evalActiveVoxelBoundingBox() };
        if ( box.empty() ) {
            MKT_THROW_RUNTIME_ERROR( "The selected VDB cloud domain is empty." );
        }
        openvdb::Vec3d minimum{ 1e30 };
        openvdb::Vec3d maximum{ -1e30 };
        for ( u32 corner{}; corner < 8; ++corner ) {
            openvdb::Vec3d index{};
            for ( u32 axis{}; axis < 3; ++axis ) {
                index[axis] = ( corner & ( 1u << axis ) ) ? double( box.max()[axis] ) + .5 : double( box.min()[axis] ) - .5;
            }
            const auto world{ domain.transform().indexToWorld( index ) };
            for ( u32 axis{}; axis < 3; ++axis ) {
                minimum[axis] = std::min( minimum[axis], world[axis] );
                maximum[axis] = std::max( maximum[axis], world[axis] );
            }
        }
        const auto span{ maximum - minimum };
        const double longest{ std::max( span.x(), std::max( span.y(), span.z() ) ) };
        CloudImportedVolume result{};
        uint3 size{};
        for ( u32 axis{}; axis < 3; ++axis ) {
            size[axis] = glm::clamp( static_cast<u32>( std::ceil( settings.mMaxResolution * span[axis] / longest ) ), 1u, settings.mMaxResolution );
            result.mWorldMinimum[axis] = static_cast<f32>( minimum[axis] );
            result.mWorldMaximum[axis] = static_cast<f32>( maximum[axis] );
        }
        result.mGrid.mDimensions = size;
        result.mGrid.mVoxels.resize( usize( size.x ) * size.y * size.z );
        eastl::vector<f32> scalar( result.mGrid.mVoxels.size() );
        f32 peak{ 1 };
        for ( u32 z{}; z < size.z; ++z ) {
            for ( u32 y{}; y < size.y; ++y ) {
                for ( u32 x{}; x < size.x; ++x ) {
                    const usize i{ x + usize( size.x ) * ( y + usize( size.y ) * z ) };
                    const openvdb::Vec3d world{ minimum.x() + span.x() * ( x + .5 ) / size.x,
                        minimum.y() + span.y() * ( y + .5 ) / size.y, minimum.z() + span.z() * ( z + .5 ) / size.z };
                    const f32 p{ profile.Valid() ? profile.Sample( world ) : 1 };
                    const f32 d{ density.Valid() ? density.Sample( world ) : 1 };
                    const f32 type{ detail.Valid() ? detail.Sample( world ) : settings.mDetailType };
                    result.mGrid.mVoxels[i] = { glm::clamp( p, 0.0f, 1.0f ), glm::clamp( type, 0.0f, 1.0f ), glm::max( d, 0.0f ) };
                    scalar[i] = glm::max( d, 0.0f );
                    peak = glm::max( peak, scalar[i] );
                }
            }
        }
        result.mDensityMultiplier = peak;
        if ( !profile.Valid() ) {
            for ( auto &value: scalar ) {
                value /= peak;
            }
            const float3 proportions{ result.mWorldMaximum - result.mWorldMinimum };
            result.mGrid.mVoxels = CloudDensityBaker::BuildVolume( size, scalar, settings.mDetailType, settings.mProfileDepth, proportions );
            if ( detail.Valid() ) {
                // The scalar conversion rebuilds only profile/density; preserve the sampled type field.
                for ( u32 z{}; z < size.z; ++z ) {
                    for ( u32 y{}; y < size.y; ++y ) {
                        for ( u32 x{}; x < size.x; ++x ) {
                            const openvdb::Vec3d world{ minimum.x() + span.x() * ( x + .5 ) / size.x, minimum.y() + span.y() * ( y + .5 ) / size.y, minimum.z() + span.z() * ( z + .5 ) / size.z };
                            result.mGrid.mVoxels[x + usize( size.x ) * ( y + usize( size.y ) * z )].mDetailType = glm::clamp( detail.Sample( world ), 0.0f, 1.0f );
                        }
                    }
                }
            }
        }
        else {
            for ( auto &voxel: result.mGrid.mVoxels ) {
                voxel.mDensityScale /= peak;
            }
        }
        return result;
#else
        MKT_THROW_RUNTIME_ERROR( "Native VDB import requires MIKOTO_ENABLE_OPENVDB at configuration time." );
#endif
    }

    auto CloudVolumeIO::ImportSlices( const filesystem::Path &path, const CloudVolumeImportSettings &settings ) -> CloudImportedVolume {
        const auto source{ path.GetPathTyped<std::filesystem::path>() };
        const auto stem{ source.stem().string() };
        const std::regex numbering{ R"(^(.*?)([0-9]+)([^0-9]*)$)" };
        std::smatch first{};
        if ( !std::regex_match( stem, first, numbering ) ) {
            MKT_THROW_RUNTIME_ERROR( "Choose a numbered TGA slice, such as cloud_000.tga." );
        }
        struct Slice {
            u32 mIndex{};
            std::filesystem::path mPath{};
        };
        std::vector<Slice> slices{};
        for ( const auto &entry: std::filesystem::directory_iterator{ source.parent_path() } ) {
            if ( !entry.is_regular_file() ) {
                continue;
            }
            auto extension{ entry.path().extension().string() };
            std::transform( extension.begin(), extension.end(), extension.begin(), []( unsigned char c ) {
                return static_cast<char>( std::tolower( c ) );
            } );
            if ( extension != ".tga" ) {
                continue;
            }
            const auto name{ entry.path().stem().string() };
            std::smatch match{};
            if ( std::regex_match( name, match, numbering ) && match[1] == first[1] && match[3] == first[3] ) {
                const auto index{ std::stoull( match[2].str() ) };
                if ( index > UINT32_MAX ) {
                    MKT_THROW_RUNTIME_ERROR( "TGA slice index exceeds the supported range." );
                }
                slices.push_back( { static_cast<u32>( index ), entry.path() } );
            }
        }
        if ( slices.empty() || slices.size() > settings.mMaxResolution ) {
            MKT_THROW_RUNTIME_ERROR( "TGA stack must contain 1..256 slices within the import resolution limit." );
        }
        std::sort( slices.begin(), slices.end(), []( const auto &a, const auto &b ) {
            return a.mIndex < b.mIndex;
        } );
        CloudImportedVolume result{};
        stbi_set_flip_vertically_on_load_thread( 0 );
        for ( usize z{}; z < slices.size(); ++z ) {
            if ( z && slices[z].mIndex != slices[z - 1].mIndex + 1 ) {
                MKT_THROW_RUNTIME_ERROR( "TGA cloud stack contains duplicate indices or missing slices." );
            }
            int width{};
            int height{};
            int channels{};
            std::unique_ptr<stbi_uc, decltype( &stbi_image_free )> pixels{ stbi_load( slices[z].mPath.string().c_str(), &width, &height, &channels, 4 ), stbi_image_free };
            if ( !pixels || width < 1 || height < 1 || width > as<i32>( settings.mMaxResolution ) || height > as<i32>( settings.mMaxResolution ) || channels < 3 ) {
                MKT_THROW_RUNTIME_ERROR( "TGA cloud slices require RGB/RGBA data and dimensions within the import limit." );
            }
            if ( settings.mSliceLayout == CloudSliceLayout::eDistanceProfileTypeDensity && channels != 4 ) {
                MKT_THROW_RUNTIME_ERROR( "Distance/profile/type/density slices require four channels." );
            }
            if ( z == 0 ) {
                result.mGrid.mDimensions = { width, height, slices.size() };
                result.mGrid.mVoxels.resize( usize( width ) * height * slices.size() );
            }
            else if ( result.mGrid.mDimensions.x != width || result.mGrid.mDimensions.y != height ) {
                MKT_THROW_RUNTIME_ERROR( "TGA cloud slices have inconsistent dimensions." );
            }
            for ( usize i{}; i < usize( width ) * height; ++i ) {
                const usize firstChannel{ i * 4 + ( settings.mSliceLayout == CloudSliceLayout::eDistanceProfileTypeDensity ? 1 : 0 ) };
                result.mGrid.mVoxels[i + z * width * height] = { pixels.get()[firstChannel] / 255.0f, pixels.get()[firstChannel + 1] / 255.0f, pixels.get()[firstChannel + 2] / 255.0f };
            }
        }
        result.mWorldMaximum = float3{ result.mGrid.mDimensions };
        return result;
    }

    auto CloudVolumeIO::ExportVdb( const filesystem::Path &path, const CloudImportedVolume &volume ) -> void {
#if MIKOTO_ENABLE_OPENVDB
        const auto size{ volume.mGrid.mDimensions };
        const float3 span{ volume.mWorldMaximum - volume.mWorldMinimum };
        openvdb::initialize();
        openvdb::GridPtrVec grids{};
        const char *names[]{ "dimensional_profile", "detail_type", "density_scale" };
        for ( u32 channel{}; channel < 3; ++channel ) {
            auto grid{ openvdb::FloatGrid::create( 0 ) };
            grid->setName( names[channel] );
            grid->setGridClass( openvdb::GRID_FOG_VOLUME );
            auto transform{ openvdb::math::Transform::createLinearTransform( 1.0 ) };
            transform->postScale( openvdb::Vec3d{ span.x / size.x, span.y / size.y, span.z / size.z } );
            transform->postTranslate( openvdb::Vec3d{ volume.mWorldMinimum.x + span.x / size.x * .5,
                volume.mWorldMinimum.y + span.y / size.y * .5, volume.mWorldMinimum.z + span.z / size.z * .5 } );
            grid->setTransform( transform );
            auto accessor{ grid->getAccessor() };
            for ( u32 z{}; z < size.z; ++z ) {
                for ( u32 y{}; y < size.y; ++y ) {
                    for ( u32 x{}; x < size.x; ++x ) {
                        const auto &voxel{ volume.mGrid.mVoxels[x + usize( size.x ) * ( y + usize( size.y ) * z )] };
                        const f32 value{ channel == 0 ? voxel.mProfile : channel == 1 ? voxel.mDetailType
                                                                                      : voxel.mDensityScale * volume.mDensityMultiplier };
                        // Keep a shared active domain even where profile/type/density are zero.
                        accessor.setValue( openvdb::Coord{ as<i32>( x ), as<i32>( y ), as<i32>( z ) }, value );
                    }
                }
            }
            grids.push_back( grid );
        }
        openvdb::io::File file{ path.GetC_Str() };
        file.write( grids );
        file.close();
#else
        MKT_THROW_RUNTIME_ERROR( "Native VDB export requires MIKOTO_ENABLE_OPENVDB at configuration time." );
#endif
    }

}// namespace mikoto::renderer
