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

#include <Library/Math/Math.hh>
#include <Material/PhysicalMaterial.hh>
#include <Material/Texture2D.hh>
#include <ranges>

namespace Mikoto {

    PhysicalMaterial::PhysicalMaterial( const std::string_view name )
        : Material{ name } {
        Initialize();
    }

    PhysicalMaterial::PhysicalMaterial( const MaterialProperties& props ) {
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

        m_IsDoubleSided = props.IsDoubleSided;

        for ( const auto& texture: props.TexturesByUri | std::ranges::views::values ) {
            SetTexture( texture.As<Texture2D>()->GetMapType(), texture );
        }

        Initialize();
    }
    
    auto PhysicalMaterial::RemoveTexture( MapType type ) -> void {
        m_Textures.erase( type );
    }
    
    auto PhysicalMaterial::GetAlphaMask() const -> PBR_AlphaMode {
        return m_AlphaMask;
    }
    
    auto PhysicalMaterial::SetAlphaMask( PBR_AlphaMode mode ) -> void {
        m_AlphaMask = mode;
    }
    
    auto PhysicalMaterial::GetWorkflow() const -> PBR_Workflow {
        return m_Workflow;
    }

    auto PhysicalMaterial::SetWorkflow( PBR_Workflow mode ) -> void {
        m_Workflow = mode;
    }

    auto PhysicalMaterial::GetBaseColorFactor() const -> const Vec4F& {
        return m_BaseColorFactor;
    }

    auto PhysicalMaterial::SetBaseColorFactor( const Vec4F& value ) -> void {
        m_BaseColorFactor = value;
    }

    auto PhysicalMaterial::GetDiffuseFactor() const -> const Vec4F& {
        return m_DiffuseFactor;
    }

    auto PhysicalMaterial::SetDiffuseFactor( const Vec4F& value ) -> void {
        m_DiffuseFactor = value;
    }

    auto PhysicalMaterial::GetSpecularFactor() const -> const Vec4F& {
        return m_SpecularFactor;
    }

    auto PhysicalMaterial::SetSpecularFactor( const Vec4F& value ) -> void {
        m_SpecularFactor = value;
    }

    auto PhysicalMaterial::GetEmissiveFactor() const -> const Vec3F& {
        return m_EmissiveFactor;
    }

    auto PhysicalMaterial::GetAoFactor() const -> float {
        return m_AoFactor;
    }

    auto PhysicalMaterial::SetEmissiveFactor( const Vec3F& value ) -> void {
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

    auto PhysicalMaterial::GetBaseColorTextureSet() const -> Int32 {
        return m_BaseColorTextureSet;
    }

    auto PhysicalMaterial::SetBaseColorTextureSet( Int32 set ) -> void {
        m_BaseColorTextureSet = set;
    }

    auto PhysicalMaterial::GetSpecularGlossinessSet() const -> Int32 {
        return m_SpecularGlossinessTextureSet;
    }

    auto PhysicalMaterial::GetMetallicRoughnessTextureSet() const -> Int32 {
        return m_MetallicRoughnessTextureSet;
    }

    auto PhysicalMaterial::SetMetallicRoughnessTextureSet( Int32 set ) -> void {
        m_MetallicRoughnessTextureSet = set;
    }

    auto PhysicalMaterial::SetSpecularGlossinessSet( Int32 set ) -> void {
        m_SpecularGlossinessTextureSet = set;
    }

    auto PhysicalMaterial::GetNormalTextureSet() const -> Int32 {
        return m_NormalTextureSet;
    }

    auto PhysicalMaterial::SetNormalTextureSet( Int32 set ) -> void {
        m_NormalTextureSet = set;
    }

    auto PhysicalMaterial::GetOcclusionTextureSet() const -> Int32 {
        return m_OcclusionTextureSet;
    }

    auto PhysicalMaterial::SetOcclusionTextureSet( Int32 set ) -> void {
        m_OcclusionTextureSet = set;
    }

    auto PhysicalMaterial::GetEmissiveTextureSet() const -> Int32 {
        return m_EmissiveTextureSet;
    }

    auto PhysicalMaterial::IsDoubleSided() const -> bool {
        return m_IsDoubleSided;
    }

    auto PhysicalMaterial::IsBloomy() const -> bool {
        return m_IsBloomy;
    }

    auto PhysicalMaterial::SetEmissiveTextureSet( Int32 set ) -> void {
        m_EmissiveTextureSet = set;
    }

    auto PhysicalMaterial::EnableBloom( bool enable ) -> void {
        m_IsBloomy = enable;
    }

    auto PhysicalMaterial::SetTexture( const MapType type, const TextureHandle& texture ) -> void {
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

    auto PhysicalMaterial::GetTexture( const MapType type ) const -> TextureHandle {
        if ( const auto it{ m_Textures.find( type ) }; it != m_Textures.end() ) {
            return it->second;
        }

        return TextureHandle::CreateEmpty();
    }

    PhysicalMaterial::~PhysicalMaterial() {
        if ( m_IsAllocated ) {
            Release();
        }
    }

    auto PhysicalMaterial::Release() -> void {
        m_IsAllocated = false;
    }

    auto PhysicalMaterial::Initialize() -> void {
        m_IsAllocated = true;
    }
}