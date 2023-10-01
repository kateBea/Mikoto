/**
 * Serializer.hh
 * Created by kate on 9/30/23.
 * */

// C++ Standard Library
#include <memory>
#include <fstream>
#include <sstream>

// Third-Party Libraries
#include <entt/entt.hpp>
#include <yaml-cpp/yaml.h>

// Project Headers
#include <Core/Logger.hh>
#include <Core/Serializer.hh>
#include <Scene/Entity.hh>

namespace YAML {
    template<>
    struct convert<glm::vec3> {
        static Node encode(const glm::vec3& rhs) {
            Node node{};
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            return node;
        }

        static bool decode(const Node& node, glm::vec3& rhs) {
            if(!node.IsSequence() || node.size() != glm::vec3::length()) {
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
        static Node encode(const glm::vec4& rhs) {
            Node node{};
            node.push_back(rhs.x);
            node.push_back(rhs.y);
            node.push_back(rhs.z);
            node.push_back(rhs.w);
            return node;
        }

        static bool decode(const Node& node, glm::vec4& rhs) {
            if(!node.IsSequence() || node.size() != glm::vec4::length()) {
                return false;
            }

            rhs.x = node[0].as<glm::vec4::type::value_type>();
            rhs.y = node[1].as<glm::vec4::type::value_type>();
            rhs.z = node[2].as<glm::vec4::type::value_type>();
            rhs.w = node[3].as<glm::vec4::type::value_type>();

            return true;
        }
    };
}

namespace Mikoto::Serializer {
    static auto operator<<(YAML::Emitter& out, const glm::vec4& v) -> YAML::Emitter& {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << v.w << YAML::EndSeq;
        return out;
    }

    static auto operator<<(YAML::Emitter& out, const glm::vec3& v) -> YAML::Emitter& {
        out << YAML::Flow;
        out << YAML::BeginSeq << v.x << v.y << v.z << YAML::EndSeq;
        return out;
    }

    static auto SerializeEntityComponent(const TransformComponent& transform, YAML::Emitter& emitter) -> void {
        const auto& position{ transform.GetTranslation() };
        const auto& rotation{ transform.GetRotation() };
        const auto& scale{ transform.GetScale() };

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Position" << YAML::Value << position;
        emitter << YAML::Key << "Rotation" << YAML::Value << rotation;
        emitter << YAML::Key << "Scale" << YAML::Value << scale;
        emitter << YAML::EndMap;
    }

    static auto SerializeEntityComponent(const TagComponent& tag, YAML::Emitter& emitter) -> void {
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Name" << YAML::Value << tag.GetTag();
        emitter << YAML::Key << "Visibility" << YAML::Value << tag.IsVisible();
        emitter << YAML::EndMap;
    }

    static auto SerializeEntityComponent(const RenderComponent& renderComponent, YAML::Emitter& emitter) -> void {
        emitter << YAML::BeginMap;

        const auto& renderData{ renderComponent.GetObjectData() };

        emitter << YAML::Key << "IsPrefab" << YAML::Value << renderData.IsPrefab;

        if (renderData.IsPrefab) {
            emitter << YAML::Key << "PrefabType" << YAML::Value << PrefabTypeStr(renderData.PrefabType).data();
        }

        emitter << YAML::EndMap;
    }

    static auto SerializeEntityComponent(const MaterialComponent& material, YAML::Emitter& emitter) -> void {
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Color" << YAML::Value << material.GetColor();
        emitter << YAML::EndMap;
    }

    static auto SerializeEntity(YAML::Emitter& emitter, const entt::registry& reg, const entt::entity& entity) -> void {
        // Map containing entity components
        emitter << YAML::BeginMap;

        emitter << YAML::Key << "Object";
        emitter << YAML::Value << "12345678971"; // TODO: object ID goes here (it is a global unique identifier)

        // By default, all objects from a Scene have a Tag Component, no need to check if it exists
        emitter << YAML::Key << "TagComponent" << YAML::Value;
        SerializeEntityComponent(reg.get<TagComponent>(entity), emitter);

        // By default, all objects from a Scene have a Transform Component, no need to check if it exists
        emitter << YAML::Key << "TransformComponent" << YAML::Value;
        SerializeEntityComponent(reg.get<TransformComponent>(entity), emitter);

        // Serialize Render Component
        if (reg.all_of<RenderComponent>(entity)) {
            emitter << YAML::Key << "RenderComponent" << YAML::Value;
            SerializeEntityComponent(reg.get<RenderComponent>(entity), emitter);
        }

        // Serialize Material Component
        if (reg.all_of<MaterialComponent>(entity)) {
            emitter << YAML::Key << "MaterialComponent" << YAML::Value;
            SerializeEntityComponent(reg.get<MaterialComponent>(entity), emitter);
        }

        emitter << YAML::EndMap;
    }

    SceneSerializer::SceneSerializer(std::shared_ptr<Scene> scene)
        :   m_Scene{ std::move(scene) }
    {

    }

    auto SceneSerializer::Serialize(const Path_T& saveFilePath) -> void {
        // YAML Emitter
        YAML::Emitter emitter{};

        // Overwrite the file
        std::ofstream outputFile{ saveFilePath, std::ios_base::out | std::ios_base::trunc };

        if (!outputFile.is_open()) {
            MKT_CORE_LOGGER_ERROR("Could not open file '{}' required for scene serialization", saveFilePath.string());
            return;
        }

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Scene" << YAML::Value << m_Scene->GetName();
        emitter << YAML::Key << "Objects" << YAML::Value << YAML::BeginSeq;

        m_Scene->GetRegistry().each([&](auto entity) -> void {
            if (entity != entt::null) {
                SerializeEntity(emitter, m_Scene->GetRegistry(), entity);
            }
        });

        emitter << YAML::EndSeq;
        emitter << YAML::EndMap;

        outputFile << emitter.c_str();
    }

    auto SceneSerializer::Deserialize(const Path_T& saveFilePath) -> void {
        std::ifstream inputFile{ saveFilePath };

        if (!inputFile.is_open()) {
            MKT_CORE_LOGGER_ERROR("Could not open file '{}' required for scene deserialization", saveFilePath.string());
            return;
        }

        std::stringstream stream{};
        stream << inputFile.rdbuf();

        YAML::Node data{ YAML::Load(stream.str()) };

        if (data.IsNull()) {
            MKT_CORE_LOGGER_WARN("File opened '{}' but contains no data for deserialization", saveFilePath.string());
            return;
        }

        if (data["Scene"].IsNull()) {
            MKT_CORE_LOGGER_WARN("File opened '{}' but contains Scene Node", saveFilePath.string());
            return;
        }

        const std::string sceneName{ data["Scene"].as<std::string>() };
        const auto sceneEntities{ data["Objects"] };

        if (!sceneEntities.IsNull()) {
            for (const auto& object : sceneEntities) {
                Entity entity{};

                // Get the entity ID
                const std::string uuid{ object["Object"].as<std::string >() };
                const std::string name{ object["TagComponent"]["Name"].as<std::string>() };

                // Get Render component
                if (!object["RenderComponent"].IsNull()) {
                    const bool isPrefab{ object["RenderComponent"]["IsPrefab"].as<bool>() };
                    PrefabSceneObject type{};

                    if (isPrefab) {
                        type = PrefabTypeFromName(object["RenderComponent"]["PrefabType"].as<std::string>());
                        entity = Scene::CreatePrefabObject(name, m_Scene, type);
                    }
                    else {
                        entity = Scene::CreateEmptyObject(name, m_Scene);
                    }
                }

                // Get Material component
                if (!object["MaterialComponent"].IsNull()) {
                    const auto color{ object["MaterialComponent"]["Color"].as<glm::vec4>() };
                    entity.GetComponent<MaterialComponent>().SetColor(color);
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
    }
}
