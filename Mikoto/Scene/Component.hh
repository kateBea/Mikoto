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

#ifndef MIKOTO_SCENE_COMPONENT_HH
#define MIKOTO_SCENE_COMPONENT_HH

#include <array>
#include <string>

#include <EASTL/utility.h>
#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/unique_ptr.h>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/compatibility.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Math/Math.hh>
#include <Math/Random.hh>

#include <Filesystem/FileService.hh>

#include <Assets/AssetModel.hh>
#include <Assets/AssetVideo.hh>
#include <Assets/AssetTexture.hh>
#include <Assets/AssetSkeleton.hh>
#include <Assets/AssetRegistryService.hh>

#include <Material/Material.hh>
#include <Material/TextMaterial.hh>
#include <Material/MaterialGraph.hh>
#include <Material/VolumeMaterial.hh>
#include <Material/SurfaceMaterial.hh>
#include <Material/TerrainMaterial.hh>

#include <Animation/KeyframeClip.hh>

#include <Audio/AudioClip.hh>
#include <Audio/AudioMixer.hh>
#include <Audio/AudioDevice.hh>
#include <Audio/AudioListener.hh>

#include <Physics/Shape.hh>

#include <Text/Utility.hh>
#include <Text/TextLayout.hh>
#include <Text/UnicodeString.hh>

#include <Renderer/Text/Font.hh>
#include <Renderer/Core/Light.hh>
#include <Renderer/Gui/Canvas.hh>
#include <Renderer/Core/TerrainGeneration.hh>
#include <Renderer/Particle/ParticleEmitter.hh>

#include <Scene/SceneCamera.hh>

#include <Scripting/Script.hh>

namespace mikoto::renderer {
    class ISceneRenderer;
}

namespace mikoto::scene {

    class Scene;
    class Entity;
    class CameraComponent;

    class TagComponent {
    public:
        explicit TagComponent() = default;
        explicit TagComponent( eastl::string_view tag, bool active = true );

        TagComponent( const TagComponent& other );
        TagComponent( TagComponent&& other ) noexcept;
        auto operator=( const TagComponent& other ) -> TagComponent&;
        auto operator=( TagComponent&& other ) noexcept -> TagComponent&;

        MKT_NODISCARD auto GetGuid() const -> core::u32;
        MKT_NODISCARD auto GetGuidObject() -> math::random::Guid&;
        MKT_NODISCARD auto IsActive() const -> bool;
        MKT_NODISCARD auto GetTag() const -> const eastl::string&;

        auto SetActive( bool value ) -> void;
        auto SetTag( eastl::string_view newName ) -> void;

    private:
        friend class Scene;
        friend class Entity;
        Scene* mScene{};

        eastl::string mTag{};
        math::random::Guid mGuid{};

        bool mIsActive{};
    };


    // Used to identify objects that must be highlighted or
    // should be part of outline, highlight passes
    class HighlightComponent final {
    public:
        auto SetHighlighted( bool value ) -> void;
        MKT_NODISCARD auto IsHighlighted() const -> bool;

    private:
        bool mIsHighlighted{ false };
    };


    class TransformComponent {
    public:
        explicit TransformComponent();

        TransformComponent(
            const core::float3& position,
            const core::float3& size,
            const core::float3& angles,
            const core::float4x4& worldTransform = math::constants::Identity<core::float4x4>() );

        TransformComponent( const TransformComponent& other );
        TransformComponent( TransformComponent&& other ) noexcept;
        auto operator=( const TransformComponent& other ) -> TransformComponent&;
        auto operator=( TransformComponent&& other ) noexcept -> TransformComponent&;

        MKT_NODISCARD auto GetTranslation() const -> const core::float3&;
        MKT_NODISCARD auto GetRotation() const -> const core::float3&;
        MKT_NODISCARD auto GetScale() const -> const core::float3&;
        MKT_NODISCARD auto GetTransform() const -> const core::float4x4&;
        MKT_NODISCARD auto HasUniformScale() const -> bool;

        MKT_NODISCARD auto GetRotationQuat() const -> core::quat;
        MKT_NODISCARD auto GetWorldTransform() const -> const core::float4x4&;
        MKT_NODISCARD auto GetWorldRevision() const -> core::u64;

        auto SetRotation( const core::float3& value ) -> void;
        auto SetTranslation( const core::float3& value ) -> void;
        auto SetTransform( const core::float4x4& transform ) -> void;
        auto SetRotation( const core::quat& quaternion ) -> void;

        auto SetWorldTransform( const core::float4x4& worldTransform ) -> void;

        auto SetScale( const core::float3& value ) -> void;
        auto SetUniformSale( const bool value ) -> void;

        auto ComputeTransform( const core::float3& position, const core::float3& size, const core::float3& angles ) -> void;

        ~TransformComponent() = default;

    private:
        friend class Scene;

        auto RecomputeLocalTransform() -> void;
        static auto NextWorldRevision() -> core::u64;

        // Transform vectors
        core::float3 mTranslation{};
        core::float3 mRotation{};
        core::float3 mScale{};

        // Defines transformations in world space
        core::float4x4 mWorldTransform{ math::constants::Identity<core::float4x4>() };

        // Model matrix (defines object translation, rotation and scale
        // according to the current transform values/vectors in model space
        core::float4x4 mTransform{};

        bool mHasUniformScale{};
        // Assignment and local edits invalidate the scene-owned world cache.
        bool mTransformDirty{ true };
        core::u64 mWorldRevision{ NextWorldRevision() };
    };


    /**
     * Defines parent-child relationships between entities.
     * Establishes a structural hierarchy with dependencies,
     * allowing entities to inherit transformations and other properties from their parents.
     */
    class RelationComponent {
    public:
        explicit RelationComponent( const eastl::optional<core::u64> parent = eastl::nullopt );

        RelationComponent( const RelationComponent& other );
        RelationComponent( RelationComponent&& other ) = default;

        auto operator=( const RelationComponent& other ) -> RelationComponent&;
        auto operator=( RelationComponent&& other ) -> RelationComponent& = default;

        auto RegisterChild( core::u64 id ) -> void;
        auto EraseChild( core::u64 id ) -> void;

        MKT_NODISCARD auto IsChild( const core::u64 id ) const -> bool;
        MKT_NODISCARD auto HasChildren() const -> bool;

        auto SetParent( core::u64 uid ) -> void;
        MKT_NODISCARD auto HasParent() const -> bool;
        MKT_NODISCARD auto GetParent() const -> const eastl::optional<core::u64>&;

        MKT_NODISCARD auto IsLeaf() const -> bool;
        MKT_NODISCARD auto GetChildren() const -> const ankerl::unordered_dense::set<core::u64>&;

        ~RelationComponent() = default;

    private:
        eastl::optional<core::u64> mParent{};
        // Organizational membership is independent of the transform parent.
        eastl::optional<core::u64> mGroup{};
        ankerl::unordered_dense::set<core::u64> mChildrenIDs{};
        friend class Scene;
        Scene* mScene{};
        core::u64 mEntityID{};
    };


    /**
     * Defines logical associations between entities without establishing
     * hierarchical or transform dependencies. Groups are entities themselves
     * and may contain both regular entities and other groups.
     */
    class GroupComponent {
    public:
        GroupComponent() = default;

        // Membership belongs to the scene; use Scene::DuplicateEntity to clone groups.
        GroupComponent( const GroupComponent& other ) = delete;
        GroupComponent( GroupComponent&& other ) = default;

        auto operator=( const GroupComponent& other ) -> GroupComponent& = delete;
        auto operator=( GroupComponent&& other ) -> GroupComponent& = default;

        auto AddEntity( core::u64 entityID ) -> bool;
        auto RemoveEntity( core::u64 entityID ) -> void;

        MKT_NODISCARD auto Contains( core::u64 entityID ) const -> bool;
        MKT_NODISCARD auto HasChildren() const -> bool;

        MKT_NODISCARD auto GetChildren() const -> const ankerl::unordered_dense::set<core::u64>&;

        ~GroupComponent() = default;

    private:
        // I do not need an ID here, Groups are entities and as such
        // they grab their ID from the tag component

        // GUIDs of entities belonging to this group.
        // Members may themselves contain GroupComponent.
        ankerl::unordered_dense::set<core::u64> mChildren{};
        friend class Scene;
        Scene* mScene{};
        core::u64 mEntityID{};
    };


    class MaterialComponent {
    public:
        // Is internal is used for material that are part of loaded models
        explicit MaterialComponent( material::MaterialHandle mat =
            material::MaterialHandle::CreateEmpty(), bool isInternal = true );

        MKT_NODISCARD auto HasMaterial() const -> bool;
        MKT_NODISCARD auto GetMaterial() const -> material::MaterialHandle;

        // Borrow for render gathering without incrementing the reference count.
        MKT_NODISCARD auto GetMaterialReference() const -> const material::MaterialHandle&;
        MKT_NODISCARD auto IsInternal() const -> bool;

        auto SetIsInternal( bool value ) -> void;
        auto SetMaterial( material::MaterialHandle mat ) -> void;

        ~MaterialComponent() = default;

        MKT_PUSH_DEFAULT_MOVE_AND_COPY( MaterialComponent );

    private:
        bool mIsInternalMaterial{ true };
        material::MaterialHandle mMaterial{};
        // Authoring graph ownership belongs to the concrete material asset.

    };


    /**
     * This component will contain the data to render an object, such
     * as vertex buffers, index buffers, although this component won't be visible
     * in the editor UI
     * */
    class MeshComponent {
    public:
        explicit MeshComponent( asset::ModelHandle model = asset::ModelHandle::CreateEmpty(), core::i32 meshIndex = {} );
        /** Release only render residency; mesh identity, transforms and scene edits survive. */
        auto UpdateResidency( bool wanted ) -> void;
        MKT_NODISCARD auto GetStreamingCenter() const -> const core::float3&;
        MKT_NODISCARD auto GetStreamingExtent() const -> const core::float3&;

        ~MeshComponent() = default;

        auto SetMesh( asset::ModelHandle model, core::i32 meshIndex ) -> void;

        MKT_NODISCARD auto GetMeshIndex() const -> core::i32;
        MKT_NODISCARD auto GetModelPath() const -> const filesystem::Path&;

        MKT_NODISCARD auto HasMesh() const -> bool;
        MKT_NODISCARD auto GetMesh() const -> const asset::MeshNode*;

        MKT_NODISCARD auto GetMesh() -> asset::MeshNode*;

        MKT_NODISCARD auto GetModel() -> asset::ModelHandle;
        MKT_NODISCARD auto GetModel() const -> asset::ModelHandle;

        MKT_NODISCARD auto GetName() const -> eastl::string_view;

        MKT_NODISCARD auto IsSkinned() const -> bool;
        MKT_NODISCARD auto HasArmature() const -> bool;

        auto SetAnimator(core::u64 id) -> void;

        MKT_NODISCARD auto HasAnimations() const -> bool;

    private:
        core::i32 mMeshIndex{ -1 };

        asset::ModelHandle mModel{};
        asset::AssetHandle<asset::Model> mResidencyRequest{};
        core::float3 mStreamingCenter{};
        core::float3 mStreamingExtent{};
        std::chrono::steady_clock::time_point mLastResidentUse{ std::chrono::steady_clock::now() };
        bool mStreamingPinned{};

        filesystem::Path mPath{};
        std::string mName{};

        // Animator Controlling this mesh
        // We need to make sure the passed animation
        // uses an skeleton that is compatible with the bone IDs of this mesh
        core::u64 mAnimatorID{};
        bool mIsSkinned{ false };
    };


    class LightComponent {
    public:
        explicit LightComponent( renderer::LightType type = renderer::LightType::ePoint );

        LightComponent( const LightComponent& other ) = default;
        LightComponent( LightComponent&& other ) = default;

        auto operator=( const LightComponent& other ) -> LightComponent& = default;
        auto operator=( LightComponent&& other ) -> LightComponent& = default;

        ~LightComponent() = default;

        MKT_NODISCARD auto IsTypeActive( const renderer::LightType type ) const -> bool;

        MKT_NODISCARD auto GetActiveType() const -> renderer::LightType;
        auto SetActiveType( const renderer::LightType type ) -> void;

        template<typename T>
        MKT_NODISCARD auto Get() -> T& {
            MKT_BEGIN_PROFILER();
            if constexpr ( std::is_same_v<T, renderer::PointLight> ) {
                return mPointLight;
            } else if constexpr ( std::is_same_v<T, renderer::SpotLight> ) {
                return mSpotLight;
            } else if constexpr ( std::is_same_v<T, renderer::DirectionalLight> ) {
                return mDirectionalLight;
            } else if constexpr ( std::is_same_v<T, renderer::AreaLight> ) {
                return mAreaLight;
            } else {
                MKT_STATIC_ASSERT( false, "Unsupported light type in LightComponent::Get()" );
            }
        }

        template<typename T>
        MKT_NODISCARD auto Get() const -> const T& {
            MKT_BEGIN_PROFILER();
            if constexpr ( std::is_same_v<T, renderer::PointLight> ) {
                return mPointLight;
            } else if constexpr ( std::is_same_v<T, renderer::SpotLight> ) {
                return mSpotLight;
            } else if constexpr ( std::is_same_v<T, renderer::DirectionalLight> ) {
                return mDirectionalLight;
            } else if constexpr ( std::is_same_v<T, renderer::AreaLight> ) {
                return mAreaLight;
            } else {
                MKT_STATIC_ASSERT( false, "Unsupported light type in LightComponent::Get()" );
            }
        }

    private:
        renderer::SpotLight mSpotLight{};
        renderer::PointLight mPointLight{};
        renderer::AreaLight mAreaLight{};
        renderer::DirectionalLight mDirectionalLight{};

        renderer::LightType mType{ renderer::LightType::ePoint };
    };


    class AudioMixerComponent {
    public:
        AudioMixerComponent() = default;

        AudioMixerComponent( const AudioMixerComponent& other );
        AudioMixerComponent( AudioMixerComponent&& ) = default;

        auto operator=( AudioMixerComponent&& ) -> AudioMixerComponent& = default;
        auto operator=( const AudioMixerComponent& other ) -> AudioMixerComponent&;

        auto SetMute( bool mute ) -> void;
        auto SetVolume( core::f32 volume ) -> void;

        MKT_NODISCARD auto GetMixer( audio::AudioDevice* device ) -> audio::AudioMixer*;

        MKT_NODISCARD auto IsMuted() const -> bool;
        MKT_NODISCARD auto GetVolume() const -> core::f32;
    private:
        core::f32 mVolume{ 1 };
        eastl::shared_ptr<audio::AudioMixer> mRuntime{};

        bool mIsMuted{};
    };


    struct AudioMixerGroupComponent {
        core::u64 mMixerID{};
        core::u64 mOutputID{};
        float mVolume{ 1 };
        bool mMuted{};
    };


    // Authored video settings only; VideoService owns each runtime player.
    class VideoComponent : public core::RevisionTracked {
    public:
        VideoComponent() = default;

        VideoComponent( const VideoComponent& other ) = default;
        VideoComponent( VideoComponent&& ) = default;

        auto operator=( VideoComponent&& ) -> VideoComponent& = default;
        auto operator=( const VideoComponent& other ) -> VideoComponent& = default;

        MKT_NODISCARD auto GetOutputMap() const -> video::OutputMap;
        MKT_NODISCARD auto GetClip() const -> const asset::AssetRef<asset::VideoClip>&;
        MKT_NODISCARD auto GetPlaybackSettings() const -> const video::PlaybackSettings&;

        auto SetOutputMap( video::OutputMap map ) -> void;
        auto SetClip( const asset::AssetRef<asset::VideoClip>& clip ) -> void;
        auto SetPlaybackSettings( const video::PlaybackSettings& settings ) -> void;

    private:
        video::PlaybackSettings mPlayback{};
        video::OutputMap mOutputMap{ video::OutputMap::eBaseColor };

        asset::AssetRef<asset::VideoClip> mClip{};
    };


    enum class TextureRenderOutput {
        eBaseColor,
        eEmissive
    };

    enum class TextureRenderRate {
        e30FPS,
        e60FPS,
        e120FPS
    };

    enum class TextureRenderPath {
        eRasterization,
        ePathTracing,
    };

    class TextureRenderComponent final {
    public:
        TextureRenderComponent();

        TextureRenderComponent( const TextureRenderComponent& other );
        TextureRenderComponent( TextureRenderComponent&& other ) noexcept;

        auto operator=( TextureRenderComponent&& other ) noexcept -> TextureRenderComponent&;
        auto operator=( const TextureRenderComponent& other ) -> TextureRenderComponent&;

        ~TextureRenderComponent();

        auto SetRenderRate( TextureRenderRate rate ) -> void;
        auto SetRenderPath( TextureRenderPath path ) -> void;
        auto SetRenderOutput( TextureRenderOutput output ) -> void;

        auto SetCamera( const scene::Camera* camera, core::u64 entityID = 0 ) -> void;
        auto SetResolution( renderer::rhi::RenderResolution resolution ) -> void;

        auto Update( Scene& scene, const CameraComponent& camera, const core::float4x4& cameraWorld,
            const material::MaterialHandle& source, core::f32 timeStep ) -> void;

        MKT_NODISCARD auto GetRenderer() -> renderer::ISceneRenderer*;
        MKT_NODISCARD auto GetRenderRate() const -> TextureRenderRate;
        MKT_NODISCARD auto GetRenderPath() const -> TextureRenderPath;
        MKT_NODISCARD auto GetCamera() const -> const scene::Camera *;
        MKT_NODISCARD auto GetCameraEntityID() const -> core::u64;
        MKT_NODISCARD auto GetRenderOutput() const -> TextureRenderOutput;
        MKT_NODISCARD auto GetMaterial() const -> material::MaterialHandle;
        MKT_NODISCARD auto GetTexture() const -> renderer::rhi::TextureHandle;
        MKT_NODISCARD auto GetTextureRevision() const -> core::u64;
        MKT_NODISCARD auto GetResolution() const -> renderer::rhi::RenderResolution;


    private:
        struct Runtime;

        const scene::Camera *mCamera{};
        core::u64 mCameraEntityID{};
        eastl::unique_ptr<Runtime> mRuntime{};
        renderer::rhi::RenderResolution mResolution{ renderer::rhi::RenderResolution::e1080P };

        TextureRenderRate mRenderRate{ TextureRenderRate::e30FPS };
        TextureRenderPath mRenderPath{ TextureRenderPath::eRasterization };
        TextureRenderOutput mRenderOutput{ TextureRenderOutput::eBaseColor };
        bool mCaptureDirty{ true };
    };


    class AudioSourceComponent {
    public:
        AudioSourceComponent() = default;
        explicit AudioSourceComponent( const filesystem::Path& path );

        AudioSourceComponent( const AudioSourceComponent& other );
        AudioSourceComponent( AudioSourceComponent&& other ) = default;

        auto operator=( const AudioSourceComponent& other ) -> AudioSourceComponent&;
        auto operator=( AudioSourceComponent&& other ) noexcept -> AudioSourceComponent&;

        auto SetClip( const audio::AudioHandle& clip ) -> void;
        auto SetSettings( audio::SourceSettings settings ) -> void;
        MKT_NODISCARD auto GetSettings() const -> const audio::SourceSettings&;
        core::u64 mOutputID{};

        MKT_NODISCARD auto GetClip() const -> audio::AudioHandle;
        MKT_NODISCARD auto GetSource() const -> audio::AudioSourceHandle;

        ~AudioSourceComponent();

    private:
        audio::AudioHandle mClip{};
        audio::AudioSourceHandle mAudioSource{};
        audio::SourceSettings mSettings{};
    };


    class AudioListenerComponent {
    public:
        explicit AudioListenerComponent() = default;

        AudioListenerComponent( const AudioListenerComponent& other );
        AudioListenerComponent( AudioListenerComponent&& other ) = default;

        auto operator=( const AudioListenerComponent& other ) -> AudioListenerComponent&;
        auto operator=( AudioListenerComponent&& other ) -> AudioListenerComponent& = default;

        auto SetListener( audio::AudioListener* listener) -> void;

        auto Disable() -> void;
        auto SetEnabled( bool value ) -> void;

        MKT_NODISCARD auto IsEnabled() const -> bool;
        MKT_NODISCARD auto IsActive() -> bool;
        MKT_NODISCARD auto GetListenerPtr() const -> audio::AudioListener*;

        MKT_NODISCARD auto GetListener() -> audio::AudioListener&;
        MKT_NODISCARD auto GetListener() const -> const audio::AudioListener&;

        ~AudioListenerComponent() = default;

    private:
        audio::AudioListener* mListener{};
        bool mEnabled{ true };
    };


    enum class VehicleType {
        eWheeled,
        eMotorcycle,
        eTracked,
        eSixDof
    };

    /** Chassis-local wheel and suspension settings, in metres and radians. */
    struct VehicleWheelDescription {
        core::float3 mPosition{};
        core::float3 mSuspensionDirection{ 0.0f, -1.0f, 0.0f };
        core::f32 mRadius{ 0.35f };
        core::f32 mWidth{ 0.22f };
        core::f32 mSuspensionMin{ 0.15f };
        core::f32 mSuspensionMax{ 0.45f };
        core::f32 mSuspensionFrequency{ 2.0f };
        core::f32 mSuspensionDamping{ 0.7f };
        core::f32 mMaxSteerAngle{ 0.5235988f };
        core::f32 mMaxBrakeTorque{ 1500.0f };
        core::f32 mMaxHandBrakeTorque{ 4000.0f };
        bool mDriven{ true };
        bool mSteered{};
        auto operator==( const VehicleWheelDescription& ) const -> bool = default;
    };


    /** Vehicle chassis configuration in meters, with +Y up and +Z forward.
     * Attach beside a dynamic rigid body and a non-trigger collider. Keep the
     * chassis at unit world scale; scale imported body visuals as children.
     */
    struct VehicleComponent {
        explicit VehicleComponent() = default;

        VehicleComponent( const VehicleComponent& other ) = default;
        VehicleComponent( VehicleComponent&& other ) = default;

        auto operator=( const VehicleComponent& other ) -> VehicleComponent& = default;
        auto operator=( VehicleComponent&& other ) -> VehicleComponent& = default;

        ~VehicleComponent() = default;

        core::f32 mHalfTrack{ 0.85f };
        core::f32 mHalfWheelbase{ 1.35f };
        core::f32 mWheelRadius{ 0.35f };
        core::f32 mWheelWidth{ 0.22f };
        core::f32 mSuspensionMin{ 0.15f };
        core::f32 mSuspensionMax{ 0.45f };
        core::f32 mMaxSteerDegrees{ 30.0f };
        core::f32 mEngineTorque{ 500.0f };
        /** Suspension attachment and mass offsets are relative to the chassis origin. */
        core::f32 mWheelAnchorHeight{};
        core::f32 mCenterOfMassOffsetY{};
        core::f32 mSuspensionFrequency{ 2.0f };
        core::f32 mSuspensionDamping{ 0.7f };
        core::f32 mMaxPitchRollDegrees{ 180.0f };
        /** Tire grip multiplier; Jolt's sample uses 10 to preserve its original tuning. */
        core::f32 mLongitudinalImpulseScale{ 1.0f };
        bool mFourWheelDrive{ true };
        bool mCylinderWheelCast{};

        /** Transient inputs; throttle/steer are [-1, 1], brakes are [0, 1]. */
        core::f32 mThrottle{};
        core::f32 mSteer{};
        core::f32 mBrake{};
        core::f32 mHandBrake{};

        /** Reject invalid authoring data before it reaches the physics solver. */
        MKT_NODISCARD auto IsValid() const -> bool;

        /** Runtime controls do not require recreating the constraint. */
        MKT_NODISCARD auto HasSameSettings( const VehicleComponent& other ) const -> bool;

        // An empty wheel list selects the corresponding Jolt sample's wheel layout.
        auto SetVehicleType( VehicleType type ) -> void;
        MKT_NODISCARD auto GetVehicleType() const -> VehicleType;
        auto SetWheelDescriptions( const eastl::vector<VehicleWheelDescription>& wheels ) -> void;
        MKT_NODISCARD auto GetWheelDescriptions() const -> const eastl::vector<VehicleWheelDescription>&;

    private:
        VehicleType mVehicleType{ VehicleType::eWheeled };
        eastl::vector<VehicleWheelDescription> mWheelDescriptions{};
    };


    /** Ordered box plank in a suspension bridge. Siblings under the same parent
     * are joined at both deck edges; end planks should have static rigid bodies.
     */
    struct BridgeSegmentComponent {
        core::u32 mIndex{};
    };

    /** Bind a direct child of a vehicle to a simulated wheel. No second body is
     * needed. Indices follow the vehicle's authored wheel list or sample layout. Visual correction
     * holds mesh normalization, rotation and scale independently of suspension.
     */
    struct VehicleWheelComponent {
        core::u32 mIndex{};
        core::float4x4 mVisualCorrection{ 1.0f };
        /** Calipers follow suspension and steering without rotating with the tire. */
        bool mFollowSpin{ true };
    };


    class RigidBodyComponent {
    public:
        enum class BodyType {
            eStatic,
            eKinematic,
            eDynamic
        };

        explicit RigidBodyComponent() = default;

        explicit RigidBodyComponent( const core::u64 bodyID );

        RigidBodyComponent( const RigidBodyComponent& other );
        RigidBodyComponent( RigidBodyComponent&& ) noexcept = default;

        auto operator=( const RigidBodyComponent& other ) -> RigidBodyComponent&;

        auto operator=( RigidBodyComponent&& ) noexcept -> RigidBodyComponent& = default;

        ~RigidBodyComponent() = default;

        MKT_NODISCARD auto GetMass() const -> float;
        auto SetMass( const float mass ) -> void;

        MKT_NODISCARD auto static GetMaxBodyMass() -> float;

        MKT_NODISCARD auto GetFriction() const -> float;
        auto SetFriction( const float friction ) -> void;

        MKT_NODISCARD auto UseGravity() const -> bool;
        auto SetUseGravity( const bool enabled ) -> void;

        MKT_NODISCARD auto GetBodyType() const -> BodyType;
        MKT_NODISCARD auto IsBodyType( const BodyType type ) const -> bool;
        MKT_NODISCARD auto IsDynamic() const -> bool;
        auto SetBodyType( const BodyType type ) -> void;

        MKT_NODISCARD auto GetBodyID() const -> core::u64;
        MKT_NODISCARD auto IsValidBodyID() const -> bool;

        auto SetLinearVelocity( float x, float y, float z ) -> void;

        auto SetLinearVelocity( const core::float3& velocity ) -> void;

        auto SetAngularVelocity( float x, float y, float z ) -> void;

        auto SetAngularVelocity( const core::float3& velocity ) -> void;

        MKT_NODISCARD auto GetLinearVelocity() const -> core::float3;
        MKT_NODISCARD auto GetAngularVelocity() const -> core::float3;

        auto SetBodyID( const core::u64 bodyID ) -> void;

        auto RemoveBodyID() -> void;

        auto SetRestitution( float value ) -> void;
        MKT_NODISCARD auto GetRestitution() const -> float;

    private:
        float mMass{ 1.0f };
        float mRestitution{ 0.01f };
        float mFriction{ 1.0f };
        bool mUseGravity{ true };
        BodyType mBodyType{ BodyType::eKinematic };

        core::float3 mLinearVelocity{};
        core::float3 mAngularVelocity{ 0.0f, 0.0f, 0.0f };

        core::u64 mBodyID{};
        bool mIsValidBody{ false };
    };


    class MeshColliderComponent {
    public:
        // Convex hulls support moving bodies. Triangle meshes are static only.
        auto IsConvex() const -> bool;
        auto SetConvex( bool value ) -> void;
        explicit MeshColliderComponent() = default;

        MeshColliderComponent( const MeshColliderComponent& other ) = default;
        MeshColliderComponent( MeshColliderComponent&& other ) = default;

        auto operator=( const MeshColliderComponent& other ) -> MeshColliderComponent& = default;
        auto operator=( MeshColliderComponent&& other ) -> MeshColliderComponent& = default;

        ~MeshColliderComponent() = default;

        /**
         * @brief Returns the local collider offset.
         * @returns Offset from the entity origin. */
        MKT_NODISCARD auto GetOffset() const -> const core::float3&;

        /**
         * @brief Updates the local collider offset.
         * @param offset Offset from the entity origin. */
        auto SetOffset( const core::float3& offset ) -> void;

        /**
         * @brief Returns whether the shape emits overlap events without physical response.
         * @returns Trigger state. */
        MKT_NODISCARD auto IsTrigger() const -> bool;

        /**
         * @brief Updates trigger behaviour.
         * @param value True to make the collider a trigger. */
        auto SetTrigger( const bool value ) -> void;

        /**
         * @brief Returns the concrete collider kind.
         * @returns Mesh collider type. */
        MKT_NODISCARD static auto GetColliderType() -> physics::ColliderType;

    private:
        core::float3 mOffset{};
        bool mIsTrigger{false};
        bool mConvex{ true };
    };


    class CapsuleColliderComponent {
    public:
        explicit CapsuleColliderComponent() = default;

        CapsuleColliderComponent( const CapsuleColliderComponent& other ) = default;
        CapsuleColliderComponent( CapsuleColliderComponent&& other ) = default;

        auto operator=( const CapsuleColliderComponent& other ) -> CapsuleColliderComponent& = default;
        auto operator=( CapsuleColliderComponent&& other ) -> CapsuleColliderComponent& = default;

        ~CapsuleColliderComponent() = default;

        /** @brief Returns the radius of the capsule hemispheres. @returns Non-negative radius. */
        MKT_NODISCARD auto GetRadius() const -> core::f32;
        /** @brief Sets the radius of the capsule hemispheres. @param radius New non-negative radius. */
        auto SetRadius( const core::f32 radius ) -> void;
        /** @brief Returns half the height of the cylindrical capsule section. @returns Half-height. */
        MKT_NODISCARD auto GetHalfHeight() const -> core::f32;
        /** @brief Sets half the height of the cylindrical capsule section. @param height New non-negative half-height. */
        auto SetHalfHeight( const core::f32 height ) -> void;
        /** @brief Returns the local collider offset. @returns Offset from the entity origin. */
        MKT_NODISCARD auto GetOffset() const -> const core::float3&;
        /** @brief Updates the local collider offset. @param offset Offset from the entity origin. */
        auto SetOffset( const core::float3& offset ) -> void;
        /** @brief Returns whether the shape is a trigger. @returns Trigger state. */
        MKT_NODISCARD auto IsTrigger() const -> bool;
        /** @brief Updates trigger behaviour. @param value True to make the collider a trigger. */
        auto SetTrigger( const bool value ) -> void;
        /** @brief Returns the concrete collider kind. @returns Capsule collider type. */
        MKT_NODISCARD static auto GetColliderType() -> physics::ColliderType;

    private:
        core::f32 mRadius{ 0.5f };
        core::f32 mHalfHeight{ 0.5f };
        core::float3 mOffset{};
        bool mIsTrigger{};
    };


    class SphereColliderComponent {
    public:
        explicit SphereColliderComponent() = default;

        SphereColliderComponent( const SphereColliderComponent& other ) = default;
        SphereColliderComponent( SphereColliderComponent&& other ) = default;

        auto operator=( const SphereColliderComponent& other ) -> SphereColliderComponent& = default;
        auto operator=( SphereColliderComponent&& other ) -> SphereColliderComponent& = default;

        ~SphereColliderComponent() = default;

        /** @brief Returns the sphere radius. @returns Non-negative radius. */
        MKT_NODISCARD auto GetRadius() const -> core::f32;
        /** @brief Sets the sphere radius. @param radius New non-negative radius. */
        auto SetRadius( const core::f32 radius ) -> void;
        /** @brief Returns the local collider offset. @returns Offset from the entity origin. */
        MKT_NODISCARD auto GetOffset() const -> const core::float3&;
        /** @brief Updates the local collider offset. @param offset Offset from the entity origin. */
        auto SetOffset( const core::float3& offset ) -> void;
        /** @brief Returns whether the shape is a trigger. @returns Trigger state. */
        MKT_NODISCARD auto IsTrigger() const -> bool;
        /** @brief Updates trigger behaviour. @param value True to make the collider a trigger. */
        auto SetTrigger( const bool value ) -> void;
        /** @brief Returns the concrete collider kind. @returns Sphere collider type. */
        MKT_NODISCARD static auto GetColliderType() -> physics::ColliderType;

    private:
        core::f32 mRadius{ 0.5f };
        core::float3 mOffset{};
        bool mIsTrigger{};
    };


    class BoxColliderComponent {
    public:
        explicit BoxColliderComponent() = default;

        BoxColliderComponent( const BoxColliderComponent& other ) = default;
        BoxColliderComponent( BoxColliderComponent&& other ) = default;

        auto operator=( const BoxColliderComponent& other ) -> BoxColliderComponent& = default;
        auto operator=( BoxColliderComponent&& other ) -> BoxColliderComponent& = default;

        ~BoxColliderComponent() = default;

        /** @brief Returns box half extents in local space. @returns Positive half extents. */
        MKT_NODISCARD auto GetHalfExtents() const -> const core::float3&;
        /** @brief Sets box half extents in local space. @param extents Positive half extents. */
        auto SetHalfExtents( const core::float3& extents ) -> void;
        /** @brief Returns the local collider offset. @returns Offset from the entity origin. */
        MKT_NODISCARD auto GetOffset() const -> const core::float3&;
        /** @brief Updates the local collider offset. @param offset Offset from the entity origin. */
        auto SetOffset( const core::float3& offset ) -> void;
        /** @brief Returns whether the shape is a trigger. @returns Trigger state. */
        MKT_NODISCARD auto IsTrigger() const -> bool;
        /** @brief Updates trigger behaviour. @param value True to make the collider a trigger. */
        auto SetTrigger( const bool value ) -> void;
        /** @brief Returns the concrete collider kind. @returns Box collider type. */
        MKT_NODISCARD static auto GetColliderType() -> physics::ColliderType;

    private:
        core::float3 mHalfExtents{ 0.5f };
        core::float3 mOffset{};
        bool mIsTrigger{};
    };


    // Physics authoring sketches: settings/accessors only. Jolt integration, editor UI and
    // scene serialization remain pending. Setters store authored values without cooking or validation.
    // Distances use meters, angles use radians, and entity IDs are scene GUIDs (0 means unassigned unless noted).
    enum class CompoundColliderMode {
        eStatic, // Immutable shape collection; the owning rigid body may still be dynamic.
        eMutable
    };

    enum class CharacterControllerType {
        eVirtual,
        eRigidBody
    };

    enum class JointAxisMotion {
        eFixed,
        eFree,
        eLimited
    };

    enum class JointMotorMode {
        eOff,
        eVelocity,
        ePosition
    };

    /** Spring authoring data; frequency is in Hz and damping is a dimensionless ratio. */
    struct JointSpringSettings {
        core::f32 mFrequency{};
        core::f32 mDamping{ 0.7f };
    };

    /** Scalar joint drive. Targets use meters/radians and their per-second velocities.
     * The force limit is in newtons for linear drives and newton-meters for angular drives.
     */
    struct JointMotorSettings {
        JointMotorMode mMode{ JointMotorMode::eOff };
        core::f32 mTargetVelocity{};
        core::f32 mTargetPosition{};
        core::f32 mMaxForceOrTorque{ 1000.0f };
        JointSpringSettings mSpring{};
    };

    /** One Hermite spline point; positions/tangents are in the owning path frame. */
    struct JointPathPoint {
        core::float3 mPosition{};
        core::float3 mTangent{ 1.0f, 0.0f, 0.0f };
        core::float3 mNormal{ 0.0f, 1.0f, 0.0f };
    };

    /** Associates an imported skeleton bone with its authored body and optional joint entity. */
    struct RagdollBoneDescription {
        eastl::string mBoneName{};
        core::u64 mBodyEntityID{};
        core::u64 mJointEntityID{};
    };

    /** Describes landscape collision from a landscape entity or a heightmap asset. No height data is loaded here. */
    class HeightFieldColliderComponent final : public core::RevisionTracked {
    public:
        explicit HeightFieldColliderComponent() = default;

        HeightFieldColliderComponent( const HeightFieldColliderComponent& other ) = default;
        HeightFieldColliderComponent( HeightFieldColliderComponent&& other ) = default;

        auto operator=( const HeightFieldColliderComponent& other ) -> HeightFieldColliderComponent& = default;
        auto operator=( HeightFieldColliderComponent&& other ) -> HeightFieldColliderComponent& = default;

        auto SetLandscapeEntityID( core::u64 value ) -> void;
        MKT_NODISCARD auto GetLandscapeEntityID() const -> core::u64;

        auto SetHeightmap( const asset::AssetRef<asset::Texture>& value ) -> void;
        MKT_NODISCARD auto GetHeightmap() const -> const asset::AssetRef<asset::Texture>&;

        auto SetSampleSpacing( const core::float2& value ) -> void;
        MKT_NODISCARD auto GetSampleSpacing() const -> const core::float2&;

        auto SetHeightScale( core::f32 value ) -> void;
        MKT_NODISCARD auto GetHeightScale() const -> core::f32;

        auto SetBaseElevation( core::f32 value ) -> void;
        MKT_NODISCARD auto GetBaseElevation() const -> core::f32;

        auto SetOffset( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetOffset() const -> const core::float3&;

        auto SetTrigger( bool value ) -> void;
        MKT_NODISCARD auto IsTrigger() const -> bool;

        MKT_NODISCARD static auto GetColliderType() -> physics::ColliderType;

        ~HeightFieldColliderComponent() = default;

    private:
        core::u64 mLandscapeEntityID{};
        asset::AssetRef<asset::Texture> mHeightmap{};
        core::float2 mSampleSpacing{ 1.0f };
        core::f32 mHeightScale{ 64.0f };
        core::f32 mBaseElevation{ 0.0f };
        core::float3 mOffset{ 0.0f };
        bool mTrigger{ false };
    };

    /** Groups referenced collider entities into one body's shape; child transforms supply local shape placement. */
    class CompoundColliderComponent final : public core::RevisionTracked {
    public:
        explicit CompoundColliderComponent() = default;

        CompoundColliderComponent( const CompoundColliderComponent& other ) = default;
        CompoundColliderComponent( CompoundColliderComponent&& other ) = default;

        auto operator=( const CompoundColliderComponent& other ) -> CompoundColliderComponent& = default;
        auto operator=( CompoundColliderComponent&& other ) -> CompoundColliderComponent& = default;

        auto SetColliderEntities( const eastl::vector<core::u64>& value ) -> void;
        MKT_NODISCARD auto GetColliderEntities() const -> const eastl::vector<core::u64>&;

        auto SetMode( CompoundColliderMode value ) -> void;
        MKT_NODISCARD auto GetMode() const -> CompoundColliderMode;

        auto SetOffset( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetOffset() const -> const core::float3&;

        auto SetTrigger( bool value ) -> void;
        MKT_NODISCARD auto IsTrigger() const -> bool;

        MKT_NODISCARD static auto GetColliderType() -> physics::ColliderType;

        ~CompoundColliderComponent() = default;

    private:
        eastl::vector<core::u64> mColliderEntities{};
        CompoundColliderMode mMode{ CompoundColliderMode::eStatic };
        core::float3 mOffset{ 0.0f };
        bool mTrigger{ false };
    };

    /** Describes a solid cylinder along local Y. Half-height excludes no additional caps. */
    class CylinderColliderComponent final : public core::RevisionTracked {
    public:
        explicit CylinderColliderComponent() = default;

        CylinderColliderComponent( const CylinderColliderComponent& other ) = default;
        CylinderColliderComponent( CylinderColliderComponent&& other ) = default;

        auto operator=( const CylinderColliderComponent& other ) -> CylinderColliderComponent& = default;
        auto operator=( CylinderColliderComponent&& other ) -> CylinderColliderComponent& = default;

        auto SetRadius( core::f32 value ) -> void;
        MKT_NODISCARD auto GetRadius() const -> core::f32;

        auto SetHalfHeight( core::f32 value ) -> void;
        MKT_NODISCARD auto GetHalfHeight() const -> core::f32;

        auto SetOffset( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetOffset() const -> const core::float3&;

        auto SetTrigger( bool value ) -> void;
        MKT_NODISCARD auto IsTrigger() const -> bool;

        MKT_NODISCARD static auto GetColliderType() -> physics::ColliderType;

        ~CylinderColliderComponent() = default;

    private:
        core::f32 mRadius{ 0.5f };
        core::f32 mHalfHeight{ 0.5f };
        core::float3 mOffset{ 0.0f };
        bool mTrigger{ false };
    };

    /** Describes a capsule with different upper and lower radii; half-height separates the hemisphere centers. */
    class TaperedCapsuleColliderComponent final : public core::RevisionTracked {
    public:
        explicit TaperedCapsuleColliderComponent() = default;

        TaperedCapsuleColliderComponent( const TaperedCapsuleColliderComponent& other ) = default;
        TaperedCapsuleColliderComponent( TaperedCapsuleColliderComponent&& other ) = default;

        auto operator=( const TaperedCapsuleColliderComponent& other ) -> TaperedCapsuleColliderComponent& = default;
        auto operator=( TaperedCapsuleColliderComponent&& other ) -> TaperedCapsuleColliderComponent& = default;

        auto SetTopRadius( core::f32 value ) -> void;
        MKT_NODISCARD auto GetTopRadius() const -> core::f32;

        auto SetBottomRadius( core::f32 value ) -> void;
        MKT_NODISCARD auto GetBottomRadius() const -> core::f32;

        auto SetHalfHeight( core::f32 value ) -> void;
        MKT_NODISCARD auto GetHalfHeight() const -> core::f32;

        auto SetOffset( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetOffset() const -> const core::float3&;

        auto SetTrigger( bool value ) -> void;
        MKT_NODISCARD auto IsTrigger() const -> bool;

        MKT_NODISCARD static auto GetColliderType() -> physics::ColliderType;

        ~TaperedCapsuleColliderComponent() = default;

    private:
        core::f32 mTopRadius{ 0.35f };
        core::f32 mBottomRadius{ 0.5f };
        core::f32 mHalfHeight{ 0.5f };
        core::float3 mOffset{ 0.0f };
        bool mTrigger{ false };
    };

    /** Describes a cylinder with different radii at its upper and lower ends, along local Y. */
    class TaperedCylinderColliderComponent final : public core::RevisionTracked {
    public:
        explicit TaperedCylinderColliderComponent() = default;

        TaperedCylinderColliderComponent( const TaperedCylinderColliderComponent& other ) = default;
        TaperedCylinderColliderComponent( TaperedCylinderColliderComponent&& other ) = default;

        auto operator=( const TaperedCylinderColliderComponent& other ) -> TaperedCylinderColliderComponent& = default;
        auto operator=( TaperedCylinderColliderComponent&& other ) -> TaperedCylinderColliderComponent& = default;

        auto SetTopRadius( core::f32 value ) -> void;
        MKT_NODISCARD auto GetTopRadius() const -> core::f32;

        auto SetBottomRadius( core::f32 value ) -> void;
        MKT_NODISCARD auto GetBottomRadius() const -> core::f32;

        auto SetHalfHeight( core::f32 value ) -> void;
        MKT_NODISCARD auto GetHalfHeight() const -> core::f32;

        auto SetOffset( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetOffset() const -> const core::float3&;

        auto SetTrigger( bool value ) -> void;
        MKT_NODISCARD auto IsTrigger() const -> bool;

        MKT_NODISCARD static auto GetColliderType() -> physics::ColliderType;

        ~TaperedCylinderColliderComponent() = default;

    private:
        core::f32 mTopRadius{ 0.35f };
        core::f32 mBottomRadius{ 0.5f };
        core::f32 mHalfHeight{ 0.5f };
        core::float3 mOffset{ 0.0f };
        bool mTrigger{ false };
    };

    /** Describes an infinite static collision plane: dot(normal, localPosition) = distance; the normal must be unit length. */
    class PlaneColliderComponent final : public core::RevisionTracked {
    public:
        explicit PlaneColliderComponent() = default;

        PlaneColliderComponent( const PlaneColliderComponent& other ) = default;
        PlaneColliderComponent( PlaneColliderComponent&& other ) = default;

        auto operator=( const PlaneColliderComponent& other ) -> PlaneColliderComponent& = default;
        auto operator=( PlaneColliderComponent&& other ) -> PlaneColliderComponent& = default;

        auto SetNormal( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetNormal() const -> const core::float3&;

        auto SetDistance( core::f32 value ) -> void;
        MKT_NODISCARD auto GetDistance() const -> core::f32;

        auto SetOffset( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetOffset() const -> const core::float3&;

        auto SetTrigger( bool value ) -> void;
        MKT_NODISCARD auto IsTrigger() const -> bool;

        MKT_NODISCARD static auto GetColliderType() -> physics::ColliderType;

        ~PlaneColliderComponent() = default;

    private:
        core::float3 mNormal{ 0.0f, 1.0f, 0.0f };
        core::f32 mDistance{ 0.0f };
        core::float3 mOffset{ 0.0f };
        bool mTrigger{ false };
    };

    /** Shared joint authoring data. Store each joint on its own entity; body IDs reference scene entities, not Jolt bodies.
     * Body A must be assigned; body B == 0 anchors to the world. Anchors/rotations define local frames (world space for body B == 0).
     * The local X axis is the primary hinge/slider axis. All angular settings and angular motor targets use radians. */
    class JointComponent : public core::RevisionTracked {
    public:
        explicit JointComponent() = default;

        JointComponent( const JointComponent& other ) = default;
        JointComponent( JointComponent&& other ) = default;

        auto operator=( const JointComponent& other ) -> JointComponent& = default;
        auto operator=( JointComponent&& other ) -> JointComponent& = default;

        auto SetBodyAEntityID( core::u64 value ) -> void;
        MKT_NODISCARD auto GetBodyAEntityID() const -> core::u64;

        auto SetBodyBEntityID( core::u64 value ) -> void;
        MKT_NODISCARD auto GetBodyBEntityID() const -> core::u64;

        auto SetAnchorA( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetAnchorA() const -> const core::float3&;

        auto SetAnchorB( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetAnchorB() const -> const core::float3&;

        auto SetRotationA( const core::quat& value ) -> void;
        MKT_NODISCARD auto GetRotationA() const -> const core::quat&;

        auto SetRotationB( const core::quat& value ) -> void;
        MKT_NODISCARD auto GetRotationB() const -> const core::quat&;

        auto SetCollideConnected( bool value ) -> void;
        MKT_NODISCARD auto CanConnectedBodiesCollide() const -> bool;

        auto SetEnabled( bool value ) -> void;
        MKT_NODISCARD auto IsEnabled() const -> bool;

        ~JointComponent() = default;

    private:
        core::u64 mBodyAEntityID{};
        core::u64 mBodyBEntityID{};
        core::float3 mAnchorA{ 0.0f };
        core::float3 mAnchorB{ 0.0f };
        core::quat mRotationA{ 1.0f, 0.0f, 0.0f, 0.0f };
        core::quat mRotationB{ 1.0f, 0.0f, 0.0f, 0.0f };
        bool mCollideConnected{ false };
        bool mEnabled{ true };
    };

    /** Keeps two anchors within a distance range; spring settings allow a soft link such as a suspension or rope limit. */
    class DistanceJointComponent final : public JointComponent {
    public:
        explicit DistanceJointComponent() = default;

        DistanceJointComponent( const DistanceJointComponent& other ) = default;
        DistanceJointComponent( DistanceJointComponent&& other ) = default;

        auto operator=( const DistanceJointComponent& other ) -> DistanceJointComponent& = default;
        auto operator=( DistanceJointComponent&& other ) -> DistanceJointComponent& = default;

        auto SetMinDistance( core::f32 value ) -> void;
        MKT_NODISCARD auto GetMinDistance() const -> core::f32;

        auto SetMaxDistance( core::f32 value ) -> void;
        MKT_NODISCARD auto GetMaxDistance() const -> core::f32;

        auto SetSpring( const JointSpringSettings& value ) -> void;
        MKT_NODISCARD auto GetSpring() const -> const JointSpringSettings&;

        ~DistanceJointComponent() = default;

    private:
        core::f32 mMinDistance{ 0.0f };
        core::f32 mMaxDistance{ 1.0f };
        JointSpringSettings mSpring{};
    };

    /** Locks the connected bodies' relative position and orientation, using the shared joint frames. */
    class FixedJointComponent final : public JointComponent {
    public:
        explicit FixedJointComponent() = default;

        FixedJointComponent( const FixedJointComponent& other ) = default;
        FixedJointComponent( FixedJointComponent&& other ) = default;

        auto operator=( const FixedJointComponent& other ) -> FixedJointComponent& = default;
        auto operator=( FixedJointComponent&& other ) -> FixedJointComponent& = default;

        ~FixedJointComponent() = default;
    };

    /** Keeps two anchors together while allowing the bodies to rotate freely around that point. */
    class PointJointComponent final : public JointComponent {
    public:
        explicit PointJointComponent() = default;

        PointJointComponent( const PointJointComponent& other ) = default;
        PointJointComponent( PointJointComponent&& other ) = default;

        auto operator=( const PointJointComponent& other ) -> PointJointComponent& = default;
        auto operator=( PointJointComponent&& other ) -> PointJointComponent& = default;

        ~PointJointComponent() = default;
    };

    /** Allows rotation around the joint frame's X axis, with angular limits and an optional motor. */
    class HingeJointComponent final : public JointComponent {
    public:
        explicit HingeJointComponent() = default;

        HingeJointComponent( const HingeJointComponent& other ) = default;
        HingeJointComponent( HingeJointComponent&& other ) = default;

        auto operator=( const HingeJointComponent& other ) -> HingeJointComponent& = default;
        auto operator=( HingeJointComponent&& other ) -> HingeJointComponent& = default;

        auto SetMinAngle( core::f32 value ) -> void;
        MKT_NODISCARD auto GetMinAngle() const -> core::f32;

        auto SetMaxAngle( core::f32 value ) -> void;
        MKT_NODISCARD auto GetMaxAngle() const -> core::f32;

        auto SetLimitSpring( const JointSpringSettings& value ) -> void;
        MKT_NODISCARD auto GetLimitSpring() const -> const JointSpringSettings&;

        auto SetMotor( const JointMotorSettings& value ) -> void;
        MKT_NODISCARD auto GetMotor() const -> const JointMotorSettings&;

        ~HingeJointComponent() = default;

    private:
        core::f32 mMinAngle{ -1.5707963f };
        core::f32 mMaxAngle{ 1.5707963f };
        JointSpringSettings mLimitSpring{};
        JointMotorSettings mMotor{};
    };

    /** Allows translation along the joint frame's X axis, with distance limits and an optional motor. */
    class SliderJointComponent final : public JointComponent {
    public:
        explicit SliderJointComponent() = default;

        SliderJointComponent( const SliderJointComponent& other ) = default;
        SliderJointComponent( SliderJointComponent&& other ) = default;

        auto operator=( const SliderJointComponent& other ) -> SliderJointComponent& = default;
        auto operator=( SliderJointComponent&& other ) -> SliderJointComponent& = default;

        auto SetMinDistance( core::f32 value ) -> void;
        MKT_NODISCARD auto GetMinDistance() const -> core::f32;

        auto SetMaxDistance( core::f32 value ) -> void;
        MKT_NODISCARD auto GetMaxDistance() const -> core::f32;

        auto SetLimitSpring( const JointSpringSettings& value ) -> void;
        MKT_NODISCARD auto GetLimitSpring() const -> const JointSpringSettings&;

        auto SetMotor( const JointMotorSettings& value ) -> void;
        MKT_NODISCARD auto GetMotor() const -> const JointMotorSettings&;

        ~SliderJointComponent() = default;

    private:
        core::f32 mMinDistance{ -1.0f };
        core::f32 mMaxDistance{ 1.0f };
        JointSpringSettings mLimitSpring{};
        JointMotorSettings mMotor{};
    };

    /** Keeps anchors together and limits the angle between their primary axes to a cone. */
    class ConeJointComponent final : public JointComponent {
    public:
        explicit ConeJointComponent() = default;

        ConeJointComponent( const ConeJointComponent& other ) = default;
        ConeJointComponent( ConeJointComponent&& other ) = default;

        auto operator=( const ConeJointComponent& other ) -> ConeJointComponent& = default;
        auto operator=( ConeJointComponent&& other ) -> ConeJointComponent& = default;

        auto SetHalfConeAngle( core::f32 value ) -> void;
        MKT_NODISCARD auto GetHalfConeAngle() const -> core::f32;

        ~ConeJointComponent() = default;

    private:
        core::f32 mHalfConeAngle{ 0.7853982f };
    };

    /** Limits two swing angles and twist around the primary axis, for joints such as a shoulder. */
    class SwingTwistJointComponent final : public JointComponent {
    public:
        explicit SwingTwistJointComponent() = default;

        SwingTwistJointComponent( const SwingTwistJointComponent& other ) = default;
        SwingTwistJointComponent( SwingTwistJointComponent&& other ) = default;

        auto operator=( const SwingTwistJointComponent& other ) -> SwingTwistJointComponent& = default;
        auto operator=( SwingTwistJointComponent&& other ) -> SwingTwistJointComponent& = default;

        auto SetNormalHalfConeAngle( core::f32 value ) -> void;
        MKT_NODISCARD auto GetNormalHalfConeAngle() const -> core::f32;

        auto SetPlaneHalfConeAngle( core::f32 value ) -> void;
        MKT_NODISCARD auto GetPlaneHalfConeAngle() const -> core::f32;

        auto SetMinTwistAngle( core::f32 value ) -> void;
        MKT_NODISCARD auto GetMinTwistAngle() const -> core::f32;

        auto SetMaxTwistAngle( core::f32 value ) -> void;
        MKT_NODISCARD auto GetMaxTwistAngle() const -> core::f32;

        auto SetMotors( const eastl::array<JointMotorSettings, 3>& value ) -> void;
        MKT_NODISCARD auto GetMotors() const -> const eastl::array<JointMotorSettings, 3>&;

        ~SwingTwistJointComponent() = default;

    private:
        core::f32 mNormalHalfConeAngle{ 0.7853982f };
        core::f32 mPlaneHalfConeAngle{ 0.7853982f };
        core::f32 mMinTwistAngle{ -0.7853982f };
        core::f32 mMaxTwistAngle{ 0.7853982f };
        eastl::array<JointMotorSettings, 3> mMotors{};
    };

    /** Controls each translation and rotation axis independently: free, fixed or limited, with optional motors.
     * Motor entries are translation X/Y/Z followed by rotation X/Y/Z; zero-initialized motion arrays lock every axis. */
    class SixDofJointComponent final : public JointComponent {
    public:
        explicit SixDofJointComponent() = default;

        SixDofJointComponent( const SixDofJointComponent& other ) = default;
        SixDofJointComponent( SixDofJointComponent&& other ) = default;

        auto operator=( const SixDofJointComponent& other ) -> SixDofJointComponent& = default;
        auto operator=( SixDofJointComponent&& other ) -> SixDofJointComponent& = default;

        auto SetTranslationMotion( const eastl::array<JointAxisMotion, 3>& value ) -> void;
        MKT_NODISCARD auto GetTranslationMotion() const -> const eastl::array<JointAxisMotion, 3>&;

        auto SetRotationMotion( const eastl::array<JointAxisMotion, 3>& value ) -> void;
        MKT_NODISCARD auto GetRotationMotion() const -> const eastl::array<JointAxisMotion, 3>&;

        auto SetMinTranslation( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetMinTranslation() const -> const core::float3&;

        auto SetMaxTranslation( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetMaxTranslation() const -> const core::float3&;

        auto SetMinRotation( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetMinRotation() const -> const core::float3&;

        auto SetMaxRotation( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetMaxRotation() const -> const core::float3&;

        auto SetMotors( const eastl::array<JointMotorSettings, 6>& value ) -> void;
        MKT_NODISCARD auto GetMotors() const -> const eastl::array<JointMotorSettings, 6>&;

        ~SixDofJointComponent() = default;

    private:
        eastl::array<JointAxisMotion, 3> mTranslationMotion{};
        eastl::array<JointAxisMotion, 3> mRotationMotion{};
        core::float3 mMinTranslation{ 0.0f };
        core::float3 mMaxTranslation{ 0.0f };
        core::float3 mMinRotation{ 0.0f };
        core::float3 mMaxRotation{ 0.0f };
        eastl::array<JointMotorSettings, 6> mMotors{};
    };

    /** Constrains a body to a Hermite spline authored in body A's local joint frame, with optional drive along the path. */
    class PathJointComponent final : public JointComponent {
    public:
        explicit PathJointComponent() = default;

        PathJointComponent( const PathJointComponent& other ) = default;
        PathJointComponent( PathJointComponent&& other ) = default;

        auto operator=( const PathJointComponent& other ) -> PathJointComponent& = default;
        auto operator=( PathJointComponent&& other ) -> PathJointComponent& = default;

        auto SetControlPoints( const eastl::vector<JointPathPoint>& value ) -> void;
        MKT_NODISCARD auto GetControlPoints() const -> const eastl::vector<JointPathPoint>&;

        auto SetLooping( bool value ) -> void;
        MKT_NODISCARD auto IsLooping() const -> bool;

        auto SetInitialFraction( core::f32 value ) -> void;
        MKT_NODISCARD auto GetInitialFraction() const -> core::f32;

        auto SetMaxFrictionForce( core::f32 value ) -> void;
        MKT_NODISCARD auto GetMaxFrictionForce() const -> core::f32;

        auto SetMotor( const JointMotorSettings& value ) -> void;
        MKT_NODISCARD auto GetMotor() const -> const JointMotorSettings&;

        ~PathJointComponent() = default;

    private:
        eastl::vector<JointPathPoint> mControlPoints{};
        bool mLooping{ false };
        core::f32 mInitialFraction{ 0.0f };
        core::f32 mMaxFrictionForce{ 0.0f };
        JointMotorSettings mMotor{};
    };

    /** Couples two existing hinge joints by a rotation ratio; joint IDs reference the entities that own those hinges. */
    class GearJointComponent final : public JointComponent {
    public:
        explicit GearJointComponent() = default;

        GearJointComponent( const GearJointComponent& other ) = default;
        GearJointComponent( GearJointComponent&& other ) = default;

        auto operator=( const GearJointComponent& other ) -> GearJointComponent& = default;
        auto operator=( GearJointComponent&& other ) -> GearJointComponent& = default;

        auto SetHingeAEntityID( core::u64 value ) -> void;
        MKT_NODISCARD auto GetHingeAEntityID() const -> core::u64;

        auto SetHingeBEntityID( core::u64 value ) -> void;
        MKT_NODISCARD auto GetHingeBEntityID() const -> core::u64;

        auto SetRatio( core::f32 value ) -> void;
        MKT_NODISCARD auto GetRatio() const -> core::f32;

        ~GearJointComponent() = default;

    private:
        core::u64 mHingeAEntityID{};
        core::u64 mHingeBEntityID{};
        core::f32 mRatio{ 1.0f };
    };

    /** Couples an existing hinge and slider so rotational motion drives linear motion through a configured ratio. */
    class RackAndPinionJointComponent final : public JointComponent {
    public:
        explicit RackAndPinionJointComponent() = default;

        RackAndPinionJointComponent( const RackAndPinionJointComponent& other ) = default;
        RackAndPinionJointComponent( RackAndPinionJointComponent&& other ) = default;

        auto operator=( const RackAndPinionJointComponent& other ) -> RackAndPinionJointComponent& = default;
        auto operator=( RackAndPinionJointComponent&& other ) -> RackAndPinionJointComponent& = default;

        auto SetHingeEntityID( core::u64 value ) -> void;
        MKT_NODISCARD auto GetHingeEntityID() const -> core::u64;

        auto SetSliderEntityID( core::u64 value ) -> void;
        MKT_NODISCARD auto GetSliderEntityID() const -> core::u64;

        auto SetRatio( core::f32 value ) -> void;
        MKT_NODISCARD auto GetRatio() const -> core::f32;

        ~RackAndPinionJointComponent() = default;

    private:
        core::u64 mHingeEntityID{};
        core::u64 mSliderEntityID{};
        core::f32 mRatio{ 1.0f };
    };

    /** Links two body anchors through fixed world-space points, constraining the weighted total rope length. */
    class PulleyJointComponent final : public JointComponent {
    public:
        explicit PulleyJointComponent() = default;

        PulleyJointComponent( const PulleyJointComponent& other ) = default;
        PulleyJointComponent( PulleyJointComponent&& other ) = default;

        auto operator=( const PulleyJointComponent& other ) -> PulleyJointComponent& = default;
        auto operator=( PulleyJointComponent&& other ) -> PulleyJointComponent& = default;

        auto SetFixedPointA( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetFixedPointA() const -> const core::float3&;

        auto SetFixedPointB( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetFixedPointB() const -> const core::float3&;

        auto SetRatio( core::f32 value ) -> void;
        MKT_NODISCARD auto GetRatio() const -> core::f32;

        auto SetMinLength( core::f32 value ) -> void;
        MKT_NODISCARD auto GetMinLength() const -> core::f32;

        auto SetMaxLength( core::f32 value ) -> void;
        MKT_NODISCARD auto GetMaxLength() const -> core::f32;

        ~PulleyJointComponent() = default;

    private:
        core::float3 mFixedPointA{ 0.0f, 5.0f, 0.0f };
        core::float3 mFixedPointB{ 2.0f, 5.0f, 0.0f };
        core::f32 mRatio{ 1.0f };
        core::f32 mMinLength{ 0.0f };
        core::f32 mMaxLength{ 10.0f };
    };

    /** Describes a controllable capsule with slope, stair and pushing settings. Gameplay supplies movement and jump intent.
     * Half-height describes the cylindrical capsule section. PhysicsWorld owns the selected Jolt character and its lifetime. */
    class CharacterControllerComponent final : public core::RevisionTracked {
    public:
        /** World-space horizontal velocity and one-step jump speed, supplied by gameplay scripts. */
        core::float3 mDesiredVelocity{};
        core::f32 mJumpSpeed{};
        bool mGrounded{};
        explicit CharacterControllerComponent() = default;

        CharacterControllerComponent( const CharacterControllerComponent& other ) = default;
        CharacterControllerComponent( CharacterControllerComponent&& other ) = default;

        auto operator=( const CharacterControllerComponent& other ) -> CharacterControllerComponent& = default;
        auto operator=( CharacterControllerComponent&& other ) -> CharacterControllerComponent& = default;

        auto SetControllerType( CharacterControllerType value ) -> void;
        MKT_NODISCARD auto GetControllerType() const -> CharacterControllerType;

        auto SetRadius( core::f32 value ) -> void;
        MKT_NODISCARD auto GetRadius() const -> core::f32;

        auto SetHalfHeight( core::f32 value ) -> void;
        MKT_NODISCARD auto GetHalfHeight() const -> core::f32;

        auto SetMaxSlopeAngle( core::f32 value ) -> void;
        MKT_NODISCARD auto GetMaxSlopeAngle() const -> core::f32;

        auto SetMaxStepHeight( core::f32 value ) -> void;
        MKT_NODISCARD auto GetMaxStepHeight() const -> core::f32;

        auto SetMaxStrength( core::f32 value ) -> void;
        MKT_NODISCARD auto GetMaxStrength() const -> core::f32;

        auto SetGravityFactor( core::f32 value ) -> void;
        MKT_NODISCARD auto GetGravityFactor() const -> core::f32;

        auto SetStickToFloor( bool value ) -> void;
        MKT_NODISCARD auto DoesStickToFloor() const -> bool;

        auto SetEnabled( bool value ) -> void;
        MKT_NODISCARD auto IsEnabled() const -> bool;

        ~CharacterControllerComponent() = default;

    private:
        CharacterControllerType mControllerType{ CharacterControllerType::eVirtual };
        core::f32 mRadius{ 0.3f };
        core::f32 mHalfHeight{ 0.6f };
        core::f32 mMaxSlopeAngle{ 0.7853982f };
        core::f32 mMaxStepHeight{ 0.4f };
        core::f32 mMaxStrength{ 100.0f };
        core::f32 mGravityFactor{ 1.0f };
        bool mStickToFloor{ true };
        bool mEnabled{ true };
    };

    /** Maps skeleton bones to authored body/joint entities for ragdoll simulation and animation/physics pose blending.
     * SimulationBlend is an authoring weight: 0 uses animation, 1 uses the simulated pose. */
    class RagdollComponent final : public core::RevisionTracked {
    public:
        explicit RagdollComponent() = default;

        RagdollComponent( const RagdollComponent& other ) = default;
        RagdollComponent( RagdollComponent&& other ) = default;

        auto operator=( const RagdollComponent& other ) -> RagdollComponent& = default;
        auto operator=( RagdollComponent&& other ) -> RagdollComponent& = default;

        auto SetSkeleton( const asset::AssetRef<asset::Skeleton>& value ) -> void;
        MKT_NODISCARD auto GetSkeleton() const -> const asset::AssetRef<asset::Skeleton>&;

        auto SetBoneMappings( const eastl::vector<RagdollBoneDescription>& value ) -> void;
        MKT_NODISCARD auto GetBoneMappings() const -> const eastl::vector<RagdollBoneDescription>&;

        auto SetSimulationBlend( core::f32 value ) -> void;
        MKT_NODISCARD auto GetSimulationBlend() const -> core::f32;

        auto SetEnabled( bool value ) -> void;
        MKT_NODISCARD auto IsEnabled() const -> bool;

        ~RagdollComponent() = default;

    private:
        asset::AssetRef<asset::Skeleton> mSkeleton{};
        eastl::vector<RagdollBoneDescription> mBoneMappings{};
        core::f32 mSimulationBlend{ 1.0f };
        bool mEnabled{ true };
    };

    /** Describes a deformable mesh, vertex masses, pinned vertices and optional internal pressure.
     * PhysicsWorld creates Jolt vertex constraints and publishes instance-local deformation to the renderer. */
    class SoftBodyComponent : public core::RevisionTracked {
    public:
        explicit SoftBodyComponent() = default;

        SoftBodyComponent( const SoftBodyComponent& other ) = default;
        SoftBodyComponent( SoftBodyComponent&& other ) = default;

        auto operator=( const SoftBodyComponent& other ) -> SoftBodyComponent& = default;
        auto operator=( SoftBodyComponent&& other ) -> SoftBodyComponent& = default;

        auto SetModel( const asset::AssetRef<asset::Model>& value ) -> void;
        MKT_NODISCARD auto GetModel() const -> const asset::AssetRef<asset::Model>&;

        auto SetMeshIndex( core::u32 value ) -> void;
        MKT_NODISCARD auto GetMeshIndex() const -> core::u32;

        auto SetVertexMass( core::f32 value ) -> void;
        MKT_NODISCARD auto GetVertexMass() const -> core::f32;

        auto SetGravityFactor( core::f32 value ) -> void;
        MKT_NODISCARD auto GetGravityFactor() const -> core::f32;

        auto SetLinearDamping( core::f32 value ) -> void;
        MKT_NODISCARD auto GetLinearDamping() const -> core::f32;

        auto SetPressure( core::f32 value ) -> void;
        MKT_NODISCARD auto GetPressure() const -> core::f32;

        auto SetEdgeCompliance( core::f32 value ) -> void;
        MKT_NODISCARD auto GetEdgeCompliance() const -> core::f32;

        auto SetPinnedVertices( const eastl::vector<core::u32>& value ) -> void;
        MKT_NODISCARD auto GetPinnedVertices() const -> const eastl::vector<core::u32>&;

        auto SetEnabled( bool value ) -> void;
        MKT_NODISCARD auto IsEnabled() const -> bool;

        ~SoftBodyComponent() = default;

    private:
        asset::AssetRef<asset::Model> mModel{};
        core::u32 mMeshIndex{};
        core::f32 mVertexMass{ 1.0f };
        core::f32 mGravityFactor{ 1.0f };
        core::f32 mLinearDamping{ 0.05f };
        core::f32 mPressure{ 0.0f };
        core::f32 mEdgeCompliance{ 0.0f };
        eastl::vector<core::u32> mPinnedVertices{};
        bool mEnabled{ true };
    };

    /** Runtime geometry owned by one deformable instance. Never serialized or copied. */
    struct DeformedMeshComponent {
        core::Ref<renderer::MeshRenderData> mMesh{};
    };

    /** Specializes soft-body authoring for fabric with bending compliance.
     * Drag/lift values are reserved: the bundled Jolt API does not simulate cloth aerodynamics.
     * Pinned vertex indices and source mesh settings are inherited from SoftBodyComponent. */
    class ClothComponent final : public SoftBodyComponent {
    public:
        explicit ClothComponent() = default;

        ClothComponent( const ClothComponent& other ) = default;
        ClothComponent( ClothComponent&& other ) = default;

        auto operator=( const ClothComponent& other ) -> ClothComponent& = default;
        auto operator=( ClothComponent&& other ) -> ClothComponent& = default;

        auto SetBendCompliance( core::f32 value ) -> void;
        MKT_NODISCARD auto GetBendCompliance() const -> core::f32;

        auto SetDrag( core::f32 value ) -> void;
        MKT_NODISCARD auto GetDrag() const -> core::f32;

        auto SetLift( core::f32 value ) -> void;
        MKT_NODISCARD auto GetLift() const -> core::f32;

        ~ClothComponent() = default;

    private:
        core::f32 mBendCompliance{ 0.00001f };
        core::f32 mDrag{ 0.1f };
        core::f32 mLift{ 0.1f };
    };

    /** Describes how a rigid body responds to a referenced water volume through buoyancy and linear/angular drag. */
    class BuoyancyComponent final : public core::RevisionTracked {
    public:
        explicit BuoyancyComponent() = default;

        BuoyancyComponent( const BuoyancyComponent& other ) = default;
        BuoyancyComponent( BuoyancyComponent&& other ) = default;

        auto operator=( const BuoyancyComponent& other ) -> BuoyancyComponent& = default;
        auto operator=( BuoyancyComponent&& other ) -> BuoyancyComponent& = default;

        auto SetWaterVolumeEntityID( core::u64 value ) -> void;
        MKT_NODISCARD auto GetWaterVolumeEntityID() const -> core::u64;

        auto SetBuoyancyFactor( core::f32 value ) -> void;
        MKT_NODISCARD auto GetBuoyancyFactor() const -> core::f32;

        auto SetLinearDrag( core::f32 value ) -> void;
        MKT_NODISCARD auto GetLinearDrag() const -> core::f32;

        auto SetAngularDrag( core::f32 value ) -> void;
        MKT_NODISCARD auto GetAngularDrag() const -> core::f32;

        auto SetEnabled( bool value ) -> void;
        MKT_NODISCARD auto IsEnabled() const -> bool;

        ~BuoyancyComponent() = default;

    private:
        core::u64 mWaterVolumeEntityID{};
        core::f32 mBuoyancyFactor{ 1.0f };
        core::f32 mLinearDrag{ 0.1f };
        core::f32 mAngularDrag{ 0.05f };
        bool mEnabled{ true };
    };

    /** Describes a water region, density, flow and optional water-surface entity for Jolt buoyancy queries.
     * The entity transform places the volume; SurfaceOffset is its local water level when no surface entity is assigned. */
    class WaterVolumeComponent final : public core::RevisionTracked {
    public:
        explicit WaterVolumeComponent() = default;

        WaterVolumeComponent( const WaterVolumeComponent& other ) = default;
        WaterVolumeComponent( WaterVolumeComponent&& other ) = default;

        auto operator=( const WaterVolumeComponent& other ) -> WaterVolumeComponent& = default;
        auto operator=( WaterVolumeComponent&& other ) -> WaterVolumeComponent& = default;

        auto SetHalfExtents( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetHalfExtents() const -> const core::float3&;

        auto SetSurfaceOffset( core::f32 value ) -> void;
        MKT_NODISCARD auto GetSurfaceOffset() const -> core::f32;

        auto SetDensity( core::f32 value ) -> void;
        MKT_NODISCARD auto GetDensity() const -> core::f32;

        auto SetFlowVelocity( const core::float3& value ) -> void;
        MKT_NODISCARD auto GetFlowVelocity() const -> const core::float3&;

        auto SetWaterSurfaceEntityID( core::u64 value ) -> void;
        MKT_NODISCARD auto GetWaterSurfaceEntityID() const -> core::u64;

        auto SetEnabled( bool value ) -> void;
        MKT_NODISCARD auto IsEnabled() const -> bool;

        ~WaterVolumeComponent() = default;

    private:
        core::float3 mHalfExtents{ 50.0f, 5.0f, 50.0f };
        core::f32 mSurfaceOffset{ 0.0f };
        core::f32 mDensity{ 1000.0f };
        core::float3 mFlowVelocity{ 0.0f };
        core::u64 mWaterSurfaceEntityID{};
        bool mEnabled{ true };
    };


    enum class CloudShape : core::u32 { eSkyLayer, eBox, eSphere };
    enum class FogShape : core::u32 { eGlobal, eBox, eSphere };

    // Shapes delimit a medium without adding a visible/refractive surface.
    // Density and optical properties belong to the shared VolumeMaterial asset.
    class CloudComponent {
    public:
        CloudComponent();

        CloudComponent( const CloudComponent& other ) = default;
        CloudComponent( CloudComponent&& other ) = default;

        auto operator=( const CloudComponent& other ) -> CloudComponent& = default;
        auto operator=( CloudComponent&& other ) -> CloudComponent& = default;

        MKT_NODISCARD auto GetShape() const -> CloudShape;
        MKT_NODISCARD auto GetBottomAltitude() const -> core::f32;
        MKT_NODISCARD auto GetLayerThickness() const -> core::f32;
        MKT_NODISCARD auto GetDimensions() const -> const core::float3&;
        MKT_NODISCARD auto GetHorizonRadius() const -> core::f32;
        MKT_NODISCARD auto GetMaterial() const -> material::VolumeMaterialHandle;
        MKT_NODISCARD auto GetDensityMultiplier() const -> core::f32;
        MKT_NODISCARD auto GetWindDirection() const -> const core::float2&;
        MKT_NODISCARD auto GetWindSpeed() const -> core::f32;
        MKT_NODISCARD auto IsShadowCaster() const -> bool;
        MKT_NODISCARD auto IsShadowReceiver() const -> bool;

        auto SetShape( CloudShape shape ) -> void;
        auto SetBottomAltitude( core::f32 altitude ) -> void;
        auto SetLayerThickness( core::f32 thickness ) -> void;
        auto SetDimensions( const core::float3& dimensions ) -> void;
        auto SetHorizonRadius( core::f32 radius ) -> void;
        auto SetMaterial( material::VolumeMaterialHandle material ) -> void;
        auto SetDensityMultiplier( core::f32 multiplier ) -> void;
        auto SetWindDirection( const core::float2& direction ) -> void;
        auto SetWindSpeed( core::f32 speed ) -> void;
        auto SetCastShadows( bool value ) -> void;
        auto SetReceiveShadows( bool value ) -> void;

        ~CloudComponent() = default;

    private:
        CloudShape mShape{ CloudShape::eSkyLayer };
        core::f32 mBottomAltitude{ 1500.0f };
        core::f32 mLayerThickness{ 4000.0f };
        core::float3 mDimensions{ 1000.0f };
        core::f32 mHorizonRadius{ 100000.0f };
        material::VolumeMaterialHandle mMaterial{};
        core::f32 mDensityMultiplier{ 1.0f };
        core::float2 mWindDirection{ 1.0f, 0.0f };
        core::f32 mWindSpeed{ 10.0f };
        bool mCastShadows{ true };
        bool mReceiveShadows{ true };
    };

    class FogComponent {
    public:
        FogComponent();

        FogComponent( const FogComponent& other ) = default;
        FogComponent( FogComponent&& other ) = default;

        auto operator=( const FogComponent& other ) -> FogComponent& = default;
        auto operator=( FogComponent&& other ) -> FogComponent& = default;

        MKT_NODISCARD auto GetShape() const -> FogShape;
        MKT_NODISCARD auto GetDimensions() const -> const core::float3&;
        MKT_NODISCARD auto GetHorizonRadius() const -> core::f32;
        MKT_NODISCARD auto GetMaterial() const -> material::VolumeMaterialHandle;
        MKT_NODISCARD auto GetDensityMultiplier() const -> core::f32;
        MKT_NODISCARD auto IsShadowCaster() const -> bool;
        MKT_NODISCARD auto IsShadowReceiver() const -> bool;

        auto SetShape( FogShape shape ) -> void;
        auto SetDimensions( const core::float3& dimensions ) -> void;
        auto SetHorizonRadius( core::f32 radius ) -> void;
        auto SetMaterial( material::VolumeMaterialHandle material ) -> void;
        auto SetDensityMultiplier( core::f32 multiplier ) -> void;
        auto SetCastShadows( bool value ) -> void;
        auto SetReceiveShadows( bool value ) -> void;

        ~FogComponent() = default;

    private:
        FogShape mShape{ FogShape::eGlobal };
        core::float3 mDimensions{ 100.0f };
        // A finite world-space sphere bounds global haze and infinite-light connections.
        core::f32 mHorizonRadius{ 100000.0f };
        material::VolumeMaterialHandle mMaterial{};
        core::f32 mDensityMultiplier{ 1.0f };
        bool mCastShadows{ true };
        bool mReceiveShadows{ true };
    };

    enum class CameraClearFlags {
        eClearColor,
        eBlurredSkybox,
        eSkybox,
        eCount,
    };


    class CameraComponent {
    public:
        explicit CameraComponent( platform::IWindow* window = nullptr );

        CameraComponent( const CameraComponent& other ) = default;
        CameraComponent( CameraComponent&& other ) = default;

        auto operator=( const CameraComponent& other ) -> CameraComponent& = default;
        auto operator=( CameraComponent&& other ) -> CameraComponent& = default;

        MKT_NODISCARD auto HasCamera() const -> bool;
        MKT_NODISCARD auto IsMainCamera() const -> bool;
        auto SetMainCamera( bool main ) -> void;

        MKT_NODISCARD auto GetCamera() -> SceneCamera&;
        MKT_NODISCARD auto GetCamera() const -> const SceneCamera&;

        MKT_NODISCARD auto IsAspectRatioFixed() const -> bool;

        MKT_NODISCARD auto GetClearFlags() const -> CameraClearFlags;

        MKT_NODISCARD auto GetClearColor() const -> const renderer::rhi::Color&;

        MKT_NODISCARD auto GetGamma() const -> float;
        MKT_NODISCARD auto GetExposure() const -> float;
        MKT_NODISCARD auto GetSkyboxBlur() const -> float;

        auto SetGamma( float gamma ) -> void;
        auto SetSkyboxBlur( float value ) -> void;
        auto SetExposure( float exposure ) -> void;
        auto SetFixedAspectRatio( bool value ) -> void;
        auto OnViewportResize( core::f32 width, core::f32 height ) -> void;
        auto SetClearFlags( CameraClearFlags bg ) -> void;
        auto SetClearColor( const renderer::rhi::Color& color ) -> void;

        ~CameraComponent() = default;

    private:
        core::Ref<scene::SceneCamera> mCamera{};
        CameraClearFlags mClearFlags{ CameraClearFlags::eClearColor };

        renderer::rhi::Color mColor{ renderer::rhi::kColorGray };

        float mGamma{ 2.0f };
        float mExposure{ 2.0f };
        float mSkyboxBlur{ 0.3f };

        bool mMainCam{ true };
        bool mFixedAspectRatio{ false };
    };


    enum class TextAlignment {
        eLeft,
        eCenter,
        eRight
    };

    enum class TextBackend {
        eMSDF,
        eSlug
    };

    class TextComponent {
    public:
        explicit TextComponent() = default;

        TextComponent( eastl::string_view textContent, float size, float spacing, bool isWorld );

        TextComponent( const TextComponent& other ) = default;
        TextComponent( TextComponent&& other ) = default;

        auto operator=( const TextComponent& other ) -> TextComponent& = default;
        auto operator=( TextComponent&& other ) -> TextComponent& = default;

        auto SetFont( renderer::FontHandle font ) -> void;

        auto SetCamera( const Camera* camera ) -> void;

        auto SetIsWorldText( bool value ) -> void;

        auto SetOutlineWidth( core::f32 width ) -> void;
        auto SetOutlineColor( const core::float4& color ) -> void;
        auto SetMaterial( core::Ref<material::TextMaterial> material ) -> void;
        
        auto SetBackend( TextBackend value ) -> void;
        auto SetAlignment( TextAlignment value ) -> void;
        auto SetLineSpacing( float value ) -> void;

        auto SetSize( float value ) -> void;
        auto SetSize( const float value, bool world ) -> void;
        auto SetSpacing( float value ) -> void;
        auto SetSpacing( const float value, bool world ) -> void;

        auto SetContents( const eastl::string_view content ) -> void;

        template<typename... Args>
        auto SetColor( Args &&...args ) -> void {
            MKT_BEGIN_PROFILER();
            mColor = core::float4{ eastl::forward<Args>( args )... };
        }

        MKT_NODISCARD auto HasFont() const -> bool;
        MKT_NODISCARD auto IsWorldText() const -> bool;
        MKT_NODISCARD auto GetCamera() const -> const Camera*;

        MKT_NODISCARD auto GetFont() const -> const renderer::Font*;
        MKT_NODISCARD auto GetFontHandle() const -> renderer::FontHandle;
        MKT_NODISCARD auto GetColor() const -> const core::float4&;

        MKT_NODISCARD auto GetMaterial() const -> core::Ref<material::TextMaterial>;

        MKT_NODISCARD auto GetRenderColor() const -> core::float4;

        MKT_NODISCARD auto GetSize() const -> float;
        MKT_NODISCARD auto GetSpacing() const -> float;
        MKT_NODISCARD auto GetSize( bool world ) const -> float;
        MKT_NODISCARD auto GetSpacing( bool world ) const -> float;
        MKT_NODISCARD auto GetScreenPlacement() -> text::ScreenPlacement &;
        MKT_NODISCARD auto GetScreenPlacement() const -> const text::ScreenPlacement &;
        MKT_NODISCARD auto GetLineSpacing() const -> float;
        MKT_NODISCARD auto GetAlignment() const -> TextAlignment;
        MKT_NODISCARD auto GetBackend() const -> TextBackend;
        MKT_NODISCARD auto GetOutlineWidth() const -> core::f32;
        MKT_NODISCARD auto GetOutlineColor() const -> const core::float4&;

        MKT_NODISCARD auto GetContents() const -> const eastl::string&;
        MKT_NODISCARD auto GetUnicodeContents() const -> const text::UnicodeString&;

        MKT_NODISCARD static auto GetMinLetterSpacing() -> float;
        MKT_NODISCARD static auto GetMaxLetterSpacing() -> float;

        MKT_NODISCARD static auto GetMinLetterSize() -> float;
        MKT_NODISCARD static auto GetMaxLetterSize() -> float;

    private:
        // Preserve UTF-8 for editing/serialization and decode only on edits.
        eastl::string mTextContent{};
        text::UnicodeString mStringContent{};

        core::float4 mColor{ 1.0f, 1.0f, 0.4f, 1.0f };
        core::Ref<material::TextMaterial> mMaterial{};

        float mWorldSize{ 1.0f };

        float mScreenSize{ 32.0f };
        float mWorldSpacing{};
        float mScreenSpacing{};
        text::ScreenPlacement mScreenPlacement{};
        float mLineSpacing{ 1.0f };
        TextAlignment mAlignment{ TextAlignment::eLeft };
        TextBackend mBackend{ TextBackend::eSlug };
        core::f32 mOutlineWidth{};
        core::float4 mOutlineColor{ 0, 0, 0, 1 };

        renderer::FontHandle mFont{};
        const Camera* mCamera{ nullptr };

        bool mIsWorldText{ false };
    };


    // TODO: Support for multiple scripts
    class ScriptComponent {
    public:
        // The script can be constructed from a script on disk
        // if left empty we will use a blank script with minimal setup
        explicit ScriptComponent( const filesystem::Path& filePath = {},
                                  scripting::ScriptLanguage language = scripting::ScriptLanguage::eLua,
                                  eastl::string_view className = {} );

        ScriptComponent( const ScriptComponent& other ) = default;
        ScriptComponent( ScriptComponent&& other ) = default;

        auto operator=( const ScriptComponent& other ) -> ScriptComponent& = default;
        auto operator=( ScriptComponent&& other ) -> ScriptComponent& = default;

        /** @brief Adopts a loaded script without executing its initializer again. */
        auto SetScript( scripting::ScriptHandle handle ) -> void;
        auto SetLanguage( scripting::ScriptLanguage language ) -> void;
        auto SetManagedScript( const filesystem::Path& assembly, eastl::string_view className ) -> void;
        MKT_NODISCARD auto GetLanguage() const -> scripting::ScriptLanguage;
        MKT_NODISCARD auto GetClassName() const -> const eastl::string&;

        MKT_NODISCARD auto GetHandle() -> scripting::ScriptHandle;
        MKT_NODISCARD auto GetFilePath() const -> const filesystem::Path&;

        /** @brief Sets per-entity serialized property overrides, keyed by member name.
         * @param properties JSON object copied into the component and persisted in the scene.
         */
        auto SetProperties( eastl::string_view properties ) -> void;
        MKT_NODISCARD auto GetProperties() const -> const eastl::string&;

        ~ScriptComponent() = default;

    private:
        filesystem::Path mFilePath{};
        scripting::ScriptHandle mScript{};
        scripting::ScriptLanguage mLanguage{ scripting::ScriptLanguage::eLua };
        eastl::string mClassName{};
        eastl::string mProperties{ "{}" };
    };


    // Authored clip data is copied/serialized. Playback and preview snapshots
    // belong to this scene instance, never to a shared imported model asset.
    class KeyframeAnimationComponent {
    public:
        KeyframeAnimationComponent() = default;
        KeyframeAnimationComponent( const KeyframeAnimationComponent& other );
        auto operator=( const KeyframeAnimationComponent& other ) -> KeyframeAnimationComponent&;
        KeyframeAnimationComponent( KeyframeAnimationComponent&& ) noexcept = default;
        auto operator=( KeyframeAnimationComponent&& ) noexcept -> KeyframeAnimationComponent& = default;

        auto Play( Entity& entity, bool preview ) -> void;
        auto Pause() -> void;
        auto Stop( Entity& entity ) -> void;
        auto Seek( Entity& entity, core::f64 frame ) -> void;
        auto Update( Entity& entity, core::f64 seconds ) -> void;
        auto Apply( Entity& entity ) const -> void;
        MKT_NODISCARD auto GetFrame() const -> core::f64;
        MKT_NODISCARD auto IsPlaying() const -> bool;
        MKT_NODISCARD auto IsPreviewing() const -> bool;
        MKT_NODISCARD auto GetRestTransform() const -> const core::float4x4&;
        auto RestoreLightProperties( LightComponent& light ) const -> void;
        MKT_NODISCARD static auto ReadProperty( const Entity& entity, animation::KeyframeChannel channel ) -> core::float3;

        animation::KeyframeClip mClip{};

    private:
        auto CaptureRest( const Entity& entity ) -> void;
        core::float4x4 mRestTransform{ 1.0f };
        std::array<core::float3, 3> mRestLightColors{};
        std::array<core::f32, 3> mRestLightIntensities{};
        core::f64 mFrame{};
        bool mPlaying{};
        bool mPreviewing{};
        bool mHasSnapshot{};
        bool mHadLight{};
    };


    class AnimatorComponent {
    public:
        explicit AnimatorComponent( core::u64 ID = 0);

        AnimatorComponent( const AnimatorComponent& other ) = default;
        AnimatorComponent( AnimatorComponent&& other ) = default;

        auto operator=( const AnimatorComponent& other ) -> AnimatorComponent& = default;
        auto operator=( AnimatorComponent&& other ) -> AnimatorComponent& = default;

        ~AnimatorComponent() = default;

        auto SetAnimatorID( core::u64 ID ) -> void;
        MKT_NODISCARD auto GetAnimatorID() const -> core::u64;

    private:
        core::u64 mAnimatorID{};
    };


    class SkinnedMeshRenderer {
    public:
        explicit SkinnedMeshRenderer( core::u64 ID = 0);

        SkinnedMeshRenderer( const SkinnedMeshRenderer& other ) = default;
        SkinnedMeshRenderer( SkinnedMeshRenderer&& other ) = default;

        auto operator=( const SkinnedMeshRenderer& other ) -> SkinnedMeshRenderer& = default;
        auto operator=( SkinnedMeshRenderer&& other ) -> SkinnedMeshRenderer& = default;

        ~SkinnedMeshRenderer() = default;

        /** @brief Selects an animator using the same imported model/rig; zero detaches playback.
         * @param id Animator handle validated by the caller against the mesh's model.
         */
        auto SetAnimatorID( core::u64 id ) -> void;
        MKT_NODISCARD auto GetAnimatorID() const -> core::u64;

    private:
        core::u64 mAnimatorID{};
    };


    class PostProcessMaterialComponent {
    public:
        /** @brief Assigns a post-process material, or creates a default when empty.
         * @param material Shared post-process material for this entity.
         */
        explicit PostProcessMaterialComponent( material::MaterialHandle material = {} );

        PostProcessMaterialComponent( const PostProcessMaterialComponent& other ) = default;
        PostProcessMaterialComponent( PostProcessMaterialComponent&& other ) = default;

        auto operator=( const PostProcessMaterialComponent& other ) -> PostProcessMaterialComponent& = default;
        auto operator=( PostProcessMaterialComponent&& other ) -> PostProcessMaterialComponent& = default;

        /** @brief Gets the material submitted by this component.
         * @returns A shared handle to the assigned post-process material.
         */
        MKT_NODISCARD auto GetMaterial() const -> material::MaterialHandle;

        /** @brief Assigns the material submitted by this component.
         * @param material Nonempty PostProcessMaterial handle.
         * @throws core::InvalidArgument If material is empty or has another material type.
         */
        auto SetMaterial( material::MaterialHandle material ) -> void;

        /** Camera entity supplying grain parameters; zero leaves grain unapplied. */
        auto SetCameraEntityID( core::u64 id ) -> void;
        MKT_NODISCARD auto GetCameraEntityID() const -> core::u64;

        ~PostProcessMaterialComponent() = default;

    private:
        material::MaterialHandle mMaterial{};
        core::u64 mCameraEntityID{};
    };


    class SkyboxMaterialComponent {
    public:
        explicit SkyboxMaterialComponent() = default;

        SkyboxMaterialComponent( const SkyboxMaterialComponent& other ) = default;
        SkyboxMaterialComponent( SkyboxMaterialComponent&& other ) = default;

        auto operator=( const SkyboxMaterialComponent& other ) -> SkyboxMaterialComponent& = default;
        auto operator=( SkyboxMaterialComponent&& other ) -> SkyboxMaterialComponent& = default;

        MKT_NODISCARD auto GetMaterial() const -> material::MaterialHandle;

        auto SetMaterial( material::MaterialHandle material ) -> void;

        ~SkyboxMaterialComponent() = default;

    private:
        material::MaterialHandle mMaterial{};
    };


    class ParticleEmitterComponent {
    public:
        explicit ParticleEmitterComponent() = default;

        ParticleEmitterComponent( const ParticleEmitterComponent& other ) = default;
        ParticleEmitterComponent( ParticleEmitterComponent&& other ) = default;

        auto operator=( const ParticleEmitterComponent& other ) -> ParticleEmitterComponent& = default;
        auto operator=( ParticleEmitterComponent&& other ) -> ParticleEmitterComponent& = default;

        auto SetMaterial( material::MaterialHandle particleMaterial ) -> void;

        MKT_NODISCARD auto GetMaterial() const -> material::MaterialHandle;
        MKT_NODISCARD auto GetInstance() -> renderer::ParticleEmitterInstance&;
        MKT_NODISCARD auto GetInstance() const -> const renderer::ParticleEmitterInstance&;

        ~ParticleEmitterComponent() = default;
    private:
        material::MaterialHandle mParticleMaterial{};
        renderer::ParticleEmitterInstance mInstance{};
    };


    class CanvasComponent {
    public:
        explicit CanvasComponent( const scene::Camera* camera = nullptr );

        CanvasComponent( const CanvasComponent& other ) = default;
        CanvasComponent( CanvasComponent&& other ) = default;

        auto operator=( const CanvasComponent& other ) -> CanvasComponent& = default;
        auto operator=( CanvasComponent&& other ) -> CanvasComponent& = default;

        ~CanvasComponent() = default;

        auto GetCanvas() -> renderer::Canvas&;
        auto GetCanvas() const -> const renderer::Canvas&;

    private:
        renderer::Canvas mCanvas{};
    };


    enum class WaterShape {
        ePlane,
        eCustom,
    };

    // How is water rendered
    class WaterRenderComponent : public core::RevisionTracked {
    public:
        explicit WaterRenderComponent();

        WaterRenderComponent( const WaterRenderComponent& ) = default;
        WaterRenderComponent( WaterRenderComponent&& ) = default;

        auto operator=( const WaterRenderComponent& ) -> WaterRenderComponent& = default;
        auto operator=( WaterRenderComponent&& ) -> WaterRenderComponent& = default;

        auto SetShape( WaterShape shape ) -> void;
        auto SetMaterial( material::MaterialHandle waterMaterial ) -> void;
        MKT_NODISCARD auto GetMaterial() const -> core::Ref<material::SurfaceMaterial>;

        // Reset appearance without changing the plane's dimensions or transform.
        auto ApplyOceanPreset() -> void;
        auto SetFoamAmount( float amount ) -> void;
        MKT_NODISCARD auto GetFoamAmount() const -> float;

        auto SetPlaneResolution( core::float2 resolution ) -> void;
        auto SetPlaneDimensions( core::float2 dimensions ) -> void;

        auto SetShowGrid( bool show ) -> void;
        auto SetShowVertices( bool show ) -> void;

        // Shared tint for the grid and vertex debug overlays.
        auto SetDebugPreviewColor( core::float3 color ) -> void;
        MKT_NODISCARD auto GetDebugPreviewColor() const -> core::float3;

        MKT_NODISCARD auto IsGridVisible() const -> bool;
        MKT_NODISCARD auto AreVerticesVisible() const -> bool;

        MKT_NODISCARD auto GetShape() const -> WaterShape;
        MKT_NODISCARD auto GetPlaneDimensions() const -> core::float2;
        MKT_NODISCARD auto GetPlaneResolution() const -> core::float2;

        ~WaterRenderComponent() = default;

    private:
        WaterShape mShape{ WaterShape::ePlane };

        // Physical width/depth, independent of the number of grid cells.
        core::float2 mDimensions{ 100.0f, 100.0f };
        // Integral cell counts along X and Z.
        core::float2 mResolution{ 256, 256 };
        bool mShowGrid{ false };
        bool mShowVertices{ false };
        core::float3 mDebugPreviewColor{ 1.0f, 1.0f, 1.0f };
        float mFoamAmount{ 0.65f };

        // TODO: Add mesh asset handle for eCustom.
        material::MaterialHandle mWaterMaterial{};
    };

    enum class WaterSimulationType : core::u32 {
        eSumOfSines,
        eGerstner,
        eFastFourierTransform,
    };

    class WaterSimulationComponent {
    public:
        static constexpr core::usize kMaxWaves{ 64 };

        struct FftSettings {
            static constexpr core::f32 kMinPatchLength{ 8.0f };
            static constexpr core::f32 kMaxPatchLength{ 4096.0f };
            static constexpr core::f32 kMaxWindSpeed{ 50.0f };
            static constexpr core::f32 kMaxAmplitude{ 5.0f };
            static constexpr core::f32 kMaxChoppiness{ 2.0f };

            core::f32 mPatchLength{ 128.0f };
            core::f32 mWindSpeed{ 12.0f };
            core::f32 mWindDirection{ 0.0f }; // Degrees from local +X toward +Z.
            core::f32 mAmplitude{ 1.0f };
            core::f32 mChoppiness{ 1.0f };
            core::u32 mSeed{ 1 };
        };

        struct WaterWave {
            core::float2 mDirection{ 1.0f, 0.0f };

            float mAmplitude{ 0.5f };
            float mWavelength{ 10.0f };
            float mSpeed{ 1.0f };
            float mPhaseOffset{ 0.0f };
        };

    public:
        explicit WaterSimulationComponent();

        WaterSimulationComponent( const WaterSimulationComponent& ) = default;
        WaterSimulationComponent( WaterSimulationComponent&& ) = default;

        auto operator=( const WaterSimulationComponent& ) -> WaterSimulationComponent& = default;
        auto operator=( WaterSimulationComponent&& ) -> WaterSimulationComponent& = default;

        auto SetType( WaterSimulationType type ) -> void;
        auto ApplyOceanPreset() -> void;
        auto SetWaves( eastl::vector<WaterWave> waves ) -> void;
        auto AddWave( const WaterWave& wave ) -> void;
        auto RemoveWave( core::usize index ) -> bool;
        auto ClearWaves() -> void;
        auto SetTimeScale( float timeScale ) -> void;
        auto SetSteepness( float steepness ) -> void;
        auto SetFftSettings( const FftSettings& settings ) -> void;

        // Scene update advances time once, independently of camera/render count.
        auto Update( core::f32 timeStep ) -> void;
        auto ResetTime() -> void;

        MKT_NODISCARD auto GetType() const -> WaterSimulationType;
        MKT_NODISCARD auto GetWaves() const -> const eastl::vector<WaterWave>&;
        MKT_NODISCARD auto GetWaveCount() const -> core::usize;
        MKT_NODISCARD auto GetTimeScale() const -> float;
        MKT_NODISCARD auto GetSteepness() const -> float;
        MKT_NODISCARD auto GetTime() const -> core::f64;
        MKT_NODISCARD auto GetFftSettings() const -> const FftSettings&;

    private:
        core::f32 mTimeScale{ 1.0f };
        core::f32 mSteepness{ 0.5f };
        core::f64 mTime{};

        static constexpr WaterSimulationType kDefaultType{ WaterSimulationType::eFastFourierTransform };

        eastl::vector<WaterWave> mWaves{};
        FftSettings mFftSettings{};
        WaterSimulationType mType{ kDefaultType };
    };


    enum class LandscapeBrushType {
        eDefault,
    };

    class LandscapeComponent : public core::RevisionTracked {
    public:
        explicit LandscapeComponent() = default;

        LandscapeComponent( const LandscapeComponent& other ) = default;
        LandscapeComponent( LandscapeComponent&& other ) = default;

        auto operator=( const LandscapeComponent& other ) -> LandscapeComponent& = default;
        auto operator=( LandscapeComponent&& other ) -> LandscapeComponent& = default;

        auto SetOrigin( core::float2 origin ) -> void;
        auto SetHeightScale( core::f32 scale ) -> void;
        
        auto SetLowColor( core::float3 color ) -> void;
        auto SetHighColor( core::float3 color ) -> void;
        auto SetMiddleColor( core::float3 color ) -> void;
        
        auto SetEnableEditLayers( bool enabled ) -> void;
        auto SetTerrainSection( core::u32 quads ) -> void;

        auto SetVertexSpacing( core::f32 spacing ) -> void;
        auto SetBaseElevation( core::f32 elevation ) -> void;
        auto SetSectionsPerComponent( core::u32 sections ) -> void;
        auto SetComponentCount( core::u32 x, core::u32 y ) -> void;
        
        auto SetGeneratedNoise( const renderer::TerrainNoiseSettings &settings ) -> void;

        auto SetShowGrid( bool show ) -> void;
        auto SetShowVertices( bool show ) -> void;

        auto ClearHeightmap() -> void;
        auto SetHeightmap( const asset::AssetRef<asset::Texture> &reference ) -> void;

        MKT_NODISCARD auto GetBrushType() const -> LandscapeBrushType;
        MKT_NODISCARD auto GetWorldDescription() const -> renderer::WorldDescription;

        MKT_NODISCARD auto GetHeightmapError() const -> const eastl::string&;
        MKT_NODISCARD auto GetHeightmapReference() const -> asset::AssetRef<asset::Texture>;
        MKT_NODISCARD auto GetHeightmap() const -> eastl::shared_ptr<const renderer::TerrainHeightmap>;

        MKT_NODISCARD auto IsGridVisible() const -> bool;
        MKT_NODISCARD auto AreVerticesVisible() const -> bool;
        MKT_NODISCARD auto GetNoiseSettings() const -> renderer::TerrainNoiseSettings;
        MKT_NODISCARD auto GetNoiseGeneration() const -> eastl::shared_ptr<renderer::TerrainNoiseGeneration>;


    private:
        auto ApplyDescription( const renderer::WorldDescription& description ) -> void;
        renderer::WorldDescription mWorldDescription{};

    private:
        material::MaterialHandle mTerrainMaterial{};

        // Heightmap
        eastl::string mHeightMapError{};
        renderer::TerrainNoiseSettings mNoiseSettings{};
        asset::AssetRef<asset::Texture> mHeightMapRef{};

        LandscapeBrushType mBrushType{ LandscapeBrushType::eDefault };

        eastl::shared_ptr<const renderer::TerrainHeightmap> mHeightMap{};
        eastl::shared_ptr<renderer::TerrainNoiseGeneration> mNoiseGeneration{};

        bool mShowGrid{};
        bool mShowVertices{};
    };

    class WindSimulationComponent {
    public:
        explicit WindSimulationComponent() = default;

        WindSimulationComponent( const WindSimulationComponent& other ) = default;
        WindSimulationComponent( WindSimulationComponent&& other ) = default;

        auto operator=( const WindSimulationComponent& other ) -> WindSimulationComponent& = default;
        auto operator=( WindSimulationComponent&& other ) -> WindSimulationComponent& = default;

        ~WindSimulationComponent() = default;

    private:

    }; 

}// namespace mikoto::scene

#endif// MIKOTO_SCENE_COMPONENT_HH
