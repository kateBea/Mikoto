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

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Texture.hh>

namespace mikoto::material {

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

    // I need to lead the sampler specifications for these materials
    // some mesh are properly rendered with specific type of sampler properties
    // I had issues rendering Sponza because I was using clamp to edge whereas
    // the wall bricks needed repeat
    struct SamplingProperties {
        float MipLevels{ 1.0f };

        renderer::rhi::SamplerFilter MinFilter{ renderer::rhi::SamplerFilter::eNearest };
        renderer::rhi::SamplerFilter MagFilter{ renderer::rhi::SamplerFilter::eNearest };
        renderer::rhi::SamplerWrapMode WrapU{ renderer::rhi::SamplerWrapMode::eRepeat };
        renderer::rhi::SamplerWrapMode WrapV{ renderer::rhi::SamplerWrapMode::eRepeat };
        renderer::rhi::SamplerWrapMode WrapW{ renderer::rhi::SamplerWrapMode::eRepeat };
    };

    struct PBRMap {
        renderer::rhi::TextureHandle mTexture{};
        MapType MapType{ MapType::eInvalid };
    };

    struct PhysicMaterialDescription {
        eastl::string mName{};

        AlphaMode AlphaMask{ AlphaMode::Opaque };
        Workflow Workflow{ Workflow::eMetallicRoughness };

        // Base color/Albedo
        core::float4 BaseColorFactor{ 1.f, 1.f, 1.f, 1.f };
        core::float4 DiffuseFactor{ 1.f, 1.f, 1.f, 1.f };
        core::float4 SpecularFactor{ 0.f, 0.f, 0.f, 0.f };
        core::float3 EmissiveFactor{ 1.f, 1.f, 1.f };

        core::f32 MetallicFactor{ 1.f };
        core::f32 RoughnessFactor{ 1.f };
        core::f32 GlossinessFactor{ 1.f };
        core::f32 NormalScale{ 1.f };
        core::f32 OcclusionStrength{ 1.f };
        core::f32 EmissiveStrength{ 1.f };
        core::f32 AlphaMaskCutoff{ 1.0f };

        // Texture UV sets (Maps can either use UV0 or UV1)
        // UV0 assumed by default unless otherwise specified
        core::i32 BaseColorTextureSet{ 0 };
        core::i32 MetallicRoughnessTextureSet{ 0 };
        core::i32 SpecularGlossinessSet{ 0 };
        core::i32 NormalTextureSet{ 0 };
        core::i32 OcclusionTextureSet{ 0 };
        core::i32 EmissiveTextureSet{ 0 };

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
        auto SetTexture( MapType type, const renderer::rhi::TextureHandle& texture ) -> void;

        MKT_NODISCARD auto IsOpaque() const -> bool;
        MKT_NODISCARD auto IsTransparent() const -> bool;

        // ===============================
        // Scalar & Factor Setters
        // ===============================
        auto SetAlphaMask( AlphaMode mode ) -> void;
        auto SetWorkflow( Workflow mode ) -> void;

        auto SetBaseColorFactor( const core::float4& value ) -> void;
        auto SetDiffuseFactor( const core::float4& value ) -> void;
        auto SetSpecularFactor( const core::float4& value ) -> void;
        auto SetEmissiveFactor( const core::float3& value ) -> void;

        auto SetAoFactor( core::f32 v ) -> void;
        auto SetMetallicFactor( core::f32 v ) -> void;
        auto SetRoughnessFactor( core::f32 v ) -> void;
        auto SetGlossinessFactor( core::f32 v ) -> void;
        auto SetNormalScale( core::f32 v ) -> void;
        auto SetOcclusionStrength( core::f32 v ) -> void;
        auto SetEmissiveStrength( core::f32 v ) -> void;
        auto SetAlphaMaskCutoff( core::f32 v ) -> void;
        auto SetIsDoubleSided( bool value ) -> void;

        // ===============================
        // UV Set Setters
        // ===============================
        auto SetBaseColorTextureSet( core::i32 set ) -> void;
        auto SetMetallicRoughnessTextureSet( core::i32 set ) -> void;
        auto SetSpecularGlossinessSet( core::i32 set ) -> void;
        auto SetNormalTextureSet( core::i32 set ) -> void;
        auto SetOcclusionTextureSet( core::i32 set ) -> void;
        auto SetEmissiveTextureSet( core::i32 set ) -> void;

        // ===============================
        // Bloom
        // ===============================
        auto EnableBloom( bool enable ) -> void;

        // ===============================
        // Scalar & Factor Getters
        // ===============================
        MKT_NODISCARD auto GetAlphaMask() const -> AlphaMode;
        MKT_NODISCARD auto GetWorkflow() const -> Workflow;

        MKT_NODISCARD auto GetBaseColorFactor() const -> const core::float4&;
        MKT_NODISCARD auto GetDiffuseFactor() const -> const core::float4&;
        MKT_NODISCARD auto GetSpecularFactor() const -> const core::float4&;
        MKT_NODISCARD auto GetEmissiveFactor() const -> const core::float3&;

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
        MKT_NODISCARD auto GetBaseColorTextureSet() const -> core::i32;
        MKT_NODISCARD auto GetMetallicRoughnessTextureSet() const -> core::i32;
        MKT_NODISCARD auto GetSpecularGlossinessSet() const -> core::i32;
        MKT_NODISCARD auto GetNormalTextureSet() const -> core::i32;
        MKT_NODISCARD auto GetOcclusionTextureSet() const -> core::i32;
        MKT_NODISCARD auto GetEmissiveTextureSet() const -> core::i32;

        MKT_NODISCARD auto IsDoubleSided() const -> bool;
        MKT_NODISCARD auto IsBloomy() const -> bool;

        MKT_NODISCARD auto HasTexture( MapType type ) const -> bool;
        MKT_NODISCARD auto GetTexture( MapType type ) const -> renderer::rhi::TextureHandle;

        ~PhysicalMaterial() override = default;

    private:
        AlphaMode mAlphaMask{ AlphaMode::Opaque };
        Workflow mWorkflow{ Workflow::eMetallicRoughness };

        // Base color/Albedo
        core::float4 mBaseColorFactor{ 1.f, 1.f, 1.f, 1.f };
        core::float4 mDiffuseFactor{ 1.f, 1.f, 1.f, 1.f };
        core::float4 mSpecularFactor{ 0.f, 0.f, 0.f, 0.f };
        core::float3 mEmissiveFactor{ 0.f, 0.f, 0.f };

        // Scalars
        core::f32 mAoFactor{ 1.f };
        core::f32 mMetallicFactor{ 1.f };
        core::f32 mRoughnessFactor{ 1.f };
        core::f32 mGlossinessFactor{ 1.f };
        core::f32 mNormalScale{ 1.f };
        core::f32 mOcclusionStrength{ 1.f };
        core::f32 mEmissiveStrength{ 0.f };
        core::f32 mAlphaMaskCutoff{ 1.0f };

        // UV sets
        core::i32 mBaseColorTextureSet{};
        core::i32 mMetallicRoughnessTextureSet{};
        core::i32 mSpecularGlossinessTextureSet{};
        core::i32 mNormalTextureSet{};
        core::i32 mOcclusionTextureSet{};
        core::i32 mEmissiveTextureSet{};

        bool mIsBloomy{ false };
        bool mIsDoubleSided{ false };

        // Note: Materials reference both textures and samplers.
        // Textures provide the image data, while samplers define how that data is read
        // (filtering, addressing, LOD behavior, etc.).
        // This separation allows the same texture to be reused across materials
        // with different sampling settings.
        ankerl::unordered_dense::map<MapType, renderer::rhi::TextureHandle> mTextures{};
        ankerl::unordered_dense::map<MapType, renderer::rhi::SamplerHandle> mSamplers{};
    };
}

#endif//MIKOTO_PHYSICALLY_BASED_MATERIAL_HH
