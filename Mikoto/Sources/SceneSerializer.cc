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

#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <initializer_list>

#include <yaml-cpp/yaml.h>

#include <entt/entt.hpp>

#include <Logging/Assert.hh>
#include <Logging/Logger.hh>
#include <Scene/Entity.hh>
#include <Filesystem/FileService.hh>
#include <Scene/SceneSerializer.hh>
#include <Scene/Component.hh>

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
#define MKT_SERIALIZE_COMPONENT_IF_PRESENT( TYPE, KEY_NAME )           \
    if ( root->HasComponent<TYPE>() ) {                            \
        emitter << YAML::Key << KEY_NAME << YAML::Value;           \
        SerializeComponent( root->GetComponent<TYPE>(), emitter ); \
    }

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
        emitter << YAML::Key << "Visibility" << YAML::Value << tag.IsActive();
        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const MeshComponent& meshComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::Key << "MeshIndex" << YAML::Value << meshComponent.GetMeshIndex();
        emitter << YAML::Key << "ModelPath" << YAML::Value << meshComponent.GetModelPath().string();

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

    static auto SerializeComponent( const AudioSourceComponent& audioComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const RigidBodyComponent& physicsComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const ColliderComponent& physicsComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const CameraComponent& cameraComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const TextComponent& textComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::Key << "Contents" << YAML::Value << textComponent.GetContents();
        emitter << YAML::Key << "Color" << YAML::Value << textComponent.GetColor();
        emitter << YAML::Key << "IsWorldText" << YAML::Value << textComponent.IsWorldText();
        emitter << YAML::Key << "Size" << YAML::Value << textComponent.GetSize();
        emitter << YAML::Key << "Spacing" << YAML::Value << textComponent.GetSpacing();

        emitter << YAML::EndMap;
    }

    static auto SerializeComponent( const ScriptComponent& scriptComponent, YAML::Emitter& emitter ) -> void {
        emitter << YAML::BeginMap;

        emitter << YAML::EndMap;
    }

    static auto SerializeNode( YAML::Emitter& emitter, const Entity* root, const Scene& secene ) -> void {
        if ( root == nullptr ) {
            return;
        }

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Game Object";

        emitter << YAML::Key << "TagComponent" << YAML::Value;
        SerializeComponent( root->GetComponent<TagComponent>(), emitter );

        emitter << YAML::Key << "TransformComponent" << YAML::Value;
        SerializeComponent( root->GetComponent<TransformComponent>(), emitter );

        MKT_SERIALIZE_COMPONENT_IF_PRESENT( MeshComponent, "MeshComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( MaterialComponent, "MaterialComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( LightComponent, "LightComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( AudioSourceComponent, "AudioSourceComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( CameraComponent, "CameraComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( TextComponent, "TextComponent" );
        MKT_SERIALIZE_COMPONENT_IF_PRESENT( ScriptComponent, "ScriptComponent" );

        // I do not serialize the relationship component the hierarchy is stored splicitly by the nesting of the nodes in the yaml file. 
        // So if an entity has children they will be nested under it in the yaml file and if it does not have children it will just be a leaf node.
        for ( const auto& childID: root->GetComponent<RelationComponent>().GetChildren() ) {
            SerializeNode( emitter, secene.FindByID( childID ), secene );
        }

        emitter << YAML::EndMap;
    }

    auto SceneSerializer::Serialize( const Scene& scene, const Path& saveFilePath ) -> void {
        File* outputFile{ FileService::Get()->CreateNewFile( saveFilePath ) };

        if ( outputFile == nullptr ) {
            MKT_CORE_LOGGER_ERROR( "Could not open file '{}' required for scene serialization", saveFilePath.string() );
            return;
        }

        YAML::Emitter emitter{};

        emitter << YAML::BeginMap;
        emitter << YAML::Key << "Scene" << YAML::Value << scene.GetName();
        emitter << YAML::Key << "Objects" << YAML::Value << YAML::BeginSeq;

        for ( const auto& root: scene.GetRootEntities() ) {
            SerializeNode( emitter, root, scene );
        }

        emitter << YAML::EndSeq;
        emitter << YAML::EndMap;

        outputFile->SetContents( emitter.c_str() );
        outputFile->FlushContents();
    }

    auto SceneSerializer::Deserialize( const Path& saveFilePath ) -> Unique<Scene> {
        File* inputFile{ FileService::Get()->LoadFile( saveFilePath ) };

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
        Unique<Scene> result{ CreateScope<Scene>( sceneName ) };

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
