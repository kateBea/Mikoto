/**
 * Serializer.hh
 * Created by kate on 9/30/23.
 * */

// C++ Standard Library
#include <memory>
#include <fstream>

// Third-Party Libraries
#include <entt/entt.hpp>
#include <yaml-cpp/yaml.h>

// Project Headers
#include <Core/Logger.hh>
#include <Core/Serializer.hh>
#include <Scene/Entity.hh>

namespace Mikoto::Serializer {
    static auto SerializeEntityComponent(const TransformComponent& transform, YAML::Emitter& emitter) -> void {

        const auto& position{ transform.GetTranslation() };
        const auto& rotation{ transform.GetRotation() };
        const auto& scale{ transform.GetScale() };

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Position:" << YAML::Value << YAML::Flow << YAML::FloatPrecision(3) << YAML::BeginSeq << position.x << position.y << position.z << YAML::EndSeq;
        emitter << YAML::Key << "Rotation:" << YAML::Value << YAML::Flow << YAML::FloatPrecision(3) << YAML::BeginSeq << rotation.x << rotation.y << rotation.z << YAML::EndSeq;
        emitter << YAML::Key << "Scale:" << YAML::Value << YAML::Flow << YAML::FloatPrecision(3) << YAML::BeginSeq << scale.x << scale.y << scale.z << YAML::EndSeq;
        emitter << YAML::EndMap;
    }

    static auto SerializeEntityComponent(const TagComponent& tag, YAML::Emitter& emitter) -> void {
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Name:" << YAML::Value << tag.GetTag();
        emitter << YAML::Key << "Visibility:" << YAML::Value << tag.IsVisible();
        emitter << YAML::EndMap;
    }

    static auto SerializeEntity(YAML::Emitter& emitter, const entt::registry& reg, const entt::entity& entity) -> void {
        emitter << YAML::BeginMap;

        // By default, all objects from a Scene have a Tag Component, no need to check if it exists
        {
            emitter << YAML::Key << "Tag Component";
            emitter << YAML::Value;
            SerializeEntityComponent(reg.get<TagComponent>(entity), emitter);
        }

        if (reg.all_of<TagComponent>(entity)) {
            emitter << YAML::Key << "Transform Component";
            emitter << YAML::Value;
            SerializeEntityComponent(reg.get<TransformComponent>(entity), emitter);
        }

        emitter << YAML::EndMap;
    }

    SceneSerializer::SceneSerializer(std::shared_ptr<Scene> scene)
        :   m_Scene{ std::move(scene) }
    {

    }

    auto SceneSerializer::SerializeScene(const Path_T& saveFilePath) -> void {
        // YAML Emitter
        YAML::Emitter emitter{};

        // Overwrite the file
        std::ofstream outputFile{ saveFilePath, std::ios_base::out | std::ios_base::trunc };

        if (!outputFile.is_open()) {
            MKT_CORE_LOGGER_ERROR("Could not open file '{}' required for scene serialization", saveFilePath.string());
            return;
        }

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Scene: " << YAML::Value << m_Scene->GetName();
        emitter << YAML::Key << "Scene Objects: " << YAML::Value << YAML::BeginSeq;

        m_Scene->GetRegistry().each([&](auto entity) -> void {
            if (entity != entt::null) {
                SerializeEntity(emitter, m_Scene->GetRegistry(), entity);
            }
        });

        emitter << YAML::EndSeq;
        emitter << YAML::EndMap;

        outputFile << emitter.c_str();
    }
}
