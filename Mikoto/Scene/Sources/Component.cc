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

#include <cmath>
#include <atomic>

#include <EASTL/utility.h>
#include <EASTL/memory.h>

#include <glm/glm.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>

#include <Math/Math.hh>

#include <Assets/Utility.hh>
#include <Assets/ImageProcessing.hh>
#include <Assets/AssetRegistryService.hh>

#include <Material/PostProcessMaterial.hh>

#include <Audio/Utility.hh>

#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Core/RenderSystem.hh>
#include <Renderer/Core/SceneRenderer.hh>

#include <Scene/Scene.hh>
#include <Scene/Entity.hh>
#include <Scene/Component.hh>

#include <Scripting/ScriptingService.hh>

namespace mikoto::scene {

    using namespace mikoto::core;
    using namespace mikoto::scripting;
    using namespace mikoto::renderer;
    using namespace mikoto::renderer::rhi;

    struct TextureCaptureGuard {
        bool& mCapturing;

        ~TextureCaptureGuard() {
            mCapturing = false;
        }
    };

    struct TextureRenderComponent::Runtime {
        SceneCamera mCamera{};
        material::MaterialHandle mSource{};
        material::SurfaceMaterialHandle mMaterial{};
        rhi::TextureHandle mTexture{};
        eastl::unique_ptr<ISceneRenderer> mRenderer{};

        core::f64 mElapsed{};
        core::u64 mSourceRevision{};
        core::u64 mTextureRevision{};
        TextureRenderOutput mOutput{ TextureRenderOutput::eBaseColor };
        bool mCapturing{};
    };

    static auto SanitizeWaterWave( WaterSimulationComponent::WaterWave wave ) -> WaterSimulationComponent::WaterWave {
        if ( glm::length( wave.mDirection ) < 1e-6f ) {
            wave.mDirection = { 1.0f, 0.0f };
        }
        else {
            // Scale first so even very large finite inputs normalize safely.
            wave.mDirection /= glm::max( glm::abs( wave.mDirection.x ), glm::abs( wave.mDirection.y ) );
            wave.mDirection = glm::normalize( wave.mDirection );
        }

        wave.mAmplitude = glm::clamp( wave.mAmplitude, 0.0f, 100.0f );
        wave.mWavelength = glm::clamp( wave.mWavelength, 0.01f, 10000.0f );
        wave.mSpeed = glm::clamp( wave.mSpeed, -100.0f, 100.0f );
        wave.mPhaseOffset = std::remainder( wave.mPhaseOffset, glm::two_pi<float>() );
        return wave;
    }

    // Preserve constness when reading a property's editor or runtime value.
    template<typename Component>
    static auto VisitLight( Component& component, renderer::LightType type, auto&& visitor ) -> void {
        switch ( type ) {
            case renderer::LightType::eDirectional:
                visitor( component.template Get<renderer::DirectionalLight>() );
                break;
            case renderer::LightType::ePoint:
                visitor( component.template Get<renderer::PointLight>() );
                break;
            case renderer::LightType::eSpot:
                visitor( component.template Get<renderer::SpotLight>() );
                break;
            default:
                break;
        }
    }

    CloudComponent::CloudComponent() : mMaterial{ material::VolumeMaterialHandle::New( "Cloud" ) } {
        material::VolumeSettings settings{};
        settings.mDensityModel = material::VolumeDensityModel::eProceduralCloud;
        settings.mScattering = float3{ 0.02f };
        settings.mAnisotropy = 0.65f;
        mMaterial->SetSettings( settings );
    }

    auto CloudComponent::GetShape() const -> CloudShape {
        return mShape;
    }

    auto CloudComponent::GetBottomAltitude() const -> f32 {
        return mBottomAltitude;
    }

    auto CloudComponent::GetLayerThickness() const -> f32 {
        return mLayerThickness;
    }

    auto CloudComponent::GetDimensions() const -> const float3& {
        return mDimensions;
    }

    auto CloudComponent::GetHorizonRadius() const -> f32 {
        return mHorizonRadius;
    }

    auto CloudComponent::GetMaterial() const -> material::VolumeMaterialHandle {
        return mMaterial;
    }

    auto CloudComponent::GetDensityMultiplier() const -> f32 {
        return mDensityMultiplier;
    }

    auto CloudComponent::GetWindDirection() const -> const float2& {
        return mWindDirection;
    }

    auto CloudComponent::GetWindSpeed() const -> f32 {
        return mWindSpeed;
    }

    auto CloudComponent::IsShadowCaster() const -> bool {
        return mCastShadows;
    }

    auto CloudComponent::IsShadowReceiver() const -> bool {
        return mReceiveShadows;
    }

    auto CloudComponent::SetShape( CloudShape shape ) -> void {
        mShape = shape;
    }

    auto CloudComponent::SetBottomAltitude( f32 altitude ) -> void {
        mBottomAltitude = altitude;
    }

    auto CloudComponent::SetLayerThickness( f32 thickness ) -> void {
        mLayerThickness = thickness;
    }

    auto CloudComponent::SetDimensions( const float3& dimensions ) -> void {
        mDimensions = dimensions;
    }

    auto CloudComponent::SetHorizonRadius( f32 radius ) -> void {
        mHorizonRadius = radius;
    }

    auto CloudComponent::SetMaterial( material::VolumeMaterialHandle material ) -> void {
        mMaterial = eastl::move( material );
    }

    auto CloudComponent::SetDensityMultiplier( f32 multiplier ) -> void {
        mDensityMultiplier = multiplier;
    }

    auto CloudComponent::SetWindDirection( const float2& direction ) -> void {
        mWindDirection = direction;
    }

    auto CloudComponent::SetWindSpeed( f32 speed ) -> void {
        mWindSpeed = speed;
    }

    auto CloudComponent::SetCastShadows( bool value ) -> void {
        mCastShadows = value;
    }

    auto CloudComponent::SetReceiveShadows( bool value ) -> void {
        mReceiveShadows = value;
    }

    FogComponent::FogComponent() : mMaterial{ material::VolumeMaterialHandle::New( "Fog" ) } {
        material::VolumeSettings settings{};
        settings.mDensityModel = material::VolumeDensityModel::eExponentialHeight;
        settings.mScattering = float3{ 0.01f };
        mMaterial->SetSettings( settings );
    }

    auto FogComponent::GetShape() const -> FogShape {
        return mShape;
    }

    auto FogComponent::GetDimensions() const -> const float3& {
        return mDimensions;
    }

    auto FogComponent::GetHorizonRadius() const -> f32 {
        return mHorizonRadius;
    }

    auto FogComponent::GetMaterial() const -> material::VolumeMaterialHandle {
        return mMaterial;
    }

    auto FogComponent::GetDensityMultiplier() const -> f32 {
        return mDensityMultiplier;
    }

    auto FogComponent::IsShadowCaster() const -> bool {
        return mCastShadows;
    }

    auto FogComponent::IsShadowReceiver() const -> bool {
        return mReceiveShadows;
    }

    auto FogComponent::SetShape( FogShape shape ) -> void {
        mShape = shape;
    }

    auto FogComponent::SetDimensions( const float3& dimensions ) -> void {
        mDimensions = dimensions;
    }

    auto FogComponent::SetHorizonRadius( f32 radius ) -> void {
        mHorizonRadius = radius;
    }

    auto FogComponent::SetMaterial( material::VolumeMaterialHandle material ) -> void {
        mMaterial = eastl::move( material );
    }

    auto FogComponent::SetDensityMultiplier( f32 multiplier ) -> void {
        mDensityMultiplier = multiplier;
    }

    auto FogComponent::SetCastShadows( bool value ) -> void {
        mCastShadows = value;
    }

    auto FogComponent::SetReceiveShadows( bool value ) -> void {
        mReceiveShadows = value;
    }

    auto AudioListenerComponent::operator=( const AudioListenerComponent& other ) -> AudioListenerComponent& {
        MKT_BEGIN_PROFILER();
        // Keep the destination entity's allocated listener slot.
        SetEnabled( other.mEnabled );
        return *this;
    }

    auto RigidBodyComponent::operator=( const RigidBodyComponent& other ) -> RigidBodyComponent& {
        MKT_BEGIN_PROFILER();

        if ( this != &other ) {
            mMass = other.mMass;
            mRestitution = other.mRestitution;
            mFriction = other.mFriction;
            mUseGravity = other.mUseGravity;
            mBodyType = other.mBodyType;
            mLinearVelocity = other.mLinearVelocity;
            mAngularVelocity = other.mAngularVelocity;

            // A copied component never owns the source entity's Jolt body.
            mBodyID = 0;
            mIsValidBody = false;
        }

        return *this;
    }

    auto MeshColliderComponent::GetColliderType() -> physics::ColliderType {
        return physics::ColliderType::eMesh;
    }

    auto CapsuleColliderComponent::GetColliderType() -> physics::ColliderType {
        return physics::ColliderType::eCapsule;
    }

    auto SphereColliderComponent::GetColliderType() -> physics::ColliderType {
        return physics::ColliderType::eSphere;
    }

    auto BoxColliderComponent::GetColliderType() -> physics::ColliderType {
        return physics::ColliderType::eBox;
    }

    auto TextComponent::SetOutlineWidth( core::f32 width ) -> void {
        mOutlineWidth = glm::clamp( width, 0.0f, 0.125f );
    }

    auto TextComponent::GetOutlineWidth() const -> core::f32 {
        return mOutlineWidth;
    }

    auto TextComponent::SetOutlineColor( const core::float4& color ) -> void {
        mOutlineColor = glm::clamp( color, core::float4{ 0 }, core::float4{ 1 } );
    }

    auto TextComponent::GetOutlineColor() const -> const core::float4& {
        return mOutlineColor;
    }

    auto VehicleComponent::SetVehicleType( VehicleType type ) -> void {
        MKT_BEGIN_PROFILER();
        mVehicleType = type;
    }

    auto VehicleComponent::GetVehicleType() const -> VehicleType {
        MKT_BEGIN_PROFILER();
        return mVehicleType;
    }

    auto VehicleComponent::SetWheelDescriptions( const eastl::vector<VehicleWheelDescription>& wheels ) -> void {
        MKT_BEGIN_PROFILER();
        mWheelDescriptions = wheels;
    }

    auto VehicleComponent::GetWheelDescriptions() const -> const eastl::vector<VehicleWheelDescription>& {
        MKT_BEGIN_PROFILER();
        return mWheelDescriptions;
    }


    auto VehicleComponent::IsValid() const -> bool {
        bool left{}, right{}, driven{};

        for ( const auto& wheel: mWheelDescriptions ) {
            if ( wheel.mRadius <= 0 || wheel.mWidth <= 0 || wheel.mSuspensionMin < 0 ||
                 wheel.mSuspensionMax < wheel.mSuspensionMin || wheel.mSuspensionFrequency <= 0 ||
                 glm::length2( wheel.mSuspensionDirection ) < 0.0001f || wheel.mSuspensionDamping < 0 ||
                 wheel.mMaxSteerAngle < 0 || wheel.mMaxSteerAngle >= glm::half_pi<float>() ||
                 wheel.mMaxBrakeTorque < 0 || wheel.mMaxHandBrakeTorque < 0 ) {
                return false;
            }

            left |= wheel.mPosition.x >= 0;
            right |= wheel.mPosition.x < 0;
            driven |= wheel.mDriven;
        }

        if ( !mWheelDescriptions.empty() && ( mVehicleType == VehicleType::eTracked ? !( left && right ) : !driven ) ) {
            return false;
        }

        if ( !mWheelDescriptions.empty() &&
             ( ( mVehicleType == VehicleType::eMotorcycle && mWheelDescriptions.size() != 2 ) ||
                 ( mVehicleType == VehicleType::eSixDof && mWheelDescriptions.size() != 4 ) ||
                 ( mVehicleType == VehicleType::eWheeled && mWheelDescriptions.size() % 2 != 0 ) ||
                 ( mVehicleType == VehicleType::eTracked && mWheelDescriptions.size() < 2 ) ) ) {
            return false;
        }

        const core::f32 values[]{ mHalfTrack, mHalfWheelbase, mWheelRadius, mWheelWidth,
            mSuspensionMin, mSuspensionMax, mMaxSteerDegrees, mEngineTorque,
            mSuspensionFrequency, mMaxPitchRollDegrees, mLongitudinalImpulseScale };

        for ( const auto value: values ) {
            if ( value <= 0.0f ) {
                return false;
            }
        }

        return mSuspensionMax >= mSuspensionMin && mMaxSteerDegrees < 90.0f &&
               mMaxPitchRollDegrees <= 180.0f && mSuspensionDamping >= 0.0f;
    }

    auto VehicleComponent::HasSameSettings( const VehicleComponent& other ) const -> bool {
        return mVehicleType == other.mVehicleType && mWheelDescriptions == other.mWheelDescriptions &&
               mHalfTrack == other.mHalfTrack && mHalfWheelbase == other.mHalfWheelbase &&
               mWheelRadius == other.mWheelRadius && mWheelWidth == other.mWheelWidth &&
               mSuspensionMin == other.mSuspensionMin && mSuspensionMax == other.mSuspensionMax &&
               mMaxSteerDegrees == other.mMaxSteerDegrees && mEngineTorque == other.mEngineTorque &&
               mWheelAnchorHeight == other.mWheelAnchorHeight && mCenterOfMassOffsetY == other.mCenterOfMassOffsetY &&
               mSuspensionFrequency == other.mSuspensionFrequency && mSuspensionDamping == other.mSuspensionDamping &&
               mMaxPitchRollDegrees == other.mMaxPitchRollDegrees && mLongitudinalImpulseScale == other.mLongitudinalImpulseScale &&
               mFourWheelDrive == other.mFourWheelDrive && mCylinderWheelCast == other.mCylinderWheelCast;
    }

    MeshComponent::MeshComponent( asset::ModelHandle model, core::i32 meshIndex ) {
        SetMesh( eastl::move( model ), meshIndex );
    }

    auto MeshComponent::SetMesh( asset::ModelHandle model, core::i32 meshIndex ) -> void {
        if ( !model ) {
            return;
        }

        mModel = eastl::move( model );
        mMeshIndex = meshIndex;
        mPath = mModel->GetPath();
        mStreamingPinned = mModel->HasArmature();

        if ( HasMesh() && GetMesh()->GetRenderData() ) {
            mStreamingCenter = GetMesh()->GetRenderData()->GetSortCenter();
            mStreamingExtent = GetMesh()->GetRenderData()->GetBoundsExtent();
        }
    }

    auto MeshComponent::GetStreamingCenter() const -> const core::float3& {
        return mStreamingCenter;
    }

    auto MeshComponent::GetStreamingExtent() const -> const core::float3& {
        return mStreamingExtent;
    }

    auto MeshComponent::UpdateResidency( bool wanted ) -> void {
        if ( mStreamingPinned || mPath.IsEmpty() ) {
            return;
        }

        const auto now{ std::chrono::steady_clock::now() };

        if ( wanted ) {
            mLastResidentUse = now;

            if ( !mModel ) {
                if ( mResidencyRequest.GetState() == asset::AssetState::eUnloaded ) {
                    auto& assets{ asset::AssetRegistryService::Get() };
                    mResidencyRequest = assets->Request<asset::Model>( asset::AssetRef<asset::Model>{ mPath } );
                }

                if ( auto ready{ mResidencyRequest.GetIfReady() } ) {
                    // Restoring residency must not reapply imported transforms or
                    // replace materials edited while this entity was unloaded.
                    SetMesh( eastl::move( ready ), mMeshIndex );
                    mResidencyRequest = {};
                }
            }
        }
        else if ( now - mLastResidentUse >= std::chrono::seconds{ 10 } ) {
            mResidencyRequest = {};
            mModel.Reset();
        }
    }

    auto ParticleEmitterComponent::SetMaterial( material::MaterialHandle particleMaterial ) -> void {
        if ( !dynamic_cast<material::ParticleMaterial*>( particleMaterial.GetPtr() ) ) {
            return;
        }

        mParticleMaterial = particleMaterial;
    }

    auto ParticleEmitterComponent::GetMaterial() const -> material::MaterialHandle {
        return mParticleMaterial;
    }

    auto ParticleEmitterComponent::GetInstance() -> renderer::ParticleEmitterInstance& {
        return mInstance;
    }

    auto ParticleEmitterComponent::GetInstance() const -> const renderer::ParticleEmitterInstance& {
        return mInstance;
    }

    CanvasComponent::CanvasComponent( const scene::Camera* camera ) {
        mCanvas.SetCamera( camera );
    }

    auto CanvasComponent::GetCanvas() -> renderer::Canvas& {
        return mCanvas;
    }

    auto CanvasComponent::GetCanvas() const -> const renderer::Canvas& {
        return mCanvas;
    }

    WaterRenderComponent::WaterRenderComponent() {
        ApplyOceanPreset();
    }

    auto WaterRenderComponent::ApplyOceanPreset() -> void {
        // Replace this component's material so applying a preset does not edit
        // an asset shared by another water surface or a glass mesh.
        mWaterMaterial = core::Ref<material::SurfaceMaterial>::New( "Water" );
        auto water{ GetMaterial() };
        water->SetShadingModel( material::ShadingModel::eDielectric );
        // Absorption supplies the water tint; keep surface transmission neutral.
        water->SetBaseColorFactor( { 1.0f, 1.0f, 1.0f, 1.0f } );
        water->SetMetallicFactor( 0.0f );
        water->SetRoughnessFactor( 0.24f );
        auto dielectric{ water->GetDielectric() };
        dielectric.mIndexOfRefraction = 1.333f;
        dielectric.mAttenuationColor = { 0.12f, 0.55f, 0.68f };
        dielectric.mThickness = 8.0f;
        water->SetDielectric( dielectric );
        water->SetIsDoubleSided( true );
        mShowGrid = false;
        mShowVertices = false;
        SetDebugPreviewColor( { 1.0f, 1.0f, 1.0f } );
        mFoamAmount = 0.65f;
        SetPlaneResolution( glm::max( mResolution, core::float2{ 256.0f } ) );
    }

    auto WaterRenderComponent::SetFoamAmount( float amount ) -> void {
        mFoamAmount = glm::clamp( amount, 0.0f, 1.0f );
    }

    auto WaterRenderComponent::GetFoamAmount() const -> float {
        return mFoamAmount;
    }

    auto WaterRenderComponent::GetMaterial() const -> core::Ref<material::SurfaceMaterial> {
        return mWaterMaterial.As<material::SurfaceMaterial>();
    }

    auto WaterRenderComponent::SetShape( WaterShape shape ) -> void {
        if ( mShape != shape ) {
            mShape = shape;
            BumpRevision();
        }
    }

    auto WaterRenderComponent::SetMaterial( material::MaterialHandle waterMaterial ) -> void {
        // Water shares the surface material's dielectric model with glass.
        if ( auto* surface{ dynamic_cast<material::SurfaceMaterial*>( waterMaterial.GetPtr() ) };
            !surface || surface->GetShadingModel() != material::ShadingModel::eDielectric ) {
            return;
        }

        mWaterMaterial = waterMaterial;
    }

    auto WaterRenderComponent::GetShape() const -> WaterShape {
        return mShape;
    }

    auto WaterRenderComponent::GetPlaneResolution() const -> core::float2 {
        return mResolution;
    }

    auto WaterRenderComponent::SetPlaneResolution( core::float2 resolution ) -> void {
        resolution = glm::round( glm::clamp( resolution, core::float2{ 1 }, core::float2{ 1024 } ) );

        if ( mResolution != resolution ) {
            mResolution = resolution;
            BumpRevision();
        }
    }

    auto WaterRenderComponent::GetPlaneDimensions() const -> core::float2 {
        return mDimensions;
    }

    auto WaterRenderComponent::SetPlaneDimensions( core::float2 size ) -> void {
        size = glm::clamp( size, core::float2{ .01f }, core::float2{ 100000.f } );

        if ( mDimensions != size ) {
            mDimensions = size;
            BumpRevision();
        }
    }

    auto WaterRenderComponent::SetShowGrid( bool show ) -> void {
        mShowGrid = show;
    }

    auto WaterRenderComponent::SetShowVertices( bool show ) -> void {
        mShowVertices = show;
    }

    auto WaterRenderComponent::SetDebugPreviewColor( core::float3 color ) -> void {
        color = glm::clamp( color, core::float3{ 0.0f }, core::float3{ 1.0f } );

        if ( mDebugPreviewColor != color ) {
            mDebugPreviewColor = color;
            BumpRevision();
        }
    }

    auto WaterRenderComponent::GetDebugPreviewColor() const -> core::float3 {
        return mDebugPreviewColor;
    }

    auto WaterRenderComponent::IsGridVisible() const -> bool {
        return mShowGrid;
    }

    auto WaterRenderComponent::AreVerticesVisible() const -> bool {
        return mShowVertices;
    }

    WaterSimulationComponent::WaterSimulationComponent() {
        ApplyOceanPreset();
    }

    auto WaterSimulationComponent::ApplyOceanPreset() -> void {
        mType = kDefaultType;
        mFftSettings = FftSettings{};
        mTimeScale = 1.0f;
        ResetTime();

        // Seed the analytic modes for when the user switches away from FFT.
        mSteepness = 0.85f;
        // Wind-aligned swells with smaller crossing waves. Deep-water phase
        // speeds use sqrt(g * wavelength / 2*pi), assuming one local unit = 1 m.
        SetWaves( { { { 1.0f, 0.2f }, 0.85f, 28.0f, 6.61f, 0.0f },
            { { 0.9f, 0.4f }, 0.48f, 17.0f, 5.15f, 1.4f },
            { { 0.8f, -0.3f }, 0.26f, 10.0f, 3.95f, 2.8f },
            { { 0.7f, 0.7f }, 0.14f, 6.2f, 3.11f, 0.7f },
            { { -0.3f, 0.95f }, 0.075f, 4.1f, 2.53f, 2.1f },
            { { 1.0f, -0.5f }, 0.04f, 2.8f, 2.09f, 4.3f } } );
    }

    auto WaterSimulationComponent::SetType( WaterSimulationType type ) -> void {
        mType = type;
    }


    auto WaterSimulationComponent::SetWaves( eastl::vector<WaterWave> waves ) -> void {
        if ( waves.size() > kMaxWaves ) {
            waves.resize( kMaxWaves );
        }

        for ( auto& wave: waves ) {
            wave = SanitizeWaterWave( wave );
        }

        mWaves = eastl::move( waves );
    }

    auto WaterSimulationComponent::AddWave( const WaterWave& wave ) -> void {
        if ( mWaves.size() < kMaxWaves ) {
            mWaves.push_back( SanitizeWaterWave( wave ) );
        }
    }

    auto WaterSimulationComponent::RemoveWave( core::usize index ) -> bool {
        if ( index >= mWaves.size() ) {
            return false;
        }

        mWaves.erase( mWaves.begin() + index );
        return true;
    }

    auto WaterSimulationComponent::ClearWaves() -> void {
        mWaves.clear();
    }

    auto WaterSimulationComponent::SetTimeScale( float timeScale ) -> void {
        mTimeScale = glm::clamp( timeScale, 0.0f, 10.0f );
    }

    auto WaterSimulationComponent::SetSteepness( float steepness ) -> void {
        mSteepness = glm::clamp( steepness, 0.0f, 0.95f );
    }

    auto WaterSimulationComponent::SetFftSettings( const FftSettings& settings ) -> void {
        constexpr FftSettings defaults{};
        const auto clamp{ []( core::f32 value, core::f32 minimum, core::f32 maximum, core::f32 fallback ) {
            return std::isfinite( value ) ? glm::clamp( value, minimum, maximum ) : fallback;
        } };

        mFftSettings = {
            .mPatchLength = clamp( settings.mPatchLength, FftSettings::kMinPatchLength, FftSettings::kMaxPatchLength, defaults.mPatchLength ),
            .mWindSpeed = clamp( settings.mWindSpeed, 0.0f, FftSettings::kMaxWindSpeed, defaults.mWindSpeed ),
            .mWindDirection = std::isfinite( settings.mWindDirection ) ? std::remainder( settings.mWindDirection, 360.0f ) : defaults.mWindDirection,
            .mAmplitude = clamp( settings.mAmplitude, 0.0f, FftSettings::kMaxAmplitude, defaults.mAmplitude ),
            .mChoppiness = clamp( settings.mChoppiness, 0.0f, FftSettings::kMaxChoppiness, defaults.mChoppiness ),
            .mSeed = settings.mSeed
        };
    }

    auto WaterSimulationComponent::GetFftSettings() const -> const FftSettings& {
        return mFftSettings;
    }

    auto WaterSimulationComponent::GetSteepness() const -> float {
        return mSteepness;
    }

    auto WaterSimulationComponent::GetTime() const -> core::f64 {
        return mTime;
    }

    auto WaterSimulationComponent::ResetTime() -> void {
        mTime = 0.0;
    }

    auto WaterSimulationComponent::Update( core::f32 timeStep ) -> void {
        if ( timeStep > 0.0f ) {
            mTime += static_cast<core::f64>( timeStep ) * mTimeScale;
        }
    }

    auto WaterSimulationComponent::GetType() const -> WaterSimulationType {
        return mType;
    }

    auto WaterSimulationComponent::GetWaves() const -> const eastl::vector<WaterWave>& {
        return mWaves;
    }

    auto WaterSimulationComponent::GetWaveCount() const -> core::usize {
        return mWaves.size();
    }

    auto WaterSimulationComponent::GetTimeScale() const -> float {
        return mTimeScale;
    }

    TagComponent::TagComponent( eastl::string_view tag, bool active )
        : mTag{ tag }, mIsActive{ active } {
    }

    auto TagComponent::GetGuid() const -> core::u32 {
        return mGuid;
    }

    auto TagComponent::GetGuidObject() -> math::random::Guid& {
        return mGuid;
    }

    auto TagComponent::IsActive() const -> bool {
        return mIsActive;
    }

    auto TagComponent::GetTag() const -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mTag;
    }

    auto TagComponent::SetTag( eastl::string_view newName ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mTag == newName ) {
            return;
        }

        mTag = newName;

        if ( mScene ) {
            mScene->NotifyHierarchyChanged( false );
        }
    }

    TagComponent::TagComponent( const TagComponent& other )
        : mGuid{ other.mGuid }, mTag{ other.mTag }, mIsActive{ other.mIsActive } {
    }

    TagComponent::TagComponent( TagComponent&& other ) noexcept
        : mScene{ other.mScene }, mGuid{ other.mGuid }, mTag{ eastl::move( other.mTag ) }, mIsActive{ other.mIsActive } {
        other.mScene = nullptr;
    }

    auto TagComponent::operator=( const TagComponent& other ) -> TagComponent& {
        if ( this != &other ) {
            // Assignment edits the destination entity; it must not subscribe to
            // the source scene or replace the GUID used by the owning scene map.
            SetTag( other.mTag );
            mIsActive = other.mIsActive;
        }

        return *this;
    }

    auto TagComponent::operator=( TagComponent&& other ) noexcept -> TagComponent& {
        if ( this != &other ) {
            // EnTT may move components when compacting storage. Transfer their
            // ownership and identity along with the data in that case.
            mScene = other.mScene;
            mGuid = other.mGuid;
            mTag = eastl::move( other.mTag );
            mIsActive = other.mIsActive;
            other.mScene = nullptr;
        }

        return *this;
    }

    RelationComponent::RelationComponent( const RelationComponent& other )
        : mParent{ other.mParent }, mChildrenIDs{ other.mChildrenIDs } {
    }

    auto RelationComponent::operator=( const RelationComponent& other ) -> RelationComponent& {
        if ( this != &other ) {
            if ( mScene ) {
                // An attached relation owns its children. Copying another
                // entity's child IDs would create two owners for one subtree.
                SetParent( other.mParent.value_or( 0 ) );
            }
            else {
                mParent = other.mParent;
                mChildrenIDs = other.mChildrenIDs;
            }
        }

        return *this;
    }

    auto RelationComponent::RegisterChild( core::u64 id ) -> void {
        if ( mChildrenIDs.insert( id ).second && mScene ) {
            mScene->NotifyHierarchyChanged( true );
        }
    }

    auto RelationComponent::EraseChild( core::u64 id ) -> void {
        if ( mChildrenIDs.erase( id ) && mScene ) {
            mScene->NotifyHierarchyChanged( true );
        }
    }

    auto RelationComponent::SetParent( core::u64 uid ) -> void {
        if ( mScene ) {
            ( void )mScene->ReparentEntity( mEntityID, uid );
        }
        else {
            mParent = uid ? eastl::make_optional( uid ) : eastl::nullopt;
        }
    }

    auto GroupComponent::AddEntity( core::u64 entityID ) -> bool {
        return mScene ? mScene->SetEntityGroup( entityID, mEntityID ) : mChildren.insert( entityID ).second;
    }

    auto GroupComponent::RemoveEntity( core::u64 entityID ) -> void {
        if ( !Contains( entityID ) ) {
            return;
        }

        if ( mScene ) {
            ( void )mScene->SetEntityGroup( entityID, 0 );
        }
        else {
            mChildren.erase( entityID );
        }
    }

    auto GroupComponent::Contains( core::u64 entityID ) const -> bool {
        return mChildren.contains( entityID );
    }

    auto GroupComponent::HasChildren() const -> bool {
        return !mChildren.empty();
    }

    auto GroupComponent::GetChildren() const -> const ankerl::unordered_dense::set<core::u64>& {
        return mChildren;
    }

    auto TagComponent::SetActive( bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mIsActive = value;
    }

    auto HighlightComponent::SetHighlighted( bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mIsHighlighted = value;
    }

    auto HighlightComponent::IsHighlighted() const -> bool {
        return mIsHighlighted;
    }

    TransformComponent::TransformComponent() {
        MKT_BEGIN_PROFILER();
        ComputeTransform( mTranslation, mScale, mRotation );
        mWorldTransform = mTransform;
    }

    TransformComponent::TransformComponent(
        const core::float3& position,
        const core::float3& size,
        const core::float3& angles,
        const core::float4x4& worldTransform )
        : mWorldTransform{ worldTransform } {
        MKT_BEGIN_PROFILER();
        ComputeTransform( position, size, angles );
    }

    TransformComponent::TransformComponent( const TransformComponent& other ) {
        *this = other;
    }

    TransformComponent::TransformComponent( TransformComponent&& other ) noexcept {
        *this = other;
    }

    auto TransformComponent::NextWorldRevision() -> core::u64 {
        static std::atomic<core::u64> revision{ 1 };
        return revision.fetch_add( 1, std::memory_order_relaxed );
    }

    auto TransformComponent::operator=( const TransformComponent& other ) -> TransformComponent& {
        if ( this != &other ) {
            mTranslation = other.mTranslation;
            mRotation = other.mRotation;
            mScale = other.mScale;
            mTransform = other.mTransform;
            mWorldTransform = other.mWorldTransform;
            mWorldRevision = NextWorldRevision();
            mHasUniformScale = other.mHasUniformScale;
            // A clean source can be copied into a different scene or parent.
            mTransformDirty = true;
        }

        return *this;
    }

    auto TransformComponent::operator=( TransformComponent&& other ) noexcept -> TransformComponent& {
        return *this = static_cast<const TransformComponent&>( other );
    }

    auto TransformComponent::GetTranslation() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mTranslation;
    }

    auto TransformComponent::GetRotation() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mRotation;
    }

    auto TransformComponent::GetScale() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mScale;
    }

    auto TransformComponent::GetTransform() const -> const core::float4x4& {
        MKT_BEGIN_PROFILER();
        return mTransform;
    }

    auto TransformComponent::HasUniformScale() const -> bool {
        MKT_BEGIN_PROFILER();
        return mHasUniformScale;
    }

    auto TransformComponent::GetRotationQuat() const -> core::quat {
        MKT_BEGIN_PROFILER();
        return glm::quat_cast( math::EntityRotationMatrix( mRotation ) );
    }

    auto TransformComponent::GetWorldTransform() const -> const core::float4x4& {
        return mWorldTransform;
    }

    auto TransformComponent::SetWorldTransform( const core::float4x4& worldTransform ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mWorldTransform != worldTransform ) {
            mWorldTransform = worldTransform;
            mWorldRevision = NextWorldRevision();
        }
        // Local space remains authoritative at the next scene update.
        mTransformDirty = true;
    }

    auto TransformComponent::ComputeTransform( const core::float3& position, const core::float3& size, const core::float3& angles ) -> void {
        MKT_BEGIN_PROFILER();
        mTranslation = position;
        mRotation = angles;
        mScale = size;

        // The stored transform is parent-relative, using degrees in YXZ order.
        // Editor pivot handling is applied before assigning this local transform.
        RecomputeLocalTransform();
    }

    auto TransformComponent::RecomputeLocalTransform() -> void {
        const auto transform{ math::RecomputeTransform( mTranslation, mScale, mRotation ) };

        if ( mTransform != transform ) {
            mTransform = transform;
            mTransformDirty = true;
        }
    }

    MaterialComponent::MaterialComponent( material::MaterialHandle mat, bool isInternal )
        : mIsInternalMaterial{ isInternal } {
        MKT_BEGIN_PROFILER();
        // Set material does some necessary validations
        SetMaterial( mat );
    }

    auto MaterialComponent::HasMaterial() const -> bool {
        return !mMaterial.IsEmpty();
    }

    auto MaterialComponent::GetMaterial() const -> material::MaterialHandle {
        MKT_BEGIN_PROFILER();
        return mMaterial;
    }

    auto MaterialComponent::GetMaterialReference() const -> const material::MaterialHandle& {
        return mMaterial;
    }

    auto MaterialComponent::IsInternal() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsInternalMaterial;
    }

    auto MaterialComponent::SetIsInternal( bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mIsInternalMaterial = value;
    }

    auto MaterialComponent::SetMaterial( material::MaterialHandle mat ) -> void {
        MKT_BEGIN_PROFILER();
        // Needs to be a surface material and not null
        material::SurfaceMaterial* pMaterial{ dynamic_cast<material::SurfaceMaterial*>( mat.GetPtr() ) };

        if ( !pMaterial ) {
            return;
        }

        mMaterial = mat;
    }

    TextureRenderComponent::TextureRenderComponent() = default;

    // Entity copies retain authored settings and create independent GPU state on first use.
    TextureRenderComponent::TextureRenderComponent( const TextureRenderComponent& other )
        : mCamera{ other.mCamera },
          mCameraEntityID{ other.mCameraEntityID },
          mResolution{ other.mResolution },
          mRenderRate{ other.mRenderRate },
          mRenderPath{ other.mRenderPath },
          mRenderOutput{ other.mRenderOutput } {
    }

    TextureRenderComponent::TextureRenderComponent( TextureRenderComponent&& other ) noexcept = default;

    auto TextureRenderComponent::operator=( TextureRenderComponent&& other ) noexcept -> TextureRenderComponent& = default;

    auto TextureRenderComponent::operator=( const TextureRenderComponent& other ) -> TextureRenderComponent& {
        if ( this != &other ) {
            *this = TextureRenderComponent{ other };
        }

        return *this;
    }

    TextureRenderComponent::~TextureRenderComponent() = default;

    auto TextureRenderComponent::SetRenderRate( TextureRenderRate rate ) -> void {
        mCaptureDirty |= mRenderRate != rate;
        mRenderRate = rate;
    }

    auto TextureRenderComponent::SetRenderPath( TextureRenderPath path ) -> void {
        mCaptureDirty |= mRenderPath != path;
        mRenderPath = path;
    }

    auto TextureRenderComponent::SetRenderOutput( TextureRenderOutput output ) -> void {
        mCaptureDirty |= mRenderOutput != output;
        mRenderOutput = output;
    }

    auto TextureRenderComponent::SetCamera( const Camera* camera, core::u64 entityID ) -> void {
        mCaptureDirty |= mCamera != camera || mCameraEntityID != entityID;
        mCamera = camera;
        mCameraEntityID = entityID;
    }

    auto TextureRenderComponent::SetResolution( renderer::rhi::RenderResolution resolution ) -> void {
        mCaptureDirty |= mResolution != resolution;
        mResolution = resolution;
    }

    auto TextureRenderComponent::GetRenderer() -> renderer::ISceneRenderer* {
        return mRuntime ? mRuntime->mRenderer.get() : nullptr;
    }

    auto TextureRenderComponent::Update( 
        Scene& scene, const 
        CameraComponent& camera, 
        const core::float4x4& cameraWorld, 
        const material::MaterialHandle& source, 
        core::f32 timeStep ) -> void 
    {
        MKT_BEGIN_PROFILER();

        auto* rendering{ RenderSystem::GetPtr() };
        auto* device{ rendering ? rendering->GetGpuDevice() : nullptr };

        if ( !mCamera || !device || ( !device->IsGraphicsApi( rhi::GraphicsAPI::eVulkan ) &&
            !device->IsGraphicsApi( rhi::GraphicsAPI::eD3D12 ) ) ) {
            return;
        }

        if ( !mRuntime ) {
            mRuntime = eastl::make_unique<Runtime>();
        }

        auto& runtime{ *mRuntime };
        const core::f64 interval{ 1.0 / ( mRenderRate == TextureRenderRate::e120FPS ? 120.0 :
            mRenderRate == TextureRenderRate::e60FPS ? 60.0 : 30.0 ) };
        runtime.mElapsed += std::isfinite( timeStep ) ? glm::max( core::f64{ timeStep }, 0.0 ) : 0.0;

        // Like a SubViewport, a capture renders an additional view without ticking its world again.
        // Reference: https://docs.godotengine.org/en/stable/classes/class_subviewport.html
        if ( !mCaptureDirty && runtime.mElapsed < interval ) {
            return;
        }

        if ( !runtime.mRenderer ) {
            runtime.mRenderer = ISceneRenderer::Create()
                .SetName( "TextureRenderer" )
                .SetDevice( device )
                .SetRenderResolution( mResolution )
                .Build();
            runtime.mRenderer->SetEditorOverlaysEnabled( false );
        }

        auto& renderer{ *runtime.mRenderer };
        auto settings{ renderer.GetSettings() };
        settings.mResolution = mResolution;
        settings.mRayTracing.mPathTracingEnabled = mRenderPath == TextureRenderPath::ePathTracing;
        renderer.ApplySettings( settings );

        if ( !renderer.HasFrameGraph() ) {
            renderer.Initialize();
        }

        // Copy the lens through Camera, preserving SceneCamera's intrusive reference counter.
        // This follows EditorLayer::UpdateGameCamera; the receiver's transform is unrelated.
        static_cast<Camera&>( runtime.mCamera ) = camera.GetCamera();
        runtime.mCamera.SetVignetteSettings( camera.GetCamera().GetVignetteSettings() );
        runtime.mCamera.SetFilmGrainSettings( camera.GetCamera().GetFilmGrainSettings() );

        const auto dimensions{ rhi::InferDimensions( mResolution ) };
        runtime.mCamera.SetViewportSize( dimensions.first, dimensions.second );
        runtime.mCamera.SetRenderPose( Camera::ExtractRenderPose( cameraWorld ) );

        renderer.SetMainCamera( &runtime.mCamera );
        renderer.SetClearColor( camera.GetClearColor() );
        renderer.SetExposure( camera.GetExposure() );
        renderer.SetSkyboxBlur( camera.GetSkyboxBlur() );
        renderer.SetEnableInfiniteGrid( false );
        // Material sampling expects linear values; the consuming view applies display gamma.
        renderer.SetGamma( 1.0f );

        const auto flags{ camera.GetClearFlags() };
        renderer.SetRenderBackground( flags == CameraClearFlags::eSkybox ? SceneBackgroundType::eSkybox :
            flags == CameraClearFlags::eBlurredSkybox ? SceneBackgroundType::ePrefilterMap : SceneBackgroundType::eClearColor );
        scene.Update( &renderer );

        // The receiving surface uses its authored material in its own capture. Sampling this
        // renderer's output while writing it would create an attachment feedback loop.
        runtime.mCapturing = true;
        const TextureCaptureGuard capture{ runtime.mCapturing };
        renderer.Render( &scene );
        runtime.mTexture = renderer.GetFinalImage();

        if ( !runtime.mTexture ) {
            return;
        }

        ++runtime.mTextureRevision;
        runtime.mElapsed = std::fmod( runtime.mElapsed, interval );
        mCaptureDirty = false;

        // Adapt VideoService's rendering-only material clone: authored assets remain shared.
        // Viewport-texture reference: https://docs.godotengine.org/en/stable/tutorials/shaders/using_viewport_as_texture.html
        const auto* physical{ dynamic_cast<const material::SurfaceMaterial*>( source.GetPtr() ) };

        if ( !physical ) {
            runtime.mSource.Reset();
            runtime.mMaterial.Reset();
            return;
        }

        if ( !runtime.mMaterial || runtime.mSource.GetPtr() != physical || runtime.mSourceRevision != physical->GetRevision() ||
            runtime.mOutput != mRenderOutput ) {
            runtime.mSource = source;
            runtime.mSourceRevision = physical->GetRevision();
            runtime.mOutput = mRenderOutput;
            runtime.mMaterial = material::SurfaceMaterialHandle::New( *physical );

            if ( mRenderOutput == TextureRenderOutput::eEmissive ) {
                runtime.mMaterial->SetEmissiveFactor( core::float3{ 1.0f } );
                runtime.mMaterial->SetEmissiveStrength( 1.0f );
            }
        }

        const auto map{ mRenderOutput == TextureRenderOutput::eEmissive ? material::MapType::eEmissive :
            physical->GetWorkflow() == material::Workflow::eSpecularGlossiness ? material::MapType::eDiffuse : material::MapType::eBaseColor };
        runtime.mMaterial->SetTexture( map, runtime.mTexture );
        runtime.mMaterial->UseTexture( map );
    }

    auto TextureRenderComponent::GetRenderRate() const -> TextureRenderRate {
        return mRenderRate;
    }

    auto TextureRenderComponent::GetRenderPath() const -> TextureRenderPath {
        return mRenderPath;
    }

    auto TextureRenderComponent::GetCamera() const -> const Camera* {
        return mCamera;
    }

    auto TextureRenderComponent::GetCameraEntityID() const -> core::u64 {
        return mCameraEntityID;
    }

    auto TextureRenderComponent::GetRenderOutput() const -> TextureRenderOutput {
        return mRenderOutput;
    }

    auto TextureRenderComponent::GetMaterial() const -> material::MaterialHandle {
        if ( mCamera && mRuntime && !mRuntime->mCapturing ) {
            return mRuntime->mMaterial;
        }

        return {};
    }

    auto TextureRenderComponent::GetTexture() const -> renderer::rhi::TextureHandle {
        return mRuntime ? mRuntime->mTexture : rhi::TextureHandle{};
    }

    auto TextureRenderComponent::GetTextureRevision() const -> core::u64 {
        return mRuntime && !mRuntime->mCapturing ? mRuntime->mTextureRevision : 0;
    }

    auto TextureRenderComponent::GetResolution() const -> renderer::rhi::RenderResolution {
        return mResolution;
    }

    auto CameraComponent::SetClearFlags( CameraClearFlags bg ) -> void {
        MKT_BEGIN_PROFILER();
        mClearFlags = bg;
    }

    auto CameraComponent::GetClearFlags() const -> CameraClearFlags {
        MKT_BEGIN_PROFILER();
        return mClearFlags;
    }

    auto CameraComponent::SetClearColor( const renderer::rhi::Color& color ) -> void {
        MKT_BEGIN_PROFILER();
        mColor = color;
    }

    auto CameraComponent::GetClearColor() const -> const renderer::rhi::Color& {
        MKT_BEGIN_PROFILER();
        return mColor;
    }

    auto CameraComponent::SetMainCamera( bool main ) -> void {
        MKT_BEGIN_PROFILER();
        mMainCam = main;
    }

    auto CameraComponent::IsMainCamera() const -> bool {
        MKT_BEGIN_PROFILER();
        return mMainCam;
    }

    CameraComponent::CameraComponent( platform::IWindow* window ) {
        MKT_BEGIN_PROFILER();
        SceneCameraDescription cameraDescription{
            .mFov = 45.0f,
            .mNearPlane = 0.1f,
            .mFarPlane = 3000.0f,
            .mWindow = window
        };

        if ( window ) {
            cameraDescription.SetAspectRatio( core::as<core::f32>( window->GetWidth() ), core::as<core::f32>( window->GetHeight() ) );
        }

        mCamera = core::Ref<SceneCamera>::New( cameraDescription );

        if ( window ) {
            mCamera->SetViewportSize( core::as<core::f32>( window->GetWidth() ), core::as<core::f32>( window->GetHeight() ) );
        }
    }

    auto CameraComponent::HasCamera() const -> bool {
        MKT_BEGIN_PROFILER();
        return mCamera != nullptr;
    }

    auto CameraComponent::GetCamera() -> SceneCamera& {
        MKT_BEGIN_PROFILER();
        return *mCamera;
    }

    auto CameraComponent::GetCamera() const -> const SceneCamera& {
        MKT_BEGIN_PROFILER();
        return *mCamera;
    }

    auto CameraComponent::IsAspectRatioFixed() const -> bool {
        MKT_BEGIN_PROFILER();
        return mFixedAspectRatio;
    }

    auto CameraComponent::SetGamma( float gamma ) -> void {
        MKT_BEGIN_PROFILER();
        mGamma = gamma;
    }

    auto CameraComponent::SetSkyboxBlur( float value ) -> void {
        MKT_BEGIN_PROFILER();
        mSkyboxBlur = math::Clamp( value, 0.0f, 1.0f );
    }

    auto CameraComponent::SetExposure( float exposure ) -> void {
        MKT_BEGIN_PROFILER();
        mExposure = exposure;
    }

    auto CameraComponent::GetGamma() const -> float {
        MKT_BEGIN_PROFILER();
        return mGamma;
    }

    auto CameraComponent::GetExposure() const -> float {
        MKT_BEGIN_PROFILER();
        return mExposure;
    }

    auto CameraComponent::GetSkyboxBlur() const -> float {
        MKT_BEGIN_PROFILER();
        return mSkyboxBlur;
    }

    auto CameraComponent::SetFixedAspectRatio( const bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mFixedAspectRatio = value;
    }

    auto CameraComponent::OnViewportResize( core::f32 width, core::f32 height ) -> void {
        MKT_BEGIN_PROFILER();
        // Both editor resize and runtime camera selection must honor this flag.
        if ( mCamera && !mFixedAspectRatio ) {
            mCamera->SetViewportSize( width, height );
        }
    }

    ScriptComponent::ScriptComponent( const filesystem::Path& filePath, scripting::ScriptLanguage language, eastl::string_view className )
        : mFilePath{ filePath }, mLanguage{ language }, mClassName{ className } {
    }

    auto ScriptComponent::SetLanguage( scripting::ScriptLanguage language ) -> void {
        if ( mLanguage == language ) {
            return;
        }

        if ( mScript ) {
            mScript->Stop();
        }

        mScript.Reset();
        mFilePath = {};
        mClassName.clear();
        mProperties = "{}";
        mLanguage = language;
    }

    auto ScriptComponent::SetManagedScript( const filesystem::Path& assembly, eastl::string_view className ) -> void {
        if ( mFilePath.GetAbsolute() != assembly.GetAbsolute() || mClassName != className || mLanguage != scripting::ScriptLanguage::eCSharp ) {
            mProperties = "{}";
        }

        if ( mScript ) {
            mScript->Stop();
        }

        mScript.Reset();
        mLanguage = scripting::ScriptLanguage::eCSharp;
        mFilePath = assembly;
        mClassName = className;
    }

    auto ScriptComponent::SetProperties( eastl::string_view properties ) -> void {
        mProperties = properties;
    }

    auto ScriptComponent::GetProperties() const -> const eastl::string& {
        return mProperties;
    }

    auto ScriptComponent::GetLanguage() const -> scripting::ScriptLanguage {
        return mLanguage;
    }

    auto ScriptComponent::GetClassName() const -> const eastl::string& {
        return mClassName;
    }

    auto ScriptComponent::SetScript( scripting::ScriptHandle handle ) -> void {
        MKT_BEGIN_PROFILER();

        if ( handle.IsEmpty() ) {
            return;
        }

        if ( mScript == handle ) {
            return;
        }

        if ( mScript ) {
            mScript->Stop();
        }

        // FileService returns an absolute path even when the component was saved
        // with a relative path. Loading the same script must preserve overrides.
        if ( mFilePath.GetAbsolute() != handle->GetFile()->GetPath().GetAbsolute() || mLanguage != handle->GetLanguage() ) {
            mProperties = "{}";
        }

        mLanguage = handle->GetLanguage();

        if ( mLanguage != scripting::ScriptLanguage::eCSharp ) {
            mClassName.clear();
        }

        mFilePath = handle->GetFile()->GetPath();
        mScript = eastl::move( handle );
    }

    auto ScriptComponent::GetHandle() -> scripting::ScriptHandle {
        MKT_BEGIN_PROFILER();
        return mScript;
    }

    auto ScriptComponent::GetFilePath() const -> const filesystem::Path& {
        MKT_BEGIN_PROFILER();
        return mFilePath;
    }


    auto TransformComponent::SetTransform( const core::float4x4& transform ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mTransform == transform ) {
            return;
        }

        mTransform = transform;
        math::Decompose( mTransform, mTranslation, mRotation, mScale );
        mTransformDirty = true;
    }

    auto TransformComponent::SetTranslation( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mTranslation == value ) {
            return;
        }

        mTranslation = value;
        // Translation must preserve the exact imported/gizmo basis, including
        // negative scale and shear under a nonuniformly scaled parent.
        mTransform[3] = core::float4{ value, 1.0f };
        mTransformDirty = true;
    }

    auto TransformComponent::SetRotation( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mRotation == value ) {
            return;
        }
        // Replace only the orientation. Rebuilding from displayed TRS values
        // would discard shear introduced by a scaled parent or imported mesh.
        const auto delta{ math::EntityRotationMatrix( value ) * glm::transpose( math::EntityRotationMatrix( mRotation ) ) };

        for ( int column{}; column < 3; ++column ) {
            mTransform[column] = delta * mTransform[column];
        }

        mRotation = value;
        mTransformDirty = true;
    }

    auto TransformComponent::SetRotation( const core::quat& quaternion ) -> void {
        MKT_BEGIN_PROFILER();
        SetRotation( math::EntityRotationDegrees( glm::mat4_cast( glm::normalize( quaternion ) ) ) );
    }

    auto TransformComponent::SetScale( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !mHasUniformScale ) {
            mScale = value;
        }
        else {
            float offSet{ 0 };

            if ( value.x != mScale.x ) {
                offSet = value.x - mScale.x;
            }
            else if ( value.y != mScale.y ) {
                offSet = value.y - mScale.y;
            }
            else if ( value.z != mScale.z ) {
                offSet = value.z - mScale.z;
            }

            if ( offSet != 0 ) {
                mScale.x += offSet;
                mScale.y += offSet;
                mScale.z += offSet;
            }
        }

        RecomputeLocalTransform();
    }

    auto TransformComponent::SetUniformSale( const bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mHasUniformScale = value;
    }

    auto TransformComponent::GetWorldRevision() const -> core::u64 {
        return mWorldRevision;
    }


    RelationComponent::RelationComponent( const eastl::optional<core::u64> parent )
        : mParent{ parent } {
    }


    auto RelationComponent::IsChild( const core::u64 id ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mChildrenIDs.contains( id );
    }


    auto RelationComponent::HasChildren() const -> bool {
        MKT_BEGIN_PROFILER();
        return !mChildrenIDs.empty();
    }


    auto RelationComponent::HasParent() const -> bool {
        MKT_BEGIN_PROFILER();
        return mParent.has_value();
    }


    auto RelationComponent::GetParent() const -> const eastl::optional<core::u64>& {
        MKT_BEGIN_PROFILER();
        return mParent;
    }


    auto RelationComponent::IsLeaf() const -> bool {
        MKT_BEGIN_PROFILER();
        return mChildrenIDs.empty();
    }


    auto RelationComponent::GetChildren() const -> const ankerl::unordered_dense::set<core::u64>& {
        MKT_BEGIN_PROFILER();
        return ( mChildrenIDs );
    }

    auto MeshComponent::GetMeshIndex() const -> core::i32 {
        MKT_BEGIN_PROFILER();
        return mMeshIndex;
    }


    auto MeshComponent::GetModelPath() const -> const filesystem::Path& {
        MKT_BEGIN_PROFILER();
        return mModel.IsEmpty() ? mPath : mModel->GetPath();
    }


    auto MeshComponent::HasMesh() const -> bool {
        return !mModel.IsEmpty() && mMeshIndex >= 0 && static_cast<core::usize>( mMeshIndex ) < mModel->GetMeshNodeCount();
    }


    auto MeshComponent::GetMesh() const -> const asset::MeshNode* {
        if ( !HasMesh() ) {
            return nullptr;
        }

        return std::addressof( mModel->GetMeshNode( static_cast<core::u32>( mMeshIndex ) ) );
    }


    auto MeshComponent::GetMesh() -> asset::MeshNode* {
        if ( !HasMesh() ) {
            return nullptr;
        }

        return std::addressof( mModel->GetMeshNode( static_cast<core::u32>( mMeshIndex ) ) );
    }


    auto MeshComponent::GetModel() -> asset::ModelHandle {
        return mModel;
    }


    auto MeshComponent::GetModel() const -> asset::ModelHandle {
        return mModel;
    }


    auto MeshComponent::GetName() const -> eastl::string_view {
        MKT_BEGIN_PROFILER();
        return GetModelPath().GetFilename();
    }


    auto MeshComponent::IsSkinned() const -> bool {
        MKT_BEGIN_PROFILER();
        return mModel && mModel->IsSkinned();
    }


    auto MeshComponent::HasArmature() const -> bool {
        MKT_BEGIN_PROFILER();
        return mModel && mModel->HasArmature();
    }


    auto MeshComponent::SetAnimator( core::u64 id ) -> void {
        MKT_BEGIN_PROFILER();
        mAnimatorID = id;
    }


    auto MeshComponent::HasAnimations() const -> bool {
        MKT_BEGIN_PROFILER();
        return mModel && mModel->HasAnimations();
    }


    LightComponent::LightComponent( renderer::LightType type )
        : mType{ type } {
    }


    auto LightComponent::IsTypeActive( const renderer::LightType type ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mType == type;
    }


    auto LightComponent::GetActiveType() const -> renderer::LightType {
        MKT_BEGIN_PROFILER();
        return mType;
    }


    auto LightComponent::SetActiveType( const renderer::LightType type ) -> void {
        MKT_BEGIN_PROFILER();
        mType = type;
    }

    AudioMixerComponent::AudioMixerComponent( const AudioMixerComponent& other )
        : mVolume{ other.mVolume }, mIsMuted{ other.mIsMuted } {
    }

    auto AudioMixerComponent::operator=( const AudioMixerComponent& other ) -> AudioMixerComponent& {
        if ( this != &other ) {
            mRuntime.reset();
            mVolume = other.mVolume;
            mIsMuted = other.mIsMuted;
        }

        return *this;
    }

    auto AudioMixerComponent::SetMute( bool mute ) -> void {
        mIsMuted = mute;
    }

    auto AudioMixerComponent::SetVolume( core::f32 volume ) -> void {
        mVolume = math::Clamp( volume, 0.0f, 1000.0f );
    }

    auto AudioMixerComponent::IsMuted() const -> bool {
        return mIsMuted;
    }

    auto AudioMixerComponent::GetVolume() const -> core::f32 {
        return mVolume;
    }


    auto AudioMixerComponent::GetMixer( audio::AudioDevice* device ) -> audio::AudioMixer* {
        if ( device && device->IsInitialized() && ( !mRuntime || !mRuntime->IsInitialized() ) ) {
            // Publish the runtime only after its native buses are ready.
            auto mixer{ eastl::make_shared<audio::AudioMixer>( device ) };
            mixer->Initialize();
            mRuntime = eastl::move( mixer );
        }

        return mRuntime && mRuntime->IsInitialized() ? mRuntime.get() : nullptr;
    }

    AudioSourceComponent::AudioSourceComponent( const filesystem::Path& path ) {
        MKT_BEGIN_PROFILER();

        if ( path.IsEmpty() ) {
            return;
        }

        if ( auto file{ filesystem::FileService::Get()->LoadFile( path ) }; !file ) {
            MKT_CORE_LOGGER_ERROR( "AudioSourceComponent - Failed to load audio file: {}", path.GetC_Str() );
        }
        else {
            const asset::AudioLoadDescription desc{
                .mVolume = 0.5f,
                .mFile = file,
            };

            if ( audio::AudioHandle handle{ asset::AssetRegistryService::Get()->LoadAsset<audio::Audio>( desc ) }; handle.IsEmpty() ) {
                MKT_CORE_LOGGER_ERROR( "AudioSourceComponent - Audio handle is empty: {}", path.GetC_Str() );
            }
            else {
                SetClip( handle );
            }
        }
    }

    AudioSourceComponent::AudioSourceComponent( const AudioSourceComponent& other ) {
        MKT_BEGIN_PROFILER();
        *this = other;
    }

    AudioSourceComponent::~AudioSourceComponent() {
        MKT_BEGIN_PROFILER();

        if ( mAudioSource ) {
            mAudioSource->Stop();
        }
    }

    auto AudioSourceComponent::GetSource() const -> audio::AudioSourceHandle {
        MKT_BEGIN_PROFILER();
        return mAudioSource;
    }

    auto AudioSourceComponent::SetClip( const audio::AudioHandle& clip ) -> void {
        MKT_BEGIN_PROFILER();
        auto replacement = clip ? clip->CreateSource() : audio::AudioSourceHandle{};

        if ( clip && !replacement ) {
            return;// Failed replacement keeps the current working source.
        }

        mSettings = GetSettings();

        if ( replacement ) {
            replacement->SetSettings( mSettings );
        }

        if ( mAudioSource ) {
            mAudioSource->Stop();
        }

        mClip = clip;
        mAudioSource = eastl::move( replacement );
    }


    auto AudioSourceComponent::GetClip() const -> audio::AudioHandle {
        MKT_BEGIN_PROFILER();
        return mClip;
    }

    auto AudioSourceComponent::GetSettings() const -> const audio::SourceSettings& {
        return mAudioSource ? mAudioSource->GetSettings() : mSettings;
    }

    auto AudioSourceComponent::SetSettings( audio::SourceSettings settings ) -> void {
        mSettings = audio::SanitizeAudio( settings );

        if ( mAudioSource ) {
            mAudioSource->SetSettings( mSettings );
        }
    }


    AudioListenerComponent::AudioListenerComponent( const AudioListenerComponent& other )
        : mEnabled{ other.mEnabled } {
    }


    auto AudioListenerComponent::SetListener( audio::AudioListener* listener ) -> void {
        MKT_BEGIN_PROFILER();
        mListener = listener;
    }


    auto AudioListenerComponent::Disable() -> void {
        MKT_BEGIN_PROFILER();
        SetEnabled( false );
    }


    auto AudioListenerComponent::GetListenerPtr() const -> audio::AudioListener* {
        MKT_BEGIN_PROFILER();
        return mListener;
    }


    auto AudioListenerComponent::SetEnabled( bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mEnabled = value;

        if ( mListener ) {
            mListener->SetEnabled( value );
        }
    }


    auto AudioListenerComponent::IsEnabled() const -> bool {
        MKT_BEGIN_PROFILER();
        return mEnabled;
    }


    auto AudioListenerComponent::IsActive() -> bool {
        MKT_BEGIN_PROFILER();
        return mListener != nullptr;
    }


    auto AudioListenerComponent::GetListener() -> audio::AudioListener& {
        MKT_BEGIN_PROFILER();
        return *mListener;
    }


    auto AudioListenerComponent::GetListener() const -> const audio::AudioListener& {
        MKT_BEGIN_PROFILER();
        return *mListener;
    }


    RigidBodyComponent::RigidBodyComponent( const core::u64 bodyID )
        : mBodyID{ bodyID }, mIsValidBody{ true } {
    }


    RigidBodyComponent::RigidBodyComponent( const RigidBodyComponent& other ) {
        MKT_BEGIN_PROFILER();
        *this = other;
    }


    auto RigidBodyComponent::GetMass() const -> float {
        MKT_BEGIN_PROFILER();
        return mMass;
    }


    auto RigidBodyComponent::SetMass( const float mass ) -> void {
        MKT_BEGIN_PROFILER();
        mMass = math::Clamp( mass, 1.0f, GetMaxBodyMass() );
    }


    auto RigidBodyComponent::GetMaxBodyMass() -> float {
        MKT_BEGIN_PROFILER();
        return 300000.0f;
    }


    auto RigidBodyComponent::GetFriction() const -> float {
        MKT_BEGIN_PROFILER();
        return mFriction;
    }


    auto RigidBodyComponent::SetFriction( const float friction ) -> void {
        MKT_BEGIN_PROFILER();
        mFriction = math::Clamp( friction, 0.0f, 1.0f );
    }


    auto RigidBodyComponent::UseGravity() const -> bool {
        MKT_BEGIN_PROFILER();
        return mUseGravity;
    }


    auto RigidBodyComponent::SetUseGravity( const bool enabled ) -> void {
        MKT_BEGIN_PROFILER();
        mUseGravity = enabled;
    }


    auto RigidBodyComponent::GetBodyType() const -> BodyType {
        MKT_BEGIN_PROFILER();
        return mBodyType;
    }


    auto RigidBodyComponent::IsBodyType( const BodyType type ) const -> bool {
        MKT_BEGIN_PROFILER();
        return mBodyType == type;
    }


    auto RigidBodyComponent::IsDynamic() const -> bool {
        MKT_BEGIN_PROFILER();
        return mBodyType == BodyType::eDynamic;
    }


    auto RigidBodyComponent::SetBodyType( const BodyType type ) -> void {
        MKT_BEGIN_PROFILER();
        mBodyType = type;
    }


    auto RigidBodyComponent::GetBodyID() const -> core::u64 {
        MKT_BEGIN_PROFILER();
        return mBodyID;
    }


    auto RigidBodyComponent::IsValidBodyID() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsValidBody;
    }


    auto RigidBodyComponent::SetLinearVelocity( float x, float y, float z ) -> void {
        MKT_BEGIN_PROFILER();
        mLinearVelocity = { x, y, z };
    }


    auto RigidBodyComponent::SetLinearVelocity( const core::float3& velocity ) -> void {
        MKT_BEGIN_PROFILER();
        mLinearVelocity = velocity;
    }


    auto RigidBodyComponent::SetAngularVelocity( float x, float y, float z ) -> void {
        MKT_BEGIN_PROFILER();
        mAngularVelocity = { x, y, z };
    }


    auto RigidBodyComponent::SetAngularVelocity( const core::float3& velocity ) -> void {
        MKT_BEGIN_PROFILER();
        mAngularVelocity = velocity;
    }


    auto RigidBodyComponent::GetLinearVelocity() const -> core::float3 {
        MKT_BEGIN_PROFILER();
        return mLinearVelocity;
    }


    auto RigidBodyComponent::GetAngularVelocity() const -> core::float3 {
        MKT_BEGIN_PROFILER();
        return mAngularVelocity;
    }


    auto RigidBodyComponent::SetBodyID( const core::u64 bodyID ) -> void {
        MKT_BEGIN_PROFILER();
        mBodyID = bodyID;
        mIsValidBody = true;
    }


    auto RigidBodyComponent::RemoveBodyID() -> void {
        MKT_BEGIN_PROFILER();
        mBodyID = 0;
        mIsValidBody = false;
    }


    auto RigidBodyComponent::SetRestitution( float value ) -> void {
        MKT_BEGIN_PROFILER();
        mRestitution = math::Clamp( value, 0.0f, 1.0f );
    }


    auto RigidBodyComponent::GetRestitution() const -> float {
        MKT_BEGIN_PROFILER();
        return mRestitution;
    }


    auto MeshColliderComponent::IsConvex() const -> bool {
        MKT_BEGIN_PROFILER();
        return mConvex;
    }


    auto MeshColliderComponent::SetConvex( bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mConvex = value;
    }


    auto MeshColliderComponent::GetOffset() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mOffset;
    }


    auto MeshColliderComponent::SetOffset( const core::float3& offset ) -> void {
        MKT_BEGIN_PROFILER();
        mOffset = offset;
    }


    auto MeshColliderComponent::IsTrigger() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsTrigger;
    }


    auto MeshColliderComponent::SetTrigger( const bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mIsTrigger = value;
    }


    auto CapsuleColliderComponent::GetRadius() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mRadius;
    }


    auto CapsuleColliderComponent::SetRadius( const core::f32 radius ) -> void {
        MKT_BEGIN_PROFILER();
        mRadius = glm::max( radius, 0.001f );
    }


    auto CapsuleColliderComponent::GetHalfHeight() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mHalfHeight;
    }


    auto CapsuleColliderComponent::SetHalfHeight( const core::f32 height ) -> void {
        MKT_BEGIN_PROFILER();
        mHalfHeight = glm::max( height, 0.0f );
    }


    auto CapsuleColliderComponent::GetOffset() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mOffset;
    }


    auto CapsuleColliderComponent::SetOffset( const core::float3& offset ) -> void {
        MKT_BEGIN_PROFILER();
        mOffset = offset;
    }


    auto CapsuleColliderComponent::IsTrigger() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsTrigger;
    }


    auto CapsuleColliderComponent::SetTrigger( const bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mIsTrigger = value;
    }


    auto SphereColliderComponent::GetRadius() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mRadius;
    }


    auto SphereColliderComponent::SetRadius( const core::f32 radius ) -> void {
        MKT_BEGIN_PROFILER();
        mRadius = glm::max( radius, 0.001f );
    }


    auto SphereColliderComponent::GetOffset() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mOffset;
    }


    auto SphereColliderComponent::SetOffset( const core::float3& offset ) -> void {
        MKT_BEGIN_PROFILER();
        mOffset = offset;
    }


    auto SphereColliderComponent::IsTrigger() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsTrigger;
    }


    auto SphereColliderComponent::SetTrigger( const bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mIsTrigger = value;
    }


    auto BoxColliderComponent::GetHalfExtents() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mHalfExtents;
    }


    auto BoxColliderComponent::SetHalfExtents( const core::float3& extents ) -> void {
        MKT_BEGIN_PROFILER();
        mHalfExtents = glm::max( extents, core::float3{ 0.001f } );
    }


    auto BoxColliderComponent::GetOffset() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mOffset;
    }


    auto BoxColliderComponent::SetOffset( const core::float3& offset ) -> void {
        MKT_BEGIN_PROFILER();
        mOffset = offset;
    }


    auto BoxColliderComponent::IsTrigger() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsTrigger;
    }


    auto BoxColliderComponent::SetTrigger( const bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mIsTrigger = value;
    }


    TextComponent::TextComponent( eastl::string_view textContent, float size, float spacing, bool isWorld )
        : mTextContent{ textContent }, mStringContent{ textContent }, mIsWorldText{ isWorld } {
        MKT_BEGIN_PROFILER();
        SetSize( size );
        SetSpacing( spacing );
    }


    auto TextComponent::SetFont( renderer::FontHandle font ) -> void {
        MKT_BEGIN_PROFILER();

        if ( !font.IsEmpty() ) {
            mFont = font;
        }
    }


    auto TextComponent::HasFont() const -> bool {
        MKT_BEGIN_PROFILER();
        return !mFont.IsEmpty();
    }


    auto TextComponent::SetCamera( const Camera* camera ) -> void {
        MKT_BEGIN_PROFILER();

        if ( camera != nullptr ) {
            mCamera = camera;
        }
    }


    auto TextComponent::SetIsWorldText( bool value ) -> void {
        MKT_BEGIN_PROFILER();
        mIsWorldText = value;
    }


    auto TextComponent::IsWorldText() const -> bool {
        MKT_BEGIN_PROFILER();
        return mIsWorldText;
    }


    auto TextComponent::GetCamera() const -> const Camera* {
        MKT_BEGIN_PROFILER();
        return mCamera;
    }


    auto TextComponent::GetFont() const -> const renderer::Font* {
        MKT_BEGIN_PROFILER();
        return mFont.GetPtr();
    }


    auto TextComponent::GetFontHandle() const -> renderer::FontHandle {
        MKT_BEGIN_PROFILER();
        return mFont;
    }


    auto TextComponent::GetColor() const -> const core::float4& {
        MKT_BEGIN_PROFILER();
        return mColor;
    }


    auto TextComponent::SetMaterial( core::Ref<material::TextMaterial> material ) -> void {
        mMaterial = eastl::move( material );
    }


    auto TextComponent::GetMaterial() const -> core::Ref<material::TextMaterial> {
        return mMaterial;
    }


    auto TextComponent::GetRenderColor() const -> core::float4 {
        return mMaterial ? mMaterial->Evaluate( mColor ) : mColor;
    }


    auto TextComponent::GetSize() const -> float {
        MKT_BEGIN_PROFILER();
        return GetSize( mIsWorldText );
    }


    auto TextComponent::GetSpacing() const -> float {
        MKT_BEGIN_PROFILER();
        return GetSpacing( mIsWorldText );
    }


    auto TextComponent::GetSize( bool world ) const -> float {
        MKT_BEGIN_PROFILER();
        return world ? mWorldSize : mScreenSize;
    }


    auto TextComponent::GetSpacing( bool world ) const -> float {
        MKT_BEGIN_PROFILER();
        return world ? mWorldSpacing : mScreenSpacing;
    }


    auto TextComponent::GetScreenPlacement() -> text::ScreenPlacement& {
        MKT_BEGIN_PROFILER();
        return mScreenPlacement;
    }


    auto TextComponent::GetScreenPlacement() const -> const text::ScreenPlacement& {
        MKT_BEGIN_PROFILER();
        return mScreenPlacement;
    }


    auto TextComponent::GetLineSpacing() const -> float {
        MKT_BEGIN_PROFILER();
        return mLineSpacing;
    }


    auto TextComponent::GetAlignment() const -> TextAlignment {
        MKT_BEGIN_PROFILER();
        return mAlignment;
    }


    auto TextComponent::GetBackend() const -> TextBackend {
        MKT_BEGIN_PROFILER();
        return mBackend;
    }


    auto TextComponent::SetBackend( TextBackend value ) -> void {
        MKT_BEGIN_PROFILER();
        mBackend = value;
    }


    auto TextComponent::SetAlignment( TextAlignment value ) -> void {
        MKT_BEGIN_PROFILER();
        mAlignment = value;
    }


    auto TextComponent::SetLineSpacing( float value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( value > 0.0f ) {
            mLineSpacing = value;
        }
    }


    auto TextComponent::GetContents() const -> const eastl::string& {
        MKT_BEGIN_PROFILER();
        return mTextContent;
    }


    auto TextComponent::GetUnicodeContents() const -> const text::UnicodeString& {
        MKT_BEGIN_PROFILER();
        return mStringContent;
    }


    auto TextComponent::GetMinLetterSpacing() -> float {
        MKT_BEGIN_PROFILER();
        return 0.0f;
    }


    auto TextComponent::GetMaxLetterSpacing() -> float {
        MKT_BEGIN_PROFILER();
        return 1000.0f;
    }


    auto TextComponent::GetMinLetterSize() -> float {
        MKT_BEGIN_PROFILER();
        return 1.0f;
    }


    auto TextComponent::GetMaxLetterSize() -> float {
        MKT_BEGIN_PROFILER();
        return 10000.0f;
    }


    auto TextComponent::SetSize( float value ) -> void {
        MKT_BEGIN_PROFILER();
        SetSize( value, mIsWorldText );
    }


    auto TextComponent::SetSize( const float value, bool world ) -> void {
        MKT_BEGIN_PROFILER();

        if ( value > 0.0f ) {
            ( world ? mWorldSize : mScreenSize ) = value;
        }
    }


    auto TextComponent::SetSpacing( float value ) -> void {
        MKT_BEGIN_PROFILER();
        SetSpacing( value, mIsWorldText );
    }


    auto TextComponent::SetSpacing( const float value, bool world ) -> void {
        MKT_BEGIN_PROFILER();
        ( world ? mWorldSpacing : mScreenSpacing ) = value;
    }


    auto TextComponent::SetContents( const eastl::string_view content ) -> void {
        MKT_BEGIN_PROFILER();

        if ( content == mTextContent ) {
            return;
        }

        mTextContent = content;
        mStringContent.AssignUtf8( mTextContent );
    }


    KeyframeAnimationComponent::KeyframeAnimationComponent( const KeyframeAnimationComponent& other )
        : mClip{ other.mClip } {
    }

    auto KeyframeAnimationComponent::operator=( const KeyframeAnimationComponent& other ) -> KeyframeAnimationComponent& {
        if ( this != &other ) {
            *this = KeyframeAnimationComponent{ other };
        }

        return *this;
    }

    auto KeyframeAnimationComponent::ReadProperty( const Entity& entity, animation::KeyframeChannel channel ) -> core::float3 {
        const auto& transform{ entity.GetComponent<TransformComponent>() };

        switch ( channel ) {
            case animation::KeyframeChannel::ePosition:
                return transform.GetTranslation();
            case animation::KeyframeChannel::eRotation:
                return transform.GetRotation();
            case animation::KeyframeChannel::eScale:
                return transform.GetScale();
            default:
                break;
        }

        core::float3 result{};

        if ( entity.HasComponent<LightComponent>() ) {
            const auto& light{ entity.GetComponent<LightComponent>() };
            VisitLight( light, light.GetActiveType(), [&]( const auto& value ) {
                result = channel == animation::KeyframeChannel::eLightColor ? value.GetColor() : core::float3{ value.GetIntensity(), 0.0f, 0.0f };
            } );
        }

        return result;
    }

    auto KeyframeAnimationComponent::CaptureRest( const Entity& entity ) -> void {
        if ( mHasSnapshot ) {
            return;
        }

        mRestTransform = entity.GetComponent<TransformComponent>().GetTransform();
        mHadLight = entity.HasComponent<LightComponent>();

        if ( mHadLight ) {
            const auto& light{ entity.GetComponent<LightComponent>() };

            for ( core::usize index{}; index < mRestLightColors.size(); ++index ) {
                VisitLight( light, static_cast<renderer::LightType>( index ), [&]( const auto& value ) {
                    mRestLightColors[index] = value.GetColor();
                    mRestLightIntensities[index] = value.GetIntensity();
                } );
            }
        }

        mHasSnapshot = true;
    }

    auto KeyframeAnimationComponent::Play( Entity& entity, bool preview ) -> void {
        mClip.Sanitize();
        CaptureRest( entity );
        mPreviewing = preview;
        mPlaying = true;

        if ( mFrame >= mClip.mLastFrame ) {
            mFrame = 0.0;
        }

        Apply( entity );
    }

    auto KeyframeAnimationComponent::Pause() -> void {
        mPlaying = false;
    }

    auto KeyframeAnimationComponent::RestoreLightProperties( LightComponent& light ) const -> void {
        if ( mHasSnapshot && mHadLight ) {
            for ( core::usize index{}; index < mRestLightColors.size(); ++index ) {
                VisitLight( light, static_cast<renderer::LightType>( index ), [&]( auto& value ) {
                    value.SetColor( mRestLightColors[index] );
                    value.SetIntensity( mRestLightIntensities[index] );
                } );
            }
        }
    }

    auto KeyframeAnimationComponent::Stop( Entity& entity ) -> void {
        if ( mHasSnapshot ) {
            entity.GetComponent<TransformComponent>().SetTransform( mRestTransform );

            if ( entity.HasComponent<LightComponent>() ) {
                RestoreLightProperties( entity.GetComponent<LightComponent>() );
            }
        }

        mHasSnapshot = false;
        mPlaying = false;
        mPreviewing = false;
        mFrame = 0.0;
    }

    auto KeyframeAnimationComponent::Seek( Entity& entity, core::f64 frame ) -> void {
        mClip.Sanitize();
        CaptureRest( entity );
        mPreviewing = true;
        mFrame = glm::clamp( frame, 0.0, static_cast<core::f64>( mClip.mLastFrame ) );
        Apply( entity );
    }

    auto KeyframeAnimationComponent::Update( Entity& entity, core::f64 seconds ) -> void {
        if ( !mPlaying ) {
            return;
        }

        mFrame = mClip.Advance( mFrame, seconds );
        Apply( entity );

        if ( !mClip.mLoop && mFrame >= mClip.mLastFrame ) {
            mPlaying = false;
        }
    }

    auto KeyframeAnimationComponent::Apply( Entity& entity ) const -> void {
        MKT_BEGIN_PROFILER();
        auto& transform{ entity.GetComponent<TransformComponent>() };

        for ( core::usize index{}; index < mClip.mTracks.size(); ++index ) {
            const auto& track{ mClip.mTracks[index] };

            if ( track.GetKeys().empty() ) {
                continue;
            }

            const auto channel{ static_cast<animation::KeyframeChannel>( index ) };
            const core::float3 value{ track.Sample( mFrame, ReadProperty( entity, channel ) ) };

            switch ( channel ) {
                case animation::KeyframeChannel::ePosition:
                    transform.SetTranslation( value );
                    break;
                // Euler degrees are intentional: 0 -> 360 must make a full
                // turn, unlike shortest-path quaternion interpolation.
                case animation::KeyframeChannel::eRotation:
                    transform.SetRotation( value );
                    break;
                case animation::KeyframeChannel::eScale: {
                    const bool uniform{ transform.HasUniformScale() };
                    transform.SetUniformSale( false );
                    transform.SetScale( value );
                    transform.SetUniformSale( uniform );
                    break;
                }

                default: {
                    if ( entity.HasComponent<LightComponent>() ) {
                        auto& light{ entity.GetComponent<LightComponent>() };
                        VisitLight( light, light.GetActiveType(), [&]( auto& target ) {
                            if ( channel == animation::KeyframeChannel::eLightColor ) {
                                target.SetColor( glm::max( value, core::float3{ 0.0f } ) );
                            }
                            else {
                                target.SetIntensity( value.x );
                            }
                        } );
                    }

                    break;
                }
            }
        }
    }

    auto KeyframeAnimationComponent::GetFrame() const -> core::f64 {
        return mFrame;
    }

    auto KeyframeAnimationComponent::IsPlaying() const -> bool {
        return mPlaying;
    }

    auto KeyframeAnimationComponent::IsPreviewing() const -> bool {
        return mPreviewing;
    }

    auto KeyframeAnimationComponent::GetRestTransform() const -> const core::float4x4& {
        return mRestTransform;
    }

    AnimatorComponent::AnimatorComponent( core::u64 ID )
        : mAnimatorID{ ID } {
    }


    auto AnimatorComponent::SetAnimatorID( core::u64 ID ) -> void {
        MKT_BEGIN_PROFILER();
        mAnimatorID = ID;
    }


    auto AnimatorComponent::GetAnimatorID() const -> core::u64 {
        MKT_BEGIN_PROFILER();
        return mAnimatorID;
    }


    SkinnedMeshRenderer::SkinnedMeshRenderer( core::u64 ID )
        : mAnimatorID{ ID } {
    }


    auto SkinnedMeshRenderer::SetAnimatorID( core::u64 id ) -> void {
        mAnimatorID = id;
    }

    auto SkinnedMeshRenderer::GetAnimatorID() const -> core::u64 {
        return mAnimatorID;
    }


    PostProcessMaterialComponent::PostProcessMaterialComponent( material::MaterialHandle material ) {
        if ( material ) {
            SetMaterial( eastl::move( material ) );
        }
        else {
            SetMaterial( core::Ref<material::PostProcessMaterial>::New() );
        }
    }

    auto PostProcessMaterialComponent::SetMaterial( material::MaterialHandle material ) -> void {
        if ( !dynamic_cast<material::PostProcessMaterial*>( material.GetPtr() ) ) {
            MKT_THROW_INVALID_ARGUMENT( "Post-process components require a post-process material." );
        }

        mMaterial = eastl::move( material );
    }

    auto PostProcessMaterialComponent::GetMaterial() const -> material::MaterialHandle {
        MKT_BEGIN_PROFILER();
        return mMaterial;
    }

    auto PostProcessMaterialComponent::SetCameraEntityID( core::u64 id ) -> void {
        mCameraEntityID = id;
    }

    auto PostProcessMaterialComponent::GetCameraEntityID() const -> core::u64 {
        return mCameraEntityID;
    }


    auto SkyboxMaterialComponent::GetMaterial() const -> material::MaterialHandle {
        MKT_BEGIN_PROFILER();
        return mMaterial;
    }


    auto SkyboxMaterialComponent::SetMaterial( material::MaterialHandle material ) -> void {
        MKT_BEGIN_PROFILER();
        mMaterial = material;
    }


    auto AudioSourceComponent::operator=( const AudioSourceComponent& other ) -> AudioSourceComponent& {
        MKT_BEGIN_PROFILER();

        if ( this != &other ) {
            auto copy = other.mAudioSource ? other.mAudioSource->Clone() : audio::AudioSourceHandle{};

            if ( mAudioSource ) {
                mAudioSource->Stop();
            }

            mClip = other.mClip;
            mSettings = other.GetSettings();
            mOutputID = other.mOutputID;
            mAudioSource = eastl::move( copy );
        }

        return *this;
    }

    auto AudioSourceComponent::operator=( AudioSourceComponent&& other ) noexcept -> AudioSourceComponent& {
        MKT_BEGIN_PROFILER();

        if ( this != &other ) {
            if ( mAudioSource ) {
                mAudioSource->Stop();
            }

            mClip = eastl::move( other.mClip );
            mSettings = other.GetSettings();
            mOutputID = other.mOutputID;
            mAudioSource = eastl::move( other.mAudioSource );
        }

        return *this;
    }

    auto LandscapeComponent::GetBrushType() const -> LandscapeBrushType {
        return mBrushType;
    }

    auto LandscapeComponent::GetWorldDescription() const -> renderer::WorldDescription {
        return mWorldDescription;
    }

    auto LandscapeComponent::ApplyDescription( const renderer::WorldDescription& description ) -> void {
        if ( !renderer::IsTerrainDescriptionValid( description ) ) {
            return;
        }

        const auto& current{ mWorldDescription };

        if ( current.mOrigin == description.mOrigin &&
             current.mQuadsPerSection == description.mQuadsPerSection &&
             current.mSectionsPerComponent == description.mSectionsPerComponent &&
             current.mComponentsX == description.mComponentsX && current.mComponentsY == description.mComponentsY &&
             current.mVertexSpacing == description.mVertexSpacing && current.mHeightScale == description.mHeightScale &&
             current.mBaseElevation == description.mBaseElevation && current.mEnableEditLayers == description.mEnableEditLayers &&
             current.mLowColor == description.mLowColor && current.mMiddleColor == description.mMiddleColor && current.mHighColor == description.mHighColor ) {
            return;
        }

        mWorldDescription = description;
        BumpRevision();
    }

    auto LandscapeComponent::SetTerrainSection( core::u32 quads ) -> void {
        auto description{ mWorldDescription };
        description.mQuadsPerSection = quads;
        ApplyDescription( description );
    }

    auto LandscapeComponent::SetSectionsPerComponent( core::u32 sections ) -> void {
        auto description{ mWorldDescription };
        description.mSectionsPerComponent = sections;
        ApplyDescription( description );
    }

    auto LandscapeComponent::SetOrigin( core::float2 origin ) -> void {
        auto description{ mWorldDescription };
        description.mOrigin = origin;
        ApplyDescription( description );
    }

    auto LandscapeComponent::SetVertexSpacing( core::f32 spacing ) -> void {
        auto description{ mWorldDescription };
        description.mVertexSpacing = spacing;
        ApplyDescription( description );
    }

    auto LandscapeComponent::SetHeightScale( core::f32 scale ) -> void {
        auto description{ mWorldDescription };
        description.mHeightScale = scale;
        ApplyDescription( description );
    }

    auto LandscapeComponent::SetBaseElevation( core::f32 elevation ) -> void {
        auto description{ mWorldDescription };
        description.mBaseElevation = elevation;
        ApplyDescription( description );
    }

    auto LandscapeComponent::SetComponentCount( core::u32 x, core::u32 y ) -> void {
        auto description{ mWorldDescription };
        description.mComponentsX = x;
        description.mComponentsY = y;
        ApplyDescription( description );
    }

    auto LandscapeComponent::SetLowColor( core::float3 color ) -> void {
        auto description{ mWorldDescription };
        description.mLowColor = color;
        ApplyDescription( description );
    }

    auto LandscapeComponent::SetMiddleColor( core::float3 color ) -> void {
        auto description{ mWorldDescription };
        description.mMiddleColor = color;
        ApplyDescription( description );
    }

    auto LandscapeComponent::SetHighColor( core::float3 color ) -> void {
        auto description{ mWorldDescription };
        description.mHighColor = color;
        ApplyDescription( description );
    }

    auto LandscapeComponent::SetEnableEditLayers( bool enabled ) -> void {
        auto description{ mWorldDescription };
        description.mEnableEditLayers = enabled;
        ApplyDescription( description );
    }

    auto LandscapeComponent::SetHeightmap( const asset::AssetRef<asset::Texture>& reference ) -> void {
        mNoiseGeneration.reset();
        mHeightMapRef = reference;
        mHeightMap.reset();
        mHeightMapError.clear();
        BumpRevision();

        if ( reference.GetPath().IsEmpty() ) {
            return;
        }

        try {
            // Use the shared image pipeline, preserving HDR and 16-bit height data.
            auto image{ asset::ProcessImage2D( reference.GetPath(), true ) };
            mHeightMap = eastl::make_shared<renderer::TerrainHeightmap>( eastl::move( image ) );
        } catch ( const std::exception& error ) {
            mHeightMapError = error.what();
        }
    }

    auto LandscapeComponent::ClearHeightmap() -> void {
        if ( !mHeightMapRef.GetPath().IsEmpty() || mHeightMap || mNoiseGeneration ) {
            SetHeightmap( {} );
        }
    }

    auto LandscapeComponent::GetHeightmapReference() const -> asset::AssetRef<asset::Texture> {
        return mHeightMapRef;
    }

    auto LandscapeComponent::GetHeightmap() const -> eastl::shared_ptr<const renderer::TerrainHeightmap> {
        return mNoiseGeneration && mNoiseGeneration->GetHeightmap() ? mNoiseGeneration->GetHeightmap() : mHeightMap;
    }

    auto LandscapeComponent::GetHeightmapError() const -> const eastl::string& {
        return mNoiseGeneration ? mNoiseGeneration->GetError() : mHeightMapError;
    }

    auto LandscapeComponent::SetGeneratedNoise( const renderer::TerrainNoiseSettings& settings ) -> void {
        if ( !settings.IsValid() ) {
            return;
        }

        if ( mNoiseGeneration && mNoiseSettings == settings && mNoiseGeneration->GetError().empty() ) {
            return;
        }
        // Keep the last complete image visible while a newer request is in flight.
        mHeightMap = GetHeightmap();
        mNoiseSettings = settings;
        mNoiseGeneration = eastl::make_shared<renderer::TerrainNoiseGeneration>( settings );
        mHeightMapRef = {};
        mHeightMapError.clear();
        BumpRevision();
    }

    auto LandscapeComponent::GetNoiseSettings() const -> renderer::TerrainNoiseSettings {
        return mNoiseSettings;
    }

    auto LandscapeComponent::GetNoiseGeneration() const -> eastl::shared_ptr<renderer::TerrainNoiseGeneration> {
        return mNoiseGeneration;
    }

    auto LandscapeComponent::SetShowGrid( bool show ) -> void {
        if ( mShowGrid != show ) {
            mShowGrid = show;
            BumpRevision();
        }
    }

    auto LandscapeComponent::SetShowVertices( bool show ) -> void {
        if ( mShowVertices != show ) {
            mShowVertices = show;
            BumpRevision();
        }
    }

    auto LandscapeComponent::IsGridVisible() const -> bool {
        return mShowGrid;
    }

    auto LandscapeComponent::AreVerticesVisible() const -> bool {
        return mShowVertices;
    }

    // Authoring-only physics component accessors. No native bodies or constraints are owned here.
    auto HeightFieldColliderComponent::SetLandscapeEntityID( core::u64 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mLandscapeEntityID != value ) {
            mLandscapeEntityID = value;
            BumpRevision();
        }
    }

    auto HeightFieldColliderComponent::GetLandscapeEntityID() const -> core::u64 {
        MKT_BEGIN_PROFILER();
        return mLandscapeEntityID;
    }

    auto HeightFieldColliderComponent::SetHeightmap( const asset::AssetRef<asset::Texture>& value ) -> void {
        MKT_BEGIN_PROFILER();
        mHeightmap = value;
        BumpRevision();
    }

    auto HeightFieldColliderComponent::GetHeightmap() const -> const asset::AssetRef<asset::Texture>& {
        MKT_BEGIN_PROFILER();
        return mHeightmap;
    }

    auto HeightFieldColliderComponent::SetSampleSpacing( const core::float2& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mSampleSpacing != value ) {
            mSampleSpacing = value;
            BumpRevision();
        }
    }

    auto HeightFieldColliderComponent::GetSampleSpacing() const -> const core::float2& {
        MKT_BEGIN_PROFILER();
        return mSampleSpacing;
    }

    auto HeightFieldColliderComponent::SetHeightScale( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mHeightScale != value ) {
            mHeightScale = value;
            BumpRevision();
        }
    }

    auto HeightFieldColliderComponent::GetHeightScale() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mHeightScale;
    }

    auto HeightFieldColliderComponent::SetBaseElevation( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mBaseElevation != value ) {
            mBaseElevation = value;
            BumpRevision();
        }
    }

    auto HeightFieldColliderComponent::GetBaseElevation() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mBaseElevation;
    }

    auto HeightFieldColliderComponent::SetOffset( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mOffset != value ) {
            mOffset = value;
            BumpRevision();
        }
    }

    auto HeightFieldColliderComponent::GetOffset() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mOffset;
    }

    auto HeightFieldColliderComponent::SetTrigger( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mTrigger != value ) {
            mTrigger = value;
            BumpRevision();
        }
    }

    auto HeightFieldColliderComponent::IsTrigger() const -> bool {
        MKT_BEGIN_PROFILER();
        return mTrigger;
    }

    auto HeightFieldColliderComponent::GetColliderType() -> physics::ColliderType {
        return physics::ColliderType::eHeightField;
    }

    auto CompoundColliderComponent::SetColliderEntities( const eastl::vector<core::u64>& value ) -> void {
        MKT_BEGIN_PROFILER();
        mColliderEntities = value;
        BumpRevision();
    }

    auto CompoundColliderComponent::GetColliderEntities() const -> const eastl::vector<core::u64>& {
        MKT_BEGIN_PROFILER();
        return mColliderEntities;
    }

    auto CompoundColliderComponent::SetMode( CompoundColliderMode value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMode != value ) {
            mMode = value;
            BumpRevision();
        }
    }

    auto CompoundColliderComponent::GetMode() const -> CompoundColliderMode {
        MKT_BEGIN_PROFILER();
        return mMode;
    }

    auto CompoundColliderComponent::SetOffset( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mOffset != value ) {
            mOffset = value;
            BumpRevision();
        }
    }

    auto CompoundColliderComponent::GetOffset() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mOffset;
    }

    auto CompoundColliderComponent::SetTrigger( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mTrigger != value ) {
            mTrigger = value;
            BumpRevision();
        }
    }

    auto CompoundColliderComponent::IsTrigger() const -> bool {
        MKT_BEGIN_PROFILER();
        return mTrigger;
    }

    auto CompoundColliderComponent::GetColliderType() -> physics::ColliderType {
        return physics::ColliderType::eCompound;
    }

    auto CylinderColliderComponent::SetRadius( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mRadius != value ) {
            mRadius = value;
            BumpRevision();
        }
    }

    auto CylinderColliderComponent::GetRadius() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mRadius;
    }

    auto CylinderColliderComponent::SetHalfHeight( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mHalfHeight != value ) {
            mHalfHeight = value;
            BumpRevision();
        }
    }

    auto CylinderColliderComponent::GetHalfHeight() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mHalfHeight;
    }

    auto CylinderColliderComponent::SetOffset( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mOffset != value ) {
            mOffset = value;
            BumpRevision();
        }
    }

    auto CylinderColliderComponent::GetOffset() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mOffset;
    }

    auto CylinderColliderComponent::SetTrigger( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mTrigger != value ) {
            mTrigger = value;
            BumpRevision();
        }
    }

    auto CylinderColliderComponent::IsTrigger() const -> bool {
        MKT_BEGIN_PROFILER();
        return mTrigger;
    }

    auto CylinderColliderComponent::GetColliderType() -> physics::ColliderType {
        return physics::ColliderType::eCylinder;
    }

    auto TaperedCapsuleColliderComponent::SetTopRadius( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mTopRadius != value ) {
            mTopRadius = value;
            BumpRevision();
        }
    }

    auto TaperedCapsuleColliderComponent::GetTopRadius() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mTopRadius;
    }

    auto TaperedCapsuleColliderComponent::SetBottomRadius( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mBottomRadius != value ) {
            mBottomRadius = value;
            BumpRevision();
        }
    }

    auto TaperedCapsuleColliderComponent::GetBottomRadius() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mBottomRadius;
    }

    auto TaperedCapsuleColliderComponent::SetHalfHeight( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mHalfHeight != value ) {
            mHalfHeight = value;
            BumpRevision();
        }
    }

    auto TaperedCapsuleColliderComponent::GetHalfHeight() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mHalfHeight;
    }

    auto TaperedCapsuleColliderComponent::SetOffset( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mOffset != value ) {
            mOffset = value;
            BumpRevision();
        }
    }

    auto TaperedCapsuleColliderComponent::GetOffset() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mOffset;
    }

    auto TaperedCapsuleColliderComponent::SetTrigger( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mTrigger != value ) {
            mTrigger = value;
            BumpRevision();
        }
    }

    auto TaperedCapsuleColliderComponent::IsTrigger() const -> bool {
        MKT_BEGIN_PROFILER();
        return mTrigger;
    }

    auto TaperedCapsuleColliderComponent::GetColliderType() -> physics::ColliderType {
        return physics::ColliderType::eTaperedCapsule;
    }

    auto TaperedCylinderColliderComponent::SetTopRadius( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mTopRadius != value ) {
            mTopRadius = value;
            BumpRevision();
        }
    }

    auto TaperedCylinderColliderComponent::GetTopRadius() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mTopRadius;
    }

    auto TaperedCylinderColliderComponent::SetBottomRadius( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mBottomRadius != value ) {
            mBottomRadius = value;
            BumpRevision();
        }
    }

    auto TaperedCylinderColliderComponent::GetBottomRadius() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mBottomRadius;
    }

    auto TaperedCylinderColliderComponent::SetHalfHeight( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mHalfHeight != value ) {
            mHalfHeight = value;
            BumpRevision();
        }
    }

    auto TaperedCylinderColliderComponent::GetHalfHeight() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mHalfHeight;
    }

    auto TaperedCylinderColliderComponent::SetOffset( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mOffset != value ) {
            mOffset = value;
            BumpRevision();
        }
    }

    auto TaperedCylinderColliderComponent::GetOffset() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mOffset;
    }

    auto TaperedCylinderColliderComponent::SetTrigger( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mTrigger != value ) {
            mTrigger = value;
            BumpRevision();
        }
    }

    auto TaperedCylinderColliderComponent::IsTrigger() const -> bool {
        MKT_BEGIN_PROFILER();
        return mTrigger;
    }

    auto TaperedCylinderColliderComponent::GetColliderType() -> physics::ColliderType {
        return physics::ColliderType::eTaperedCylinder;
    }

    auto PlaneColliderComponent::SetNormal( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mNormal != value ) {
            mNormal = value;
            BumpRevision();
        }
    }

    auto PlaneColliderComponent::GetNormal() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mNormal;
    }

    auto PlaneColliderComponent::SetDistance( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mDistance != value ) {
            mDistance = value;
            BumpRevision();
        }
    }

    auto PlaneColliderComponent::GetDistance() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mDistance;
    }

    auto PlaneColliderComponent::SetOffset( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mOffset != value ) {
            mOffset = value;
            BumpRevision();
        }
    }

    auto PlaneColliderComponent::GetOffset() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mOffset;
    }

    auto PlaneColliderComponent::SetTrigger( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mTrigger != value ) {
            mTrigger = value;
            BumpRevision();
        }
    }

    auto PlaneColliderComponent::IsTrigger() const -> bool {
        MKT_BEGIN_PROFILER();
        return mTrigger;
    }

    auto PlaneColliderComponent::GetColliderType() -> physics::ColliderType {
        return physics::ColliderType::ePlane;
    }

    auto JointComponent::SetBodyAEntityID( core::u64 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mBodyAEntityID != value ) {
            mBodyAEntityID = value;
            BumpRevision();
        }
    }

    auto JointComponent::GetBodyAEntityID() const -> core::u64 {
        MKT_BEGIN_PROFILER();
        return mBodyAEntityID;
    }

    auto JointComponent::SetBodyBEntityID( core::u64 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mBodyBEntityID != value ) {
            mBodyBEntityID = value;
            BumpRevision();
        }
    }

    auto JointComponent::GetBodyBEntityID() const -> core::u64 {
        MKT_BEGIN_PROFILER();
        return mBodyBEntityID;
    }

    auto JointComponent::SetAnchorA( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mAnchorA != value ) {
            mAnchorA = value;
            BumpRevision();
        }
    }

    auto JointComponent::GetAnchorA() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mAnchorA;
    }

    auto JointComponent::SetAnchorB( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mAnchorB != value ) {
            mAnchorB = value;
            BumpRevision();
        }
    }

    auto JointComponent::GetAnchorB() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mAnchorB;
    }

    auto JointComponent::SetRotationA( const core::quat& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mRotationA != value ) {
            mRotationA = value;
            BumpRevision();
        }
    }

    auto JointComponent::GetRotationA() const -> const core::quat& {
        MKT_BEGIN_PROFILER();
        return mRotationA;
    }

    auto JointComponent::SetRotationB( const core::quat& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mRotationB != value ) {
            mRotationB = value;
            BumpRevision();
        }
    }

    auto JointComponent::GetRotationB() const -> const core::quat& {
        MKT_BEGIN_PROFILER();
        return mRotationB;
    }

    auto JointComponent::SetCollideConnected( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mCollideConnected != value ) {
            mCollideConnected = value;
            BumpRevision();
        }
    }

    auto JointComponent::CanConnectedBodiesCollide() const -> bool {
        MKT_BEGIN_PROFILER();
        return mCollideConnected;
    }

    auto JointComponent::SetEnabled( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mEnabled != value ) {
            mEnabled = value;
            BumpRevision();
        }
    }

    auto JointComponent::IsEnabled() const -> bool {
        MKT_BEGIN_PROFILER();
        return mEnabled;
    }

    auto DistanceJointComponent::SetMinDistance( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMinDistance != value ) {
            mMinDistance = value;
            BumpRevision();
        }
    }

    auto DistanceJointComponent::GetMinDistance() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mMinDistance;
    }

    auto DistanceJointComponent::SetMaxDistance( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMaxDistance != value ) {
            mMaxDistance = value;
            BumpRevision();
        }
    }

    auto DistanceJointComponent::GetMaxDistance() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mMaxDistance;
    }

    auto DistanceJointComponent::SetSpring( const JointSpringSettings& value ) -> void {
        MKT_BEGIN_PROFILER();
        mSpring = value;
        BumpRevision();
    }

    auto DistanceJointComponent::GetSpring() const -> const JointSpringSettings& {
        MKT_BEGIN_PROFILER();
        return mSpring;
    }

    auto HingeJointComponent::SetMinAngle( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMinAngle != value ) {
            mMinAngle = value;
            BumpRevision();
        }
    }

    auto HingeJointComponent::GetMinAngle() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mMinAngle;
    }

    auto HingeJointComponent::SetMaxAngle( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMaxAngle != value ) {
            mMaxAngle = value;
            BumpRevision();
        }
    }

    auto HingeJointComponent::GetMaxAngle() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mMaxAngle;
    }

    auto HingeJointComponent::SetLimitSpring( const JointSpringSettings& value ) -> void {
        MKT_BEGIN_PROFILER();
        mLimitSpring = value;
        BumpRevision();
    }

    auto HingeJointComponent::GetLimitSpring() const -> const JointSpringSettings& {
        MKT_BEGIN_PROFILER();
        return mLimitSpring;
    }

    auto HingeJointComponent::SetMotor( const JointMotorSettings& value ) -> void {
        MKT_BEGIN_PROFILER();
        mMotor = value;
        BumpRevision();
    }

    auto HingeJointComponent::GetMotor() const -> const JointMotorSettings& {
        MKT_BEGIN_PROFILER();
        return mMotor;
    }

    auto SliderJointComponent::SetMinDistance( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMinDistance != value ) {
            mMinDistance = value;
            BumpRevision();
        }
    }

    auto SliderJointComponent::GetMinDistance() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mMinDistance;
    }

    auto SliderJointComponent::SetMaxDistance( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMaxDistance != value ) {
            mMaxDistance = value;
            BumpRevision();
        }
    }

    auto SliderJointComponent::GetMaxDistance() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mMaxDistance;
    }

    auto SliderJointComponent::SetLimitSpring( const JointSpringSettings& value ) -> void {
        MKT_BEGIN_PROFILER();
        mLimitSpring = value;
        BumpRevision();
    }

    auto SliderJointComponent::GetLimitSpring() const -> const JointSpringSettings& {
        MKT_BEGIN_PROFILER();
        return mLimitSpring;
    }

    auto SliderJointComponent::SetMotor( const JointMotorSettings& value ) -> void {
        MKT_BEGIN_PROFILER();
        mMotor = value;
        BumpRevision();
    }

    auto SliderJointComponent::GetMotor() const -> const JointMotorSettings& {
        MKT_BEGIN_PROFILER();
        return mMotor;
    }

    auto ConeJointComponent::SetHalfConeAngle( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mHalfConeAngle != value ) {
            mHalfConeAngle = value;
            BumpRevision();
        }
    }

    auto ConeJointComponent::GetHalfConeAngle() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mHalfConeAngle;
    }

    auto SwingTwistJointComponent::SetNormalHalfConeAngle( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mNormalHalfConeAngle != value ) {
            mNormalHalfConeAngle = value;
            BumpRevision();
        }
    }

    auto SwingTwistJointComponent::GetNormalHalfConeAngle() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mNormalHalfConeAngle;
    }

    auto SwingTwistJointComponent::SetPlaneHalfConeAngle( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mPlaneHalfConeAngle != value ) {
            mPlaneHalfConeAngle = value;
            BumpRevision();
        }
    }

    auto SwingTwistJointComponent::GetPlaneHalfConeAngle() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mPlaneHalfConeAngle;
    }

    auto SwingTwistJointComponent::SetMinTwistAngle( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMinTwistAngle != value ) {
            mMinTwistAngle = value;
            BumpRevision();
        }
    }

    auto SwingTwistJointComponent::GetMinTwistAngle() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mMinTwistAngle;
    }

    auto SwingTwistJointComponent::SetMaxTwistAngle( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMaxTwistAngle != value ) {
            mMaxTwistAngle = value;
            BumpRevision();
        }
    }

    auto SwingTwistJointComponent::GetMaxTwistAngle() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mMaxTwistAngle;
    }

    auto SwingTwistJointComponent::SetMotors( const eastl::array<JointMotorSettings, 3>& value ) -> void {
        MKT_BEGIN_PROFILER();
        mMotors = value;
        BumpRevision();
    }

    auto SwingTwistJointComponent::GetMotors() const -> const eastl::array<JointMotorSettings, 3>& {
        MKT_BEGIN_PROFILER();
        return mMotors;
    }

    auto SixDofJointComponent::SetTranslationMotion( const eastl::array<JointAxisMotion, 3>& value ) -> void {
        MKT_BEGIN_PROFILER();
        mTranslationMotion = value;
        BumpRevision();
    }

    auto SixDofJointComponent::GetTranslationMotion() const -> const eastl::array<JointAxisMotion, 3>& {
        MKT_BEGIN_PROFILER();
        return mTranslationMotion;
    }

    auto SixDofJointComponent::SetRotationMotion( const eastl::array<JointAxisMotion, 3>& value ) -> void {
        MKT_BEGIN_PROFILER();
        mRotationMotion = value;
        BumpRevision();
    }

    auto SixDofJointComponent::GetRotationMotion() const -> const eastl::array<JointAxisMotion, 3>& {
        MKT_BEGIN_PROFILER();
        return mRotationMotion;
    }

    auto SixDofJointComponent::SetMinTranslation( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMinTranslation != value ) {
            mMinTranslation = value;
            BumpRevision();
        }
    }

    auto SixDofJointComponent::GetMinTranslation() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mMinTranslation;
    }

    auto SixDofJointComponent::SetMaxTranslation( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMaxTranslation != value ) {
            mMaxTranslation = value;
            BumpRevision();
        }
    }

    auto SixDofJointComponent::GetMaxTranslation() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mMaxTranslation;
    }

    auto SixDofJointComponent::SetMinRotation( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMinRotation != value ) {
            mMinRotation = value;
            BumpRevision();
        }
    }

    auto SixDofJointComponent::GetMinRotation() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mMinRotation;
    }

    auto SixDofJointComponent::SetMaxRotation( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMaxRotation != value ) {
            mMaxRotation = value;
            BumpRevision();
        }
    }

    auto SixDofJointComponent::GetMaxRotation() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mMaxRotation;
    }

    auto SixDofJointComponent::SetMotors( const eastl::array<JointMotorSettings, 6>& value ) -> void {
        MKT_BEGIN_PROFILER();
        mMotors = value;
        BumpRevision();
    }

    auto SixDofJointComponent::GetMotors() const -> const eastl::array<JointMotorSettings, 6>& {
        MKT_BEGIN_PROFILER();
        return mMotors;
    }

    auto PathJointComponent::SetControlPoints( const eastl::vector<JointPathPoint>& value ) -> void {
        MKT_BEGIN_PROFILER();
        mControlPoints = value;
        BumpRevision();
    }

    auto PathJointComponent::GetControlPoints() const -> const eastl::vector<JointPathPoint>& {
        MKT_BEGIN_PROFILER();
        return mControlPoints;
    }

    auto PathJointComponent::SetLooping( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mLooping != value ) {
            mLooping = value;
            BumpRevision();
        }
    }

    auto PathJointComponent::IsLooping() const -> bool {
        MKT_BEGIN_PROFILER();
        return mLooping;
    }

    auto PathJointComponent::SetInitialFraction( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mInitialFraction != value ) {
            mInitialFraction = value;
            BumpRevision();
        }
    }

    auto PathJointComponent::GetInitialFraction() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mInitialFraction;
    }

    auto PathJointComponent::SetMaxFrictionForce( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMaxFrictionForce != value ) {
            mMaxFrictionForce = value;
            BumpRevision();
        }
    }

    auto PathJointComponent::GetMaxFrictionForce() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mMaxFrictionForce;
    }

    auto PathJointComponent::SetMotor( const JointMotorSettings& value ) -> void {
        MKT_BEGIN_PROFILER();
        mMotor = value;
        BumpRevision();
    }

    auto PathJointComponent::GetMotor() const -> const JointMotorSettings& {
        MKT_BEGIN_PROFILER();
        return mMotor;
    }

    auto GearJointComponent::SetHingeAEntityID( core::u64 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mHingeAEntityID != value ) {
            mHingeAEntityID = value;
            BumpRevision();
        }
    }

    auto GearJointComponent::GetHingeAEntityID() const -> core::u64 {
        MKT_BEGIN_PROFILER();
        return mHingeAEntityID;
    }

    auto GearJointComponent::SetHingeBEntityID( core::u64 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mHingeBEntityID != value ) {
            mHingeBEntityID = value;
            BumpRevision();
        }
    }

    auto GearJointComponent::GetHingeBEntityID() const -> core::u64 {
        MKT_BEGIN_PROFILER();
        return mHingeBEntityID;
    }

    auto GearJointComponent::SetRatio( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mRatio != value ) {
            mRatio = value;
            BumpRevision();
        }
    }

    auto GearJointComponent::GetRatio() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mRatio;
    }

    auto RackAndPinionJointComponent::SetHingeEntityID( core::u64 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mHingeEntityID != value ) {
            mHingeEntityID = value;
            BumpRevision();
        }
    }

    auto RackAndPinionJointComponent::GetHingeEntityID() const -> core::u64 {
        MKT_BEGIN_PROFILER();
        return mHingeEntityID;
    }

    auto RackAndPinionJointComponent::SetSliderEntityID( core::u64 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mSliderEntityID != value ) {
            mSliderEntityID = value;
            BumpRevision();
        }
    }

    auto RackAndPinionJointComponent::GetSliderEntityID() const -> core::u64 {
        MKT_BEGIN_PROFILER();
        return mSliderEntityID;
    }

    auto RackAndPinionJointComponent::SetRatio( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mRatio != value ) {
            mRatio = value;
            BumpRevision();
        }
    }

    auto RackAndPinionJointComponent::GetRatio() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mRatio;
    }

    auto PulleyJointComponent::SetFixedPointA( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mFixedPointA != value ) {
            mFixedPointA = value;
            BumpRevision();
        }
    }

    auto PulleyJointComponent::GetFixedPointA() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mFixedPointA;
    }

    auto PulleyJointComponent::SetFixedPointB( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mFixedPointB != value ) {
            mFixedPointB = value;
            BumpRevision();
        }
    }

    auto PulleyJointComponent::GetFixedPointB() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mFixedPointB;
    }

    auto PulleyJointComponent::SetRatio( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mRatio != value ) {
            mRatio = value;
            BumpRevision();
        }
    }

    auto PulleyJointComponent::GetRatio() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mRatio;
    }

    auto PulleyJointComponent::SetMinLength( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMinLength != value ) {
            mMinLength = value;
            BumpRevision();
        }
    }

    auto PulleyJointComponent::GetMinLength() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mMinLength;
    }

    auto PulleyJointComponent::SetMaxLength( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMaxLength != value ) {
            mMaxLength = value;
            BumpRevision();
        }
    }

    auto PulleyJointComponent::GetMaxLength() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mMaxLength;
    }

    auto CharacterControllerComponent::SetControllerType( CharacterControllerType value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mControllerType != value ) {
            mControllerType = value;
            BumpRevision();
        }
    }

    auto CharacterControllerComponent::GetControllerType() const -> CharacterControllerType {
        MKT_BEGIN_PROFILER();
        return mControllerType;
    }

    auto CharacterControllerComponent::SetRadius( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mRadius != value ) {
            mRadius = value;
            BumpRevision();
        }
    }

    auto CharacterControllerComponent::GetRadius() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mRadius;
    }

    auto CharacterControllerComponent::SetHalfHeight( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mHalfHeight != value ) {
            mHalfHeight = value;
            BumpRevision();
        }
    }

    auto CharacterControllerComponent::GetHalfHeight() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mHalfHeight;
    }

    auto CharacterControllerComponent::SetMaxSlopeAngle( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMaxSlopeAngle != value ) {
            mMaxSlopeAngle = value;
            BumpRevision();
        }
    }

    auto CharacterControllerComponent::GetMaxSlopeAngle() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mMaxSlopeAngle;
    }

    auto CharacterControllerComponent::SetMaxStepHeight( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMaxStepHeight != value ) {
            mMaxStepHeight = value;
            BumpRevision();
        }
    }

    auto CharacterControllerComponent::GetMaxStepHeight() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mMaxStepHeight;
    }

    auto CharacterControllerComponent::SetMaxStrength( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMaxStrength != value ) {
            mMaxStrength = value;
            BumpRevision();
        }
    }

    auto CharacterControllerComponent::GetMaxStrength() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mMaxStrength;
    }

    auto CharacterControllerComponent::SetGravityFactor( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mGravityFactor != value ) {
            mGravityFactor = value;
            BumpRevision();
        }
    }

    auto CharacterControllerComponent::GetGravityFactor() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mGravityFactor;
    }

    auto CharacterControllerComponent::SetStickToFloor( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mStickToFloor != value ) {
            mStickToFloor = value;
            BumpRevision();
        }
    }

    auto CharacterControllerComponent::DoesStickToFloor() const -> bool {
        MKT_BEGIN_PROFILER();
        return mStickToFloor;
    }

    auto CharacterControllerComponent::SetEnabled( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mEnabled != value ) {
            mEnabled = value;
            BumpRevision();
        }
    }

    auto CharacterControllerComponent::IsEnabled() const -> bool {
        MKT_BEGIN_PROFILER();
        return mEnabled;
    }

    auto RagdollComponent::SetSkeleton( const asset::AssetRef<asset::Skeleton>& value ) -> void {
        MKT_BEGIN_PROFILER();
        mSkeleton = value;
        BumpRevision();
    }

    auto RagdollComponent::GetSkeleton() const -> const asset::AssetRef<asset::Skeleton>& {
        MKT_BEGIN_PROFILER();
        return mSkeleton;
    }

    auto RagdollComponent::SetBoneMappings( const eastl::vector<RagdollBoneDescription>& value ) -> void {
        MKT_BEGIN_PROFILER();
        mBoneMappings = value;
        BumpRevision();
    }

    auto RagdollComponent::GetBoneMappings() const -> const eastl::vector<RagdollBoneDescription>& {
        MKT_BEGIN_PROFILER();
        return mBoneMappings;
    }

    auto RagdollComponent::SetSimulationBlend( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mSimulationBlend != value ) {
            mSimulationBlend = value;
            BumpRevision();
        }
    }

    auto RagdollComponent::GetSimulationBlend() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mSimulationBlend;
    }

    auto RagdollComponent::SetEnabled( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mEnabled != value ) {
            mEnabled = value;
            BumpRevision();
        }
    }

    auto RagdollComponent::IsEnabled() const -> bool {
        MKT_BEGIN_PROFILER();
        return mEnabled;
    }

    auto SoftBodyComponent::SetModel( const asset::AssetRef<asset::Model>& value ) -> void {
        MKT_BEGIN_PROFILER();
        mModel = value;
        BumpRevision();
    }

    auto SoftBodyComponent::GetModel() const -> const asset::AssetRef<asset::Model>& {
        MKT_BEGIN_PROFILER();
        return mModel;
    }

    auto SoftBodyComponent::SetMeshIndex( core::u32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mMeshIndex != value ) {
            mMeshIndex = value;
            BumpRevision();
        }
    }

    auto SoftBodyComponent::GetMeshIndex() const -> core::u32 {
        MKT_BEGIN_PROFILER();
        return mMeshIndex;
    }

    auto SoftBodyComponent::SetVertexMass( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mVertexMass != value ) {
            mVertexMass = value;
            BumpRevision();
        }
    }

    auto SoftBodyComponent::GetVertexMass() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mVertexMass;
    }

    auto SoftBodyComponent::SetGravityFactor( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mGravityFactor != value ) {
            mGravityFactor = value;
            BumpRevision();
        }
    }

    auto SoftBodyComponent::GetGravityFactor() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mGravityFactor;
    }

    auto SoftBodyComponent::SetLinearDamping( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mLinearDamping != value ) {
            mLinearDamping = value;
            BumpRevision();
        }
    }

    auto SoftBodyComponent::GetLinearDamping() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mLinearDamping;
    }

    auto SoftBodyComponent::SetPressure( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mPressure != value ) {
            mPressure = value;
            BumpRevision();
        }
    }

    auto SoftBodyComponent::GetPressure() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mPressure;
    }

    auto SoftBodyComponent::SetEdgeCompliance( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mEdgeCompliance != value ) {
            mEdgeCompliance = value;
            BumpRevision();
        }
    }

    auto SoftBodyComponent::GetEdgeCompliance() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mEdgeCompliance;
    }

    auto SoftBodyComponent::SetPinnedVertices( const eastl::vector<core::u32>& value ) -> void {
        MKT_BEGIN_PROFILER();
        mPinnedVertices = value;
        BumpRevision();
    }

    auto SoftBodyComponent::GetPinnedVertices() const -> const eastl::vector<core::u32>& {
        MKT_BEGIN_PROFILER();
        return mPinnedVertices;
    }

    auto SoftBodyComponent::SetEnabled( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mEnabled != value ) {
            mEnabled = value;
            BumpRevision();
        }
    }

    auto SoftBodyComponent::IsEnabled() const -> bool {
        MKT_BEGIN_PROFILER();
        return mEnabled;
    }

    auto ClothComponent::SetBendCompliance( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mBendCompliance != value ) {
            mBendCompliance = value;
            BumpRevision();
        }
    }

    auto ClothComponent::GetBendCompliance() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mBendCompliance;
    }

    auto ClothComponent::SetDrag( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mDrag != value ) {
            mDrag = value;
            BumpRevision();
        }
    }

    auto ClothComponent::GetDrag() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mDrag;
    }

    auto ClothComponent::SetLift( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mLift != value ) {
            mLift = value;
            BumpRevision();
        }
    }

    auto ClothComponent::GetLift() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mLift;
    }

    auto BuoyancyComponent::SetWaterVolumeEntityID( core::u64 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mWaterVolumeEntityID != value ) {
            mWaterVolumeEntityID = value;
            BumpRevision();
        }
    }

    auto BuoyancyComponent::GetWaterVolumeEntityID() const -> core::u64 {
        MKT_BEGIN_PROFILER();
        return mWaterVolumeEntityID;
    }

    auto BuoyancyComponent::SetBuoyancyFactor( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mBuoyancyFactor != value ) {
            mBuoyancyFactor = value;
            BumpRevision();
        }
    }

    auto BuoyancyComponent::GetBuoyancyFactor() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mBuoyancyFactor;
    }

    auto BuoyancyComponent::SetLinearDrag( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mLinearDrag != value ) {
            mLinearDrag = value;
            BumpRevision();
        }
    }

    auto BuoyancyComponent::GetLinearDrag() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mLinearDrag;
    }

    auto BuoyancyComponent::SetAngularDrag( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mAngularDrag != value ) {
            mAngularDrag = value;
            BumpRevision();
        }
    }

    auto BuoyancyComponent::GetAngularDrag() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mAngularDrag;
    }

    auto BuoyancyComponent::SetEnabled( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mEnabled != value ) {
            mEnabled = value;
            BumpRevision();
        }
    }

    auto BuoyancyComponent::IsEnabled() const -> bool {
        MKT_BEGIN_PROFILER();
        return mEnabled;
    }

    auto WaterVolumeComponent::SetHalfExtents( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mHalfExtents != value ) {
            mHalfExtents = value;
            BumpRevision();
        }
    }

    auto WaterVolumeComponent::GetHalfExtents() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mHalfExtents;
    }

    auto WaterVolumeComponent::SetSurfaceOffset( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mSurfaceOffset != value ) {
            mSurfaceOffset = value;
            BumpRevision();
        }
    }

    auto WaterVolumeComponent::GetSurfaceOffset() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mSurfaceOffset;
    }

    auto WaterVolumeComponent::SetDensity( core::f32 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mDensity != value ) {
            mDensity = value;
            BumpRevision();
        }
    }

    auto WaterVolumeComponent::GetDensity() const -> core::f32 {
        MKT_BEGIN_PROFILER();
        return mDensity;
    }

    auto WaterVolumeComponent::SetFlowVelocity( const core::float3& value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mFlowVelocity != value ) {
            mFlowVelocity = value;
            BumpRevision();
        }
    }

    auto WaterVolumeComponent::GetFlowVelocity() const -> const core::float3& {
        MKT_BEGIN_PROFILER();
        return mFlowVelocity;
    }

    auto WaterVolumeComponent::SetWaterSurfaceEntityID( core::u64 value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mWaterSurfaceEntityID != value ) {
            mWaterSurfaceEntityID = value;
            BumpRevision();
        }
    }

    auto WaterVolumeComponent::GetWaterSurfaceEntityID() const -> core::u64 {
        MKT_BEGIN_PROFILER();
        return mWaterSurfaceEntityID;
    }

    auto WaterVolumeComponent::SetEnabled( bool value ) -> void {
        MKT_BEGIN_PROFILER();

        if ( mEnabled != value ) {
            mEnabled = value;
            BumpRevision();
        }
    }

    auto WaterVolumeComponent::IsEnabled() const -> bool {
        MKT_BEGIN_PROFILER();
        return mEnabled;
    }

    auto VideoComponent::SetClip( const asset::AssetRef<asset::VideoClip> &clip ) -> void {
        if ( mClip.GetID() != clip.GetID() ) {
            mClip = clip;
            BumpRevision();
        }
    }

    auto VideoComponent::GetClip() const -> const asset::AssetRef<asset::VideoClip> & {
        return mClip;
    }

    auto VideoComponent::SetPlaybackSettings( const video::PlaybackSettings &settings ) -> void {
        mPlayback = settings;
        BumpRevision();
    }

    auto VideoComponent::GetPlaybackSettings() const -> const video::PlaybackSettings & {
        return mPlayback;
    }

    auto VideoComponent::SetOutputMap( video::OutputMap map ) -> void {
        if ( mOutputMap != map ) {
            mOutputMap = map;
            BumpRevision();
        }
    }

    auto VideoComponent::GetOutputMap() const -> video::OutputMap {
        return mOutputMap;
    }
}// namespace mikoto::scene
