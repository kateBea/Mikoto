//    Copyright 2025 ケイト
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

#include <ranges>

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Math/Math.hh>

#include <Material/PhysicalMaterial.hh>

namespace mikoto::material {

    using namespace mikoto::core;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    PhysicalMaterial::PhysicalMaterial( const eastl::string_view name )
        : Material{ name } {
    }

    PhysicalMaterial::PhysicalMaterial( const PhysicMaterialDescription& props ) {
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

        for ( const auto& texture: props.mTexturesByUri | std::ranges::views::values ) {
            SetTexture( texture.mMapType, texture.mTexture );
        }
    }

    auto PhysicalMaterial::Serialize( const filesystem::Path& filename ) const -> void {

    }

    auto PhysicalMaterial::Deserialize( const filesystem::Path& filename ) const -> void {

    }

    auto PhysicalMaterial::Serialize( filesystem::FileHandle file ) const -> void {

    }

    auto PhysicalMaterial::Deserialize( filesystem::FileHandle file ) const -> void {

    }

    auto PhysicalMaterial::RemoveTexture( MapType type ) -> void {
        mTextures.erase( type );
    }

    auto PhysicalMaterial::GetAlphaMask() const -> AlphaMode {
        return mAlphaMask;
    }

    auto PhysicalMaterial::SetAlphaMask( AlphaMode mode ) -> void {
        mAlphaMask = mode;
    }

    auto PhysicalMaterial::GetWorkflow() const -> Workflow {
        return mWorkflow;
    }

    auto PhysicalMaterial::SetWorkflow( Workflow mode ) -> void {
        mWorkflow = mode;
    }

    auto PhysicalMaterial::GetBaseColorFactor() const -> const float4& {
        return mBaseColorFactor;
    }

    auto PhysicalMaterial::SetBaseColorFactor( const float4& value ) -> void {
        mBaseColorFactor = value;
    }

    auto PhysicalMaterial::GetDiffuseFactor() const -> const float4& {
        return mDiffuseFactor;
    }

    auto PhysicalMaterial::SetDiffuseFactor( const float4& value ) -> void {
        mDiffuseFactor = value;
    }

    auto PhysicalMaterial::GetSpecularFactor() const -> const float4& {
        return mSpecularFactor;
    }

    auto PhysicalMaterial::SetSpecularFactor( const float4& value ) -> void {
        mSpecularFactor = value;
    }

    auto PhysicalMaterial::GetEmissiveFactor() const -> const float3& {
        return mEmissiveFactor;
    }

    auto PhysicalMaterial::GetAoFactor() const -> float {
        return mAoFactor;
    }

    auto PhysicalMaterial::SetEmissiveFactor( const float3& value ) -> void {
        mEmissiveFactor = value;
    }

    auto PhysicalMaterial::SetAoFactor( float v ) -> void {
        mAoFactor = v;
    }

    auto PhysicalMaterial::GetMetallicFactor() const -> float {
        return mMetallicFactor;
    }

    auto PhysicalMaterial::SetMetallicFactor( float v ) -> void {
        mMetallicFactor = v;
    }

    auto PhysicalMaterial::GetRoughnessFactor() const -> float {
        return mRoughnessFactor;
    }

    auto PhysicalMaterial::SetRoughnessFactor( float v ) -> void {
        mRoughnessFactor = v;
    }

    auto PhysicalMaterial::GetGlossinessFactor() const -> float {
        return mGlossinessFactor;
    }

    auto PhysicalMaterial::SetGlossinessFactor( float v ) -> void {
        mGlossinessFactor = v;
    }

    auto PhysicalMaterial::GetNormalScale() const -> float {
        return mNormalScale;
    }

    auto PhysicalMaterial::SetNormalScale( float v ) -> void {
        mNormalScale = v;
    }

    auto PhysicalMaterial::GetOcclusionStrength() const -> float {
        return mOcclusionStrength;
    }

    auto PhysicalMaterial::SetOcclusionStrength( float v ) -> void {
        mOcclusionStrength = v;
    }

    auto PhysicalMaterial::GetEmissiveStrength() const -> float {
        return mEmissiveStrength;
    }

    auto PhysicalMaterial::SetEmissiveStrength( float v ) -> void {
        mEmissiveStrength = v;
    }

    auto PhysicalMaterial::GetAlphaMaskCutoff() const -> float {
        return mAlphaMaskCutoff;
    }

    auto PhysicalMaterial::SetAlphaMaskCutoff( float v ) -> void {
        mAlphaMaskCutoff = v;
    }

    auto PhysicalMaterial::SetIsDoubleSided( bool v ) -> void {
        mIsDoubleSided = v;
    }

    auto PhysicalMaterial::GetBaseColorTextureSet() const -> i32 {
        return mBaseColorTextureSet;
    }

    auto PhysicalMaterial::SetBaseColorTextureSet( i32 set ) -> void {
        mBaseColorTextureSet = set;
    }

    auto PhysicalMaterial::GetSpecularGlossinessSet() const -> i32 {
        return mSpecularGlossinessTextureSet;
    }

    auto PhysicalMaterial::GetMetallicRoughnessTextureSet() const -> i32 {
        return mMetallicRoughnessTextureSet;
    }

    auto PhysicalMaterial::SetMetallicRoughnessTextureSet( i32 set ) -> void {
        mMetallicRoughnessTextureSet = set;
    }

    auto PhysicalMaterial::SetSpecularGlossinessSet( i32 set ) -> void {
        mSpecularGlossinessTextureSet = set;
    }

    auto PhysicalMaterial::GetNormalTextureSet() const -> i32 {
        return mNormalTextureSet;
    }

    auto PhysicalMaterial::SetNormalTextureSet( i32 set ) -> void {
        mNormalTextureSet = set;
    }

    auto PhysicalMaterial::GetOcclusionTextureSet() const -> i32 {
        return mOcclusionTextureSet;
    }

    auto PhysicalMaterial::SetOcclusionTextureSet( i32 set ) -> void {
        mOcclusionTextureSet = set;
    }

    auto PhysicalMaterial::GetEmissiveTextureSet() const -> i32 {
        return mEmissiveTextureSet;
    }

    auto PhysicalMaterial::IsDoubleSided() const -> bool {
        return mIsDoubleSided;
    }

    auto PhysicalMaterial::IsBloomy() const -> bool {
        return mIsBloomy;
    }

    auto PhysicalMaterial::SetEmissiveTextureSet( i32 set ) -> void {
        mEmissiveTextureSet = set;
    }

    auto PhysicalMaterial::EnableBloom( bool enable ) -> void {
        mIsBloomy = enable;
    }

    auto PhysicalMaterial::SetTexture( const MapType type, const rhi::TextureHandle& texture ) -> void {
        mTextures[type] = texture;
    }

    auto PhysicalMaterial::IsOpaque() const -> bool {
        return !IsTransparent();
    }

    auto PhysicalMaterial::IsTransparent() const -> bool {
        return mAlphaMaskCutoff != 1.0f;
    }

    auto PhysicalMaterial::HasTexture( MapType type ) const -> bool {
        return mTextures.contains( type );
    }

    auto PhysicalMaterial::GetTexture( const MapType type ) const -> rhi::TextureHandle {
        if ( const auto it{ mTextures.find( type ) }; it != mTextures.end() ) {
            return it->second;
        }

        return rhi::TextureHandle::CreateEmpty();
    }

    auto PhysicalMaterial::SetShadingModel( ShadingModel model ) -> void {
        mShadingModel = model;
    }

    auto PhysicalMaterial::GetShadingModel() const -> ShadingModel {
        return mShadingModel;
    }
}// namespace mikoto::material