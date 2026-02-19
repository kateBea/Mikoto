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

#ifndef MIKOTO_PHYSICALLY_BASED_MATERIAL_HH
#define MIKOTO_PHYSICALLY_BASED_MATERIAL_HH

#include <ankerl/unordered_dense.h>

#include <Assets/Texture.hh>
#include <Common/Common.hh>
#include <Material/Material.hh>
#include <Renderer/Core/RenderUtility.hh>

namespace Mikoto {

    enum class PBR_Workflow {
        MetallicRoughness,
        SpecularGlossiness,
        Unlit
    };

    enum class PBR_AlphaMode {
        Opaque,
        Mask,
        Blend
    };

    struct MaterialProperties {
        std::string Name{};

        PBR_AlphaMode AlphaMask{ PBR_AlphaMode::Opaque };
        PBR_Workflow Workflow{ PBR_Workflow::MetallicRoughness };

        // Base color/Albedo
        Vec4F BaseColorFactor{ 1.f, 1.f, 1.f, 1.f };
        Vec4F DiffuseFactor{ 1.f, 1.f, 1.f, 1.f };
        Vec4F SpecularFactor{ 1.f, 1.f, 1.f, 1.f };
        Vec3F EmissiveFactor{ 0.f, 0.f, 0.f };

        float MetallicFactor{ 1.f };
        float RoughnessFactor{ 1.f };
        float GlossinessFactor{ 1.f };
        float NormalScale{ 1.f };
        float OcclusionStrength{ 1.f };
        float EmissiveStrength{ 1.f };
        float AlphaMaskCutoff{ 1.0f };

        // Texture UV sets (Maps can either use UV0 or UV1)
        // UV0 assumed by default unless otherwise specified
        Int32 BaseColorTextureSet{};
        Int32 PhysicalDescriptorTextureSet{};
        Int32 NormalTextureSet{};
        Int32 OcclusionTextureSet{};
        Int32 EmissiveTextureSet{};

        ankerl::unordered_dense::map<std::string, TextureHandle> TexturesByUri{};
    };

    class PBRMaterial final : public Material {
    public:
        explicit PBRMaterial( std::string_view name = "PBR" );
        explicit PBRMaterial( const MaterialProperties& props );

        auto RemoveTexture( MapType type ) -> void;
        auto SetTexture( MapType type, const TextureHandle& texture ) -> void;

        MKT_NODISCARD auto IsOpaque() const -> bool;
        MKT_NODISCARD auto IsTransparent() const -> bool;

        // ===============================
        // Scalar & Factor Setters
        // ===============================
        auto SetAlphaMask( PBR_AlphaMode mode ) -> void;
        auto SetWorkflow( PBR_Workflow mode ) -> void;

        auto SetBaseColorFactor( const Vec4F& value ) -> void;
        auto SetDiffuseFactor( const Vec4F& value ) -> void;
        auto SetSpecularFactor( const Vec4F& value ) -> void;
        auto SetEmissiveFactor( const Vec3F& value ) -> void;

        auto SetAoFactor( float v ) -> void;
        auto SetMetallicFactor( float v ) -> void;
        auto SetRoughnessFactor( float v ) -> void;
        auto SetGlossinessFactor( float v ) -> void;
        auto SetNormalScale( float v ) -> void;
        auto SetOcclusionStrength( float v ) -> void;
        auto SetEmissiveStrength( float v ) -> void;
        auto SetAlphaMaskCutoff( float v ) -> void;

        // ===============================
        // UV Set Setters
        // ===============================
        auto SetBaseColorTextureSet( Int32 set ) -> void;
        auto SetPhysicalDescriptorTextureSet( Int32 set ) -> void;
        auto SetNormalTextureSet( Int32 set ) -> void;
        auto SetOcclusionTextureSet( Int32 set ) -> void;
        auto SetEmissiveTextureSet( Int32 set ) -> void;

        // ===============================
        // Scalar & Factor Getters
        // ===============================
        MKT_NODISCARD auto GetAlphaMask() const -> PBR_AlphaMode;
        MKT_NODISCARD auto GetWorkflow() const -> PBR_Workflow;

        MKT_NODISCARD auto GetBaseColorFactor() const -> const Vec4F&;
        MKT_NODISCARD auto GetDiffuseFactor() const -> const Vec4F&;
        MKT_NODISCARD auto GetSpecularFactor() const -> const Vec4F&;
        MKT_NODISCARD auto GetEmissiveFactor() const -> const Vec3F&;

        MKT_NODISCARD auto GetAoFactor() const -> float;
        MKT_NODISCARD auto GetMetallicFactor() const -> float;
        MKT_NODISCARD auto GetRoughnessFactor() const -> float;
        MKT_NODISCARD auto GetGlossinessFactor() const -> float;
        MKT_NODISCARD auto GetNormalScale() const -> float;
        MKT_NODISCARD auto GetOcclusionStrength() const -> float;
        MKT_NODISCARD auto GetEmissiveStrength() const -> float;
        MKT_NODISCARD auto GetAlphaMaskCutoff() const -> float;

        // ===============================
        // UV Set Getters
        // ===============================
        MKT_NODISCARD auto GetBaseColorTextureSet() const -> Int32;
        MKT_NODISCARD auto GetPhysicalDescriptorTextureSet() const -> Int32;
        MKT_NODISCARD auto GetNormalTextureSet() const -> Int32;
        MKT_NODISCARD auto GetOcclusionTextureSet() const -> Int32;
        MKT_NODISCARD auto GetEmissiveTextureSet() const -> Int32;

        MKT_NODISCARD auto HasTexture( MapType type ) const -> bool;
        MKT_NODISCARD auto GetTexture( MapType type ) const -> TextureHandle;

        ~PBRMaterial() override;

    private:
        auto Release() -> void override;
        auto Initialize() -> void override;

    private:
        PBR_AlphaMode m_AlphaMask{ PBR_AlphaMode::Opaque };
        PBR_Workflow m_Workflow{ PBR_Workflow::MetallicRoughness };

        // Base color/Albedo
        Vec4F m_BaseColorFactor{ 1.f, 1.f, 1.f, 1.f };
        Vec4F m_DiffuseFactor{ 1.f, 1.f, 1.f, 1.f };
        Vec4F m_SpecularFactor{ 1.f, 1.f, 1.f, 1.f };
        Vec3F m_EmissiveFactor{ 0.f, 0.f, 0.f };

        // Scalars
        float m_AoFactor{ 1.f };
        float m_MetallicFactor{ 1.f };
        float m_RoughnessFactor{ 1.f };
        float m_GlossinessFactor{ 1.f };
        float m_NormalScale{ 1.f };
        float m_OcclusionStrength{ 1.f };
        float m_EmissiveStrength{ 1.f };
        float m_AlphaMaskCutoff{ 1.0f };

        // UV sets
        Int32 m_BaseColorTextureSet{};
        Int32 m_PhysicalDescriptorTextureSet{};
        Int32 m_NormalTextureSet{};
        Int32 m_OcclusionTextureSet{};
        Int32 m_EmissiveTextureSet{};

        // Note: Materials reference both textures and samplers.
        // Textures provide the image data, while samplers define how that data is read
        // (filtering, addressing, LOD behavior, etc.).
        // This separation allows the same texture to be reused across materials
        // with different sampling settings.
        ankerl::unordered_dense::map<MapType, TextureHandle> m_Textures{};
        ankerl::unordered_dense::map<MapType, SamplerHandle> m_Samplers{};
    };
}

#endif//MIKOTO_PHYSICALLY_BASED_MATERIAL_HH
