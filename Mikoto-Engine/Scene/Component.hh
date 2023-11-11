/**
 * Component.hh
 * Created by kate on 6/24/23.
 * */

#ifndef MIKOTO_COMPONENT_HH
#define MIKOTO_COMPONENT_HH

// C++ Standard Library
#include <functional>
#include <string>

// Third-Party Libraries
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Project Headers
#include <Common/Types.hh>
#include <Common/Common.hh>
#include <Common/Random.hh>
#include <Common/Constants.hh>
#include <Common/RenderingUtils.hh>

#include <Core/Assert.hh>

#include <Renderer/Mesh.hh>
#include <Renderer/Model.hh>

#include <Scene/SceneCamera.hh>

namespace Mikoto {
    template<typename ComponentType>
    class BaseComponent {
    public:
        explicit BaseComponent() = default;

        BaseComponent(const BaseComponent & other) = default;
        BaseComponent(BaseComponent && other) = default;

        auto operator=(const BaseComponent & other) -> BaseComponent & = default;
        auto operator=(BaseComponent && other) -> BaseComponent & = default;

        ~BaseComponent() = default;

        auto OnCreate() -> void { static_cast<ComponentType*>(this)->OnComponentAttach(); }
        auto OnUpdate() -> void { static_cast<ComponentType*>(this)->OnComponentUpdate(); }
        auto OnRemove() -> void { static_cast<ComponentType*>(this)->OnComponentRemoved(); }
    };


    class TagComponent : public BaseComponent<TagComponent> {
    public:
        explicit TagComponent() = default;

        explicit TagComponent(std::string_view tag, UInt64_T guid)
            :   m_GUID{ guid }, m_Tag{ tag }
        {
            m_Visibility = true;
        }

        TagComponent(const TagComponent& other) = default;
        TagComponent(TagComponent&& other) = default;

        auto operator=(const TagComponent& other) -> TagComponent& = default;
        auto operator=(TagComponent&& other) -> TagComponent& = default;

        MKT_NODISCARD auto IsVisible() const -> bool { return m_Visibility; }
        MKT_NODISCARD auto GetTag() const -> const std::string& { return m_Tag; }
        MKT_NODISCARD auto GetGUID() const -> UInt64_T { return m_GUID.Get(); }

        auto SetTag(std::string_view newName) -> void { m_Tag = newName; }
        auto SetVisibility(bool value) -> void { m_Visibility = value; }

        auto OnComponentAttach() -> void { MKT_APP_LOGGER_DEBUG("Added component TAG_COMPONENT"); }
        auto OnComponentUpdate() -> void { MKT_APP_LOGGER_DEBUG("Updated component TAG_COMPONENT"); }
        auto OnComponentRemoved() -> void { MKT_APP_LOGGER_DEBUG("Removed component TAG_COMPONENT"); }
    private:
        Random::GUID::UUID m_GUID{};
        std::string m_Tag{};
        bool m_Visibility{};
    };



    class TransformComponent : public BaseComponent<TransformComponent> {
    public:
        explicit TransformComponent() = default;
        explicit TransformComponent(const glm::mat4& data) { m_Transform = data; }

        TransformComponent(const glm::vec3& position, const glm::vec3& size, const glm::vec3& angles = glm::vec3(0.0f)) {
            ComputeTransform(position, size, angles);
        }

        TransformComponent(const TransformComponent& other) = default;
        TransformComponent(TransformComponent&& other) = default;

        auto operator=(const TransformComponent& other) -> TransformComponent& = default;
        auto operator=(TransformComponent&& other) -> TransformComponent& = default;

        MKT_NODISCARD auto GetTranslation() const -> const glm::vec3& { return m_Translation; }
        MKT_NODISCARD auto GetRotation() const -> const glm::vec3& { return m_Rotation; }
        MKT_NODISCARD auto GetScale() const -> const glm::vec3& { return m_Scale; }
        MKT_NODISCARD auto GetTransform() const -> const glm::mat4& { return m_Transform; }

        /**
         * Computes the model matrix for for this component according to the transform vectors
         * @param position specifies the object translation value
         * @param size specifies the object scaling value
         * @param angles specifies Euler angles rotations (each component represents an angle in radians)
         * */
        auto ComputeTransform(const glm::vec3& position, const glm::vec3& size, const glm::vec3& angles = glm::vec3(0.0f)) -> void {
            m_Translation = position;
            m_Rotation = angles;
            m_Scale = size;

            RecomputeTransform();
        }

        auto SetTransform(const glm::mat4& transform) -> void {
            m_Transform = transform;

            // Update translation, rotation and scale accordingly
            m_Translation = glm::vec3(m_Transform[3]);
        }

        auto SetTranslation(const glm::vec3& value) -> void { m_Translation = value; RecomputeTransform(); }
        auto SetRotation(const glm::vec3& value) -> void { m_Rotation = value; RecomputeTransform(); }
        auto SetScale(const glm::vec3& value) -> void { m_Scale = value; RecomputeTransform(); }

        ~TransformComponent() = default;

        auto OnComponentAttach() -> void { MKT_APP_LOGGER_DEBUG("Added component TRANSFORM_COMPONENT"); }
        auto OnComponentUpdate() -> void { MKT_APP_LOGGER_DEBUG("Updated component TRANSFORM_COMPONENT"); }
        auto OnComponentRemoved() -> void { MKT_APP_LOGGER_DEBUG("Removed component TRANSFORM_COMPONENT"); }

    private:
        /**
         * Computes the model matrix as in Translate * Ry * Rx * Rz * Scale (where R represents a
         * rotation in the desired axis. Rotation convention uses Tait-Bryan angles with axis order
         * Y(1), X(2), Z(3)
         * */
        auto RecomputeTransform() -> void {
            // Compute scale matrix
            const glm::mat4 scale{ glm::scale(GLM_IDENTITY_MAT4, m_Scale) };

            // Compute rotation matrix
            glm::mat4 rotation{ glm::rotate(GLM_IDENTITY_MAT4, (float)glm::radians(m_Rotation.y), GLM_UNIT_VECTOR_Y) };
            rotation = glm::rotate(rotation, (float)glm::radians(m_Rotation.x), GLM_UNIT_VECTOR_X);
            rotation = glm::rotate(rotation, (float)glm::radians(m_Rotation.z), GLM_UNIT_VECTOR_Z);

            m_Transform =  glm::translate(GLM_IDENTITY_MAT4, m_Translation) * rotation * scale;
        }

    private:
        // Transform vectors
        glm::vec3 m_Translation{};
        glm::vec3 m_Rotation{};
        glm::vec3 m_Scale{};

        // Model matrix (defines object translation, rotation and scale
        // according to the current transform values/vectors
        glm::mat4 m_Transform{};
    };



    /**
     * Contains the material information of an entity
     * */
    class MaterialComponent : public BaseComponent<MaterialComponent> {
    public:
        explicit MaterialComponent() = default;

        MaterialComponent(const MaterialComponent & other) = default;
        MaterialComponent(MaterialComponent && other) = default;

        auto operator=(const MaterialComponent & other) -> MaterialComponent & = default;
        auto operator=(MaterialComponent && other) -> MaterialComponent & = default;

        MKT_NODISCARD auto Get() -> Material& { return *m_Material; }
        MKT_NODISCARD auto GetColor() const -> const glm::vec4& { return m_Color; }
        MKT_NODISCARD auto GetMaterialList() -> std::vector<std::shared_ptr<Material>>& { return m_MaterialList; }

        auto SetColor(const glm::vec4& value) -> void { m_Color = value; }
        auto SetMaterial(std::shared_ptr<Material> mat) -> void { m_Material = mat; }

        auto Add(std::shared_ptr<Material>&& mat) -> void { m_MaterialList.emplace_back(mat); }

        ~MaterialComponent() = default;

        auto OnComponentAttach() -> void { MKT_APP_LOGGER_DEBUG("Added component MATERIAL_COMPONENT"); }
        auto OnComponentUpdate() -> void { MKT_APP_LOGGER_DEBUG("Updated component MATERIAL_COMPONENT"); }
        auto OnComponentRemoved() -> void { MKT_APP_LOGGER_DEBUG("Removed component MATERIAL_COMPONENT"); }

    private:
        std::shared_ptr<Material> m_Material{};

        std::vector<std::shared_ptr<Material>> m_MaterialList{};

        glm::vec4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };



    /**
     * This component will contain the data to render an object, such
     * as vertex buffers, index buffers, although this component won't be visible
     * in the editor UI
     * */
    class RenderComponent : public BaseComponent<RenderComponent> {
    public:
        explicit RenderComponent() = default;

        RenderComponent(const RenderComponent & other) = default;
        RenderComponent(RenderComponent && other) = default;

        auto operator=(const RenderComponent & other) -> RenderComponent & = default;
        auto operator=(RenderComponent && other) -> RenderComponent & = default;

        ~RenderComponent() = default;

        auto GetObjectData() -> SceneObjectData& { return m_RenderableData; }
        MKT_NODISCARD auto GetObjectData() const -> const SceneObjectData& { return m_RenderableData; }

        auto OnComponentAttach() -> void { MKT_APP_LOGGER_DEBUG("Added component RENDERER_COMPONENT"); }
        auto OnComponentUpdate() -> void { MKT_APP_LOGGER_DEBUG("Updated component RENDERER_COMPONENT"); }
        auto OnComponentRemoved() -> void { MKT_APP_LOGGER_DEBUG("Removed component RENDERER_COMPONENT"); }

    private:
        SceneObjectData m_RenderableData{};
    };



    class LightComponent : public BaseComponent<LightComponent> {
    public:
        explicit LightComponent() = default;

        LightComponent(const LightComponent & other) = default;
        LightComponent(LightComponent && other) = default;

        auto operator=(const LightComponent & other) -> LightComponent & = default;
        auto operator=(LightComponent && other) -> LightComponent & = default;

        ~LightComponent() = default;

        MKT_NODISCARD auto GetColor() const -> const glm::vec4& { return m_Color; }
        MKT_NODISCARD auto GetType() const -> LightType { return m_Type; }
        MKT_NODISCARD auto GetData() const -> const LightData&{ return m_Data; }
        MKT_NODISCARD auto GetData() -> LightData&{ return m_Data; }

        MKT_NODISCARD auto GetDirLightData() -> DirectionalLight& { return m_Data.DireLightData; }
        MKT_NODISCARD auto GetSpotLightData() -> SpotLight& { return m_Data.SpotLightData; }
        MKT_NODISCARD auto GetPointLightData() -> PointLight& { return m_Data.PointLightDat; }

        MKT_NODISCARD auto IsActive() const -> bool { return m_Active; }

        auto SetType(LightType type) -> void { m_Type = type; }
        auto SetColor(const glm::vec4& color) -> void { m_Color = color; }
        auto SetActive(bool value) -> void { m_Active = value; }

        auto OnComponentAttach() -> void { MKT_APP_LOGGER_DEBUG("Added component LIGHT_COMPONENT"); }
        auto OnComponentUpdate() -> void { MKT_APP_LOGGER_DEBUG("Updated component LIGHT_COMPONENT"); }
        auto OnComponentRemoved() -> void { MKT_APP_LOGGER_DEBUG("Removed component LIGHT_COMPONENT"); }

    private:
        LightData m_Data{};
        LightType m_Type{ LightType::POINT_LIGHT_TYPE };
        glm::vec4 m_Color{};

        bool m_Active{};
    };




    class AudioComponent : public BaseComponent<AudioComponent> {
    public:
        explicit AudioComponent() = default;

        AudioComponent(const AudioComponent & other) = default;
        AudioComponent(AudioComponent && other) = default;

        auto operator=(const AudioComponent & other) -> AudioComponent & = default;
        auto operator=(AudioComponent && other) -> AudioComponent & = default;

        ~AudioComponent() = default;

        MKT_NODISCARD auto GetVolume() const -> float { return m_Volume; }
        MKT_NODISCARD auto GetSourcePath() const -> const Path_T& { return m_Clip; }
        MKT_NODISCARD auto IsMuted() const -> bool { return m_Muted; }
        MKT_NODISCARD auto IsLooping() const -> bool { return m_Loop; }

        auto Mute(bool value) -> void { m_Muted = value; }
        auto SetVolume(float volume) -> void { m_Volume = (volume > 0.0f) ? volume : m_Volume; }
        auto SetLooping(bool value) -> void { m_Loop = value; }

        auto OnComponentAttach() -> void { MKT_APP_LOGGER_DEBUG("Added component AUDIO_COMPONENT"); }
        auto OnComponentUpdate() -> void { MKT_APP_LOGGER_DEBUG("Updated component AUDIO_COMPONENT"); }
        auto OnComponentRemoved() -> void { MKT_APP_LOGGER_DEBUG("Removed component AUDIO_COMPONENT"); }

    private:
        Path_T m_Clip{ "No clip file loaded (MP3, WAV...) " };
        float m_Volume{};
        bool m_Muted{};
        bool m_Loop{};
    };



    class PhysicsComponent : public BaseComponent<PhysicsComponent> {
    public:
        explicit PhysicsComponent() = default;

        PhysicsComponent(const PhysicsComponent & other) = default;
        PhysicsComponent(PhysicsComponent && other) = default;

        auto operator=(const PhysicsComponent & other) -> PhysicsComponent & = default;
        auto operator=(PhysicsComponent && other) -> PhysicsComponent & = default;

        ~PhysicsComponent() = default;

        auto GetMass() const -> float { return m_Mass; }

        auto OnComponentAttach() -> void { MKT_APP_LOGGER_DEBUG("Added component PHYSICS_COMPONENT"); }
        auto OnComponentUpdate() -> void { MKT_APP_LOGGER_DEBUG("Updated component PHYSICS_COMPONENT"); }
        auto OnComponentRemoved() -> void { MKT_APP_LOGGER_DEBUG("Removed component PHYSICS_COMPONENT"); }

    private:
        float m_Mass{};

        bool m_CollisionDetection{};
        bool m_UseGravity{};
    };


    class CameraComponent : public BaseComponent<CameraComponent> {
    public:
        explicit CameraComponent(std::shared_ptr<SceneCamera> camera, bool mainCam = true, bool fixedAspectRation = false)
            :   m_Camera{ camera ? std::make_shared<SceneCamera>() : std::move(camera) }, m_MainCam{ mainCam }, m_FixedAspectRatio{ fixedAspectRation }
        {
            // This should not assert. Just a temporary sanity check
            MKT_ASSERT(m_Camera != nullptr, "Camera is NULL");
        }

        CameraComponent(const CameraComponent& other) = default;
        CameraComponent(CameraComponent&& other) = default;

        auto operator=(const CameraComponent& other) -> CameraComponent& = default;
        auto operator=(CameraComponent&& other) -> CameraComponent& = default;

        MKT_NODISCARD auto IsMainCamera() const -> bool { return m_MainCam; }
        MKT_NODISCARD auto GetCamera() -> SceneCamera& { return *m_Camera; }
        MKT_NODISCARD auto GetCamera() const -> const SceneCamera& { return *m_Camera; }
        MKT_NODISCARD auto GetCameraPtr() const -> std::shared_ptr<SceneCamera> { return m_Camera; }
        MKT_NODISCARD auto IsAspectRatioFixed() const -> bool { return m_FixedAspectRatio; }

        auto EnableFixedAspectRatio() -> void { m_FixedAspectRatio = true; }
        auto DisableFixedAspectRatio() -> void { m_FixedAspectRatio = false; }

        ~CameraComponent() = default;

        auto OnComponentAttach() -> void {
            // newComponent.GetCameraPtr()->SetViewportSize(scene->m_ViewportWidth, scene->m_ViewportHeight);
            MKT_APP_LOGGER_DEBUG("Added component CAMERA_COMPONENT");
        }

        auto OnComponentUpdate() -> void { MKT_APP_LOGGER_DEBUG("Updated component CAMERA_COMPONENT"); }
        auto OnComponentRemoved() -> void { MKT_APP_LOGGER_DEBUG("Removed component CAMERA_COMPONENT"); }
    private:
        std::shared_ptr<SceneCamera> m_Camera{};
        bool m_MainCam{ true };
        bool m_FixedAspectRatio{ false };
    };


    // ====== Section pending of review =======
    // Scripting is not supported yet.


    /**
     * Checks if a scriptable entity has an OnCreate, OnDestroy and Present function
     * needed when binding and scriptable entity to the native script component
     * */
    template<typename ScriptableEntityType>
    concept HasOnCreate = requires (std::shared_ptr<ScriptableEntityType> scriptable) { scriptable->OnCreate(); };

    template<typename ScriptableEntityType>
    concept HasOnUpdate = requires (std::shared_ptr<ScriptableEntityType> scriptable) { scriptable->Present(0, nullptr); };

    template<typename ScriptableEntityType>
    concept HasOnDestroy = requires (std::shared_ptr<ScriptableEntityType> scriptable) { scriptable->OnDestroy(); };

    // support for C++ scripting which is the native engine language
    class ScriptableEntity;

    class NativeScriptComponent : public BaseComponent<NativeScriptComponent> {
    public:
        explicit NativeScriptComponent() = default;

        NativeScriptComponent(const NativeScriptComponent& other) = default;
        NativeScriptComponent(NativeScriptComponent&& other) = default;

        auto operator=(const NativeScriptComponent& other) -> NativeScriptComponent& = default;
        auto operator=(NativeScriptComponent&& other) -> NativeScriptComponent& = default;

        template<typename ScriptableEntityType>
            requires HasOnCreate<ScriptableEntityType> &&
                     HasOnUpdate<ScriptableEntityType> &&
                     HasOnDestroy<ScriptableEntityType>
        auto Bind() -> void {
            m_OnCreateFunc = [](std::shared_ptr<ScriptableEntityType> scriptable) -> void { scriptable->OnCreate(); };
            m_OnUpdateFunc = [](std::shared_ptr<ScriptableEntityType> scriptable) -> void { scriptable->Present(0, nullptr); };
            m_OnDestroyFunc = [](std::shared_ptr<ScriptableEntityType> scriptable) -> void { scriptable->OnDestroy(); };
        }

        auto OnComponentAttach() -> void { MKT_APP_LOGGER_DEBUG("Added component NATIVE_SCRIPT_COMPONENT"); }
        auto OnComponentUpdate() -> void { MKT_APP_LOGGER_DEBUG("Updated component NATIVE_SCRIPT_COMPONENT"); }
        auto OnComponentRemoved() -> void { MKT_APP_LOGGER_DEBUG("Removed component NATIVE_SCRIPT_COMPONENT"); }

    private:
        std::function<void(std::shared_ptr<ScriptableEntity> scriptable)> m_OnCreateFunc{};
        std::function<void(std::shared_ptr<ScriptableEntity> scriptable)> m_OnUpdateFunc{};
        std::function<void(std::shared_ptr<ScriptableEntity> scriptable)> m_OnDestroyFunc{};
    };
}

#endif // MIKOTO_COMPONENT_HH