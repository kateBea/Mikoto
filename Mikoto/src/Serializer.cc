/**
 * Serializer.hh
 * Created by kate on 9/30/23.
 * */

// C++ Standard Library
#include <memory>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <initializer_list>

// Third-Party Libraries
#include <nfd.hpp>
#include <entt/entt.hpp>
#include <yaml-cpp/yaml.h>

// Project Headers
#include <Core/Assert.hh>
#include <Core/FileManager.hh>
#include <Core/Logger.hh>
#include <Scene/Entity.hh>
#include <Scene/SceneManager.hh>

#include "../../Mikoto-Editor/Tools/ConsoleManager.hh"

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

namespace Mikoto::FileManager {
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


        emitter << YAML::EndMap;
    }

    static auto SerializeEntityComponent(const MaterialComponent& material, YAML::Emitter& emitter) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeEntity(YAML::Emitter& emitter, const entt::registry& reg, const entt::entity& entity) -> void {
        // Map containing entity components
        emitter << YAML::BeginMap;

        emitter << YAML::Key << "Object";
        emitter << YAML::Value << std::to_string(reg.get<TagComponent>(entity).GetGUID());

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

    auto SceneSerializer::Serialize(Scene& scene, const Path_T &saveFilePath) -> void {
        // YAML Emitter
        YAML::Emitter emitter{};

        // Overwrite the file
        std::ofstream outputFile{ saveFilePath, std::ios_base::out | std::ios_base::trunc };

        if (!outputFile.is_open()) {
            MKT_CORE_LOGGER_ERROR("Could not open file '{}' required for scene serialization", saveFilePath.string());
            return;
        }

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Scene" << YAML::Value << scene.GetName();
        emitter << YAML::Key << "Objects" << YAML::Value << YAML::BeginSeq;

        for (const auto& [entityId, storage] : scene.GetRegistry().storage()) {
            if (entityId != entt::null) {
                SerializeEntity(emitter, scene.GetRegistry(), static_cast<const entt::entity>( entityId ) );
            }
        }

        emitter << YAML::EndSeq;
        emitter << YAML::EndMap;

        outputFile << emitter.c_str();
    }

    auto SceneSerializer::Deserialize(const Path_T &saveFilePath) -> void {
        std::ifstream inputFile{ saveFilePath };

        if (!inputFile.is_open()) {
            MKT_CORE_LOGGER_ERROR("Could not open file '{}' required for scene deserialization", saveFilePath.string());
            return;
        }

        std::stringstream stream{};
        stream << inputFile.rdbuf();

        YAML::Node data{ YAML::Load(stream.str()) };

        if (data.IsNull()) {
            auto message{ fmt::format("File opened '{}' but contains no data for deserialization", saveFilePath.string()) };

            ConsoleManager::PushMessage(ConsoleLogLevel::CONSOLE_ERROR, message);
            MKT_CORE_LOGGER_WARN("{}", message);
            return;
        }

        if (data["Scene"].IsNull()) {
            auto message{ fmt::format("File opened [{}] but contains Scene Node", saveFilePath.string()) };

            ConsoleManager::PushMessage(ConsoleLogLevel::CONSOLE_ERROR, message);
            MKT_CORE_LOGGER_WARN("{}", message);
            return;
        }

        // Recreate a new scene on top of which we are going to deserialize
        const std::string sceneName{ data["Scene"].as<std::string>() };
        auto& newScene{ SceneManager::MakeNewScene(sceneName) };
        SceneManager::SetActiveScene(newScene);

        const auto sceneEntities{ data["Objects"] };

        if (!sceneEntities.IsNull()) {
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
    }

    auto SaveDialog(const std::string& defaultName, const std::initializer_list<std::pair<std::string, std::string>>& filters) -> std::string {
        std::string saveFilePath{};

        // Process filters
        std::vector<nfdfilteritem_t> filterItems{};

        for (const auto& [filterName, filterExtensions] : filters) {
            filterItems.emplace_back(nfdfilteritem_t{ filterName.data(), filterExtensions.data() });
        }

        // initialize NFD
        NFD::Guard nfdGuard{};

        // auto-freeing memory
        NFD::UniquePath outPath{};

        // show the dialog
        nfdresult_t result{ NFD::SaveDialog(outPath, filterItems.data(), filterItems.size(), nullptr, defaultName.data()) };

        if (result == NFD_OKAY) {
            saveFilePath = outPath.get();
        }
        else if (result == NFD_CANCEL) {
            MKT_CORE_LOGGER_INFO("User canceled File open dialog");
        }
        else {
            MKT_CORE_LOGGER_ERROR("Error in  File open dialog: {}", NFD::GetError());
        }

        // NFD::Guard will automatically quit NFD.

        return saveFilePath;
    }

    auto OpenDialog(const std::initializer_list<std::pair<std::string, std::string>>& filters) -> std::string {
        std::string filePath{};

        // Process filters
        std::vector<nfdfilteritem_t> filterItems{};

        for (const auto& [filterName, filterExtensions] : filters) {
            filterItems.emplace_back(nfdfilteritem_t{ filterName.data(), filterExtensions.data() });
        }

        // initialize NFD
        NFD::Guard nfdGuard{};

        // auto-freeing memory
        NFD::UniquePath outPath{};

        // show the dialog
        nfdresult_t result{ NFD::OpenDialog(outPath, filterItems.data(), filterItems.size()) };

        if (result == NFD_OKAY) {
            filePath = outPath.get();
        }
        else if (result == NFD_CANCEL) {
            MKT_CORE_LOGGER_INFO("User canceled File open dialog");
        }
        else {
            MKT_CORE_LOGGER_ERROR("Error in  File open dialog: {}", NFD::GetError());
        }

        // NFD::Guard will automatically quit NFD.

        return filePath;
    }

    auto Init() -> void {
        const auto result{ NFD::Init() == NFD_OKAY };
        if (!result) {
            MKT_THROW_RUNTIME_ERROR("Failed to initialized File dialog library NFD");
        }
    }

    auto Shutdown() -> void {
        NFD::Quit();
    }
}
