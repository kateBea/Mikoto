//
// Created by zanet on 3/29/2025.
//

#include <Material/PBRMaterial.hh>
#include <Library/Math/Math.hh>

namespace Mikoto {

    PBRMaterial::PBRMaterial( std::string_view name )
        : Material{ name } {
        Initialize();
    }
    auto PBRMaterial::RemoveTextureType( MapType type ) -> void {
        m_Textures.erase( type );
    }

    auto PBRMaterial::SetTextureType( const MapType type, const TextureHandle& texture ) -> void {
        m_Textures[ type ] = texture;
    }

    auto PBRMaterial::IsOpaque() const -> bool {
        return m_Alpha == 1.0f;
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

    auto PBRMaterial::SetEmissiveFactor( float emissive ) -> void {
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

    auto PBRMaterial::GetEmissiveFactor() const -> float {
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