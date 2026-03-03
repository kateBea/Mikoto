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

#include <Library/Math/Math.hh>
#include <Material/PBRMaterial.hh>
#include <Material/Texture2D.hh>

namespace Mikoto {

    PBRMaterial::PBRMaterial( const std::string_view name )
        : Material{ name } {
        Initialize();
    }

    PBRMaterial::PBRMaterial( const MaterialProperties& props ) {
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
    
    auto PBRMaterial::RemoveTexture( MapType type ) -> void {
        m_Textures.erase( type );
    }

    auto PBRMaterial::GetAlphaMask() const -> PBR_AlphaMode {
        return m_AlphaMask;
    }

    auto PBRMaterial::SetAlphaMask( PBR_AlphaMode mode ) -> void {
        m_AlphaMask = mode;
    }

    auto PBRMaterial::GetWorkflow() const -> PBR_Workflow {
        return m_Workflow;
    }

    auto PBRMaterial::SetWorkflow( PBR_Workflow mode ) -> void {
        m_Workflow = mode;
    }

    auto PBRMaterial::GetBaseColorFactor() const -> const Vec4F& {
        return m_BaseColorFactor;
    }

    auto PBRMaterial::SetBaseColorFactor( const Vec4F& value ) -> void {
        m_BaseColorFactor = value;
    }

    auto PBRMaterial::GetDiffuseFactor() const -> const Vec4F& {
        return m_DiffuseFactor;
    }

    auto PBRMaterial::SetDiffuseFactor( const Vec4F& value ) -> void {
        m_DiffuseFactor = value;
    }

    auto PBRMaterial::GetSpecularFactor() const -> const Vec4F& {
        return m_SpecularFactor;
    }

    auto PBRMaterial::SetSpecularFactor( const Vec4F& value ) -> void {
        m_SpecularFactor = value;
    }

    auto PBRMaterial::GetEmissiveFactor() const -> const Vec3F& {
        return m_EmissiveFactor;
    }

    auto PBRMaterial::GetAoFactor() const -> float {
        return m_AoFactor;
    }

    auto PBRMaterial::SetEmissiveFactor( const Vec3F& value ) -> void {
        m_EmissiveFactor = value;
    }

    auto PBRMaterial::SetAoFactor( float v ) -> void {
        m_AoFactor = v;
    }

    auto PBRMaterial::GetMetallicFactor() const -> float {
        return m_MetallicFactor;
    }

    auto PBRMaterial::SetMetallicFactor( float v ) -> void {
        m_MetallicFactor = v;
    }

    auto PBRMaterial::GetRoughnessFactor() const -> float {
        return m_RoughnessFactor;
    }

    auto PBRMaterial::SetRoughnessFactor( float v ) -> void {
        m_RoughnessFactor = v;
    }

    auto PBRMaterial::GetGlossinessFactor() const -> float {
        return m_GlossinessFactor;
    }

    auto PBRMaterial::SetGlossinessFactor( float v ) -> void {
        m_GlossinessFactor = v;
    }

    auto PBRMaterial::GetNormalScale() const -> float {
        return m_NormalScale;
    }

    auto PBRMaterial::SetNormalScale( float v ) -> void {
        m_NormalScale = v;
    }

    auto PBRMaterial::GetOcclusionStrength() const -> float {
        return m_OcclusionStrength;
    }

    auto PBRMaterial::SetOcclusionStrength( float v ) -> void {
        m_OcclusionStrength = v;
    }

    auto PBRMaterial::GetEmissiveStrength() const -> float {
        return m_EmissiveStrength;
    }

    auto PBRMaterial::SetEmissiveStrength( float v ) -> void {
        m_EmissiveStrength = v;
    }

    auto PBRMaterial::GetAlphaMaskCutoff() const -> float {
        return m_AlphaMaskCutoff;
    }

    auto PBRMaterial::SetAlphaMaskCutoff( float v ) -> void {
        m_AlphaMaskCutoff = v;
    }

    auto PBRMaterial::SetIsDoubleSided( bool v ) -> void {
        m_IsDoubleSided = v;
    }

    auto PBRMaterial::GetBaseColorTextureSet() const -> Int32 {
        return m_BaseColorTextureSet;
    }

    auto PBRMaterial::SetBaseColorTextureSet( Int32 set ) -> void {
        m_BaseColorTextureSet = set;
    }

    auto PBRMaterial::GetSpecularGlossinessSet() const -> Int32 {
        return m_SpecularGlossinessTextureSet;
    }

    auto PBRMaterial::GetMetallicRoughnessTextureSet() const -> Int32 {
        return m_MetallicRoughnessTextureSet;
    }

    auto PBRMaterial::SetMetallicRoughnessTextureSet( Int32 set ) -> void {
        m_MetallicRoughnessTextureSet = set;
    }

    auto PBRMaterial::SetSpecularGlossinessSet( Int32 set ) -> void {
        m_SpecularGlossinessTextureSet = set;
    }

    auto PBRMaterial::GetNormalTextureSet() const -> Int32 {
        return m_NormalTextureSet;
    }

    auto PBRMaterial::SetNormalTextureSet( Int32 set ) -> void {
        m_NormalTextureSet = set;
    }

    auto PBRMaterial::GetOcclusionTextureSet() const -> Int32 {
        return m_OcclusionTextureSet;
    }

    auto PBRMaterial::SetOcclusionTextureSet( Int32 set ) -> void {
        m_OcclusionTextureSet = set;
    }

    auto PBRMaterial::GetEmissiveTextureSet() const -> Int32 {
        return m_EmissiveTextureSet;
    }

    auto PBRMaterial::IsDoubleSided() const -> bool {
        return m_IsDoubleSided;
    }

    auto PBRMaterial::SetEmissiveTextureSet( Int32 set ) -> void {
        m_EmissiveTextureSet = set;
    }


    auto PBRMaterial::SetTexture( const MapType type, const TextureHandle& texture ) -> void {
        m_Textures[type] = texture;
    }

    auto PBRMaterial::IsOpaque() const -> bool {
        return !IsTransparent();
    }

    auto PBRMaterial::IsTransparent() const -> bool {
        return m_AlphaMaskCutoff != 1.0f;
    }

    auto PBRMaterial::HasTexture( MapType type ) const -> bool {
        return m_Textures.contains( type );
    }

    auto PBRMaterial::GetTexture( const MapType type ) const -> TextureHandle {
        if ( const auto it{ m_Textures.find( type ) }; it != m_Textures.end() ) {
            return it->second;
        }

        return TextureHandle::CreateEmpty();
    }

    PBRMaterial::~PBRMaterial() {
        if ( m_IsAllocated ) {
            Release();
        }
    }

    auto PBRMaterial::Release() -> void {
        m_IsAllocated = false;
    }

    auto PBRMaterial::Initialize() -> void {
        m_IsAllocated = true;
    }
}