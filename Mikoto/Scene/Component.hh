/**
 * Component.hh
 * Created by kate on 6/24/23.
 * */

#ifndef MIKOTO_COMPONENT_HH
#define MIKOTO_COMPONENT_HH

// C++ Standard Library
#include <functional>
#include <string>
#include <unordered_set>

// Third-Party Libraries
#include <glm/glm.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Project Headers
#include <Assets/Audio.hh>
#include <Assets/Font.hh>
#include <Assets/Model.hh>
#include <Common/Common.hh>
#include <Library/Math/Math.hh>
#include <Library/Random/Random.hh>
#include <Library/Utility/Types.hh>
#include <Material/Material.hh>
#include <Renderer/Light.hh>
#include <Scene/SceneCamera.hh>
#include <Audio/AudioListener.hh>

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

        explicit TagComponent( const std::string_view tag )
            :   m_Tag{ tag }, m_Visibility{ true }
        {
        }

        TagComponent(const TagComponent& other) = default;
        TagComponent(TagComponent&& other) noexcept = default;

        auto operator=(const TagComponent& other) -> TagComponent& = default;
        auto operator=(TagComponent&& other) -> TagComponent& = default;

        MKT_NODISCARD auto IsVisible() const -> bool { return m_Visibility; }
        MKT_NODISCARD auto GetTag() const -> const std::string& { return m_Tag; }
        MKT_NODISCARD auto GetGUID() const -> UInt64_T { return m_GUID.Get(); }

        auto SetTag( const std::string_view newName) -> void { m_Tag = newName; }
        auto SetVisibility( const bool value) -> void { m_Visibility = value; }

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }
    private:
        std::string m_Tag{};
        bool m_Visibility{};
        GlobalUniqueID m_GUID{};
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
        MKT_NODISCARD auto HasUniformScale() const -> bool { return m_HasUniformScale; }

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

            m_Transform = Math::RecomputeTransform(position, size, angles);
        }

        auto SetTransform(const glm::mat4& transform) -> void {
            m_Transform = transform;

            glm::vec3 translate{};
            glm::vec3 rotate{};
            glm::vec3 scale{};

            const bool success{ Math::DecomposeTransform(m_Transform, translate, rotate, scale) };

            if (success) {
                m_Scale = scale;
                m_Translation = translate;
                m_Rotation = rotate;
            }
        }

        auto SetTranslation(const glm::vec3& value) -> void {
            m_Translation = value;

            m_Transform = Math::RecomputeTransform(m_Translation, m_Scale, m_Rotation);
        }

        auto SetRotation(const glm::vec3& value) -> void {
            m_Rotation = value;

            m_Transform = Math::RecomputeTransform(m_Translation, m_Scale, m_Rotation);
        }

        auto SetScale(const glm::vec3& value) -> void {
            if (!m_HasUniformScale) {
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

                if (offSet != 0) {
                    m_Scale.x += offSet;
                    m_Scale.y += offSet;
                    m_Scale.z += offSet;
                }
            }

            m_Transform = Math::RecomputeTransform(m_Translation, m_Scale, m_Rotation);
        }

        auto WantUniformSale(const bool value) -> void { m_HasUniformScale = value; }

        ~TransformComponent() = default;

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

    private:


    private:
        // Transform vectors
        glm::vec3 m_Translation{};
        glm::vec3 m_Rotation{};
        glm::vec3 m_Scale{};

        // Model matrix (defines object translation, rotation and scale
        // according to the current transform values/vectors
        glm::mat4 m_Transform{};

        bool m_HasUniformScale{};
    };

    class RelationComponent : public BaseComponent<RelationComponent> {
    public:
        explicit RelationComponent() = default;

        RelationComponent(const RelationComponent& other) = default;
        RelationComponent(RelationComponent&& other) = default;

        auto operator=(const RelationComponent& other) -> RelationComponent& = default;
        auto operator=(RelationComponent&& other) -> RelationComponent& = default;

        auto RegisterChild(const UInt64_T id) -> void { m_ChildrenIDs.emplace(id); }
        auto EraseChild(const UInt64_T id) -> void { m_ChildrenIDs.erase(id); }

        MKT_NODISCARD auto At(const Size_T index) const -> UInt64_T {
            return *std::next(m_ChildrenIDs.begin(), index);
        }

        MKT_NODISCARD auto IsChild(const UInt64_T id) const -> bool {return m_ChildrenIDs.contains(id); }
        MKT_NODISCARD auto HasChildren() const -> bool {return !m_ChildrenIDs.empty(); }

        MKT_NODISCARD auto GetChildren() const -> decltype( auto ) { return (m_ChildrenIDs); }

        MKT_NODISCARD auto IsLeaf() const -> bool { return m_ChildrenIDs.empty(); }

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

        ~RelationComponent() = default;

    private:

        ankerl::unordered_dense::set<UInt64_T> m_ChildrenIDs{};
    };



    /**
     * Contains the material information of an entity. It describes how this object looks like
     * */
    class MaterialComponent : public BaseComponent<MaterialComponent> {
    public:
        explicit MaterialComponent(Scope_T<Material>&& mat = nullptr)
            : m_Material{ std::move(mat) }
        {}

        MaterialComponent(MaterialComponent&&) = default;
        auto operator=(MaterialComponent&&) -> MaterialComponent& = default;

        MKT_NODISCARD auto HasMaterial() const -> bool { return m_Material != nullptr; }
        MKT_NODISCARD auto GetMaterial() -> Material& { return *m_Material; }
        MKT_NODISCARD auto GetMaterial() const -> const Material& { return *m_Material; }

        auto SetMaterial(Scope_T<Material>&& mat) -> void { m_Material = std::move(mat); }

        ~MaterialComponent() = default;

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

    private:
        Scope_T<Material> m_Material{};
    };



    /**
     * This component will contain the data to render an object, such
     * as vertex buffers, index buffers, although this component won't be visible
     * in the editor UI
     * */
    class RenderComponent : public BaseComponent<RenderComponent> {
    public:
        explicit RenderComponent() = default;

        ~RenderComponent() = default;

        auto SetMesh(const MeshNode* mesh) {
            if (mesh != nullptr) {
                m_Mesh = mesh;
            }
        }

        MKT_NODISCARD auto HasMesh() const -> bool { return m_Mesh != nullptr; }
        MKT_NODISCARD auto GetMesh() const -> const MeshNode* { return m_Mesh; }
        MKT_NODISCARD auto GetPath() const -> const Path_T& { return m_Path; }
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

    private:
        const MeshNode* m_Mesh{};

        Path_T m_Path{};
        std::string m_Name{};
    };

    class LightComponent : public BaseComponent<LightComponent> {
    public:
        explicit LightComponent() = default;

        LightComponent(const LightComponent & other) = default;
        LightComponent(LightComponent && other) = default;

        auto operator=(const LightComponent & other) -> LightComponent & = default;
        auto operator=(LightComponent && other) -> LightComponent & = default;

        ~LightComponent() = default;

        MKT_NODISCARD auto IsTypeActive(const LightType type ) const -> bool {
            return m_Type == type;
        }

        MKT_NODISCARD auto GetActiveType() const -> LightType { return m_Type; }

        MKT_NODISCARD auto GetLight() -> Light& { return m_Data; }
        MKT_NODISCARD auto GetLight() const -> const Light& { return m_Data; }

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

    private:
        Light m_Data{};
        LightType m_Type{ LightType::POINT_LIGHT_TYPE };
    };


    class AudioSourceComponent : public BaseComponent<AudioSourceComponent> {
    public:
        explicit AudioSourceComponent() = default;

        AudioSourceComponent(const AudioSourceComponent & other) = default;
        AudioSourceComponent(AudioSourceComponent && other) = default;

        auto operator=(const AudioSourceComponent & other) -> AudioSourceComponent & = default;
        auto operator=(AudioSourceComponent && other) -> AudioSourceComponent & = default;

        ~AudioSourceComponent() = default;

        MKT_NODISCARD auto GetSource() const -> AudioSourceHandle { return m_AudioSource; }


        auto SetClipt(AudioHandle clip) -> void {

            m_Clip = clip;
            m_AudioSource = m_Clip->CreateSource();
        }

        MKT_NODISCARD auto GetClip() const -> AudioHandle { return m_Clip; }

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

    private:
        AudioHandle m_Clip{};
        AudioSourceHandle m_AudioSource{};
    };


    class AudioListenerComponent : public BaseComponent<AudioListenerComponent> {
    public:
        explicit AudioListenerComponent() = default;

        AudioListenerComponent(const AudioListenerComponent & other) = default;
        AudioListenerComponent(AudioListenerComponent && other) = default;

        auto operator=(const AudioListenerComponent & other) -> AudioListenerComponent & = default;
        auto operator=(AudioListenerComponent && other) -> AudioListenerComponent & = default;

        ~AudioListenerComponent() = default;


        auto SetListener(AudioListener* listener) -> void {
            if (listener != nullptr) {
                m_Listener = listener;
            }
        }

        MKT_NODISCARD auto GetListener() -> AudioListener* { return m_Listener; }
        MKT_NODISCARD auto GetListener() const -> const AudioListener* { return m_Listener; }

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

    private:
        AudioListener* m_Listener{};
    };

    class RigidBodyComponent : public BaseComponent<RigidBodyComponent> {
    public:
        explicit RigidBodyComponent() = default;

        RigidBodyComponent(const RigidBodyComponent & other) = default;
        RigidBodyComponent(RigidBodyComponent && other) = default;

        auto operator=(const RigidBodyComponent & other) -> RigidBodyComponent & = default;
        auto operator=(RigidBodyComponent && other) -> RigidBodyComponent & = default;

        ~RigidBodyComponent() = default;

        MKT_NODISCARD auto GetMass() const -> float { return m_Mass; }

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

    private:
        float m_Mass{};
        float m_Friction{};
        bool m_UseGravity{};
    };

    class ColliderComponent : public BaseComponent<ColliderComponent> {
    public:
        explicit ColliderComponent() = default;

        ColliderComponent(const ColliderComponent & other) = default;
        ColliderComponent(ColliderComponent && other) = default;

        auto operator=(const ColliderComponent & other) -> ColliderComponent & = default;
        auto operator=(ColliderComponent && other) -> ColliderComponent & = default;

        ~ColliderComponent() = default;

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

    private:

    };


    class CameraComponent : public BaseComponent<CameraComponent> {
    public:
        explicit CameraComponent() = default;

        explicit CameraComponent( Scope_T<SceneCamera>&& camera,  const bool mainCam = true, const bool fixedAspectRation = false)
            :   m_Camera{ camera != nullptr ? std::move(camera) : CreateScope<SceneCamera>() }, m_MainCam{ mainCam }, m_FixedAspectRatio{ fixedAspectRation }
        {

        }

        CameraComponent(const CameraComponent& other) = default;
        CameraComponent(CameraComponent&& other) noexcept = default;

        auto operator=(const CameraComponent& other) -> CameraComponent& = default;
        auto operator=(CameraComponent&& other) -> CameraComponent& = default;

        MKT_NODISCARD auto IsMainCamera() const -> bool { return m_MainCam; }
        MKT_NODISCARD auto GetCamera() -> SceneCamera& { return *m_Camera; }
        MKT_NODISCARD auto GetCamera() const -> const SceneCamera& { return *m_Camera; }
        MKT_NODISCARD auto IsAspectRatioFixed() const -> bool { return m_FixedAspectRatio; }

        auto EnableFixedAspectRatio() -> void { m_FixedAspectRatio = true; }
        auto DisableFixedAspectRatio() -> void { m_FixedAspectRatio = false; }

        ~CameraComponent() = default;

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }
    private:
        Scope_T<SceneCamera> m_Camera{};

        bool m_MainCam{ true };
        bool m_FixedAspectRatio{ false };
    };

    class TextComponent : public BaseComponent<TextComponent> {
    public:
        explicit TextComponent() = default;

        TextComponent(const TextComponent& other) = default;
        TextComponent(TextComponent&& other) = default;

        auto operator=(const TextComponent& other) -> TextComponent& = default;
        auto operator=(TextComponent&& other) -> TextComponent& = default;

        auto LoadFont(Font* font) -> void {
            if (font) {
                m_Font = font;
            }
        }

        auto SetCamera(const Camera* camera) -> void {
            if (camera != nullptr) {
                m_Camera = camera;
            }
        }

        MKT_NODISCARD auto GetCamera() const -> const Camera* { return m_Camera; }

        MKT_NODISCARD auto GetFont() const -> const Font* { return m_Font; }
        MKT_NODISCARD auto GetColor() const -> const glm::vec4& { return m_Color; }

        MKT_NODISCARD auto GetSize() const -> float { return m_Size; }
        MKT_NODISCARD auto GetSpacing() const -> float { return m_Spacing; }
        MKT_NODISCARD auto GetContents() const -> const std::string& { return m_TextContent; }

        MKT_NODISCARD static auto GetMinLetterSpacing() -> float { return 1.0f; }
        MKT_NODISCARD static auto GetMaxLetterSpacing() -> float { return 10.0f; }

        MKT_NODISCARD static auto GetMinLetterSize() -> float { return 1.0f; }
        MKT_NODISCARD static auto GetMaxLetterSize() -> float { return 10.0f; }

        auto SetSize(const float value) -> void {
            if (value != 0) {
                m_Size = value;
            }
        }
        auto SetSpacing(const float value) -> void {
            if (value != 0) {
                m_Spacing = value;
            }
        }

        auto SetContents(const std::string_view content ) -> void { m_TextContent = content; }

        template<typename... Args>
        auto SetColor(Args&&... args ) -> void  { m_Color = glm::vec4{ std::forward<Args>(args)...}; }

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

    private:
        std::string m_TextContent{};

        glm::vec4 m_Color{ 1.0f, 1.0f, 0.4f, 1.0f };

        float m_Size{ 12 };
        float m_Spacing{ 0 };

        const Font* m_Font{ nullptr };
        const Camera* m_Camera{ nullptr };
    };

    // Scripting with c++
    class NativeScriptComponent : public BaseComponent<NativeScriptComponent> {
    public:
        explicit NativeScriptComponent(const Path_T& script)
            : m_ScriptPath{ script }
        {}

        NativeScriptComponent(const NativeScriptComponent& other) = default;
        NativeScriptComponent(NativeScriptComponent&& other) = default;

        auto operator=(const NativeScriptComponent& other) -> NativeScriptComponent& = default;
        auto operator=(NativeScriptComponent&& other) -> NativeScriptComponent& = default;

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

        ~NativeScriptComponent() = default;

    private:

        Path_T m_ScriptPath{};

    };

    class ScriptComponent : public BaseComponent<ScriptComponent> {
    public:
        explicit ScriptComponent(const Path_T& script)
            : m_ScriptPath{ script }
        {}

        ScriptComponent(const ScriptComponent& other) = default;
        ScriptComponent(ScriptComponent&& other) = default;

        auto operator=(const ScriptComponent& other) -> ScriptComponent& = default;
        auto operator=(ScriptComponent&& other) -> ScriptComponent& = default;

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

        ~ScriptComponent() = default;

    private:

        Path_T m_ScriptPath{};

    };
}

#endif // MIKOTO_COMPONENT_HH