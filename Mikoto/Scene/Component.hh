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
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Project Headers
#include <Assets/AssetsService.hh>
#include <Assets/Audio.hh>
#include <Assets/Font.hh>
#include <Assets/Model.hh>
#include <Audio/AudioDevice.hh>
#include <Audio/AudioListener.hh>
#include <Common/Common.hh>
#include <Filesystem/FileService.hh>
#include <Library/Math/Math.hh>
#include <Library/Random/Random.hh>
#include <Library/Utility/Types.hh>
#include <Material/Material.hh>
#include <Renderer/Light.hh>
#include <Scene/SceneCamera.hh>

namespace Mikoto {

    class TagComponent {
    public:
        explicit TagComponent() = default;

        explicit TagComponent( const std::string_view tag )
            :   m_Tag{ tag }, m_Visibility{ true }
        {}

        TagComponent(const TagComponent& other) = default;
        TagComponent(TagComponent&& other) noexcept = default;

        auto operator=(const TagComponent& other) -> TagComponent& = default;
        auto operator=(TagComponent&& other) -> TagComponent& = default;

        MKT_NODISCARD auto IsVisible() const -> bool { return m_Visibility; }
        MKT_NODISCARD auto GetTag() const -> const std::string& { return m_Tag; }
        MKT_NODISCARD auto GetGUID() const -> UInt64 { return m_GUID.Get(); }

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



    class TransformComponent {
    public:
        explicit TransformComponent() = default;

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

            Vec3F translate{};
            Vec3F rotate{};
            Vec3F scale{};

            const bool success{ Math::DecomposeTransform(m_Transform, translate, rotate, scale) };

            if (success) {
                m_Scale = scale;
                m_Translation = translate;
                m_Rotation = rotate;
            }
        }

        auto SetTranslation(const Vec3F& value) -> void {
            m_Translation = value;

            m_Transform = Math::RecomputeTransform(m_Translation, m_Scale, m_Rotation);
        }

        auto SetRotation(const Vec3F& value) -> void {
            m_Rotation = value;

            m_Transform = Math::RecomputeTransform(m_Translation, m_Scale, m_Rotation);
        }

        auto SetScale(const Vec3F& value) -> void {
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

        auto SetUniformSale(const bool value) -> void { m_HasUniformScale = value; }

        ~TransformComponent() = default;

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

    private:


    private:
        // Transform vectors
        Vec3F m_Translation{};
        Vec3F m_Rotation{};
        Vec3F m_Scale{};

        // Model matrix (defines object translation, rotation and scale
        // according to the current transform values/vectors
        glm::mat4 m_Transform{};

        bool m_HasUniformScale{};
    };

    class RelationComponent {
    public:
        explicit RelationComponent() = default;

        RelationComponent(const RelationComponent& other) = default;
        RelationComponent(RelationComponent&& other) = default;

        auto operator=(const RelationComponent& other) -> RelationComponent& = default;
        auto operator=(RelationComponent&& other) -> RelationComponent& = default;

        auto RegisterChild(const UInt64 id) -> void { m_ChildrenIDs.emplace(id); }
        auto EraseChild(const UInt64 id) -> void { m_ChildrenIDs.erase(id); }

        MKT_NODISCARD auto IsChild(const UInt64 id) const -> bool {return m_ChildrenIDs.contains(id); }
        MKT_NODISCARD auto HasChildren() const -> bool {return !m_ChildrenIDs.empty(); }

        MKT_NODISCARD auto IsLeaf() const -> bool { return m_ChildrenIDs.empty(); }
        MKT_NODISCARD auto GetChildren() const -> decltype( auto ) { return (m_ChildrenIDs); }

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

        ~RelationComponent() = default;

    private:

        ankerl::unordered_dense::set<UInt64> m_ChildrenIDs{};
    };

    /**
     * Contains the material information of an entity. It describes how this object looks like
     * */
    class MaterialComponent {
    public:
        explicit MaterialComponent(MaterialHandle mat)
            : m_Material{ mat } {
        }

        MaterialComponent(MaterialComponent&&) = default;
        auto operator=(MaterialComponent&&) -> MaterialComponent& = default;

        MKT_NODISCARD auto GetMaterial() -> MaterialHandle { return m_Material; }

        ~MaterialComponent() = default;

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

    private:
        MaterialHandle m_Material{};
    };

    /**
     * This component will contain the data to render an object, such
     * as vertex buffers, index buffers, although this component won't be visible
     * in the editor UI
     * */
    class RenderComponent {
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
        MKT_NODISCARD auto GetPath() const -> const Path& { return m_Path; }
        MKT_NODISCARD auto GetName() const -> const std::string& { return m_Name; }

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

    private:
        const MeshNode* m_Mesh{};

        Path m_Path{};
        std::string m_Name{};
    };

    class LightComponent {
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
        MKT_NODISCARD auto SetActiveType(const LightType type) -> void { m_Type = type; }

        MKT_NODISCARD auto GetLight() -> Light& { return m_Data; }
        MKT_NODISCARD auto GetLight() const -> const Light& { return m_Data; }

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

    private:
        Light m_Data{};
        LightType m_Type{ LightType::POINT_LIGHT_TYPE };
    };


    class AudioSourceComponent {
    public:
        explicit AudioSourceComponent(const Path& path = "") {
            if ( auto file{ FileService::Get()->LoadFile( path ) }; !file ) {
                MKT_CORE_LOGGER_ERROR( "AudioSourceComponent - Failed to load audio file: {}", path.string() );
            } else {
                const AudioLoadDescription desc{
                    .AudioFile{ file },
                    .Volume{ 0.5f }
                };

                if ( AudioHandle handle{ AssetsService::Get()->LoadAsset<Audio>( desc ) }; handle.IsEmpty() ) {
                    MKT_CORE_LOGGER_ERROR( "AudioSourceComponent - Audio handle is empty: {}", path.string() );
                } else {
                    m_AudioSource = handle->CreateSource();
                }
            }
        }

        AudioSourceComponent(const AudioSourceComponent & other) = default;
        AudioSourceComponent(AudioSourceComponent && other) = default;

        auto operator=(const AudioSourceComponent & other) -> AudioSourceComponent & = default;
        auto operator=(AudioSourceComponent && other) -> AudioSourceComponent & = default;

        ~AudioSourceComponent() = default;

        MKT_NODISCARD auto GetSource() const -> AudioSourceHandle { return m_AudioSource; }

        auto SetClip( const AudioHandle& clip ) -> void {

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


    class AudioListenerComponent {
    public:
        explicit AudioListenerComponent() = default;

        AudioListenerComponent(const AudioListenerComponent & other) = default;
        AudioListenerComponent(AudioListenerComponent && other) = default;

        auto operator=(const AudioListenerComponent & other) -> AudioListenerComponent & = default;
        auto operator=(AudioListenerComponent && other) -> AudioListenerComponent & = default;

        ~AudioListenerComponent() = default;


        MKT_NODISCARD auto GetListener() -> AudioListener& { return m_Listener; }
        MKT_NODISCARD auto GetListener() const -> const AudioListener& { return m_Listener; }

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

    private:
        AudioListener m_Listener{};
    };

    class RigidBodyComponent {
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

    class ColliderComponent {
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
    };


    class CameraComponent {
    public:
        explicit CameraComponent() = default;

        explicit CameraComponent( Unique<SceneCamera>&& camera,  const bool mainCam = true, const bool fixedAspectRation = false)
            :   m_Camera{ camera != nullptr ? std::move(camera) : CreateScope<SceneCamera>() }, m_MainCam{ mainCam }, m_FixedAspectRatio{ fixedAspectRation }
        {

        }

        CameraComponent(CameraComponent&& other) noexcept = default;
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

        // Camera component has its own camera not shared
        DISABLE_COPY_FOR( CameraComponent );

    private:
        Unique<SceneCamera> m_Camera{};

        bool m_MainCam{ true };
        bool m_FixedAspectRatio{ false };
    };

    class TextComponent {
    public:
        explicit TextComponent() = default;

        TextComponent(const TextComponent& other) = default;
        TextComponent(TextComponent&& other) = default;

        auto operator=(const TextComponent& other) -> TextComponent& = default;
        auto operator=(TextComponent&& other) -> TextComponent& = default;

        auto SetFont( const Font* font) -> void {
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

        Vec4F m_Color{ 1.0f, 1.0f, 0.4f, 1.0f };

        float m_Size{ 12 };
        float m_Spacing{ 0 };

        const Font* m_Font{ nullptr };
        const Camera* m_Camera{ nullptr };
    };

    class ScriptComponent  {
    public:
        explicit ScriptComponent( const Path& script ) {
            m_Script = FileService::Get()->LoadFile( script );
        }

        ScriptComponent(const ScriptComponent& other) = default;
        ScriptComponent(ScriptComponent&& other) = default;

        auto operator=(const ScriptComponent& other) -> ScriptComponent& = default;
        auto operator=(ScriptComponent&& other) -> ScriptComponent& = default;

        auto OnComponentAttach() -> void {  }
        auto OnComponentUpdate() -> void {  }
        auto OnComponentRemoved() -> void {  }

        auto SetScript(const File* script ) -> void {
            m_Script = script;
        }

        MKT_NODISCARD auto GetScript() const -> const File* { return m_Script; }
        MKT_NODISCARD auto HasScript() const -> bool { return m_Script != nullptr; }

        ~ScriptComponent() = default;

    private:

        const File* m_Script{};
    };
}

#endif // MIKOTO_COMPONENT_HH