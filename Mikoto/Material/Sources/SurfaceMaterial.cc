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
#include <filesystem>

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <glm/gtx/compatibility.hpp>

#include <Assets/AssetRegistryService.hh>
#include <Assets/ImageProcessing.hh>

#include <Core/Exception.hh>
#include <Core/Profiler.hh>

#include <Filesystem/FileService.hh>

#include <Material/SurfaceMaterial.hh>

#include <Math/Math.hh>

namespace mikoto::material {

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    SurfaceMaterial::SurfaceMaterial( const eastl::string_view name )
        : mName{ name } {
        mUseTextures.fill( true );
    }

    SurfaceMaterial::SurfaceMaterial( const SurfaceMaterial &other )
        : mName{ other.mName },
          mGraph{ other.mGraph },
          mShadingModel{ other.mShadingModel },
          mSubsurface{ other.mSubsurface },
          mSurfaceShading{ other.mSurfaceShading },
          mDielectric{ other.mDielectric },
          mConductor{ other.mConductor },
          mAlphaMask{ other.mAlphaMask },
          mWorkflow{ other.mWorkflow },
          mBaseColorFactor{ other.mBaseColorFactor },
          mDiffuseFactor{ other.mDiffuseFactor },
          mSpecularFactor{ other.mSpecularFactor },
          mEmissiveFactor{ other.mEmissiveFactor },
          mAoFactor{ other.mAoFactor },
          mMetallicFactor{ other.mMetallicFactor },
          mRoughnessFactor{ other.mRoughnessFactor },
          mGlossinessFactor{ other.mGlossinessFactor },
          mCheckerboard{ other.mCheckerboard },
          mFoliageWind{ other.mFoliageWind },
          mHeightMapping{ other.mHeightMapping },
          mUvTransform{ other.mUvTransform },
          mNormalScale{ other.mNormalScale },
          mClearCoatNormalScale{ other.mClearCoatNormalScale },
          mClearCoatNormalTextureSet{ other.mClearCoatNormalTextureSet },
          mOcclusionStrength{ other.mOcclusionStrength },
          mEmissiveStrength{ other.mEmissiveStrength },
          mAlphaMaskCutoff{ other.mAlphaMaskCutoff },
          mBaseColorTextureSet{ other.mBaseColorTextureSet },
          mMetallicRoughnessTextureSet{ other.mMetallicRoughnessTextureSet },
          mSpecularGlossinessTextureSet{ other.mSpecularGlossinessTextureSet },
          mNormalTextureSet{ other.mNormalTextureSet },
          mOcclusionTextureSet{ other.mOcclusionTextureSet },
          mEmissiveTextureSet{ other.mEmissiveTextureSet },
          mIsBloomy{ other.mIsBloomy },
          mIsDoubleSided{ other.mIsDoubleSided },
          mStreamedTextures{ other.mStreamedTextures },
          mTextures{ other.mTextures },
          mUseTextures{ other.mUseTextures },
          mSamplers{ other.mSamplers } {
    }

    SurfaceMaterial::SurfaceMaterial( const SurfaceMaterialDescription &props )
        : SurfaceMaterial{ props.mName } {
        mAlphaMask = props.mAlphaMask;
        mWorkflow = props.mWorkflow;

        mBaseColorFactor = props.BaseColorFactor;
        mDiffuseFactor = props.DiffuseFactor;
        mSpecularFactor = props.SpecularFactor;
        mEmissiveFactor = props.EmissiveFactor;

        mMetallicFactor = props.MetallicFactor;
        mRoughnessFactor = props.RoughnessFactor;
        mGlossinessFactor = props.GlossinessFactor;

        mNormalScale = props.NormalScale;
        SetClearCoatNormalScale( props.ClearCoatNormalScale );
        SetClearCoatNormalTextureSet( props.ClearCoatNormalTextureSet );

        mShadingModel = props.Unlit ? ShadingModel::eUnlit : props.mShadingModel;
        SetSurfaceShading( props.mSurfaceShading );
        SetCheckerboard( props.mCheckerboard );
        SetFoliageWind( props.mFoliageWind );
        SetDielectric( props.mDielectric );
        SetConductor( props.mConductor );

        mOcclusionStrength = props.OcclusionStrength;
        mEmissiveStrength = props.EmissiveStrength;
        mAlphaMaskCutoff = props.AlphaMaskCutoff;

        mBaseColorTextureSet = props.BaseColorTextureSet;
        mMetallicRoughnessTextureSet = props.MetallicRoughnessTextureSet;
        mSpecularGlossinessTextureSet = props.SpecularGlossinessSet;
        mNormalTextureSet = props.NormalTextureSet;
        mOcclusionTextureSet = props.OcclusionTextureSet;
        mEmissiveTextureSet = props.EmissiveTextureSet;

        mIsDoubleSided = props.mIsDoubleSided;

        for ( const auto &[path, texture]: props.mTextureBindings ) {
            if ( texture.mTexture ) {
                if ( path.IsEmpty() ) {
                    SetTexture( texture.mMapType, texture.mTexture );
                }
                else {
                    const asset::AssetRef<asset::Texture> reference{ path };
                    SetTexture( texture.mMapType, asset::AssetRegistryService::Get()->RegisterLoaded(
                                                      reference, core::Ref<asset::Texture>::New( texture.mTexture, reference ) ) );
                }
            }
            else if ( !path.IsEmpty() ) {
                SetTextureReference( texture.mMapType, path );
            }
        }
    }

    auto SurfaceMaterial::SetTextureReference( MapType type, const filesystem::Path &path ) -> void {
        if ( type == MapType::eHeight ) {
            const std::lock_guard lock{ mHeightImageMutex };
            mHeightImage.Reset();
            mHeightImagePath = {};
        }
        mTextures.erase( type );
        mStreamedTextures[type] = { asset::AssetRef<asset::Texture>{ path }, {} };
        BumpRevision();
    }

    auto SurfaceMaterial::SetCheckerboard( const CheckerboardSettings &settings ) -> void {
        mCheckerboard = settings;
        BumpRevision();
    }

    auto SurfaceMaterial::GetCheckerboard() const -> const CheckerboardSettings & {
        return mCheckerboard;
    }

    auto SurfaceMaterial::SetFoliageWind( const FoliageWindSettings &settings ) -> void {
        mFoliageWind = settings;
        BumpRevision();
    }

    auto SurfaceMaterial::GetFoliageWind() const -> const FoliageWindSettings & {
        return mFoliageWind;
    }

    auto SurfaceMaterial::SetHeightMapping( HeightMappingSettings settings ) -> void {
        settings.mScale = glm::clamp( settings.mScale, -1000.0f, 1000.0f );
        settings.mMidlevel = glm::clamp( settings.mMidlevel, 0.0f, 1.0f );
        settings.mUVSet = glm::min( settings.mUVSet, 1u );
        settings.mSteps = glm::clamp( settings.mSteps, 8u, 128u );
        mHeightMapping = settings;
        BumpRevision();
    }

    auto SurfaceMaterial::GetHeightMapping() const -> const HeightMappingSettings & {
        return mHeightMapping;
    }

    auto SurfaceMaterial::GetHeightImage() const -> asset::ImageHandle {
        const std::lock_guard lock{ mHeightImageMutex };
        const auto path{ GetTexturePath( MapType::eHeight ) };
        if ( path != mHeightImagePath ) {
            mHeightImage.Reset();
            mHeightImagePath = path;
        }
        if ( !mHeightImage && !path.IsEmpty() ) {
            mHeightImage = asset::ProcessImage2D( path );
        }
        return mHeightImage;
    }

    auto SurfaceMaterial::GetTexturePath( MapType type ) const -> filesystem::Path {
        if ( const auto found{ mStreamedTextures.find( type ) }; found != mStreamedTextures.end() ) {
            return found->second.mReference.GetPath();
        }

        return {};
    }

    auto SurfaceMaterial::UpdateTextureResidency( bool wanted ) -> void {
        const auto now{ std::chrono::steady_clock::now() };

        if ( wanted ) {
            mLastTextureUse = now;
        }

        for ( auto &[type, binding]: mStreamedTextures ) {
            if ( !wanted ) {
                // Hysteresis prevents camera motion and multiple views from
                // repeatedly loading and dropping the same shared material.
                if ( now - mLastTextureUse >= std::chrono::seconds{ 10 } ) {
                    binding.mRequest = {};

                    if ( mTextures.erase( type ) ) {
                        BumpRevision();
                    }
                }

                continue;
            }

            if ( binding.mRequest.GetState() == asset::AssetState::eUnloaded ) {
                binding.mRequest = asset::AssetRegistryService::Get()->Request<asset::Texture>( binding.mReference );
            }

            const auto asset{ binding.mRequest.GetIfReady() };
            auto ready{ asset ? asset->GetResource() : renderer::rhi::TextureHandle{} };

            if ( ready && GetTexture( type ) != ready ) {
                mTextures[type] = eastl::move( ready );
                BumpRevision();
            }
        }
    }

    auto SurfaceMaterial::RemoveTexture( MapType type ) -> void {
        MKT_BEGIN_PROFILER();
        const bool removedReference{ mStreamedTextures.erase( type ) != 0 };

        if ( mTextures.erase( type ) || removedReference ) {
            BumpRevision();
        }
    }

    auto SurfaceMaterial::GetAlphaMask() const -> AlphaMode {
        MKT_BEGIN_PROFILER();
        return mAlphaMask;
    }

    auto SurfaceMaterial::SetAlphaMask( AlphaMode mode ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mAlphaMask == mode ) {
            return;
        }

        mAlphaMask = mode;
        BumpRevision();
    }

    auto SurfaceMaterial::GetWorkflow() const -> Workflow {
        MKT_BEGIN_PROFILER();
        return mWorkflow;
    }

    auto SurfaceMaterial::SetWorkflow( Workflow mode ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mWorkflow == mode ) {
            return;
        }

        mWorkflow = mode;
        BumpRevision();
    }

    auto SurfaceMaterial::GetBaseColorFactor() const -> const core::float4 & {
        MKT_BEGIN_PROFILER();
        return mBaseColorFactor;
    }

    auto SurfaceMaterial::SetBaseColorFactor( const core::float4 &value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mBaseColorFactor == value ) {
            return;
        }

        mBaseColorFactor = value;
        BumpRevision();
    }

    auto SurfaceMaterial::GetDiffuseFactor() const -> const core::float4 & {
        MKT_BEGIN_PROFILER();
        return mDiffuseFactor;
    }

    auto SurfaceMaterial::SetDiffuseFactor( const core::float4 &value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mDiffuseFactor == value ) {
            return;
        }

        mDiffuseFactor = value;
        BumpRevision();
    }

    auto SurfaceMaterial::GetSpecularFactor() const -> const core::float4 & {
        MKT_BEGIN_PROFILER();
        return mSpecularFactor;
    }

    auto SurfaceMaterial::SetSpecularFactor( const core::float4 &value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mSpecularFactor == value ) {
            return;
        }

        mSpecularFactor = value;
        BumpRevision();
    }

    auto SurfaceMaterial::GetEmissiveFactor() const -> const core::float3 & {
        MKT_BEGIN_PROFILER();
        return mEmissiveFactor;
    }

    auto SurfaceMaterial::GetAoFactor() const -> float {
        MKT_BEGIN_PROFILER();
        return mAoFactor;
    }

    auto SurfaceMaterial::SetEmissiveFactor( const core::float3 &value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mEmissiveFactor == value ) {
            return;
        }

        mEmissiveFactor = value;
        BumpRevision();
    }

    auto SurfaceMaterial::SetAoFactor( float v ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mAoFactor == v ) {
            return;
        }

        mAoFactor = v;
        BumpRevision();
    }

    auto SurfaceMaterial::GetMetallicFactor() const -> float {
        MKT_BEGIN_PROFILER();
        return mMetallicFactor;
    }

    auto SurfaceMaterial::SetMetallicFactor( float v ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMetallicFactor == v ) {
            return;
        }

        mMetallicFactor = v;
        BumpRevision();
    }

    auto SurfaceMaterial::GetRoughnessFactor() const -> float {
        MKT_BEGIN_PROFILER();
        return mRoughnessFactor;
    }

    auto SurfaceMaterial::SetRoughnessFactor( float v ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mRoughnessFactor == v ) {
            return;
        }

        mRoughnessFactor = v;
        BumpRevision();
    }

    auto SurfaceMaterial::GetGlossinessFactor() const -> float {
        MKT_BEGIN_PROFILER();
        return mGlossinessFactor;
    }

    auto SurfaceMaterial::SetGlossinessFactor( float v ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mGlossinessFactor == v ) {
            return;
        }

        mGlossinessFactor = v;
        BumpRevision();
    }

    auto SurfaceMaterial::GetClearCoatNormalScale() const -> float {
        return mClearCoatNormalScale;
    }

    auto SurfaceMaterial::GetClearCoatNormalTextureSet() const -> core::i32 {
        return mClearCoatNormalTextureSet;
    }

    auto SurfaceMaterial::SetClearCoatNormalScale( float v ) -> void {
        if ( mClearCoatNormalScale == v ) {
            return;
        }

        mClearCoatNormalScale = v;
        BumpRevision();
    }

    auto SurfaceMaterial::SetClearCoatNormalTextureSet( core::i32 set ) -> void {
        set = set == 1 ? 1 : 0;

        if ( mClearCoatNormalTextureSet == set ) {
            return;
        }

        mClearCoatNormalTextureSet = set;
        BumpRevision();
    }

    auto SurfaceMaterial::GetUvTransform() const -> const core::float4 & {
        return mUvTransform;
    }

    auto SurfaceMaterial::SetUvTransform( const core::float4 &value ) -> void {
        if ( value == mUvTransform ) {
            return;
        }

        mUvTransform = value;
        BumpRevision();
    }

    auto SurfaceMaterial::GetNormalScale() const -> float {
        MKT_BEGIN_PROFILER();
        return mNormalScale;
    }

    auto SurfaceMaterial::SetNormalScale( float v ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mNormalScale == v ) {
            return;
        }

        mNormalScale = v;
        BumpRevision();
    }

    auto SurfaceMaterial::GetOcclusionStrength() const -> float {
        MKT_BEGIN_PROFILER();
        return mOcclusionStrength;
    }

    auto SurfaceMaterial::SetOcclusionStrength( float v ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mOcclusionStrength == v ) {
            return;
        }

        mOcclusionStrength = v;
        BumpRevision();
    }

    auto SurfaceMaterial::GetEmissiveStrength() const -> float {
        MKT_BEGIN_PROFILER();
        return mEmissiveStrength;
    }

    auto SurfaceMaterial::SetEmissiveStrength( float v ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mEmissiveStrength == v ) {
            return;
        }

        mEmissiveStrength = v;
        BumpRevision();
    }

    auto SurfaceMaterial::GetAlphaMaskCutoff() const -> float {
        MKT_BEGIN_PROFILER();
        return mAlphaMaskCutoff;
    }

    auto SurfaceMaterial::SetAlphaMaskCutoff( float v ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mAlphaMaskCutoff == v ) {
            return;
        }

        mAlphaMaskCutoff = v;
        BumpRevision();
    }

    auto SurfaceMaterial::SetIsDoubleSided( bool v ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsDoubleSided == v ) {
            return;
        }

        mIsDoubleSided = v;
        BumpRevision();
    }

    auto SurfaceMaterial::GetBaseColorTextureSet() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mBaseColorTextureSet;
    }

    auto SurfaceMaterial::SetBaseColorTextureSet( core::i32 set ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mBaseColorTextureSet == set ) {
            return;
        }

        mBaseColorTextureSet = set;
        BumpRevision();
    }

    auto SurfaceMaterial::GetSpecularGlossinessSet() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mSpecularGlossinessTextureSet;
    }

    auto SurfaceMaterial::GetMetallicRoughnessTextureSet() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mMetallicRoughnessTextureSet;
    }

    auto SurfaceMaterial::SetMetallicRoughnessTextureSet( core::i32 set ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMetallicRoughnessTextureSet == set ) {
            return;
        }

        mMetallicRoughnessTextureSet = set;
        BumpRevision();
    }

    auto SurfaceMaterial::SetSpecularGlossinessSet( core::i32 set ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mSpecularGlossinessTextureSet == set ) {
            return;
        }

        mSpecularGlossinessTextureSet = set;
        BumpRevision();
    }

    auto SurfaceMaterial::GetNormalTextureSet() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mNormalTextureSet;
    }

    auto SurfaceMaterial::SetNormalTextureSet( core::i32 set ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mNormalTextureSet == set ) {
            return;
        }

        mNormalTextureSet = set;
        BumpRevision();
    }

    auto SurfaceMaterial::GetOcclusionTextureSet() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mOcclusionTextureSet;
    }

    auto SurfaceMaterial::SetOcclusionTextureSet( core::i32 set ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mOcclusionTextureSet == set ) {
            return;
        }

        mOcclusionTextureSet = set;
        BumpRevision();
    }

    auto SurfaceMaterial::GetEmissiveTextureSet() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mEmissiveTextureSet;
    }

    auto SurfaceMaterial::IsDoubleSided() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsDoubleSided;
    }

    auto SurfaceMaterial::IsBloomy() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsBloomy;
    }

    auto SurfaceMaterial::SetEmissiveTextureSet( core::i32 set ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mEmissiveTextureSet == set ) {
            return;
        }

        mEmissiveTextureSet = set;
        BumpRevision();
    }

    auto SurfaceMaterial::EnableBloom( bool enable ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsBloomy == enable ) {
            return;
        }

        mIsBloomy = enable;
        BumpRevision();
    }

    auto SurfaceMaterial::SetTexture( const MapType type, const renderer::rhi::TextureHandle &texture ) -> void {
        MKT_BEGIN_PROFILER();
        const bool replacedReference{ mStreamedTextures.erase( type ) != 0 };

        if ( GetTexture( type ) == texture && !replacedReference ) {
            return;
        }

        mTextures[type] = texture;
        BumpRevision();
    }

    auto SurfaceMaterial::SetTexture( MapType type, const asset::AssetHandle<asset::Texture> &texture ) -> void {
        const auto resource{ texture.GetIfReady() };
        mStreamedTextures[type] = { texture.GetRef(), texture };
        mTextures[type] = resource ? resource->GetResource() : renderer::rhi::TextureHandle{};
        BumpRevision();
    }

    auto SurfaceMaterial::UseTexture( MapType type, bool use ) -> void {
        const auto index{ static_cast<core::usize>( type ) };

        if ( index >= mUseTextures.size() || mUseTextures[index] == use ) {
            return;
        }

        mUseTextures[index] = use;
        BumpRevision();
    }

    auto SurfaceMaterial::IsTextureUsed( MapType type ) const -> bool {
        const auto index{ static_cast<core::usize>( type ) };
        return index < mUseTextures.size() && mUseTextures[index];
    }


    auto SurfaceMaterial::IsOpaque() const -> bool {
        MKT_BEGIN_PROFILER();
        return !IsTransparent();
    }

    auto SurfaceMaterial::IsTransparent() const -> bool {
        MKT_BEGIN_PROFILER();
        return mAlphaMask == AlphaMode::eBlend;
    }

    auto SurfaceMaterial::HasTexture( MapType type ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mTextures.contains( type );
    }

    auto SurfaceMaterial::GetTexture( const MapType type ) const -> rhi::TextureHandle {
        MKT_BEGIN_PROFILER();

        if ( const auto it{ mTextures.find( type ) }; it != mTextures.end() ) {
            return it->second;
        }

        return renderer::rhi::TextureHandle::CreateEmpty();
    }

    auto SurfaceMaterial::SetDielectric( const DielectricSettings &settings ) -> void {
        auto value{ settings };
        for ( u32 channel{}; channel < 3; ++channel ) {
            value.mScattering[channel] = glm::clamp( value.mScattering[channel], 0.f, 10000.f );
        }
        value.mPhaseAnisotropy = glm::clamp( value.mPhaseAnisotropy, -.99f, .99f );
        value.mAttenuationColor = glm::clamp( value.mAttenuationColor, core::float3{ 0.0001f }, core::float3{ 1 } );
        value.mIndexOfRefraction = glm::clamp( value.mIndexOfRefraction, 1.0f, 2.5f );
        value.mTransmission = glm::clamp( value.mTransmission, 0.0f, 1.0f );
        value.mThickness = glm::clamp( value.mThickness, 0.0f, 10000.0f );
        value.mAttenuationDistance = glm::clamp( value.mAttenuationDistance, 0.001f, 100000.0f );

        if ( value.mAttenuationColor == mDielectric.mAttenuationColor && value.mIndexOfRefraction == mDielectric.mIndexOfRefraction &&
             value.mTransmission == mDielectric.mTransmission && value.mThickness == mDielectric.mThickness &&
             value.mAttenuationDistance == mDielectric.mAttenuationDistance && value.mThinWalled == mDielectric.mThinWalled &&
             value.mScattering == mDielectric.mScattering && value.mPhaseAnisotropy == mDielectric.mPhaseAnisotropy ) {
            return;
        }

        mDielectric = value;
        BumpRevision();
    }

    auto SurfaceMaterial::GetDielectric() const -> const DielectricSettings & {
        return mDielectric;
    }

    auto SurfaceMaterial::SetConductor( const ConductorSettings &settings ) -> void {
        auto value{ settings };
        for ( u32 channel{}; channel < 3; ++channel ) {
            value.mEta[channel] = glm::clamp( value.mEta[channel], .0001f, 100.f );
            value.mK[channel] = glm::clamp( value.mK[channel], 0.f, 100.f );
        }
        if ( value.mEta == mConductor.mEta && value.mK == mConductor.mK ) {
            return;
        }
        mConductor = value;
        BumpRevision();
    }

    auto SurfaceMaterial::GetConductor() const -> const ConductorSettings & {
        return mConductor;
    }

    auto SurfaceMaterial::GetConductorReflectance() const -> float3 {
        const auto k2{ mConductor.mK * mConductor.mK };
        const auto minus{ mConductor.mEta - 1.f }, plus{ mConductor.mEta + 1.f };
        return ( minus * minus + k2 ) / ( plus * plus + k2 );
    }

    auto SurfaceMaterial::ApplyConductorPreset( u32 preset ) -> void {
        // PBRT's measured spectra, linearly sampled at 610/550/460 nm for the RGB adapter.
        // https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/util/spectrum.cpp
        static const ConductorSettings presets[]{
            { { .2060566f, .3504088f, 1.4186078f }, { 3.0304736f, 2.7140631f, 1.8431054f } },
            { { .3239992f, .9581618f, 1.1595675f }, { 3.0895795f, 2.5770647f, 2.4363386f } },
            { { .1273912f, .1247768f, .1433831f }, { 3.8053639f, 3.3395997f, 2.5673807f } },
            { { 1.2475087f, .958569f, .6462719f }, { 7.3684559f, 6.6869677f, 5.5901485f } }
        };
        if ( preset >= eastl::size( presets ) ) {
            return;
        }
        SetConductor( presets[preset] );
        SetShadingModel( ShadingModel::eConductor );
        SetWorkflow( Workflow::eMetallicRoughness );
        SetMetallicFactor( 1 );
        SetRoughnessFactor( .15f );
        SetBaseColorFactor( float4{ 1 } );
    }

    auto SurfaceMaterial::ApplyDielectricPreset( u32 preset ) -> void {
        // Approximate visible-light IOR values; these remain editable material properties.
        // https://pbr-book.org/4ed/Reflection_Models/Specular_Reflection_and_Transmission
        static constexpr f32 indices[]{ 1.0003f, 1.333f, 1.31f, 1.46f, 1.52f, 1.77f, 2.42f };
        if ( preset >= eastl::size( indices ) ) {
            return;
        }

        DielectricSettings settings{};
        settings.mIndexOfRefraction = indices[preset];
        SetDielectric( settings );
        SetShadingModel( ShadingModel::eDielectric );
        SetRoughnessFactor( preset == 1 ? .05f : 0.f );
        SetMetallicFactor( 0 );
        SetAlphaMask( AlphaMode::eOpaque );
    }

    auto SurfaceMaterial::ApplySubsurfacePreset( u32 preset ) -> void {
        // Measured reduced scattering/absorption in mm^-1, with g = 0.
        // https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/media.cpp
        static const float3 scattering[]{ { .74f, .88f, 1.01f }, { 1.09f, 1.59f, 1.79f }, { 2.19f, 2.62f, 3.f }, { 2.55f, 3.21f, 3.77f } };
        static const float3 absorption[]{ { .032f, .17f, .48f }, { .013f, .070f, .145f }, { .0021f, .0041f, .0071f }, { .0011f, .0024f, .014f } };
        if ( preset >= eastl::size( scattering ) ) {
            return;
        }

        auto settings{ GetSubsurface() };
        settings.mScattering = scattering[preset];
        settings.mAbsorption = absorption[preset];
        settings.mAnisotropy = 0;
        SetSubsurface( settings );
        SetShadingModel( ShadingModel::eSubsurfaceScattering );
        SetMetallicFactor( 0 );
    }

    auto SurfaceMaterial::SetShadingModel( ShadingModel model ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mShadingModel == model ) {
            return;
        }

        mShadingModel = model;
        BumpRevision();
    }

    auto SurfaceMaterial::GetShadingModel() const -> ShadingModel {
        MKT_BEGIN_PROFILER();
        return mShadingModel;
    }

    auto SurfaceMaterial::SetSurfaceShading( const SurfaceShadingSettings &settings ) -> void {
        auto value{ settings };
        auto clampUnit{ []( core::f32 v, core::f32 minimum ) {
            return glm::clamp( v, minimum, 1.0f );
        } };
        value.mCoatWeight = clampUnit( value.mCoatWeight, 0.0f );
        value.mCoatRoughness = clampUnit( value.mCoatRoughness, 0.045f );
        value.mToonThreshold = clampUnit( value.mToonThreshold, 0.0f );
        value.mToonSoftness = clampUnit( value.mToonSoftness, 0.001f );
        value.mCellBands = glm::clamp( value.mCellBands, 2, 16 );
        value.mContourWidth = glm::clamp( value.mContourWidth, 0.0f, 12.0f );

        for ( core::i32 channel{}; channel < 3; ++channel ) {
            value.mContourColor[channel] = glm::clamp( value.mContourColor[channel], 0.0f, 1.0f );
        }

        if ( value.mCoatWeight == mSurfaceShading.mCoatWeight && value.mCoatRoughness == mSurfaceShading.mCoatRoughness &&
             value.mToonThreshold == mSurfaceShading.mToonThreshold && value.mToonSoftness == mSurfaceShading.mToonSoftness &&
             value.mCellBands == mSurfaceShading.mCellBands && value.mContourColor == mSurfaceShading.mContourColor && value.mContourWidth == mSurfaceShading.mContourWidth ) {
            return;
        }

        mSurfaceShading = value;
        BumpRevision();
    }

    auto SurfaceMaterial::GetSurfaceShading() const -> const SurfaceShadingSettings & {
        return mSurfaceShading;
    }

    auto SurfaceMaterial::SetSubsurface( const SubsurfaceSettings &settings ) -> void {
        SubsurfaceSettings value{ settings };
        for ( u32 channel{}; channel < 3; ++channel ) {
            value.mAbsorption[channel] = glm::clamp( value.mAbsorption[channel], 0.f, 10000.f );
            value.mScattering[channel] = glm::clamp( value.mScattering[channel], 0.f, 10000.f );
        }
        value.mIndexOfRefraction = glm::clamp( value.mIndexOfRefraction, 1.f, 2.f );
        value.mAnisotropy = glm::clamp( value.mAnisotropy, -.99f, .99f );
        value.mUnitsPerMillimeter = glm::clamp( value.mUnitsPerMillimeter, 1e-6f, 1000.f );
        // Keep subsurface controls within the supported blur and transmission ranges.
        value.mScatterScale = glm::clamp( value.mScatterScale, 0.0f, 10.0f );
        value.mStrength = glm::clamp( value.mStrength, 0.0f, 1.0f );

        for ( core::i32 channel{}; channel < 3; ++channel ) {
            value.mColor[channel] = glm::clamp( value.mColor[channel], 0.0f, 1.0f );
        }

        for ( core::i32 channel{}; channel < 3; ++channel ) {
            value.mNear[channel] = glm::clamp( value.mNear[channel], 0.001f, 10.0f );
            value.mFar[channel] = glm::clamp( value.mFar[channel], value.mNear[channel], 10.0f );
        }

        value.mNearWeight = glm::clamp( value.mNearWeight, 0.0f, 1.0f );
        value.mDepthRejection = glm::clamp( value.mDepthRejection, 0.0f, 1.0f );
        value.mThickness = glm::clamp( value.mThickness, 0.0f, 100.0f );
        value.mTranslucency = glm::clamp( value.mTranslucency, 0.0f, 1.0f );
        value.mSamples = glm::clamp( value.mSamples, 7, 65 ) | 1;
        value.mOpacity = glm::clamp( value.mOpacity, 0.0f, 1.0f );

        if ( mSubsurface.mScatterScale == value.mScatterScale && mSubsurface.mStrength == value.mStrength &&
             mSubsurface.mColor == value.mColor && mSubsurface.mOpacity == value.mOpacity &&
             mSubsurface.mNear == value.mNear && mSubsurface.mFar == value.mFar &&
             mSubsurface.mNearWeight == value.mNearWeight && mSubsurface.mDepthRejection == value.mDepthRejection && mSubsurface.mThickness == value.mThickness &&
             mSubsurface.mTranslucency == value.mTranslucency && mSubsurface.mSamples == value.mSamples &&
             mSubsurface.mAbsorption == value.mAbsorption && mSubsurface.mScattering == value.mScattering &&
             mSubsurface.mIndexOfRefraction == value.mIndexOfRefraction && mSubsurface.mAnisotropy == value.mAnisotropy &&
             mSubsurface.mUnitsPerMillimeter == value.mUnitsPerMillimeter ) {
            return;
        }

        mSubsurface = value;
        BumpRevision();
    }

    auto SurfaceMaterial::GetSubsurface() const -> const SubsurfaceSettings & {
        return mSubsurface;
    }

    auto SurfaceMaterial::Serialize( const filesystem::Path &filename ) const -> void {
        // File assets need persistent image paths; model-owned embedded images must be exported first.
        for ( core::i32 index{}; index <= core::as<core::i32>( MapType::eHeight ); ++index ) {
            const auto texture{ GetTexture( core::as<MapType>( index ) ) };

            if ( texture && GetTexturePath( core::as<MapType>( index ) ).IsEmpty() ) {
                MKT_THROW_RUNTIME_ERROR( "Save the material's embedded texture to an image file first." );
            }
        }

        MKT_BEGIN_PROFILER();

        YAML::Node root{};
        root["Version"] = 2;
        root["Type"] = "SurfaceMaterial";
        root["Name"] = GetName().c_str();
        root["Material"] = SerializeProperties();

        // Store texture paths relative to this material file, including graph references.
        const auto directory{ filename.ToAbsolute().GetPathTyped<std::filesystem::path>().parent_path() };
        const auto relativize{ [&]( const auto &self, YAML::Node node ) -> void {
            if ( node.IsMap() ) {
                for ( const auto &entry: node ) {
                    if ( entry.first.as<std::string>() == "TexturePath" ) {
                        const auto texture{ std::filesystem::absolute( std::filesystem::path{ entry.second.as<std::string>() } ) };
                        const auto relative{ texture.lexically_relative( directory ) };
                        node["TexturePath"] = ( relative.empty() ? texture : relative ).generic_string();
                    }
                    else {
                        self( self, entry.second );
                    }
                }
            }
            else if ( node.IsSequence() ) {
                for ( const auto &entry: node ) {
                    self( self, entry );
                }
            }
        } };
        relativize( relativize, root["Material"] );

        // Publish the complete document before updating the material's source path.
        YAML::Emitter emitter{};
        emitter << root;

        if ( !emitter.good() ) {
            MKT_THROW_RUNTIME_ERROR( "Could not encode material." );
        }

        filesystem::FileHandle file{ filesystem::FileService::Get()->CreateNewFile( filename ) };

        if ( file.IsEmpty() ) {
            MKT_THROW_RUNTIME_ERROR( "Cannot open material file for writing." );
        }

        file->Write( emitter.c_str() );
        mSourcePath = filename.ToAbsolute();
    }

    auto SurfaceMaterial::Serialize( filesystem::FileHandle file ) const -> void {
        if ( !file ) {
            MKT_THROW_INVALID_ARGUMENT( "Material file is null." );
        }

        Serialize( file->GetPath() );
        file->UpdateContentsFromDisk();
    }

    auto SurfaceMaterial::Deserialize( const filesystem::Path &filename ) -> void {
        Deserialize( filesystem::FileHandle::New( filename ) );
    }

    auto SurfaceMaterial::Deserialize( filesystem::FileHandle file ) -> void {
        if ( !file ) {
            MKT_THROW_INVALID_ARGUMENT( "Material file is null." );
        }

        MKT_BEGIN_PROFILER();

        // Validate the file envelope before restoring any material state.
        auto root{ YAML::Load( file->GetContentsString().c_str() ) };

        if ( !root.IsMap() ) {
            MKT_THROW_RUNTIME_ERROR( "Material asset must be a map." );
        }

        const auto version{ root["Version"].as<core::i32>( 0 ) };
        const auto type{ root["Type"].as<std::string>() };

        if ( ( version != 1 && version != 2 ) || !root["Material"].IsMap() ) {
            MKT_THROW_RUNTIME_ERROR( "Unsupported material file version or missing properties." );
        }

        if ( type != "SurfaceMaterial" && type != "PhysicalMaterial" ) {
            MKT_THROW_RUNTIME_ERROR( "Material asset has a different type." );
        }

        // Version 1 paths were relative to the working directory; version 2 uses the file.
        const auto directory{ file->GetDirectory().GetPathTyped<std::filesystem::path>() };
        const auto resolve{ [&]( const auto &self, YAML::Node node ) -> void {
            if ( node.IsMap() ) {
                for ( const auto &entry: node ) {
                    if ( entry.first.as<std::string>() == "TexturePath" ) {
                        std::filesystem::path path{ entry.second.as<std::string>() };

                        if ( path.is_relative() && version >= 2 ) {
                            path = directory / path;
                        }

                        node["TexturePath"] = path.lexically_normal().string();
                    }
                    else {
                        self( self, entry.second );
                    }
                }
            }
            else if ( node.IsSequence() ) {
                for ( const auto &entry: node ) {
                    self( self, entry );
                }
            }
        } };
        resolve( resolve, root["Material"] );

        // Restore into a candidate so malformed properties leave this material unchanged.
        auto value{ core::Ref<SurfaceMaterial>::New( root["Name"].as<std::string>( "Material" ).c_str() ) };
        value->RestoreProperties( root["Material"] );
        value->SetName( root["Name"].as<std::string>( "Material" ).c_str() );

        mSourcePath = file->GetPath();
        BumpRevision();

        // Publish the validated identity and shading models as one material revision.
        mGraph = eastl::move( value->mGraph );
        mName = eastl::move( value->mName );
        mShadingModel = eastl::move( value->mShadingModel );
        mSubsurface = value->mSubsurface;
        mSurfaceShading = value->mSurfaceShading;
        mDielectric = value->mDielectric;
        mConductor = value->mConductor;
        mAlphaMask = eastl::move( value->mAlphaMask );
        mWorkflow = eastl::move( value->mWorkflow );

        // Keep scalar shading factors separate from the texture bindings that modulate them.
        mBaseColorFactor = eastl::move( value->mBaseColorFactor );
        mDiffuseFactor = eastl::move( value->mDiffuseFactor );
        mSpecularFactor = eastl::move( value->mSpecularFactor );
        mEmissiveFactor = eastl::move( value->mEmissiveFactor );
        mAoFactor = eastl::move( value->mAoFactor );
        mMetallicFactor = eastl::move( value->mMetallicFactor );
        mRoughnessFactor = eastl::move( value->mRoughnessFactor );
        mGlossinessFactor = eastl::move( value->mGlossinessFactor );
        mUvTransform = value->mUvTransform;
        mCheckerboard = value->mCheckerboard;
        mFoliageWind = value->mFoliageWind;
        mHeightMapping = value->mHeightMapping;
        mNormalScale = eastl::move( value->mNormalScale );
        mClearCoatNormalScale = value->mClearCoatNormalScale;
        mClearCoatNormalTextureSet = value->mClearCoatNormalTextureSet;
        mOcclusionStrength = eastl::move( value->mOcclusionStrength );
        mEmissiveStrength = eastl::move( value->mEmissiveStrength );
        mAlphaMaskCutoff = eastl::move( value->mAlphaMaskCutoff );

        mBaseColorTextureSet = eastl::move( value->mBaseColorTextureSet );
        mMetallicRoughnessTextureSet = eastl::move( value->mMetallicRoughnessTextureSet );
        mSpecularGlossinessTextureSet = eastl::move( value->mSpecularGlossinessTextureSet );
        mNormalTextureSet = eastl::move( value->mNormalTextureSet );
        mOcclusionTextureSet = eastl::move( value->mOcclusionTextureSet );
        mEmissiveTextureSet = eastl::move( value->mEmissiveTextureSet );
        mIsBloomy = eastl::move( value->mIsBloomy );
        mIsDoubleSided = eastl::move( value->mIsDoubleSided );

        // Transfer residency ownership together with the textures, samplers and usage flags.
        mStreamedTextures = eastl::move( value->mStreamedTextures );
        mTextures = eastl::move( value->mTextures );
        mUseTextures = value->mUseTextures;
        mSamplers = eastl::move( value->mSamplers );
    }

    auto SurfaceMaterial::GetGraph() -> MaterialGraph * {
        return &mGraph;
    }

    auto SurfaceMaterial::GetGraph() const -> const MaterialGraph * {
        return &mGraph;
    }

    auto SurfaceMaterial::GetName() const -> const eastl::string & {
        MKT_BEGIN_PROFILER();
        return mName;
    }

    auto SurfaceMaterial::SetName( const eastl::string_view newName ) -> void {
        MKT_BEGIN_PROFILER();
        mName = newName;
    }

    auto SurfaceMaterial::SerializeProperties() const -> YAML::Node {
        MKT_BEGIN_PROFILER();

        YAML::Node node;
        node["Name"] = GetName().c_str();
        // Scalar factors and texture coordinate sets.
        node["AoFactor"] = GetAoFactor();
        node["MetallicFactor"] = GetMetallicFactor();
        node["RoughnessFactor"] = GetRoughnessFactor();
        node["GlossinessFactor"] = GetGlossinessFactor();

        for ( core::i32 i{}; i < 4; ++i ) {
            node["UvTransform"].push_back( mUvTransform[i] );
        }

        node["Checkerboard"]["Enabled"] = mCheckerboard.mEnabled;
        node["Checkerboard"]["CellSize"] = mCheckerboard.mCellSize;

        for ( core::usize axis{}; axis < 3; ++axis ) {
            node["Checkerboard"]["ColorA"].push_back( mCheckerboard.mColorA[axis] );
            node["Checkerboard"]["ColorB"].push_back( mCheckerboard.mColorB[axis] );
            node["Checkerboard"]["Offset"].push_back( mCheckerboard.mOffset[axis] );
        }

        node["FoliageWind"]["Enabled"] = mFoliageWind.mEnabled;
        node["FoliageWind"]["Height"] = mFoliageWind.mHeight;
        node["FoliageWind"]["RootHeight"] = mFoliageWind.mRootHeight;
        node["FoliageWind"]["TrunkBend"] = mFoliageWind.mTrunkBend;
        node["FoliageWind"]["BranchMotion"] = mFoliageWind.mBranchMotion;
        node["FoliageWind"]["LeafFlutter"] = mFoliageWind.mLeafFlutter;
        node["FoliageWind"]["Frequency"] = mFoliageWind.mFrequency;

        node["HeightMapping"]["Mode"] = static_cast<core::u32>( mHeightMapping.mMode );
        node["HeightMapping"]["Scale"] = mHeightMapping.mScale;
        node["HeightMapping"]["Midlevel"] = mHeightMapping.mMidlevel;
        node["HeightMapping"]["UVSet"] = mHeightMapping.mUVSet;
        node["HeightMapping"]["Steps"] = mHeightMapping.mSteps;
        node["NormalScale"] = GetNormalScale();
        node["ClearCoatNormalScale"] = GetClearCoatNormalScale();
        node["ClearCoatNormalTextureSet"] = GetClearCoatNormalTextureSet();
        node["OcclusionStrength"] = GetOcclusionStrength();
        node["EmissiveStrength"] = GetEmissiveStrength();
        node["AlphaMaskCutoff"] = GetAlphaMaskCutoff();
        node["BaseColorTextureSet"] = GetBaseColorTextureSet();
        node["MetallicRoughnessTextureSet"] = GetMetallicRoughnessTextureSet();
        node["SpecularGlossinessSet"] = GetSpecularGlossinessSet();
        node["NormalTextureSet"] = GetNormalTextureSet();
        node["OcclusionTextureSet"] = GetOcclusionTextureSet();
        node["EmissiveTextureSet"] = GetEmissiveTextureSet();

        for ( core::usize component{}; component < 4; ++component ) {
            node["BaseColorFactor"].push_back( GetBaseColorFactor()[component] );
        }

        for ( core::usize component{}; component < 4; ++component ) {
            node["DiffuseFactor"].push_back( GetDiffuseFactor()[component] );
        }

        for ( core::usize component{}; component < 4; ++component ) {
            node["SpecularFactor"].push_back( GetSpecularFactor()[component] );
        }

        for ( core::usize component{}; component < 3; ++component ) {
            node["EmissiveFactor"].push_back( GetEmissiveFactor()[component] );
        }

        node["AlphaMask"] = static_cast<core::i32>( GetAlphaMask() );
        node["Workflow"] = static_cast<core::i32>( GetWorkflow() );
        node["ShadingModel"] = static_cast<core::i32>( GetShadingModel() );

        for ( int i{}; i < 3; ++i ) {
            node["Dielectric"]["AttenuationColor"].push_back( mDielectric.mAttenuationColor[i] );
        }

        node["Dielectric"]["IndexOfRefraction"] = mDielectric.mIndexOfRefraction;
        node["Dielectric"]["Transmission"] = mDielectric.mTransmission;
        node["Dielectric"]["Thickness"] = mDielectric.mThickness;
        node["Dielectric"]["AttenuationDistance"] = mDielectric.mAttenuationDistance;
        node["Dielectric"]["ThinWalled"] = mDielectric.mThinWalled;
        for ( u32 channel{}; channel < 3; ++channel ) {
            node["Dielectric"]["Scattering"].push_back( mDielectric.mScattering[channel] );
        }
        node["Dielectric"]["PhaseAnisotropy"] = mDielectric.mPhaseAnisotropy;
        for ( u32 channel{}; channel < 3; ++channel ) {
            node["Conductor"]["Eta"].push_back( mConductor.mEta[channel] );
            node["Conductor"]["K"].push_back( mConductor.mK[channel] );
        }

        // Surface shading and subsurface diffusion settings.
        const auto &shading{ GetSurfaceShading() };
        node["SurfaceShading"]["CoatWeight"] = shading.mCoatWeight;
        node["SurfaceShading"]["CoatRoughness"] = shading.mCoatRoughness;
        node["SurfaceShading"]["ToonThreshold"] = shading.mToonThreshold;
        node["SurfaceShading"]["ToonSoftness"] = shading.mToonSoftness;
        node["SurfaceShading"]["CellBands"] = shading.mCellBands;

        for ( core::i32 channel{}; channel < 3; ++channel ) {
            node["SurfaceShading"]["ContourColor"].push_back( shading.mContourColor[channel] );
        }

        node["SurfaceShading"]["ContourWidth"] = shading.mContourWidth;

        node["Subsurface"]["ModelVersion"] = 2;
        node["Subsurface"]["ScatterScale"] = GetSubsurface().mScatterScale;
        node["Subsurface"]["Strength"] = GetSubsurface().mStrength;
        const auto &subsurface{ GetSubsurface() };
        for ( u32 channel{}; channel < 3; ++channel ) {
            node["Subsurface"]["Absorption"].push_back( subsurface.mAbsorption[channel] );
            node["Subsurface"]["Scattering"].push_back( subsurface.mScattering[channel] );
        }
        node["Subsurface"]["IndexOfRefraction"] = subsurface.mIndexOfRefraction;
        node["Subsurface"]["Anisotropy"] = subsurface.mAnisotropy;
        node["Subsurface"]["UnitsPerMillimeter"] = subsurface.mUnitsPerMillimeter;
        node["Subsurface"]["Color"].push_back( subsurface.mColor.x );
        node["Subsurface"]["Color"].push_back( subsurface.mColor.y );
        node["Subsurface"]["Color"].push_back( subsurface.mColor.z );
        node["Subsurface"]["Opacity"] = subsurface.mOpacity;

        for ( core::i32 channel{}; channel < 3; ++channel ) {
            node["Subsurface"]["Near"].push_back( subsurface.mNear[channel] );
            node["Subsurface"]["Far"].push_back( subsurface.mFar[channel] );
        }

        node["Subsurface"]["NearWeight"] = subsurface.mNearWeight;
        node["Subsurface"]["DepthRejection"] = subsurface.mDepthRejection;
        node["Subsurface"]["Thickness"] = subsurface.mThickness;
        node["Subsurface"]["Translucency"] = subsurface.mTranslucency;
        node["Subsurface"]["Samples"] = subsurface.mSamples;
        node["DoubleSided"] = static_cast<core::i32>( IsDoubleSided() );
        node["Bloom"] = static_cast<core::i32>( IsBloomy() );
        // Persist edited model materials too; importing the model again must
        // not discard color, emission, or transparency changes made in Inspector.
        for ( core::i32 index{}; index <= static_cast<core::i32>( material::MapType::eHeight ); ++index ) {
            const auto type{ static_cast<material::MapType>( index ) };
            YAML::Node texture;
            texture["Map"] = index;
            const auto path{ GetTexturePath( type ) };
            texture["Present"] = HasTexture( type ) || !path.IsEmpty();
            texture["Used"] = IsTextureUsed( type );

            if ( !path.IsEmpty() ) {
                texture["TexturePath"] = path.GetC_Str();
            }

            node["Textures"].push_back( texture );
        }

        if ( !GetGraph()->GetNodes().empty() ) {

            node["Graph"] = GetGraph()->Serialize();
        }

        return node;
    }

    auto SurfaceMaterial::RestoreProperties( const YAML::Node &node ) -> void {
        MKT_BEGIN_PROFILER();

        if ( node["Graph"] ) {
            GetGraph()->Deserialize( node["Graph"] );
        }

        if ( node["Name"] ) {
            SetName( node["Name"].as<std::string>().c_str() );
        }

        if ( node["AoFactor"] ) {
            const auto value{ node["AoFactor"].as<core::f32>() };
            SetAoFactor( value );
        }

        if ( node["MetallicFactor"] ) {
            const auto value{ node["MetallicFactor"].as<core::f32>() };
            SetMetallicFactor( value );
        }

        if ( node["RoughnessFactor"] ) {
            const auto value{ node["RoughnessFactor"].as<core::f32>() };
            SetRoughnessFactor( value );
        }

        if ( node["GlossinessFactor"] ) {
            const auto value{ node["GlossinessFactor"].as<core::f32>() };
            SetGlossinessFactor( value );
        }

        if ( node["ClearCoatNormalScale"] ) {

            SetClearCoatNormalScale( node["ClearCoatNormalScale"].as<core::f32>() );
        }

        if ( node["ClearCoatNormalTextureSet"] ) {
            SetClearCoatNormalTextureSet( node["ClearCoatNormalTextureSet"].as<core::i32>() );
        }

        if ( const auto uv{ node["UvTransform"] }; uv && uv.IsSequence() && uv.size() == 4 ) {
            SetUvTransform( { uv[0].as<float>(), uv[1].as<float>(), uv[2].as<float>(), uv[3].as<float>() } );
        }

        // Missing settings preserve the appearance of materials saved before this feature.
        if ( const auto checker{ node["Checkerboard"] } ) {
            CheckerboardSettings settings{};
            settings.mEnabled = checker["Enabled"].as<bool>( false );
            settings.mCellSize = checker["CellSize"].as<core::f32>( 1.0f );

            const auto readColor{ [&]( core::cstr key, core::float3 &value ) {
                if ( const auto entry{ checker[key] }; entry && entry.IsSequence() && entry.size() == 3 ) {
                    value = { entry[0].as<core::f32>(), entry[1].as<core::f32>(), entry[2].as<core::f32>() };
                }
            } };

            readColor( "ColorA", settings.mColorA );
            readColor( "ColorB", settings.mColorB );
            readColor( "Offset", settings.mOffset );
            SetCheckerboard( settings );
        }

        if ( const auto foliage{ node["FoliageWind"] } ) {
            FoliageWindSettings settings{};
            settings.mEnabled = foliage["Enabled"].as<bool>( false );
            settings.mHeight = foliage["Height"].as<core::f32>( settings.mHeight );
            settings.mRootHeight = foliage["RootHeight"].as<core::f32>( settings.mRootHeight );
            settings.mTrunkBend = foliage["TrunkBend"].as<core::f32>( settings.mTrunkBend );
            settings.mBranchMotion = foliage["BranchMotion"].as<core::f32>( settings.mBranchMotion );
            settings.mLeafFlutter = foliage["LeafFlutter"].as<core::f32>( settings.mLeafFlutter );
            settings.mFrequency = foliage["Frequency"].as<core::f32>( settings.mFrequency );
            SetFoliageWind( settings );
        }

        if ( const auto height{ node["HeightMapping"] } ) {
            HeightMappingSettings settings{};
            const auto mode{ height["Mode"].as<core::u32>( 0 ) };
            if ( mode > 3 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid height mapping mode." );
            }
            settings.mMode = static_cast<HeightMappingMode>( mode );
            settings.mScale = height["Scale"].as<float>( 0.05f );
            settings.mMidlevel = height["Midlevel"].as<float>( 0.5f );
            settings.mUVSet = height["UVSet"].as<core::u32>( 0 );
            settings.mSteps = height["Steps"].as<core::u32>( 32 );
            SetHeightMapping( settings );
        }
        if ( node["NormalScale"] ) {
            const auto value{ node["NormalScale"].as<core::f32>() };
            SetNormalScale( value );
        }

        if ( node["OcclusionStrength"] ) {
            const auto value{ node["OcclusionStrength"].as<core::f32>() };
            SetOcclusionStrength( value );
        }

        if ( node["EmissiveStrength"] ) {
            const auto value{ node["EmissiveStrength"].as<core::f32>() };
            SetEmissiveStrength( value );
        }

        if ( node["AlphaMaskCutoff"] ) {
            const auto value{ node["AlphaMaskCutoff"].as<core::f32>() };
            SetAlphaMaskCutoff( value );
        }

        if ( node["BaseColorTextureSet"] ) {
            const auto value{ node["BaseColorTextureSet"].as<core::i32>() };
            SetBaseColorTextureSet( value );
        }

        if ( node["MetallicRoughnessTextureSet"] ) {
            const auto value{ node["MetallicRoughnessTextureSet"].as<core::i32>() };
            SetMetallicRoughnessTextureSet( value );
        }

        if ( node["SpecularGlossinessSet"] ) {
            const auto value{ node["SpecularGlossinessSet"].as<core::i32>() };
            SetSpecularGlossinessSet( value );
        }

        if ( node["NormalTextureSet"] ) {
            const auto value{ node["NormalTextureSet"].as<core::i32>() };
            SetNormalTextureSet( value );
        }

        if ( node["OcclusionTextureSet"] ) {
            const auto value{ node["OcclusionTextureSet"].as<core::i32>() };
            SetOcclusionTextureSet( value );
        }

        if ( node["EmissiveTextureSet"] ) {
            const auto value{ node["EmissiveTextureSet"].as<core::i32>() };
            SetEmissiveTextureSet( value );
        }

        if ( const auto value{ node["BaseColorFactor"] } ) {
            if ( !value.IsSequence() || value.size() != 4 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid material color." );
            }

            core::float4 color;

            for ( core::usize component{}; component < 4; ++component ) {
                color[component] = value[component].as<core::f32>();
            }

            SetBaseColorFactor( color );
        }

        if ( const auto value{ node["DiffuseFactor"] } ) {
            if ( !value.IsSequence() || value.size() != 4 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid material color." );
            }

            core::float4 color;

            for ( core::usize component{}; component < 4; ++component ) {
                color[component] = value[component].as<core::f32>();
            }

            SetDiffuseFactor( color );
        }

        if ( const auto value{ node["SpecularFactor"] } ) {
            if ( !value.IsSequence() || value.size() != 4 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid material color." );
            }

            core::float4 color;

            for ( core::usize component{}; component < 4; ++component ) {
                color[component] = value[component].as<core::f32>();
            }

            SetSpecularFactor( color );
        }

        if ( const auto value{ node["EmissiveFactor"] } ) {
            if ( !value.IsSequence() || value.size() != 3 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid material color." );
            }

            core::float3 color;

            for ( core::usize component{}; component < 3; ++component ) {
                color[component] = value[component].as<core::f32>();
            }

            SetEmissiveFactor( color );
        }

        if ( node["AlphaMask"] ) {
            const auto value{ node["AlphaMask"].as<core::i32>() };

            if ( value < 0 || value >= 3 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid material mode." );
            }

            SetAlphaMask( static_cast<material::AlphaMode>( value ) );
        }

        if ( node["Workflow"] ) {
            const auto value{ node["Workflow"].as<core::i32>() };

            if ( value < 0 || value >= 2 ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid material mode." );
            }

            SetWorkflow( static_cast<material::Workflow>( value ) );
        }

        if ( node["ShadingModel"] ) {
            const auto value{ node["ShadingModel"].as<core::i32>() };

            if ( value < 0 || value >= static_cast<core::i32>( ShadingModel::eCount ) ) {
                MKT_THROW_RUNTIME_ERROR( "Invalid material mode." );
            }

            SetShadingModel( static_cast<material::ShadingModel>( value ) );
        }

        if ( const auto dielectric{ node["Dielectric"] } ) {
            DielectricSettings settings{};

            if ( const auto color{ dielectric["AttenuationColor"] } ) {
                if ( !color.IsSequence() || color.size() != 3 ) {
                    MKT_THROW_RUNTIME_ERROR( "Attenuation color needs three components." );
                }

                settings.mAttenuationColor = { color[0].as<float>(), color[1].as<float>(), color[2].as<float>() };
            }

            settings.mIndexOfRefraction = dielectric["IndexOfRefraction"].as<float>( settings.mIndexOfRefraction );
            settings.mTransmission = dielectric["Transmission"].as<float>( settings.mTransmission );
            settings.mThickness = dielectric["Thickness"].as<float>( settings.mThickness );
            settings.mAttenuationDistance = dielectric["AttenuationDistance"].as<float>( settings.mAttenuationDistance );
            settings.mThinWalled = dielectric["ThinWalled"].as<bool>( false );
            if ( const auto scattering{ dielectric["Scattering"] } ) {
                if ( !scattering.IsSequence() || scattering.size() != 3 ) {
                    MKT_THROW_RUNTIME_ERROR( "Medium scattering needs three components." );
                }
                settings.mScattering = { scattering[0].as<f32>(), scattering[1].as<f32>(), scattering[2].as<f32>() };
            }
            settings.mPhaseAnisotropy = dielectric["PhaseAnisotropy"].as<f32>( 0.f );
            SetDielectric( settings );
        }

        if ( const auto conductor{ node["Conductor"] } ) {
            ConductorSettings settings{};
            for ( const auto key: { "Eta", "K" } ) {
                if ( const auto values{ conductor[key] } ) {
                    if ( !values.IsSequence() || values.size() != 3 ) {
                        MKT_THROW_RUNTIME_ERROR( "Conductor IOR needs three components." );
                    }
                    auto &target{ eastl::string_view{ key } == "Eta" ? settings.mEta : settings.mK };
                    target = { values[0].as<f32>(), values[1].as<f32>(), values[2].as<f32>() };
                }
            }
            SetConductor( settings );
        }

        if ( const auto shading{ node["SurfaceShading"] } ) {
            auto settings{ GetSurfaceShading() };

            if ( shading["CoatWeight"] ) {
                settings.mCoatWeight = shading["CoatWeight"].as<core::f32>();
            }

            if ( shading["CoatRoughness"] ) {
                settings.mCoatRoughness = shading["CoatRoughness"].as<core::f32>();
            }

            if ( shading["ToonThreshold"] ) {
                settings.mToonThreshold = shading["ToonThreshold"].as<core::f32>();
            }

            if ( shading["ToonSoftness"] ) {
                settings.mToonSoftness = shading["ToonSoftness"].as<core::f32>();
            }

            if ( shading["CellBands"] ) {
                settings.mCellBands = shading["CellBands"].as<core::i32>();
            }

            if ( shading["ContourWidth"] ) {

                settings.mContourWidth = shading["ContourWidth"].as<core::f32>();
            }

            if ( shading["ContourColor"] && shading["ContourColor"].IsSequence() && shading["ContourColor"].size() == 3 ) {
                for ( core::i32 channel{}; channel < 3; ++channel ) {
                    settings.mContourColor[channel] = shading["ContourColor"][channel].as<core::f32>();
                }
            }

            SetSurfaceShading( settings );
        }

        if ( const auto subsurface{ node["Subsurface"] } ) {
            auto settings{ GetSubsurface() };
            for ( const auto key: { "Absorption", "Scattering" } ) {
                if ( const auto values{ subsurface[key] } ) {
                    if ( !values.IsSequence() || values.size() != 3 ) {
                        MKT_THROW_RUNTIME_ERROR( "Subsurface coefficients need three components." );
                    }
                    auto &target{ eastl::string_view{ key } == "Absorption" ? settings.mAbsorption : settings.mScattering };
                    target = { values[0].as<f32>(), values[1].as<f32>(), values[2].as<f32>() };
                }
            }
            settings.mIndexOfRefraction = subsurface["IndexOfRefraction"].as<f32>( settings.mIndexOfRefraction );
            settings.mAnisotropy = subsurface["Anisotropy"].as<f32>( settings.mAnisotropy );
            settings.mUnitsPerMillimeter = subsurface["UnitsPerMillimeter"].as<f32>( settings.mUnitsPerMillimeter );

            if ( subsurface["ScatterScale"] ) {
                settings.mScatterScale = subsurface["ScatterScale"].as<core::f32>();
            }
            else if ( subsurface["Distance"] ) {
                // Legacy world-space distances have no camera-independent pixel
                // conversion. Use the new default, preserving an explicit disable.
                settings.mScatterScale = subsurface["Distance"].as<core::f32>() <= 0 ? 0.0f : 2.0f;
            }

            if ( subsurface["Strength"] ) {
                settings.mStrength = subsurface["Strength"].as<core::f32>();
            }

            if ( const auto color{ subsurface["Color"] }; color && color.IsSequence() && color.size() == 3 ) {
                settings.mColor = { color[0].as<core::f32>(), color[1].as<core::f32>(), color[2].as<core::f32>() };
            }

            if ( subsurface["Opacity"] ) {
                settings.mOpacity = subsurface["Opacity"].as<core::f32>();
            }

            for ( const auto key: { "Near", "Far" } ) {
                if ( const auto values{ subsurface[key] }; values && values.IsSequence() && values.size() == 3 ) {
                    auto &target{ key[0] == 'N' ? settings.mNear : settings.mFar };
                    target = { values[0].as<core::f32>(), values[1].as<core::f32>(), values[2].as<core::f32>() };
                }
            }

            if ( subsurface["NearWeight"] ) {

                settings.mNearWeight = subsurface["NearWeight"].as<core::f32>();
            }

            if ( subsurface["DepthRejection"] ) {
                settings.mDepthRejection = subsurface["DepthRejection"].as<core::f32>();
            }

            if ( subsurface["Thickness"] ) {
                settings.mThickness = subsurface["Thickness"].as<core::f32>();
            }

            if ( subsurface["Translucency"] ) {
                settings.mTranslucency = subsurface["Translucency"].as<core::f32>();
            }

            if ( subsurface["ScatterScale"] && subsurface["Samples"] ) {
                settings.mSamples = subsurface["Samples"].as<core::i32>();
            }

            SetSubsurface( settings );
        }

        if ( node["DoubleSided"] ) {
            SetIsDoubleSided( node["DoubleSided"].as<core::i32>() != 0 );
        }

        if ( node["Bloom"] ) {
            EnableBloom( node["Bloom"].as<core::i32>() != 0 );
        }

        // Restore references without eagerly loading streamed textures.
        if ( const auto textures{ node["Textures"] } ) {
            if ( !textures.IsSequence() ) {
                MKT_THROW_RUNTIME_ERROR( "Material textures must be a sequence." );
            }

            bool seen[static_cast<core::i32>( material::MapType::eHeight ) + 1]{};

            for ( const auto &entry: textures ) {
                const auto index{ entry["Map"].as<core::i32>() };

                if ( index < 0 || index > static_cast<core::i32>( material::MapType::eHeight ) ) {
                    MKT_THROW_RUNTIME_ERROR( "Invalid material texture slot." );
                }

                const auto type{ static_cast<material::MapType>( index ) };

                if ( seen[index] ) {
                    MKT_THROW_RUNTIME_ERROR( "Duplicate material texture slot." );
                }

                seen[index] = true;
                // Older material files sampled every assigned texture.
                UseTexture( type, entry["Used"].as<bool>( true ) );

                if ( !entry["Present"].as<bool>( true ) ) {
                    RemoveTexture( type );
                }
                else if ( entry["TexturePath"] ) {
                    const filesystem::Path path{ entry["TexturePath"].as<std::string>().c_str() };

                    if ( !path.Exists() ) {
                        MKT_THROW_RUNTIME_ERROR( string::Format( "Material '{}' texture (map {}) could not be found: {}",
                            GetName(), index, path.GetC_Str() ) );
                    }

                    SetTextureReference( type, path );
                }
            }
        }
    }

    auto SurfaceMaterial::GetSourcePath() const -> const filesystem::Path & {
        return mSourcePath;
    }
}// namespace mikoto::material
