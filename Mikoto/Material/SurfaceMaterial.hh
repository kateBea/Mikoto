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

#ifndef MIKOTO_MATERIAL_SURFACE_MATERIAL_HH
#define MIKOTO_MATERIAL_SURFACE_MATERIAL_HH

#include <mutex>

#include <EASTL/utility.h>
#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <yaml-cpp/yaml.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>

#include <Assets/Image.hh>
#include <Assets/AssetTexture.hh>
#include <Assets/AssetLoadRegistry.hh>

#include <Material/Material.hh>
#include <Material/MaterialGraph.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Texture.hh>

namespace mikoto::material {

    enum class Workflow {
        eMetallicRoughness,
        eSpecularGlossiness,
        eCount,
    };

    enum class AlphaMode {
        eOpaque,
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
        eClearCoatNormal,
        eHeight,
    };

    enum class HeightMappingMode : core::u32 {
        eNone,
        eBump,
        eParallax,
        eDisplacement
    };

    struct HeightMappingSettings {
        HeightMappingMode mMode{ HeightMappingMode::eNone };

        core::f32 mScale{ 0.05f }; // Mesh-local distance; parallax uses texture space.
        core::f32 mMidlevel{ 0.5f };
        core::u32 mUVSet{};
        core::u32 mSteps{ 32 };
    };

    enum class ShadingModel {
        eDefaultPbr,
        eClearCoat,
        eToonShading,
        eFlatShading,
        eCellShading,
        eSubsurfaceScattering,
        eDielectric,
        eConductor,
        eUnlit,

        eCount,
    };

    /** Shared by water and glass. Absorption color is the fraction of light
     * remaining after traveling attenuationDistance world units inside the medium.
     * Geometry waves belong to WaterSimulationComponent, not the material.
     */
    struct DielectricSettings {
        core::float3 mAttenuationColor{ 1.0f };
        core::f32 mIndexOfRefraction{ 1.5f };
        core::f32 mTransmission{ 1.0f };
        core::f32 mThickness{ 1.0f };
        core::f32 mAttenuationDistance{ 10.0f };
        bool mThinWalled{};
        core::float3 mScattering{}; // Homogeneous interior, inverse world units; path tracing only.
        core::f32 mPhaseAnisotropy{};
    };

    /** Complex IOR at representative RGB wavelengths. Path tracing uses exact conductor Fresnel. */
    struct ConductorSettings {
        core::float3 mEta{ 0.15f };
        core::float3 mK{ 3.5f };
    };

    /** @brief Raster shading controls. Toon uses a soft threshold; cell shading uses discrete bands. */
    struct SurfaceShadingSettings {
        core::f32 mCoatWeight{ 1.0f };
        core::f32 mCoatRoughness{ 0.1f };
        core::f32 mToonThreshold{ 0.5f };
        core::f32 mToonSoftness{ 0.05f };
        core::i32 mCellBands{ 4 };
        core::float3 mContourColor{ 0.015f };
        core::f32 mContourWidth{ 2.0f }; // Screen pixels; zero disables the silhouette.
    };


    /** @brief Artist-friendly screen-space diffusion, following bentoBAUX's Unity 6 pipeline.
     * https://bentobaux.github.io/posts/separable-subsurface-scattering-in-unity-6/
     * Sigma in pixels = profile sigma * scatter scale * 2. */
    struct SubsurfaceSettings {
        // PBRT Skin1 coefficients in mm^-1. UnitsPerMillimeter converts to scene units.
        // https://github.com/mmp/pbrt-v4/blob/master/src/pbrt/media.cpp
        core::float3 mAbsorption{ 0.032f, 0.17f, 0.48f };
        core::float3 mScattering{ 0.74f, 0.88f, 1.01f };
        core::f32 mIndexOfRefraction{ 1.3f };
        core::f32 mAnisotropy{};
        core::f32 mUnitsPerMillimeter{ 0.001f };

        core::f32 mScatterScale{ 2.0f };
        core::f32 mStrength{ 1.0f };
        // Per-channel scattering amount; preserves uniform illumination.
        core::float3 mColor{ 1.0f };
        // Scattering opacity, independent of AlphaMode and surface coverage.
        core::f32 mOpacity{ 0.0f };
        core::float3 mNear{ 0.35f, 0.07f, 0.035f };
        core::float3 mFar{ 1.0f, 0.12f, 0.10f };
        core::f32 mNearWeight{ 0.5f };
        // Relative view-depth discontinuity threshold. Zero disables depth rejection.
        core::f32 mDepthRejection{ 0.02f };
        // Optional artist-authored thickness, in the same units as transmission distances.
        core::f32 mThickness{ 1.0f };
        core::f32 mTranslucency{ 0.0f };
        core::i32 mSamples{ 65 };
    };

    /** @brief Normalized RGB weights and integer pixel offsets, center first. */
    struct SubsurfaceKernel {
        eastl::array<core::float4, 65> mSamples{};
    };

    // I need to lead the sampler specifications for these materials
    // some mesh are properly rendered with specific type of sampler properties
    // I had issues rendering Sponza because I was using clamp to edge whereas
    // the wall bricks needed repeat
    struct SamplingProperties {
        float mMipLevels{ 1.0f };

        renderer::rhi::SamplerFilter mMinFilter{ renderer::rhi::SamplerFilter::eNearest };
        renderer::rhi::SamplerFilter mMagFilter{ renderer::rhi::SamplerFilter::eNearest };
        renderer::rhi::SamplerWrapMode mWrapU{ renderer::rhi::SamplerWrapMode::eRepeat };
        renderer::rhi::SamplerWrapMode mWrapV{ renderer::rhi::SamplerWrapMode::eRepeat };
        renderer::rhi::SamplerWrapMode mWrapW{ renderer::rhi::SamplerWrapMode::eRepeat };
    };

    struct PBRMap {
        renderer::rhi::TextureHandle mTexture{};
        MapType mMapType{ MapType::eInvalid };
    };

    struct SurfaceMaterialDescription {
        eastl::string mName{};

        AlphaMode mAlphaMask{ AlphaMode::eOpaque };
        Workflow mWorkflow{ Workflow::eMetallicRoughness };

        // Base color/Albedo
        core::float4 BaseColorFactor{ 1.f, 1.f, 1.f, 1.f };
        core::float4 DiffuseFactor{ 1.f, 1.f, 1.f, 1.f };
        core::float4 SpecularFactor{ 0.f, 0.f, 0.f, 0.f };
        core::float3 EmissiveFactor{ 1.f, 1.f, 1.f };

        core::f32 MetallicFactor{ 1.f };
        core::f32 RoughnessFactor{ 1.f };
        core::f32 GlossinessFactor{ 1.f };
        core::f32 NormalScale{ 1.f };
        core::f32 ClearCoatNormalScale{ 1.f };
        ShadingModel mShadingModel{};
        SurfaceShadingSettings mSurfaceShading{};
        DielectricSettings mDielectric{};
        ConductorSettings mConductor{};
        core::f32 OcclusionStrength{ 1.f };
        core::f32 EmissiveStrength{ 1.f };
        core::f32 AlphaMaskCutoff{ 1.0f };

        // Texture UV sets (Maps can either use UV0 or UV1)
        // UV0 assumed by default unless otherwise specified
        core::i32 BaseColorTextureSet{ 0 };
        core::i32 MetallicRoughnessTextureSet{ 0 };
        core::i32 SpecularGlossinessSet{ 0 };
        core::i32 NormalTextureSet{ 0 };
        core::i32 ClearCoatNormalTextureSet{ 0 };
        core::i32 OcclusionTextureSet{ 0 };
        core::i32 EmissiveTextureSet{ 0 };

        // GLTF Extensions
        bool Unlit{ false };
        bool mIsDoubleSided{ false };

        // One image may supply several map roles. Preserve every binding while the
        // asset factory shares the underlying image resource.
        eastl::vector<eastl::pair<filesystem::Path, PBRMap>> mTextureBindings{};
    };

    class SurfaceMaterial final : public IMaterial, public core::RevisionTracked {
    public:
        explicit SurfaceMaterial( eastl::string_view name = "PBR" );
        explicit SurfaceMaterial( const SurfaceMaterialDescription& props );

        SurfaceMaterial( const SurfaceMaterial& other );

        auto Deserialize( const filesystem::Path &filename ) -> void override;
        auto Serialize( const filesystem::Path &filename ) const -> void override;

        auto Deserialize( filesystem::FileHandle file ) -> void override;
        auto Serialize( filesystem::FileHandle file ) const -> void override;

        auto SetHeightMapping( HeightMappingSettings settings ) -> void;
        auto SetTextureReference( MapType type, const filesystem::Path& path ) -> void;

        auto SetUvTransform( const core::float4& transform ) -> void;

        auto RemoveTexture( MapType type ) -> void;

        auto UpdateTextureResidency( bool wanted = true ) -> void;

        auto UseTexture( MapType type, bool use = true ) -> void;
        auto SetTexture( MapType type, const renderer::rhi::TextureHandle& texture ) -> void;
        auto SetTexture( MapType type, const asset::AssetHandle<asset::Texture>& texture ) -> void;

        MKT_NODISCARD auto GetHeightImage() const -> asset::ImageHandle;
        MKT_NODISCARD auto GetHeightMapping() const -> const HeightMappingSettings&;
        MKT_NODISCARD auto GetTexturePath( MapType type ) const -> filesystem::Path;

        MKT_NODISCARD auto IsOpaque() const -> bool;
        MKT_NODISCARD auto IsTransparent() const -> bool;

        MKT_NODISCARD auto IsTextureUsed( MapType type ) const -> bool;

        MKT_NODISCARD auto GetSourcePath() const -> const filesystem::Path&;

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
        auto SetClearCoatNormalScale( core::f32 v ) -> void;
        auto SetClearCoatNormalTextureSet( core::i32 set ) -> void;

        MKT_NODISCARD auto GetClearCoatNormalScale() const -> float;
        MKT_NODISCARD auto GetClearCoatNormalTextureSet() const -> core::i32;
        MKT_NODISCARD auto GetUvTransform() const -> const core::float4&;

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

        // ===============================
        // Shading Model
        // ===============================
        auto SetShadingModel( ShadingModel model ) -> void;
        auto SetSurfaceShading( const SurfaceShadingSettings& settings ) -> void;
        auto SetDielectric( const DielectricSettings& settings ) -> void;
        auto SetConductor( const ConductorSettings& settings ) -> void;

        MKT_NODISCARD auto GetShadingModel() const -> ShadingModel;
        MKT_NODISCARD auto GetSurfaceShading() const -> const SurfaceShadingSettings&;
        MKT_NODISCARD auto GetDielectric() const -> const DielectricSettings&;
        MKT_NODISCARD auto GetConductor() const -> const ConductorSettings&;
        MKT_NODISCARD auto GetConductorReflectance() const -> core::float3;

        /** Presets initialize the same editable optical properties; they are not shading models. */
        auto ApplyConductorPreset( core::u32 preset ) -> void;
        auto ApplyDielectricPreset( core::u32 preset ) -> void;
        auto ApplySubsurfacePreset( core::u32 preset ) -> void;

        /** @brief Updates diffusion settings and invalidates uploaded material data. */
        auto SetSubsurface( const SubsurfaceSettings& settings ) -> void;
        MKT_NODISCARD auto GetSubsurface() const -> const SubsurfaceSettings&;

        /** @brief Owns this material's editable graph independently of other instances. */
        auto GetGraph() -> MaterialGraph*;
        auto GetGraph() const -> const MaterialGraph*;

        auto SetName( eastl::string_view newName ) -> void override;
        MKT_NODISCARD auto GetName() const -> const eastl::string& override;

        ~SurfaceMaterial() override = default;

    private:
        // YAML is an implementation detail of the ISerializable entry points.
        MKT_NODISCARD auto SerializeProperties() const -> YAML::Node;
        auto RestoreProperties( const YAML::Node& node ) -> void;

        mutable filesystem::Path mSourcePath{};

        eastl::string mName{};
        SurfaceMaterialGraph mGraph{};
        ShadingModel mShadingModel{};
        SubsurfaceSettings mSubsurface{};
        SurfaceShadingSettings mSurfaceShading{};
        DielectricSettings mDielectric{};
        ConductorSettings mConductor{};

        AlphaMode mAlphaMask{ AlphaMode::eOpaque };
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
        HeightMappingSettings mHeightMapping{};
        mutable std::mutex mHeightImageMutex{};
        mutable filesystem::Path mHeightImagePath{};
        mutable asset::ImageHandle mHeightImage{};
        core::float4 mUvTransform{ 1.0f, 1.0f, 0.0f, 0.0f };
        core::f32 mNormalScale{ 1.f };
        core::f32 mClearCoatNormalScale{ 1.f };
        core::i32 mClearCoatNormalTextureSet{};
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
        struct StreamedTexture {
            asset::AssetRef<asset::Texture> mReference{};
            asset::AssetHandle<asset::Texture> mRequest{};
        };
        eastl::unordered_map<MapType, StreamedTexture> mStreamedTextures{};
        std::chrono::steady_clock::time_point mLastTextureUse{ std::chrono::steady_clock::now() };
        ankerl::unordered_dense::map<MapType, renderer::rhi::TextureHandle> mTextures{};
        eastl::array<bool, static_cast<core::usize>( MapType::eHeight ) + 1> mUseTextures{};
        ankerl::unordered_dense::map<MapType, renderer::rhi::SamplerHandle> mSamplers{};
    };

    using SurfaceMaterialHandle = core::Ref<SurfaceMaterial>;
}

#endif// MIKOTO_MATERIAL_SURFACE_MATERIAL_HH
