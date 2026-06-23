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

#include <Renderer/Core/Rhi.hh>

namespace mikoto::material {

    PhysicalMaterial::PhysicalMaterial( const eastl::string_view name )
        : Material{ name } {
    }

    PhysicalMaterial::PhysicalMaterial( const PhysicMaterialDescription& props ) {
        m_AlphaMask = props.AlphaMask;
        m_Workflow = props.Workflow;

        m_BaseColorFactor = props.BaseColorFactor;
        m_DiffuseFactor = props.DiffuseFactor;
        m_SpecularFactor = props.SpecularFactor;
        m_EmissiveFactor = props.EmissiveFactor;

        m_MetallicFactor = props.MetallicFactor;
        m_RoughnessFactor = props.RoughnessFactor;
        m_GlossinessFactor = props.GlossinessFactor;
        m_NormalScale = props.NormalScale;
        m_OcclusionStrength = props.OcclusionStrength;
        m_EmissiveStrength = props.EmissiveStrength;
        m_AlphaMaskCutoff = props.AlphaMaskCutoff;

        m_BaseColorTextureSet = props.BaseColorTextureSet;
        m_MetallicRoughnessTextureSet = props.MetallicRoughnessTextureSet;
        m_SpecularGlossinessTextureSet = props.SpecularGlossinessSet;
        m_NormalTextureSet = props.NormalTextureSet;
        m_OcclusionTextureSet = props.OcclusionTextureSet;
        m_EmissiveTextureSet = props.EmissiveTextureSet;

        m_IsDoubleSided = props.mIsDoubleSided;

        for ( const auto& texture: props.mTexturesByUri | std::ranges::views::values ) {
            SetTexture( texture.MapType, texture.mTexture );
        }
    }

    auto PhysicalMaterial::RemoveTexture( MapType type ) -> void {
        m_Textures.erase( type );
    }

    auto PhysicalMaterial::GetAlphaMask() const -> AlphaMode {
        return m_AlphaMask;
    }

    auto PhysicalMaterial::SetAlphaMask( AlphaMode mode ) -> void {
        m_AlphaMask = mode;
    }

    auto PhysicalMaterial::GetWorkflow() const -> Workflow {
        return m_Workflow;
    }

    auto PhysicalMaterial::SetWorkflow( Workflow mode ) -> void {
        m_Workflow = mode;
    }

    auto PhysicalMaterial::GetBaseColorFactor() const -> const float4& {
        return m_BaseColorFactor;
    }

    auto PhysicalMaterial::SetBaseColorFactor( const float4& value ) -> void {
        m_BaseColorFactor = value;
    }

    auto PhysicalMaterial::GetDiffuseFactor() const -> const float4& {
        return m_DiffuseFactor;
    }

    auto PhysicalMaterial::SetDiffuseFactor( const float4& value ) -> void {
        m_DiffuseFactor = value;
    }

    auto PhysicalMaterial::GetSpecularFactor() const -> const float4& {
        return m_SpecularFactor;
    }

    auto PhysicalMaterial::SetSpecularFactor( const float4& value ) -> void {
        m_SpecularFactor = value;
    }

    auto PhysicalMaterial::GetEmissiveFactor() const -> const float3& {
        return m_EmissiveFactor;
    }

    auto PhysicalMaterial::GetAoFactor() const -> float {
        return m_AoFactor;
    }

    auto PhysicalMaterial::SetEmissiveFactor( const float3& value ) -> void {
        m_EmissiveFactor = value;
    }

    auto PhysicalMaterial::SetAoFactor( float v ) -> void {
        m_AoFactor = v;
    }

    auto PhysicalMaterial::GetMetallicFactor() const -> float {
        return m_MetallicFactor;
    }

    auto PhysicalMaterial::SetMetallicFactor( float v ) -> void {
        m_MetallicFactor = v;
    }

    auto PhysicalMaterial::GetRoughnessFactor() const -> float {
        return m_RoughnessFactor;
    }

    auto PhysicalMaterial::SetRoughnessFactor( float v ) -> void {
        m_RoughnessFactor = v;
    }

    auto PhysicalMaterial::GetGlossinessFactor() const -> float {
        return m_GlossinessFactor;
    }

    auto PhysicalMaterial::SetGlossinessFactor( float v ) -> void {
        m_GlossinessFactor = v;
    }

    auto PhysicalMaterial::GetNormalScale() const -> float {
        return m_NormalScale;
    }

    auto PhysicalMaterial::SetNormalScale( float v ) -> void {
        m_NormalScale = v;
    }

    auto PhysicalMaterial::GetOcclusionStrength() const -> float {
        return m_OcclusionStrength;
    }

    auto PhysicalMaterial::SetOcclusionStrength( float v ) -> void {
        m_OcclusionStrength = v;
    }

    auto PhysicalMaterial::GetEmissiveStrength() const -> float {
        return m_EmissiveStrength;
    }

    auto PhysicalMaterial::SetEmissiveStrength( float v ) -> void {
        m_EmissiveStrength = v;
    }

    auto PhysicalMaterial::GetAlphaMaskCutoff() const -> float {
        return m_AlphaMaskCutoff;
    }

    auto PhysicalMaterial::SetAlphaMaskCutoff( float v ) -> void {
        m_AlphaMaskCutoff = v;
    }

    auto PhysicalMaterial::SetIsDoubleSided( bool v ) -> void {
        m_IsDoubleSided = v;
    }

    auto PhysicalMaterial::GetBaseColorTextureSet() const -> i32 {
        return m_BaseColorTextureSet;
    }

    auto PhysicalMaterial::SetBaseColorTextureSet( i32 set ) -> void {
        m_BaseColorTextureSet = set;
    }

    auto PhysicalMaterial::GetSpecularGlossinessSet() const -> i32 {
        return m_SpecularGlossinessTextureSet;
    }

    auto PhysicalMaterial::GetMetallicRoughnessTextureSet() const -> i32 {
        return m_MetallicRoughnessTextureSet;
    }

    auto PhysicalMaterial::SetMetallicRoughnessTextureSet( i32 set ) -> void {
        m_MetallicRoughnessTextureSet = set;
    }

    auto PhysicalMaterial::SetSpecularGlossinessSet( i32 set ) -> void {
        m_SpecularGlossinessTextureSet = set;
    }

    auto PhysicalMaterial::GetNormalTextureSet() const -> i32 {
        return m_NormalTextureSet;
    }

    auto PhysicalMaterial::SetNormalTextureSet( i32 set ) -> void {
        m_NormalTextureSet = set;
    }

    auto PhysicalMaterial::GetOcclusionTextureSet() const -> i32 {
        return m_OcclusionTextureSet;
    }

    auto PhysicalMaterial::SetOcclusionTextureSet( i32 set ) -> void {
        m_OcclusionTextureSet = set;
    }

    auto PhysicalMaterial::GetEmissiveTextureSet() const -> i32 {
        return m_EmissiveTextureSet;
    }

    auto PhysicalMaterial::IsDoubleSided() const -> bool {
        return m_IsDoubleSided;
    }

    auto PhysicalMaterial::IsBloomy() const -> bool {
        return m_IsBloomy;
    }

    auto PhysicalMaterial::SetEmissiveTextureSet( i32 set ) -> void {
        m_EmissiveTextureSet = set;
    }

    auto PhysicalMaterial::EnableBloom( bool enable ) -> void {
        m_IsBloomy = enable;
    }

    auto PhysicalMaterial::SetTexture( const MapType type, const rhi::TextureHandle& texture ) -> void {
        m_Textures[type] = texture;
    }

    auto PhysicalMaterial::IsOpaque() const -> bool {
        return !IsTransparent();
    }

    auto PhysicalMaterial::IsTransparent() const -> bool {
        return m_AlphaMaskCutoff != 1.0f;
    }

    auto PhysicalMaterial::HasTexture( MapType type ) const -> bool {
        return m_Textures.contains( type );
    }

    auto PhysicalMaterial::GetTexture( const MapType type ) const -> rhi::TextureHandle {
        if ( const auto it{ m_Textures.find( type ) }; it != m_Textures.end() ) {
            return it->second;
        }

        return rhi::TextureHandle::CreateEmpty();
    }
}