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

#include <EASTL/utility.h>

#include <glm/glm.hpp>

#include <Core/Core.hh>
#include <Core/Exception.hh>

#include <Renderer/Core/TerrainGeneration.hh>

namespace mikoto::renderer {

    using namespace mikoto::core;

    auto TerrainNoiseSettings::IsValid() const -> bool {
        return mFBM.mFrequency >= 0.01f && mFBM.mFrequency <= 64.0f &&
               mFBM.mAmplitude >= 0.0f && mFBM.mAmplitude <= 4.0f &&
               mFBM.mPersistence >= 0.0f && mFBM.mPersistence <= 1.0f &&
               mFBM.mLacunarity >= 1.0f && mFBM.mLacunarity <= 4.0f &&
               mFBM.mOctaves >= 1 && mFBM.mOctaves <= 12 &&
               mPerlinFrequency >= 0.01f && mPerlinFrequency <= 64.0f &&
               mPerlinAmplitude >= 0.0f && mPerlinAmplitude <= 4.0f &&
               mPerlinBlend >= 0.0f && mPerlinBlend <= 1.0f &&
               glm::abs( mOffset.x ) <= 256.0f && glm::abs( mOffset.y ) <= 256.0f;
    }

    TerrainNoiseGeneration::TerrainNoiseGeneration( const TerrainNoiseSettings& settings )
        : mSettings{ settings } {
    }

    auto TerrainNoiseGeneration::GetSettings() const -> TerrainNoiseSettings {
        return mSettings;
    }

    auto TerrainNoiseGeneration::GetHeightmap() const -> eastl::shared_ptr<const TerrainHeightmap> {
        return mHeightmap;
    }

    auto TerrainNoiseGeneration::GetError() const -> const eastl::string& {
        return mError;
    }

    auto TerrainNoiseGeneration::Complete( asset::ImageHandle image ) -> void {
        mHeightmap = eastl::make_shared<TerrainHeightmap>( eastl::move( image ) );
        mError.clear();
        BumpRevision();
    }

    auto TerrainNoiseGeneration::Fail( const eastl::string& error ) -> void {
        mError = error;
        BumpRevision();
    }

    auto WorldDescription::GetQuadsPerComponent() const -> core::u32 {
        return mQuadsPerSection * mSectionsPerComponent;
    }

    auto WorldDescription::GetResolutionX() const -> core::u32 {
        return mComponentsX * GetQuadsPerComponent() + 1;
    }

    auto WorldDescription::GetResolutionY() const -> core::u32 {
        return mComponentsY * GetQuadsPerComponent() + 1;
    }

    auto WorldDescription::GetTotalComponents() const -> core::u32 {
        return mComponentsX * mComponentsY;
    }

    auto WorldDescription::GetWorldSize() const -> TerrainSector {
        return { ( GetResolutionX() - 1 ) * mVertexSpacing, ( GetResolutionY() - 1 ) * mVertexSpacing };
    }

    auto IsTerrainDescriptionValid( const WorldDescription& description ) -> bool {
        constexpr core::u32 maxQuadsPerSection{ 255 };
        constexpr core::u32 maxComponentsPerAxis{ 256 };
        constexpr core::u64 maxTerrainVertices{ 4'194'304 };
        constexpr core::f32 maxElevation{ 100'000.0f };

        const auto sectionQuads{ description.mQuadsPerSection };
        const auto sections{ description.mSectionsPerComponent };
        const auto componentsX{ description.mComponentsX };
        const auto componentsY{ description.mComponentsY };

        if ( sectionQuads == 0 || sectionQuads > maxQuadsPerSection ||
             ( sections != 1 && sections != 2 ) ||
             componentsX == 0 || componentsY == 0 || componentsX > maxComponentsPerAxis || componentsY > maxComponentsPerAxis ) {
            return false;
        }

        const core::u64 quads{ as<core::u64>( sectionQuads ) * sections };
        const core::u64 columns{ quads * componentsX + 1 };
        const core::u64 rows{ quads * componentsY + 1 };
        const auto validColor{ []( core::float3 color ) {
            return color.x >= 0.0f && color.x <= 1.0f && color.y >= 0.0f && color.y <= 1.0f && color.z >= 0.0f && color.z <= 1.0f;
        } };

        return columns * rows <= maxTerrainVertices &&
               validColor( description.mLowColor ) && validColor( description.mMiddleColor ) && validColor( description.mHighColor ) &&
               description.mVertexSpacing >= 0.001f && description.mVertexSpacing <= 10'000.0f &&
               description.mHeightScale >= 0.0f && description.mHeightScale <= maxElevation &&
               std::abs( description.mBaseElevation ) <= maxElevation;
    }

    auto TerrainHeightmap::Sample( core::float2 uv ) const -> core::f32 {
        const float x{ glm::clamp( uv.x, 0.0f, 1.0f ) * ( mWidth - 1 ) };
        const float y{ glm::clamp( uv.y, 0.0f, 1.0f ) * ( mHeight - 1 ) };

        const auto x0{ as<core::u32>( x ) }, y0{ as<core::u32>( y ) };
        const auto x1{ glm::min( x0 + 1, mWidth - 1 ) }, y1{ glm::min( y0 + 1, mHeight - 1 ) };

        const auto at{ [this]( core::u32 col, core::u32 row ) {
            const auto index{ ( static_cast<core::usize>( row ) * mWidth + col ) * 4 };
            const auto* data{ mImage->mBufferSpan->GetData() };
            return mImage->mFormat == asset::ImageFormat::eRGBA32_FLOAT ? 
                as<const core::f32*>( data )[index] : as<const core::ubyte*>( data )[index] / 255.0f;
        } };

        return glm::mix( glm::mix( at( x0, y0 ), at( x1, y0 ), x - x0 ),
            glm::mix( at( x0, y1 ), at( x1, y1 ), x - x0 ), y - y0 );
    }

    TerrainHeightmap::TerrainHeightmap( asset::ImageHandle image )
        : mImage{ eastl::move( image ) } {
        if ( !mImage || !mImage->mBufferSpan || mImage->mWidth < 2 || mImage->mHeight < 2 ||
             mImage->mWidth > 8192 || mImage->mHeight > 8192 || mImage->mChannels != 4 ||
             ( mImage->mFormat != asset::ImageFormat::eRGBA8_UINT && mImage->mFormat != asset::ImageFormat::eRGBA32_FLOAT ) ) {
            MKT_THROW_RUNTIME_ERROR( "Heightmap requires an uncompressed RGBA image, 2 to 8192 pixels per axis." );
        }

        mWidth = mImage->mWidth;
        mHeight = mImage->mHeight;

        const core::u32 channelCount{ 4 };
        const core::usize samples{ as<core::usize>( mWidth ) * mHeight * channelCount };
        const bool floatingPoint{ mImage->mFormat == asset::ImageFormat::eRGBA32_FLOAT };

        if ( mImage->mBufferSpan->GetSize() < samples * ( floatingPoint ? MKT_SIZEOF( core::f32 ) : 1 ) ) {
            MKT_THROW_RUNTIME_ERROR( "Heightmap pixel buffer is incomplete." );
        }
    }
}
