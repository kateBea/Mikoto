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

#ifndef MIKOTO_COMPONENT_HH
#define MIKOTO_COMPONENT_HH

#include <optional>
#include <string>
#include <utility>
#include <functional>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Math/Math.hh>
#include <Math/Random.hh>

#include <Assets/Model.hh>
#include <Assets/AssetsService.hh>

#include <Audio/AudioClip.hh>
#include <Audio/AudioDevice.hh>
#include <Audio/AudioListener.hh>

#include <Filesystem/FileService.hh>

#include <Material/Material.hh>
#include <Material/PhysicalMaterial.hh>

#include <Physics/PhysicsUtility.hh>

#include <Renderer/Text/Font.hh>
#include <Renderer/Core/Light.hh>
#include <Renderer/Particle/ParticleEmitter.hh>

#include <Scene/SceneCamera.hh>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>


#include "Scripting/Script.hh"

namespace mikoto::scene {

    using namespace mikoto::math;
    using namespace mikoto::math::random;
    using namespace mikoto::material;
    using namespace mikoto::animation;
    using namespace mikoto::renderer;
    using namespace mikoto::audio;
    using namespace mikoto::scripting;
    using namespace mikoto::asset;

    class TagComponent {
    public:
        explicit TagComponent() = default;

        explicit TagComponent( const eastl::string_view tag, bool active = true )
            : mTag{ tag }, mIsActive{ active } {}

        TagComponent( const TagComponent& other ) = default;
        TagComponent( TagComponent&& other ) noexcept = default;

        auto operator=( const TagComponent& other ) -> TagComponent& = default;
        auto operator=( TagComponent&& other ) -> TagComponent& = default;

        MKT_NODISCARD auto IsActive() const -> bool { return mIsActive; }
        MKT_NODISCARD auto GetTag() const -> const eastl::string& { return mTag; }
        MKT_NODISCARD auto GetGUID() const -> u32 { return mGuid; }

        auto SetTag( const eastl::string_view newName ) -> void { mTag = newName; }
        auto SetActive( const bool value ) -> void { mIsActive = value; }

    private:
        eastl::string mTag{};
        Guid mGuid{};
        
        bool mIsActive{};
    };

    // Used to identify objects that must be highlighted or
    // should be part of outline, highlight passes
    class HighlightComponent final {
    public:

        auto SetHighlighted(bool value) -> void { m_IsHighlighted = value; }
        MKT_NODISCARD auto IsHighlighted() const -> bool { return m_IsHighlighted; }

    private:
        bool m_IsHighlighted{ false };
    };

    class TransformComponent {
    public:
        explicit TransformComponent() {
            ComputeTransform( m_Translation, m_Scale, m_Rotation );

            m_WorldTransform = m_Transform;
        };

        TransformComponent( const glm::vec3& position, const glm::vec3& size, const glm::vec3& angles = glm::vec3( 0.0f ) ) {
            ComputeTransform( position, size, angles );
        }

        TransformComponent( const TransformComponent& other ) = default;
        TransformComponent( TransformComponent&& other ) = default;

        auto operator=( const TransformComponent& other ) -> TransformComponent& = default;
        auto operator=( TransformComponent&& other ) -> TransformComponent& = default;

        MKT_NODISCARD auto GetTranslation() const -> const glm::vec3& { return m_Translation; }
        MKT_NODISCARD auto GetRotation() const -> const glm::vec3& { return m_Rotation; }
        MKT_NODISCARD auto GetScale() const -> const glm::vec3& { return m_Scale; }
        MKT_NODISCARD auto GetTransform() const -> const glm::mat4& { return m_Transform; }
        MKT_NODISCARD auto HasUniformScale() const -> bool { return m_HasUniformScale; }

        MKT_NODISCARD auto GetRotationQuat() const -> glm::quat {
            return glm::quat( m_Rotation );
        }

        MKT_NODISCARD auto GetWorldTransform() const -> const float4x4& { return m_WorldTransform; }
        auto SetWorldTransform( const float4x4& worldTransform ) -> void { m_WorldTransform = worldTransform; }

        auto ComputeTransform( const glm::vec3& position, const glm::vec3& size, const glm::vec3& angles = glm::vec3( 0.0f ) ) -> void {
            m_Translation = position;
            m_Rotation = angles;
            m_Scale = size;

            // NOTE:
            // Euler rotations composed via successive glm::rotate calls can produce incorrect
            // behavior because rotations are applied in world space, causing axis misalignment.
            // Use quaternion-based rotation (or consistent multiplication order) to ensure
            // rotations happen in local space around the object's pivot.
            // Pivot (center of rotation)

            // NOTE:
            // Rotations must be applied in local space (object axes), not world axes.
            // Chaining glm::rotate on an accumulating matrix can cause rotations to be
            // applied in world space, leading to incorrect behavior.
            // Use quaternions (glm::quat) or proper multiplication order to ensure
            // rotations follow the object's local axes.

            // TODO: See comment in RecomputeTransform, use RecomputeTransform that takes pivot
            m_Transform = math::RecomputeTransform( position, size, angles );
        }

        auto SetTransform( const glm::mat4& transform ) -> void {
            m_Transform = transform;
            math::Decompose( m_Transform, m_Translation, m_Rotation, m_Scale );
        }

        auto SetTranslation( const float3& value ) -> void {
            m_Translation = value;
            m_Transform = math::RecomputeTransform( m_Translation, m_Scale, m_Rotation );
        }

        auto SetRotation( const float3& value ) -> void {
            m_Rotation = value;
            m_Transform = math::RecomputeTransform( m_Translation, m_Scale, m_Rotation );
        }

        auto SetRotation( const glm::quat& quaternion ) -> void {
            m_Rotation = glm::eulerAngles( quaternion );
            m_Transform = math::RecomputeTransform( m_Translation, m_Scale, m_Rotation );
        }

        auto SetScale( const float3& value ) -> void {
            if ( !m_HasUniformScale ) {
                m_Scale = value;
            } else {
                float offSet{ 0 };

                if ( value.x != m_Scale.x ) {
                    offSet = value.x - m_Scale.x;
                } else if ( value.y != m_Scale.y ) {
                    offSet = value.y - m_Scale.y;
                } else if ( value.z != m_Scale.z ) {
                    offSet = value.z - m_Scale.z;
                }

                if ( offSet != 0 ) {
                    m_Scale.x += offSet;
                    m_Scale.y += offSet;
                    m_Scale.z += offSet;
                }
            }

            m_Transform = math::RecomputeTransform( m_Translation, m_Scale, m_Rotation );
        }

        auto SetUniformSale( const bool value ) -> void { m_HasUniformScale = value; }

        ~TransformComponent() = default;

    private:
        // Transform vectors
        float3 m_Translation{ 0.0f, 0.0f, 0.0f };
        float3 m_Rotation{};
        float3 m_Scale{};

        float4x4 m_WorldTransform{ 1.0f };

        // Model matrix (defines object translation, rotation and scale
        // according to the current transform values/vectors
        glm::mat4 m_Transform{};

        bool m_HasUniformScale{};
    };

    class RelationComponent {
    public:
        explicit RelationComponent( const std::optional<u64> parent = std::nullopt )
            : m_Parent{ parent } {}

        RelationComponent( const RelationComponent& other ) = default;
        RelationComponent( RelationComponent&& other ) = default;

        auto operator=( const RelationComponent& other ) -> RelationComponent& = default;
        auto operator=( RelationComponent&& other ) -> RelationComponent& = default;

        auto RegisterChild( const u64 id ) -> void { m_ChildrenIDs.emplace( id ); }
        auto EraseChild( const u64 id ) -> void { m_ChildrenIDs.erase( id ); }

        MKT_NODISCARD auto IsChild( const u64 id ) const -> bool { return m_ChildrenIDs.contains( id ); }
        MKT_NODISCARD auto HasChildren() const -> bool { return !m_ChildrenIDs.empty(); }

        auto SetParent( u64 uid ) -> void { m_Parent = uid; }
        MKT_NODISCARD auto HasParent() const -> bool { return m_Parent.has_value(); }
        MKT_NODISCARD auto GetParent() const -> const std::optional<u64>& { return m_Parent; }

        MKT_NODISCARD auto IsLeaf() const -> bool { return m_ChildrenIDs.empty(); }
        MKT_NODISCARD auto GetChildren() const -> decltype( auto ) { return ( m_ChildrenIDs ); }

        ~RelationComponent() = default;

    private:
        std::optional<u64> m_Parent{};
        ankerl::unordered_dense::set<u64> m_ChildrenIDs{};
    };

    class MaterialComponent {
    public:
        explicit MaterialComponent( MaterialHandle mat = MaterialHandle::CreateEmpty() )
            : mMaterial{ std::move( mat ) } {
        }

        MaterialComponent( MaterialComponent&& ) = default;
        auto operator=( MaterialComponent&& ) -> MaterialComponent& = default;

        MKT_NODISCARD auto HasMaterial() const -> bool { return !mMaterial.IsEmpty(); }
        MKT_NODISCARD auto GetMaterial() -> MaterialHandle { return mMaterial; }
        MKT_NODISCARD auto GetMaterial() const -> MaterialHandle { return mMaterial; }

        auto SetMaterial( const MaterialHandle& mat ) -> void {
            if ( !mat.IsEmpty() ) {
                mMaterial = mat;
            }
        }

        ~MaterialComponent() = default;

    private:
        MaterialHandle mMaterial{};
    };

    /**
     * This component will contain the data to render an object, such
     * as vertex buffers, index buffers, although this component won't be visible
     * in the editor UI
     * */
    class MeshComponent {
    public:
        explicit MeshComponent( ModelHandle model = ModelHandle::CreateEmpty(), i32 meshIndex = {} )
            : m_Model{ model }, m_MeshIndex{ meshIndex } 
        {
        }

        ~MeshComponent() = default;

        auto SetMesh( ModelHandle model, const i32 meshIndex ) {
            if ( !model.IsEmpty() ) {
                m_Model = model;
                m_MeshIndex = meshIndex;
            }
        }

        MKT_NODISCARD auto GetMeshIndex() const -> i32 { return m_MeshIndex; }
        MKT_NODISCARD auto GetModelPath() const -> const Path& { return m_Path; }

        MKT_NODISCARD auto HasMesh() const -> bool { return !m_Model.IsEmpty(); }
        MKT_NODISCARD auto GetMesh() const -> const MeshNode* {
            return std::addressof( m_Model->GetMeshNode( static_cast<u32>( m_MeshIndex ) ) );
        }

        MKT_NODISCARD auto GetMesh() -> MeshNode* {
            if ( !HasMesh() ) {
                return nullptr;
            }

            return std::addressof( m_Model->GetMeshNode( static_cast<u32>( m_MeshIndex ) ) );
        }

        auto GetModel() -> ModelHandle { return m_Model; }

        MKT_NODISCARD auto GetName() const -> eastl::string_view { return m_Model->GetPath().GetFilename(); }

        MKT_NODISCARD auto IsSkinned() const -> bool { return m_Model->IsSkinned(); }
        MKT_NODISCARD auto HasArmature() const -> bool { return m_Model->HasArmature(); }

        auto SetAnimator(u64 id) -> void {
            m_AnimatorID = id;
        }

        MKT_NODISCARD auto HasAnimations() const -> bool { return m_Model->HasAnimations(); }

    private:
        i32 m_MeshIndex{ -1 };

        ModelHandle m_Model{};

        Path m_Path{};
        std::string m_Name{};

        // Animator Controlling this mesh
        // We need to make sure the passed animation
        // uses an skeleton that is compatible with the bone IDs of this mesh
        u64 m_AnimatorID{};
        bool m_IsSkinned{ false };
    };

    class LightComponent {
    public:
        explicit LightComponent( LightType type = LightType::ePoint )
            : m_Type{ type } {}

        LightComponent( const LightComponent& other ) = default;
        LightComponent( LightComponent&& other ) = default;

        auto operator=( const LightComponent& other ) -> LightComponent& = default;
        auto operator=( LightComponent&& other ) -> LightComponent& = default;

        ~LightComponent() = default;

        MKT_NODISCARD auto IsTypeActive( const LightType type ) const -> bool {
            return m_Type == type;
        }

        MKT_NODISCARD auto GetActiveType() const -> LightType { return m_Type; }
        MKT_NODISCARD auto SetActiveType( const LightType type ) -> void { m_Type = type; }

        template<typename T>
        MKT_NODISCARD auto Get() -> T& {
            if constexpr ( std::is_same_v<T, PointLight> ) {
                return m_PointLight;
            } else if constexpr ( std::is_same_v<T, SpotLight> ) {
                return m_SpotLight;
            } else if constexpr ( std::is_same_v<T, DirectionalLight> ) {
                return m_DirectionalLight;
            } else {
                MKT_STATIC_ASSERT( false, "Unsupported light type in LightComponent::Get()" );
            }
        }

        template<typename T>
        MKT_NODISCARD auto Get() const -> const T& {
            if constexpr ( std::is_same_v<T, PointLight> ) {
                return m_PointLight;
            } else if constexpr ( std::is_same_v<T, SpotLight> ) {
                return m_SpotLight;
            } else if constexpr ( std::is_same_v<T, DirectionalLight> ) {
                return m_DirectionalLight;
            } else {
                MKT_STATIC_ASSERT( false, "Unsupported light type in LightComponent::Get()" );
            }
        }

    private:
        SpotLight m_SpotLight{};
        PointLight m_PointLight{};
        DirectionalLight m_DirectionalLight{};

        LightType m_Type{ LightType::ePoint };
    };


    class AudioSourceComponent {
    public:
        explicit AudioSourceComponent( const Path& path ) {
            if ( auto file{ FileService::Get()->LoadFile( path ) }; !file ) {
                MKT_CORE_LOGGER_ERROR( "AudioSourceComponent - Failed to load audio file: {}", path.GetC_Str() );
            } else {
                const AudioLoadDescription desc{
                    .mFile{ file },
                    .mVolume{ 0.5f }
                };

                if ( AudioHandle handle{ AssetsService::Get()->LoadAsset<Audio>( desc ) }; handle.IsEmpty() ) {
                    MKT_CORE_LOGGER_ERROR( "AudioSourceComponent - Audio handle is empty: {}", path.GetC_Str() );
                } else {
                    m_AudioSource = handle->CreateSource();
                }
            }
        }

        AudioSourceComponent( const AudioSourceComponent& other ) = default;
        AudioSourceComponent( AudioSourceComponent&& other ) = default;

        auto operator=( const AudioSourceComponent& other ) -> AudioSourceComponent& = default;
        auto operator=( AudioSourceComponent&& other ) -> AudioSourceComponent& = default;

        ~AudioSourceComponent() = default;

        MKT_NODISCARD auto GetSource() const -> AudioSourceHandle { return m_AudioSource; }

        auto SetClip( const AudioHandle& clip ) -> void {
            if ( !clip ) {
                return;
            }

            if ( m_AudioSource && m_AudioSource->IsPlaying() ) {
                m_AudioSource->Stop();
            }

            m_Clip = clip;
            m_AudioSource = m_Clip->CreateSource();
        }

        MKT_NODISCARD auto GetClip() const -> AudioHandle { return m_Clip; }

    private:
        AudioHandle m_Clip{};
        AudioSourceHandle m_AudioSource{};
    };


    class AudioListenerComponent {
    public:
        explicit AudioListenerComponent() = default;

        AudioListenerComponent( const AudioListenerComponent& other ) = default;
        AudioListenerComponent( AudioListenerComponent&& other ) = default;

        auto operator=( const AudioListenerComponent& other ) -> AudioListenerComponent& = default;
        auto operator=( AudioListenerComponent&& other ) -> AudioListenerComponent& = default;

        ~AudioListenerComponent() = default;

        auto SetListener( AudioListener* listener) -> void {
            if (listener) {
                m_Listener = listener;
            }
        }

        auto Disable() -> void { m_Listener = nullptr; }

        MKT_NODISCARD auto IsActive() -> bool { return m_Listener != nullptr; }

        MKT_NODISCARD auto GetListener() -> AudioListener& { return *m_Listener; }
        MKT_NODISCARD auto GetListener() const -> const AudioListener& { return *m_Listener; }

    private:
        AudioListener* m_Listener{};
    };

    class RigidBodyComponent {
    public:
        enum class BodyType {
            eStatic,
            eKinematic,
            eDynamic
        };

        explicit RigidBodyComponent() = default;

        explicit RigidBodyComponent( const u64 bodyID )
            : m_BodyID{ bodyID }, m_IsValidBody{ true } {}

        RigidBodyComponent( const RigidBodyComponent& ) = default;
        RigidBodyComponent( RigidBodyComponent&& ) noexcept = default;

        auto operator=( const RigidBodyComponent& ) -> RigidBodyComponent& = default;
        auto operator=( RigidBodyComponent&& ) noexcept -> RigidBodyComponent& = default;

        ~RigidBodyComponent() = default;

        MKT_NODISCARD auto GetMass() const -> float { return m_Mass; }
        auto SetMass( const float mass ) -> void {
            m_Mass = math::Clamp( mass, 1.0f, GetMaxBodyMass() );
        }

        MKT_NODISCARD auto static GetMaxBodyMass() -> float { return 300000.0f; }

        MKT_NODISCARD auto GetFriction() const -> float { return m_Friction; }
        auto SetFriction( const float friction ) -> void { m_Friction = friction; }

        MKT_NODISCARD auto UseGravity() const -> bool { return m_UseGravity; }
        auto SetUseGravity( const bool enabled ) -> void { m_UseGravity = enabled; }

        MKT_NODISCARD auto GetBodyType() const -> BodyType { return m_BodyType; }
        MKT_NODISCARD auto IsBodyType( const BodyType type ) const -> bool { return m_BodyType == type; }
        MKT_NODISCARD auto IsDynamic() const -> bool { return m_BodyType == BodyType::eDynamic; }
        auto SetBodyType( const BodyType type ) -> void { m_BodyType = type; }

        MKT_NODISCARD auto GetBodyID() const -> u64 { return m_BodyID; }
        MKT_NODISCARD auto IsValidBodyID() const -> u64 { return m_IsValidBody; }

        auto SetLinearVelocity( float x, float y, float z ) -> void {
            m_LinearVelocity = { x, y, z };
        }

        auto SetLinearVelocity( const float3& velocity ) -> void {
            m_LinearVelocity = velocity;
        }

        auto SetAngularVelocity( float x, float y, float z ) -> void {
            m_AngularVelocity = { x, y, z };
        }

        auto SetAngularVelocity( const float3& velocity ) -> void {
            m_AngularVelocity = velocity;
        }

        MKT_NODISCARD auto GetLinearVelocity() const -> float3 { return m_LinearVelocity; }
        MKT_NODISCARD auto GetAngularVelocity() const -> float3 { return m_AngularVelocity; }

        auto SetBodyID( const u64 bodyID ) -> void {
            m_BodyID = bodyID;
            m_IsValidBody = true;
        }

        auto RemoveBodyID() -> void {
            m_IsValidBody = false;
        }

        auto SetRestitution(float value ) -> void { m_Restitution = value; }
        MKT_NODISCARD auto GetRestitution()const  -> float { return m_Restitution; }

    private:
        float m_Mass{ 1.0f };
        float m_Restitution{ 0.01f };
        float m_Friction{ 1.0f };
        bool m_UseGravity{ true };
        BodyType m_BodyType{ BodyType::eKinematic };

        float3 m_LinearVelocity{ 0.0f, -1.0f, 0.0f };
        float3 m_AngularVelocity{ 0.0f, 0.0f, 0.0f };

        u64 m_BodyID{};
        bool m_IsValidBody{ false };
    };

    class ColliderComponent {
    public:
        explicit ColliderComponent() = default;

        ColliderComponent( const ColliderComponent& other ) = default;
        ColliderComponent( ColliderComponent&& other ) = default;

        auto operator=( const ColliderComponent& other ) -> ColliderComponent& = default;
        auto operator=( ColliderComponent&& other ) -> ColliderComponent& = default;

        ~ColliderComponent() = default;

        MKT_NODISCARD auto GetRadius() const -> core::f32 { return mRadius; }
        MKT_NODISCARD auto GetHeight() const -> core::f32 { return mHeight; }
        MKT_NODISCARD auto IsTrigger() const -> core::f32 { return mIsTrigger; }
        MKT_NODISCARD auto GetColliderType() const -> physics::ColliderType { return mType; }

    private:
        physics::ColliderType mType{ physics::ColliderType::eBox };

        core::f32 mRadius{};
        core::f32 mHeight{};

        bool mIsTrigger{false};
    };

    class CameraComponent {
    public:
        enum class Background { CLEAR_COLOR, SKYBOX };

        explicit CameraComponent() = default;

        CameraComponent( CameraComponent&& other ) noexcept = default;
        auto operator=( CameraComponent&& other ) -> CameraComponent& = default;

        MKT_NODISCARD auto IsMainCamera() const -> bool { return m_MainCam; }
        MKT_NODISCARD auto HasCamera() const -> bool { return m_Camera != nullptr; }
        MKT_NODISCARD auto GetCamera() -> SceneCamera& { return *m_Camera; }
        MKT_NODISCARD auto GetCamera() const -> const SceneCamera& { return *m_Camera; }
        MKT_NODISCARD auto IsAspectRatioFixed() const -> bool { return m_FixedAspectRatio; }

        auto SetBackGround(Background bg) -> void { m_Background = bg; }
        MKT_NODISCARD auto GetBackGround() const -> Background { return m_Background; }

        auto SetGamma( float gamma ) -> void { m_Gamma = gamma; }
        auto SetExposure( float exposure ) -> void { m_Exposure = exposure; }

        MKT_NODISCARD auto GetGamma() const -> float { return m_Gamma; }
        MKT_NODISCARD auto GetExposure() const -> float { return m_Exposure; }

        auto SetFixedAspectRatio(const bool value) -> void { m_FixedAspectRatio = value; }

        ~CameraComponent() = default;

        // Camera component has its own camera not shared
        DISABLE_COPY_FOR( CameraComponent );

    private:
        eastl::unique_ptr<SceneCamera> m_Camera{};

        Background m_Background{ Background::CLEAR_COLOR };

        float m_Gamma{ 1.0f };
        float m_Exposure{ 3.0f };

        bool m_MainCam{ true };
        bool m_FixedAspectRatio{ false };
    };

    class TextComponent {
    public:
        explicit TextComponent() = default;

        TextComponent( eastl::string_view textContent, float size, float spacing, bool isWorld )
            : mTextContent{ textContent }, mSize{ size }, mSpacing{ spacing }, mIsWorldText{ isWorld } {}

        TextComponent( const TextComponent& other ) = default;
        TextComponent( TextComponent&& other ) = default;

        auto operator=( const TextComponent& other ) -> TextComponent& = default;
        auto operator=( TextComponent&& other ) -> TextComponent& = default;

        auto SetFont( FontHandle font ) -> void {
            if ( !font.IsEmpty() ) {
                mFont = font;
            }
        }

        MKT_NODISCARD auto HasFont() const -> bool { return !mFont.IsEmpty(); }

        auto SetCamera( const Camera* camera ) -> void {
            if ( camera != nullptr ) {
                mCamera = camera;
            }
        }

        auto SetIsWorldText(bool value) -> void { mIsWorldText = value; }

        MKT_NODISCARD auto IsWorldText() const ->bool { return mIsWorldText; }
        MKT_NODISCARD auto GetCamera() const -> const Camera* { return mCamera; }

        MKT_NODISCARD auto GetFont() const -> const Font* { return mFont.GetRaw(); }
        MKT_NODISCARD auto GetFontHandle() const -> FontHandle { return mFont; }
        MKT_NODISCARD auto GetColor() const -> const glm::vec4& { return mColor; }

        MKT_NODISCARD auto GetSize() const -> float { return mSize; }
        MKT_NODISCARD auto GetSpacing() const -> float { return mSpacing; }
        MKT_NODISCARD auto GetContents() const -> const eastl::string& { return mTextContent; }

        MKT_NODISCARD static auto GetMinLetterSpacing() -> float { return 1.0f; }
        MKT_NODISCARD static auto GetMaxLetterSpacing() -> float { return 10.0f; }

        MKT_NODISCARD static auto GetMinLetterSize() -> float { return 1.0f; }
        MKT_NODISCARD static auto GetMaxLetterSize() -> float { return 10.0f; }

        auto SetSize( const float value ) -> void {
            if ( value != 0 ) {
                mSize = value;
            }
        }
        auto SetSpacing( const float value ) -> void {
            if ( value != 0 ) {
                mSpacing = value;
            }
        }

        auto SetContents( const eastl::string_view content ) -> void { mTextContent = content; }

        template<typename... Args>
        auto SetColor( Args&&... args ) -> void { mColor = glm::vec4{ std::forward<Args>( args )... }; }

    private:
        eastl::string mTextContent{};

        float4 mColor{ 1.0f, 1.0f, 0.4f, 1.0f };

        float mSize{ 12 };
        float mSpacing{ 0 };

        FontHandle mFont{};
        const Camera* mCamera{ nullptr };

        bool mIsWorldText{ false };
    };

    class ScriptComponent {
    public:
        // The script can be constructed from a script on disk
        // if left empty we will use a blank script with minimal setup
        explicit ScriptComponent(const Path& filePath)
            : mFilePath{ filePath } {}

        explicit ScriptComponent(eastl::string_view filePath = "")
            : mFilePath{ filePath } {}

        ScriptComponent( const ScriptComponent& other ) = default;
        ScriptComponent( ScriptComponent&& other ) = default;

        auto operator=( const ScriptComponent& other ) -> ScriptComponent& = default;
        auto operator=( ScriptComponent&& other ) -> ScriptComponent& = default;

        auto SetScript( ScriptHandle handle ) -> void {
            if ( !handle.IsEmpty( ) ) {
                mScript = handle;
                mFilePath = mScript->GetFile()->GetPath();
            }
        }

        MKT_NODISCARD auto GetHandle() -> ScriptHandle { return mScript; }
        MKT_NODISCARD auto GetFilePath() const -> const Path& { return mFilePath; }

        ~ScriptComponent() = default;

    private:
        Path mFilePath{};
        ScriptHandle mScript{};
    };

    class AnimatorComponent {
    public:
        explicit AnimatorComponent( u64 ID = 0)
            : m_AnimatorID{ ID } {}

        AnimatorComponent( const AnimatorComponent& other ) = default;
        AnimatorComponent( AnimatorComponent&& other ) = default;

        auto operator=( const AnimatorComponent& other ) -> AnimatorComponent& = default;
        auto operator=( AnimatorComponent&& other ) -> AnimatorComponent& = default;

        ~AnimatorComponent() = default;

        auto SetAnimatorID( u64 ID) -> void { m_AnimatorID = ID; }
        MKT_NODISCARD auto GetAnimatorID() const -> u64 { return m_AnimatorID; }

    private:
        u64 m_AnimatorID{};
    };

    class SkinnedMeshRenderer {
    public:
        explicit SkinnedMeshRenderer( u64 ID = 0)
            : m_AnimatorID{ ID } {}

        SkinnedMeshRenderer( const SkinnedMeshRenderer& other ) = default;
        SkinnedMeshRenderer( SkinnedMeshRenderer&& other ) = default;

        auto operator=( const SkinnedMeshRenderer& other ) -> SkinnedMeshRenderer& = default;
        auto operator=( SkinnedMeshRenderer&& other ) -> SkinnedMeshRenderer& = default;

        ~SkinnedMeshRenderer() = default;

        MKT_NODISCARD auto GetAnimatorID() const -> u64 { return m_AnimatorID; }

    private:
        u64 m_AnimatorID{};
    };

    class PostProcessMaterialComponent {
    public:
        explicit PostProcessMaterialComponent() = default;

        PostProcessMaterialComponent( const PostProcessMaterialComponent& other ) = default;
        PostProcessMaterialComponent( PostProcessMaterialComponent&& other ) = default;

        auto operator=( const PostProcessMaterialComponent& other ) -> PostProcessMaterialComponent& = default;
        auto operator=( PostProcessMaterialComponent&& other ) -> PostProcessMaterialComponent& = default;

        MKT_NODISCARD auto GetMaterial() -> MaterialHandle { return mMaterial; }

        ~PostProcessMaterialComponent() = default;

    private:
        MaterialHandle mMaterial{};
    };

    class SkyboxMaterialComponent {
    public:
        explicit SkyboxMaterialComponent() = default;

        SkyboxMaterialComponent( const SkyboxMaterialComponent& other ) = default;
        SkyboxMaterialComponent( SkyboxMaterialComponent&& other ) = default;

        auto operator=( const SkyboxMaterialComponent& other ) -> SkyboxMaterialComponent& = default;
        auto operator=( SkyboxMaterialComponent&& other ) -> SkyboxMaterialComponent& = default;

        MKT_NODISCARD auto GetMaterial() -> MaterialHandle { return mMaterial; }

        auto SetMaterial( MaterialHandle material ) -> void { mMaterial = material; }

        ~SkyboxMaterialComponent() = default;

    private:
        MaterialHandle mMaterial{};
    };

    class ParticleEmitterComponent {
    public:
        explicit ParticleEmitterComponent() = default;

        ParticleEmitterComponent( const ParticleEmitterComponent& other ) = default;
        ParticleEmitterComponent( ParticleEmitterComponent&& other ) = default;

        auto operator=( const ParticleEmitterComponent& other ) -> ParticleEmitterComponent& = default;
        auto operator=( ParticleEmitterComponent&& other ) -> ParticleEmitterComponent& = default;

        ~ParticleEmitterComponent() = default;

    private:
        Ref<renderer::ParticleEmitter> m_Emitter{};
        bool mIsPlaying{ true };
    };
}

#endif// MIKOTO_COMPONENT_HH