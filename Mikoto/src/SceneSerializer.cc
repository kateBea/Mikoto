/**
 * Serializer.hh
 * Created by kate on 9/30/23.
 * */

// C++ Standard Library
#include <algorithm>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Third-Party Libraries
#include <yaml-cpp/yaml.h>

#include <entt/entt.hpp>
#include <nfd.hpp>

// Project Headers
#include <Core/Logging/Assert.hh>
#include <Core/Logging/Logger.hh>
#include <Core/System/FileSystem.hh>
#include <Scene/Scene/Entity.hh>
#include <Scene/SceneSerializer.hh>

namespace YAML {
    template<>
    struct convert<glm::vec3> {
        static Node encode( const glm::vec3& rhs ) {
            Node node{};
            node.push_back( rhs.x );
            node.push_back( rhs.y );
            node.push_back( rhs.z );
            return node;
        }

        static bool decode( const Node& node, glm::vec3& rhs ) {
            if ( !node.IsSequence() || node.size() != glm::vec3::length() ) {
                return false;
            }

            rhs.x = node[0].as<glm::vec3::type::value_type>();
            rhs.y = node[1].as<glm::vec3::type::value_type>();
            rhs.z = node[2].as<glm::vec3::type::value_type>();

            return true;
        }
    };

    template<>
    struct convert<glm::vec4> {
        static Node encode( const glm::vec4& rhs ) {
            Node node{};
            node.push_back( rhs.x );
            node.push_back( rhs.y );
            node.push_back( rhs.z );
            node.push_back( rhs.w );
            return node;
        }

        static bool decode( const Node& node, glm::vec4& rhs ) {
            if ( !node.IsSequence() || node.size() != glm::vec4::length() ) {
                return false;
            }

            rhs.x = node[0].as<glm::vec4::type::value_type>();
            rhs.y = node[1].as<glm::vec4::type::value_type>();
            rhs.z = node[2].as<glm::vec4::type::value_type>();
            rhs.w = node[3].as<glm::vec4::type::value_type>();

            return true;
        }
    };
}// namespace YAML

namespace Mikoto {
    static auto operator<<( YAML::Emitter& out, const glm::vec4& v ) -> YAML::Emitter& {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
        return out;
    }

    static auto operator<<( YAML::Emitter& out, const glm::vec3& v ) -> YAML::Emitter& {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
        return out;
    }

    static auto SerializeComponent( const TransformComponent& transform, YAML::Emitter& emitter ) -> void {
        const auto& position{ transform.GetTranslation() };
        const auto& rotation{ transform.GetRotation() };
        const auto& scale{ transform.GetScale() };
        const auto& uniformScale{ transform.HasUniformScale() };

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Position" << YAML::Value << position;
        emitter << YAML::Key << "Rotation" << YAML::Value << rotation;
        emitter << YAML::Key << "Scale" << YAML::Value << scale;
        emitter << YAML::Key << "Uniform Scale" << YAML::Value << uniformScale;
        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const TagComponent& tag, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Name" << YAML::Value << tag.GetTag();
        emitter << YAML::Key << "Visibility" << YAML::Value << tag.IsVisible();
        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const RenderComponent& renderComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;


        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const MaterialComponent& materialComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const LightComponent& lightComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const AudioComponent& audioComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const PhysicsComponent& physicsComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const CameraComponent& cameraComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const TextComponent& textComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const NativeScriptComponent& nativeScriptComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeNode( YAML::Emitter& emitter, const GenTree<Entity*>::Node* node ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::Key << "Game Object";

        Entity* rootEntity{ node->data };

        // Game object name
        emitter << YAML::Value << StringUtils::ToString( rootEntity->GetComponent<TagComponent>().GetTag() );

        // Serialize TagComponent and TransformComponent (all entities have it by default)
        emitter << YAML::Key << "TagComponent" << YAML::Value;
        SerializeComponent( rootEntity->GetComponent<TagComponent>(), emitter );

        emitter << YAML::Key << "TransformComponent" << YAML::Value;
        SerializeComponent( rootEntity->GetComponent<TransformComponent>(), emitter );

        // Serialize Render Component
        if ( rootEntity->HasComponent<RenderComponent>() ) {
            emitter << YAML::Key << "RenderComponent" << YAML::Value;
            SerializeComponent( rootEntity->GetComponent<RenderComponent>(), emitter );
        }

        // Serialize Material Component
        if ( rootEntity->HasComponent<MaterialComponent>() ) {
            emitter << YAML::Key << "MaterialComponent" << YAML::Value;
            SerializeComponent( rootEntity->GetComponent<MaterialComponent>(), emitter );
        }

        // Serialize Light Component
        if ( rootEntity->HasComponent<LightComponent>() ) {
            emitter << YAML::Key << "LightComponent" << YAML::Value;
            SerializeComponent( rootEntity->GetComponent<LightComponent>(), emitter );
        }

        // Serialize Audio Component
        if ( rootEntity->HasComponent<AudioComponent>() ) {
            emitter << YAML::Key << "AudioComponent" << YAML::Value;
            SerializeComponent( rootEntity->GetComponent<AudioComponent>(), emitter );
        }

        // Serialize Physics Component
        if ( rootEntity->HasComponent<PhysicsComponent>() ) {
            emitter << YAML::Key << "PhysicsComponent" << YAML::Value;
            SerializeComponent( rootEntity->GetComponent<PhysicsComponent>(), emitter );
        }

        // Serialize Camera Component
        if ( rootEntity->HasComponent<CameraComponent>() ) {
            emitter << YAML::Key << "CameraComponent" << YAML::Value;
            SerializeComponent( rootEntity->GetComponent<CameraComponent>(), emitter );
        }

        // Serialize Text Component
        if ( rootEntity->HasComponent<TextComponent>() ) {
            emitter << YAML::Key << "TextComponent" << YAML::Value;
            SerializeComponent( rootEntity->GetComponent<TextComponent>(), emitter );
        }

        // Serialize Script Component
        if ( rootEntity->HasComponent<NativeScriptComponent>() ) {
            emitter << YAML::Key << "NativeScriptComponent" << YAML::Value;
            SerializeComponent( rootEntity->GetComponent<NativeScriptComponent>(), emitter );
        }

        if ( !node->IsLeaf() ) {
            for ( const auto& childrenNode: node->children ) {
                SerializeNode( emitter, childrenNode.get() );
            }
        }

        emitter << YAML::EndMap;
    }

    auto SceneSerializer::Serialize( const Scene& scene, const Path_T& saveFilePath ) -> void {
        FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };

        File* outputFile{ fileSystem.LoadFile( saveFilePath, MKT_FILE_OPEN_MODE_WRITE ) };

        if ( outputFile == nullptr ) {
            MKT_CORE_LOGGER_ERROR( "Could not open file '{}' required for scene serialization", saveFilePath.string() );
            return;
        }

        YAML::Emitter emitter{};

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Scene" << YAML::Value << scene.GetName();
        emitter << YAML::Key << "Objects" << YAML::Value << YAML::BeginSeq;

        const auto& hierarchy{ scene.GetHierarchy() };

        for ( const auto& entityNode: hierarchy.GetNodes() ) {
            SerializeNode( emitter, entityNode.get() );
        }

        emitter << YAML::EndSeq;
        emitter << YAML::EndMap;

        outputFile->FlushContents( emitter.c_str(), MKT_FILE_OPEN_MODE_TRUNCATE );
    }

    auto SceneSerializer::Deserialize( const Path_T& saveFilePath ) -> Scope_T<Scene> {
        FileSystem& fileSystem{ Engine::GetSystem<FileSystem>() };

        File* inputFile{ fileSystem.LoadFile( saveFilePath ) };

        if ( inputFile == nullptr ) {
            MKT_CORE_LOGGER_ERROR( "Could not open file '{}' required for scene serialization", saveFilePath.string() );
            return nullptr;
        }

        YAML::Node data{ YAML::Load( inputFile->GetFileContents() ) };

        if (data.IsNull()) {
            auto message{ fmt::format("File opened '{}' but contains no data for deserialization", saveFilePath.string()) };

            MKT_CORE_LOGGER_WARN( "{}", message );
            return nullptr;
        }

        if (data["Scene"].IsNull()) {
            auto message{ fmt::format("File opened [{}] but contains Scene Node", saveFilePath.string()) };

            MKT_CORE_LOGGER_WARN( "{}", message );
            return nullptr;
        }

        // Recreate a new scene on top of which we are going to deserialize
        const std::string sceneName{ data["Scene"].as<std::string>() };
        Scope_T<Scene> result{ CreateScope<Scene>( sceneName ) };

        const auto sceneEntities{ data["Objects"] };

        if ( !sceneEntities.IsNull() ) {

#if false
            EntityCreateInfo entityCreateInfo{};

            for (const auto& object : sceneEntities) {
                Entity entity{};

                // Get the entity ID
                const std::string uuid{ object["Object"].as<std::string>() };
                const std::string name{ object["TagComponent"]["Name"].as<std::string>() };

                // Get Render component
                if (!object["RenderComponent"].IsNull()) {
                    const bool isPrefab{ object["RenderComponent"]["IsPrefab"].as<bool>() };

                    entityCreateInfo.Name = name;
                    entityCreateInfo.PrefabType = PrefabSceneObject::NO_PREFAB_OBJECT;

                    if (isPrefab) {
                        // Get the type of prefab if it was one
                        entityCreateInfo.PrefabType = PrefabTypeFromName(object["RenderComponent"]["PrefabType"].as<std::string>());
                    }

                    //SceneManager::AddEntityToScene(newScene, entityCreateInfo);
                }

                // Get Material component
                if (!object["MaterialComponent"].IsNull()) {
                    const auto color{ object["MaterialComponent"]["Color"].as<glm::vec4>() };
                }

                // Get the Tag component
                TagComponent tagComponent{};
                tagComponent.SetTag(name);
                tagComponent.SetVisibility(object["TagComponent"]["Visibility"].as<bool>());
                MKT_CORE_LOGGER_INFO("Found entity with name {}", tagComponent.GetTag());

                // Get Transform component
                TransformComponent transformComponent{};
                const auto position{ object["TransformComponent"]["Position"].as<glm::vec3>() };
                const auto rotation{ object["TransformComponent"]["Rotation"].as<glm::vec3>() };
                const auto scale{ object["TransformComponent"]["Scale"].as<glm::vec3>() };
                transformComponent.ComputeTransform(position, scale, rotation);

                entity.GetComponent<TransformComponent>() = transformComponent;
                entity.GetComponent<TagComponent>() = tagComponent;
            }
        }
        else {
            MKT_CORE_LOGGER_INFO("File opened '{}' but has no scene objects", saveFilePath.string());
        }
#endif
        }

        return nullptr;
    }
}// namespace Mikoto
