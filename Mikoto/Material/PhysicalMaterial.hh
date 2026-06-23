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

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>

#include <Material/Material.hh>

#include <Renderer/Core/Rhi.hh>

namespace mikoto::material {

    using namespace mikoto::core;
    using namespace mikoto::renderer;

    enum class Workflow {
        eMetallicRoughness,
        eSpecularGlossiness,
        eCount,
    };

    enum class AlphaMode {
        Opaque,
        eMask,
        eBlend,
        eCount,
    };

    enum class MapType {
        eInvalid = -1,

        eDiffuse,
        eBaseColor,
        eNormal,
        eMetallic,
        eRoughness,
        eMetallicRoughness,
        eAmbientOcclusion,
        eEmissive,
        eSpecularGlossiness,
    };

    // I need to lead the sampler specifications for this materials
    // some mesh are properly rendered with specific type of sampler properties
    // I had issues rendering Sponza because I was using clamp to edge whereas
    // the wall bricks needed repeat
    struct SamplingProperties {
        float MipLevels{ 1.0f };

        rhi::SamplerFilter MinFilter{ rhi::SamplerFilter::eNearest };
        rhi::SamplerFilter MagFilter{ rhi::SamplerFilter::eNearest };
        rhi::SamplerWrapMode WrapU{ rhi::SamplerWrapMode::eRepeat };
        rhi::SamplerWrapMode WrapV{ rhi::SamplerWrapMode::eRepeat };
        rhi::SamplerWrapMode WrapW{ rhi::SamplerWrapMode::eRepeat };
    };

    struct PBRMap {
        rhi::TextureHandle mTexture{};
        MapType MapType{ MapType::eInvalid };
    };

    struct PhysicMaterialDescription {
        eastl::string mName{};

        AlphaMode AlphaMask{ AlphaMode::Opaque };
        Workflow Workflow{ Workflow::eMetallicRoughness };

        // Base color/Albedo
        float4 BaseColorFactor{ 1.f, 1.f, 1.f, 1.f };
        float4 DiffuseFactor{ 1.f, 1.f, 1.f, 1.f };
        float4 SpecularFactor{ 0.f, 0.f, 0.f, 0.f };
        float3 EmissiveFactor{ 1.f, 1.f, 1.f };

        f32 MetallicFactor{ 1.f };
        f32 RoughnessFactor{ 1.f };
        f32 GlossinessFactor{ 1.f };
        f32 NormalScale{ 1.f };
        f32 OcclusionStrength{ 1.f };
        f32 EmissiveStrength{ 1.f };
        f32 AlphaMaskCutoff{ 1.0f };

        // Texture UV sets (Maps can either use UV0 or UV1)
        // UV0 assumed by default unless otherwise specified
        i32 BaseColorTextureSet{ 0 };
        i32 MetallicRoughnessTextureSet{ 0 };
        i32 SpecularGlossinessSet{ 0 };
        i32 NormalTextureSet{ 0 };
        i32 OcclusionTextureSet{ 0 };
        i32 EmissiveTextureSet{ 0 };

        // GLTF Extensions
        bool Unlit{ false };
        bool mIsDoubleSided{ false };

        ankerl::unordered_dense::map<filesystem::Path, PBRMap> mTexturesByUri{};
        ankerl::unordered_dense::map<MapType, SamplingProperties> TexturesSamplers{};
    };

    class PhysicalMaterial final : public Material {
    public:
        explicit PhysicalMaterial( eastl::string_view name = "PBR" );
        explicit PhysicalMaterial( const PhysicMaterialDescription& props );

        auto RemoveTexture( MapType type ) -> void;
        auto SetTexture( MapType type, const rhi::TextureHandle& texture ) -> void;

        MKT_NODISCARD auto IsOpaque() const -> bool;
        MKT_NODISCARD auto IsTransparent() const -> bool;

        // ===============================
        // Scalar & Factor Setters
        // ===============================
        auto SetAlphaMask( AlphaMode mode ) -> void;
        auto SetWorkflow( Workflow mode ) -> void;

        auto SetBaseColorFactor( const float4& value ) -> void;
        auto SetDiffuseFactor( const float4& value ) -> void;
        auto SetSpecularFactor( const float4& value ) -> void;
        auto SetEmissiveFactor( const float3& value ) -> void;

        auto SetAoFactor( f32 v ) -> void;
        auto SetMetallicFactor( f32 v ) -> void;
        auto SetRoughnessFactor( f32 v ) -> void;
        auto SetGlossinessFactor( f32 v ) -> void;
        auto SetNormalScale( f32 v ) -> void;
        auto SetOcclusionStrength( f32 v ) -> void;
        auto SetEmissiveStrength( f32 v ) -> void;
        auto SetAlphaMaskCutoff( f32 v ) -> void;
        auto SetIsDoubleSided( bool value ) -> void;

        // ===============================
        // UV Set Setters
        // ===============================
        auto SetBaseColorTextureSet( i32 set ) -> void;
        auto SetMetallicRoughnessTextureSet( i32 set ) -> void;
        auto SetSpecularGlossinessSet( i32 set ) -> void;
        auto SetNormalTextureSet( i32 set ) -> void;
        auto SetOcclusionTextureSet( i32 set ) -> void;
        auto SetEmissiveTextureSet( i32 set ) -> void;

        // ===============================
        // Bloom
        // ===============================
        auto EnableBloom( bool enable ) -> void;

        // ===============================
        // Scalar & Factor Getters
        // ===============================
        MKT_NODISCARD auto GetAlphaMask() const -> AlphaMode;
        MKT_NODISCARD auto GetWorkflow() const -> Workflow;

        MKT_NODISCARD auto GetBaseColorFactor() const -> const float4&;
        MKT_NODISCARD auto GetDiffuseFactor() const -> const float4&;
        MKT_NODISCARD auto GetSpecularFactor() const -> const float4&;
        MKT_NODISCARD auto GetEmissiveFactor() const -> const float3&;

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
        MKT_NODISCARD auto GetBaseColorTextureSet() const -> i32;
        MKT_NODISCARD auto GetMetallicRoughnessTextureSet() const -> i32;
        MKT_NODISCARD auto GetSpecularGlossinessSet() const -> i32;
        MKT_NODISCARD auto GetNormalTextureSet() const -> i32;
        MKT_NODISCARD auto GetOcclusionTextureSet() const -> i32;
        MKT_NODISCARD auto GetEmissiveTextureSet() const -> i32;

        MKT_NODISCARD auto IsDoubleSided() const -> bool;
        MKT_NODISCARD auto IsBloomy() const -> bool;

        MKT_NODISCARD auto HasTexture( MapType type ) const -> bool;
        MKT_NODISCARD auto GetTexture( MapType type ) const -> rhi::TextureHandle;

        ~PhysicalMaterial() override = default;

    private:
        AlphaMode m_AlphaMask{ AlphaMode::Opaque };
        Workflow m_Workflow{ Workflow::eMetallicRoughness };

        // Base color/Albedo
        float4 m_BaseColorFactor{ 1.f, 1.f, 1.f, 1.f };
        float4 m_DiffuseFactor{ 1.f, 1.f, 1.f, 1.f };
        float4 m_SpecularFactor{ 0.f, 0.f, 0.f, 0.f };
        float3 m_EmissiveFactor{ 0.f, 0.f, 0.f };

        // Scalars
        f32 m_AoFactor{ 1.f };
        f32 m_MetallicFactor{ 1.f };
        f32 m_RoughnessFactor{ 1.f };
        f32 m_GlossinessFactor{ 1.f };
        f32 m_NormalScale{ 1.f };
        f32 m_OcclusionStrength{ 1.f };
        f32 m_EmissiveStrength{ 0.f };
        f32 m_AlphaMaskCutoff{ 1.0f };

        // UV sets
        i32 m_BaseColorTextureSet{};
        i32 m_MetallicRoughnessTextureSet{};
        i32 m_SpecularGlossinessTextureSet{};
        i32 m_NormalTextureSet{};
        i32 m_OcclusionTextureSet{};
        i32 m_EmissiveTextureSet{};

        bool m_IsBloomy{ false };
        bool m_IsDoubleSided{ false };

        // Note: Materials reference both textures and samplers.
        // Textures provide the image data, while samplers define how that data is read
        // (filtering, addressing, LOD behavior, etc.).
        // This separation allows the same texture to be reused across materials
        // with different sampling settings.
        ankerl::unordered_dense::map<MapType, rhi::TextureHandle> m_Textures{};
        ankerl::unordered_dense::map<MapType, rhi::SamplerHandle> m_Samplers{};
    };
}

#endif//MIKOTO_PHYSICALLY_BASED_MATERIAL_HH
