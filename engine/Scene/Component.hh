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
#include <Utility/Common.hh>
#include <Core/Assert.hh>
#include <Scene/SceneCamera.hh>
#include <Renderer/Mesh.hh>
#include <Renderer/Model.hh>
#include <Renderer/RenderingUtilities.hh>

namespace Mikoto {
    class TagComponent {
    public:
        explicit TagComponent() = default;

        explicit TagComponent(std::string_view tag) {
            m_Tag = tag;
        }

        TagComponent(const TagComponent& other) = default;
        TagComponent(TagComponent&& other) = default;

        auto operator=(const TagComponent& other) -> TagComponent& = default;
        auto operator=(TagComponent&& other) -> TagComponent& = default;

        KT_NODISCARD auto GetTag() const -> const std::string& { return m_Tag; }
        auto SetTag(std::string_view newName) -> void { m_Tag = newName; }
    private:
        std::string m_Tag{};
    };

    class TransformComponent {
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

        KT_NODISCARD auto GetTranslation() const -> const glm::vec3& { return m_Translation; }
        KT_NODISCARD auto GetRotation() const -> const glm::vec3& { return m_Rotation; }
        KT_NODISCARD auto GetScale() const -> const glm::vec3& { return m_Scale; }
        KT_NODISCARD auto GetTransform() const -> const glm::mat4& { return m_Transform; }

        // "angles" specifies the rotation angle in degrees for each axis
        auto ComputeTransform(const glm::vec3& position, const glm::vec3& size, const glm::vec3& angles = glm::vec3(0.0f)) -> void {
            m_Translation = position;
            m_Scale = size;
            m_Rotation = angles;

            // Matrix transformations
            RecomputeTransform();
        }

        auto SetTranslation(const glm::vec3& value) -> void { m_Translation = value; RecomputeTransform(); }
        auto SetRotation(const glm::vec3& value) -> void { m_Rotation = value; RecomputeTransform(); }
        auto SetScale(const glm::vec3& value) -> void { m_Scale = value; RecomputeTransform(); }
        auto SetTransform(const glm::mat4& value) -> void { m_Transform = value; }

        ~TransformComponent() = default;
    private:
        auto RecomputeTransform() -> void {
            // Matrix transformations
            glm::mat4 scale{ glm::scale(identMat, m_Scale) };

            glm::mat4 rotationX{ glm::rotate(identMat, (float)glm::radians(m_Rotation[0]), xAxis) };
            glm::mat4 rotationY{ glm::rotate(rotationX, (float)glm::radians(m_Rotation[1]), yAxis) };
            glm::mat4 rotation{ glm::rotate(rotationY, (float)glm::radians(m_Rotation[2]), zAxis) };

            m_Transform =  glm::translate(identMat, m_Translation) * scale * rotation;
        }

        // Constants
        static constexpr glm::vec3 xAxis{ 1.0f, 0.0f, 0.0f };
        static constexpr glm::vec3 yAxis{ 0.0f, 1.0f, 0.0f };
        static constexpr glm::vec3 zAxis{ 0.0f, 0.0f, 1.0f };
        static constexpr glm::mat4 identMat{ glm::mat4(1.0) };
    private:
        glm::vec3 m_Translation{};
        glm::vec3 m_Rotation{};
        glm::vec3 m_Scale{};

        glm::mat4 m_Transform{};
    };

    /**
     * For 2D rendering
     * @deprecated Using RenderObject instead to filter renderables
     * */
    class SpriteRendererComponent {
    public:
        explicit SpriteRendererComponent() = default;

        explicit SpriteRendererComponent(const glm::vec4& color, const DrawData& drawData = {}) {
            m_Color = color;
            m_DrawData = drawData;

            if (m_DrawData.MaterialData == nullptr)
                m_DrawData.MaterialData = Material::Create(Material::Type::STANDARD);
        }

        SpriteRendererComponent(const SpriteRendererComponent& other) = default;
        SpriteRendererComponent(SpriteRendererComponent&& other) = default;

        auto operator=(const SpriteRendererComponent& other) -> SpriteRendererComponent& = default;
        auto operator=(SpriteRendererComponent&& other) -> SpriteRendererComponent& = default;

        KT_NODISCARD auto GetColor() const -> const glm::vec4& { return m_Color; }
        KT_NODISCARD auto GetDrawData() -> DrawData& { return m_DrawData; }

        auto SetColor(const glm::vec4& value) -> void { m_Color = value; }

        ~SpriteRendererComponent() = default;

    private:
        DrawData m_DrawData{};
        glm::vec4 m_Color{};
    };

    /**
     * Represents render-able objects
     * */
    class Renderable {
    public:
        explicit Renderable() = default;

        Renderable(const Renderable& other) = default;
        Renderable(Renderable&& other) = default;

        auto operator=(const Renderable& other) -> Renderable& = default;
        auto operator=(Renderable&& other) -> Renderable& = default;

        auto GetDrawData() -> DrawData& { return m_DrawData; }

        ~Renderable() = default;

    private:
        DrawData m_DrawData{};
    };

    class CameraComponent {
    public:
        explicit CameraComponent(std::shared_ptr<SceneCamera> camera = nullptr, bool mainCam = true, bool fixedAspectRation = false)
            :   m_Camera{ camera ? std::make_shared<SceneCamera>() : std::move(camera) }, m_MainCam{ mainCam }, m_FixedAspectRatio{ fixedAspectRation }
        {
            KT_ASSERT(m_Camera != nullptr, "Camera is NULL");
        }

        CameraComponent(const CameraComponent& other) = default;
        CameraComponent(CameraComponent&& other) = default;

        auto operator=(const CameraComponent& other) -> CameraComponent& = default;
        auto operator=(CameraComponent&& other) -> CameraComponent& = default;

        KT_NODISCARD auto IsMainCamera() const -> bool { return m_MainCam; }
        KT_NODISCARD auto GetCamera() -> SceneCamera& { return *m_Camera; }
        KT_NODISCARD auto GetCamera() const -> const SceneCamera& { return *m_Camera; }
        KT_NODISCARD auto GetCameraPtr() const -> std::shared_ptr<SceneCamera> { return m_Camera; }
        KT_NODISCARD auto IsAspectRatioFixed() const -> bool { return m_FixedAspectRatio; }

        auto EnableFixedAspectRatio() -> void { m_FixedAspectRatio = true; }
        auto DisableFixedAspectRatio() -> void { m_FixedAspectRatio = false; }

        ~CameraComponent() = default;
    private:
        std::shared_ptr<SceneCamera> m_Camera{};
        bool m_MainCam{ true };
        bool m_FixedAspectRatio{ false };
    };


    /**
     * Checks if a scriptable entity has an OnCreate, OnDestroy and OnUpdate function
     * needed when binding and scriptable entity to the native script component
     * */
    template<typename ScriptableEntityType>
    concept HasOnCreate = requires (std::shared_ptr<ScriptableEntityType> scriptable) { scriptable->OnCreate(); };

    template<typename ScriptableEntityType>
    concept HasOnUpdate = requires (std::shared_ptr<ScriptableEntityType> scriptable) { scriptable->OnUpdate(); };

    template<typename ScriptableEntityType>
    concept HasOnDestroy = requires (std::shared_ptr<ScriptableEntityType> scriptable) { scriptable->OnDestroy(); };

    // support for C+++ which is the native engine language
    class ScriptableEntity;

    class NativeScriptComponent {
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
            m_OnUpdateFunc = [](std::shared_ptr<ScriptableEntityType> scriptable) -> void { scriptable->OnUpdate(); };
            m_OnDestroyFunc = [](std::shared_ptr<ScriptableEntityType> scriptable) -> void { scriptable->OnDestroy(); };
        }

    private:
        std::function<void(std::shared_ptr<ScriptableEntity> scriptable)> m_OnCreateFunc{};
        std::function<void(std::shared_ptr<ScriptableEntity> scriptable)> m_OnUpdateFunc{};
        std::function<void(std::shared_ptr<ScriptableEntity> scriptable)> m_OnDestroyFunc{};
    };

    class RenderableModel {
    public:
        explicit RenderableModel() = default;

        RenderableModel(const RenderableModel & other) = default;
        RenderableModel(RenderableModel && other) = default;

        auto operator=(const RenderableModel & other) -> RenderableModel & = default;
        auto operator=(RenderableModel && other) -> RenderableModel & = default;

    private:
        std::shared_ptr<Model> m_Model{};
    };

    class RenderableMesh {
    public:
        explicit RenderableMesh() = default;

        RenderableMesh(const RenderableMesh & other) = default;
        RenderableMesh(RenderableMesh && other) = default;

        auto operator=(const RenderableMesh & other) -> RenderableMesh & = default;
        auto operator=(RenderableMesh && other) -> RenderableMesh & = default;

    private:
        std::shared_ptr<Mesh> m_Mesh{};
    };
}

#endif // MIKOTO_COMPONENT_HH
