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

#include <Material/PBRMaterial.hh>
#include <Material/Texture2D.hh>
#include <Library/Math/Math.hh>

namespace Mikoto {

    PBRMaterial::PBRMaterial( std::string_view name )
        : Material{ name } {
        Initialize();
    }

    PBRMaterial::PBRMaterial( const MaterialProperties &props ) {
        for (const auto& texture : props.TexturesByUri | std::ranges::views::values ) {
            SetTextureType( texture.As<Texture2D>()->GetMapType(), texture );
        }

        m_AmbientOcclusion = props.OcclusionStrength;
        m_Roughness = props.RoughnessFactor;
        m_Metallic = props.MetallicFactor;
        m_Emissive = props.EmissiveStrength;
        m_EmissiveFactors = props.EmissiveFactor;
    }

    auto PBRMaterial::RemoveTextureType( MapType type ) -> void {
        m_Textures.erase( type );
    }

    auto PBRMaterial::SetTextureType( const MapType type, const TextureHandle& texture ) -> void {
        m_Textures[ type ] = texture;
    }

    auto PBRMaterial::IsOpaque() const -> bool {
        return !IsTransparent();
    }

    auto PBRMaterial::IsTransparent() const -> bool {
        return m_Alpha != 1.0f;
    }

    auto PBRMaterial::HasTextureType( MapType type ) const -> bool {
        return m_Textures.contains( type );
    }

    auto PBRMaterial::GetTextureType( const MapType type ) const -> TextureHandle {
        if ( const auto it{ m_Textures.find( type ) }; it != m_Textures.end() ) {
            return it->second;
        }

        return TextureHandle::CreateEmpty();
    }

    auto PBRMaterial::SetBlendMode( Blending alphaMode ) -> void {
        m_BlendMode = alphaMode;
    }

    auto PBRMaterial::SetSamplingMode( SamplerFilter filtering ) -> void {
        m_Filtering = filtering;
    }

    auto PBRMaterial::SetAlpha( float alpha ) -> void {
        m_Alpha = Math::Clamp( alpha, 0.0f, 1.0f );
    }

    auto PBRMaterial::SetMetallicFactor( float metallic ) -> void {
        m_Metallic = metallic;
    }

    auto PBRMaterial::SetRoughnessFactor( float roughness ) -> void {
        m_Roughness = roughness;
    }

    auto PBRMaterial::SetReflectanceFactor( float reflectance ) -> void {
        m_ReflectanceFactor = reflectance;
    }

    auto PBRMaterial::SetAoFactor( float ao ) -> void {
        m_AmbientOcclusion = ao;
    }

    auto PBRMaterial::SetEmissiveFactors( const Vec3F &factors ) -> void {
        m_EmissiveFactors = factors;
    }

    auto PBRMaterial::GetAoFactor() const -> float {
        return m_AmbientOcclusion;
    }

    auto PBRMaterial::SetEmissiveIntensity( float emissive ) -> void {
        m_Emissive = emissive;
    }

    auto PBRMaterial::GetAlpha() const -> float {
        return m_Alpha;
    }

    auto PBRMaterial::GetMetallicFactor() const -> float {
        return m_Metallic;
    }

    auto PBRMaterial::GetRoughnessFactor() const -> float {
        return m_Roughness;
    }

    auto PBRMaterial::GetReflectanceFactor() const -> float {
        return m_ReflectanceFactor;
    }

    auto PBRMaterial::GetEmissiveFactors() const -> const Vec3F & {
        return m_EmissiveFactors;
    }

    auto PBRMaterial::GetEmissiveIntensity() const -> float {
        return m_Emissive;
    }

    PBRMaterial::~PBRMaterial() {
        if (m_IsAllocated) {
            Release();
        }
    }

    auto PBRMaterial::Release() -> void {
        m_IsAllocated = false;
    }

    auto PBRMaterial::Initialize() -> void {
        m_IsAllocated = true;
    }
}// namespace Mikoto