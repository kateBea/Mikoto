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


#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Scene/Entity.hh>
#include <Scene/Component.hh>

#include <Scripting/Lua/LuaBindings.hh>


namespace mikoto::scripting {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::filesystem;

    template<typename Light>
    static auto BindLocalLight( sol::state& state, core::cstr name ) -> sol::usertype<Light> {
        MKT_BEGIN_PROFILER();
        auto light{ state.new_usertype<Light>( name, sol::no_constructor ) };
        light.set_function( "GetIntensity", []( const Light& value ) {
            return value.GetIntensity();
        } );
        light.set_function( "SetIntensity", []( Light& value, core::f32 intensity ) {
            value.SetIntensity( intensity );
        } );
        light.set_function( "GetRadius", &Light::GetRadius );
        light.set_function( "SetRadius", &Light::SetRadius );
        light.set_function( "GetColor", []( const Light& value ) -> core::float3 {
            return value.GetColor();
        } );
        light.set_function( "SetColor", []( Light& value, const core::float3& color ) {
            value.SetColor( glm::max( color, core::float3{ 0.0f } ) );
        } );
        return light;
    }

    auto SceneBinding::Init( sol::state& state ) -> void {
        MKT_BEGIN_PROFILER();
        auto entity{ state.new_usertype<scene::Entity>( "Entity" ) };

        entity.set_function( "GetCanvas", []( scene::Entity& value ) -> renderer::Canvas* {
            return value.HasComponent<scene::CanvasComponent>() ? &value.GetComponent<scene::CanvasComponent>().GetCanvas() : nullptr;
        } );
        auto canvas = state.new_usertype<renderer::Canvas>( "Canvas", sol::no_constructor );
        canvas.set_function( "GetElementCount", []( const renderer::Canvas& value ) {
            return value.GetCommands().size();
        } );
        // Keep legacy one-based command indices for existing scripts. New scripts
        // use widget IDs so reordering the hierarchy cannot redirect an action.
        canvas.set_function( "ConsumeClick", []( renderer::Canvas& value, core::usize index ) {
            return index > 0 && value.ConsumeClick( index - 1 );
        } );
        canvas.set_function( "GetValue", []( const renderer::Canvas& value, core::usize index ) {
            return index > 0 && index <= value.GetCommands().size() ? value.GetCommands()[index - 1].mValue : 0.0f;
        } );
        canvas.set_function( "SetValue", []( renderer::Canvas& value, core::usize index, float number ) {
            if ( index > 0 && index <= value.GetCommands().size() ) {
                value.GetEditableCommands()[index - 1].mValue = glm::clamp( number, 0.0f, 1.0f );
            }
        } );
        canvas.set_function( "SetText", []( renderer::Canvas& value, core::usize index, const char* text ) {
            if ( index > 0 && index <= value.GetCommands().size() ) {
                value.GetEditableCommands()[index - 1].mText = text ? text : "";
            }
        } );
        canvas.set_function( "SetVisible", []( renderer::Canvas& value, core::usize index, bool visible ) {
            if ( index > 0 && index <= value.GetCommands().size() ) {
                value.GetEditableCommands()[index - 1].mVisible = visible;
            }
        } );
        canvas.set_function( "SetEnabled", []( renderer::Canvas& value, core::usize index, bool enabled ) {
            if ( index > 0 && index <= value.GetCommands().size() ) {
                value.GetEditableCommands()[index - 1].mEnabled = enabled;
            }
        } );
        canvas.set_function( "FindElement", []( const renderer::Canvas& value, const char* name ) {
            return value.FindElement( eastl::string_view{ name ? name : "" } );
        } );
        canvas.set_function( "ConsumeElementClick", &renderer::Canvas::ConsumeElementClick );
        canvas.set_function( "ConsumeValueChange", &renderer::Canvas::ConsumeValueChange );
        canvas.set_function( "SetElementValue", &renderer::Canvas::SetValue );
        canvas.set_function( "GetElementValue", []( const renderer::Canvas& value, renderer::CanvasElementId id ) {
            const auto* command{ value.FindElement( id ) };
            return command ? command->mValue : 0.0f;
        } );
        canvas.set_function( "SetElementText", []( renderer::Canvas& value, renderer::CanvasElementId id, const char* text ) {
            if ( auto* command{ value.FindElement( id ) } ) {
                command->mText = text ? text : "";
            }
        } );
        canvas.set_function( "SetElementVisible", []( renderer::Canvas& value, renderer::CanvasElementId id, bool visible ) {
            if ( auto* command{ value.FindElement( id ) } ) {
                command->mVisible = visible;
            }
        } );
        canvas.set_function( "SetElementEnabled", []( renderer::Canvas& value, renderer::CanvasElementId id, bool enabled ) {
            if ( auto* command{ value.FindElement( id ) } ) {
                command->mEnabled = enabled;
            }
        } );
        canvas.set_function( "SetElementInteractable", []( renderer::Canvas& value, renderer::CanvasElementId id, bool interactable ) {
            if ( auto* command{ value.FindElement( id ) } ) {
                command->mInteractable = interactable;
            }
        } );
        canvas.set_function( "IsElementHovered", []( const renderer::Canvas& value, renderer::CanvasElementId id ) {
            const auto* command{ value.FindElement( id ) };
            return command && command->mHovered;
        } );
        canvas.set_function( "IsElementPressed", []( const renderer::Canvas& value, renderer::CanvasElementId id ) {
            const auto* command{ value.FindElement( id ) };
            return command && command->mPressed;
        } );
        canvas.set_function( "IsElementFocused", []( const renderer::Canvas& value, renderer::CanvasElementId id ) {
            const auto* command{ value.FindElement( id ) };
            return command && command->mFocused;
        } );

        // Return the active light payload, or nil for an absent/different type.
        // Scripts update authored ECS data; renderer uploads happen normally.
        entity.set_function( "GetPointLight", []( scene::Entity& value ) -> renderer::PointLight* {
            if ( !value.HasComponent<scene::LightComponent>() || !value.GetComponent<scene::LightComponent>().IsTypeActive( renderer::LightType::ePoint ) ) {
                return nullptr;
            }

            return &value.GetComponent<scene::LightComponent>().Get<renderer::PointLight>();
        } );
        entity.set_function( "GetSpotLight", []( scene::Entity& value ) -> renderer::SpotLight* {
            if ( !value.HasComponent<scene::LightComponent>() || !value.GetComponent<scene::LightComponent>().IsTypeActive( renderer::LightType::eSpot ) ) {
                return nullptr;
            }

            return &value.GetComponent<scene::LightComponent>().Get<renderer::SpotLight>();
        } );
        BindLocalLight<renderer::PointLight>( state, "PointLight" );
        auto spot{ BindLocalLight<renderer::SpotLight>( state, "SpotLight" ) };
        spot.set_function( "GetDirection", []( const renderer::SpotLight& value ) -> core::float3 {
            return value.GetDirection();
        } );
        spot.set_function( "SetDirection", []( renderer::SpotLight& value, const core::float3& direction ) {
            value.SetDirection( direction );
        } );
        spot.set_function( "GetAngle", &renderer::SpotLight::GetAngle );
        spot.set_function( "SetAngle", &renderer::SpotLight::SetAngle );
        spot.set_function( "GetSoftness", &renderer::SpotLight::GetSoftness );
        spot.set_function( "SetSoftness", &renderer::SpotLight::SetSoftness );

        entity.set_function( "GetRigidBody", []( scene::Entity& entity ) -> scene::RigidBodyComponent* {
            if ( !entity.HasComponent<scene::RigidBodyComponent>() ) {
                return nullptr;
            }

            return &entity.GetComponent<scene::RigidBodyComponent>();
        } );
        auto rigidBody{ state.new_usertype<scene::RigidBodyComponent>( "RigidBodyComponent", sol::no_constructor ) };
        rigidBody.set_function( "GetLinearVelocity", &scene::RigidBodyComponent::GetLinearVelocity );
        rigidBody.set_function( "GetAngularVelocity", &scene::RigidBodyComponent::GetAngularVelocity );
        // Change the ECS velocity; the existing physics synchronization applies
        // it to Jolt. Scripts never retain or manipulate raw Jolt body pointers.
        rigidBody.set_function( "SetLinearVelocity", []( scene::RigidBodyComponent& body, const core::float3& velocity ) {
            body.SetLinearVelocity( velocity );
        } );
        rigidBody.set_function( "SetAngularVelocity", []( scene::RigidBodyComponent& body, const core::float3& velocity ) {
            body.SetAngularVelocity( velocity );
        } );

        entity.set_function( "GetTag", []( scene::Entity& e ) -> scene::TagComponent& {
            return e.GetComponent<scene::TagComponent>();
        } );
        auto tag{ state.new_usertype<scene::TagComponent>( "TagComponent", sol::constructors<
                                                                               TagComponent(),
                                                                               TransformComponent( const char* )>() ) };

        tag.set_function( "GetGuid", []( const scene::TagComponent& tag ) {
            return tag.GetGuid();
        } );
        tag.set_function( "GetTag", []( const scene::TagComponent& tag ) {
            return tag.GetTag().c_str();
        } );
        tag.set_function( "IsActive", []( const scene::TagComponent& tag ) {
            return tag.IsActive();
        } );

        tag.set_function( "SetActive", []( scene::TagComponent& tag, bool active ) {
            tag.SetActive( active );
        } );
        tag.set_function( "SetTag", []( scene::TagComponent& tag, const char* name ) {
            tag.SetTag( name );
        } );


        entity.set_function( "GetComponent", []( scene::Entity& entity, const std::string& name, sol::this_state lua ) -> sol::object {
            sol::state_view state{ lua };

            if ( name == "TransformComponent" && entity.HasComponent<scene::TransformComponent>() ) {
                return sol::make_object( state, &entity.GetComponent<scene::TransformComponent>() );
            }

            if ( name == "TagComponent" && entity.HasComponent<scene::TagComponent>() ) {
                return sol::make_object( state, &entity.GetComponent<scene::TagComponent>() );
            }

            if ( name == "RigidBodyComponent" && entity.HasComponent<scene::RigidBodyComponent>() ) {
                return sol::make_object( state, &entity.GetComponent<scene::RigidBodyComponent>() );
            }

            if ( name == "CanvasComponent" && entity.HasComponent<scene::CanvasComponent>() ) {
                return sol::make_object( state, &entity.GetComponent<scene::CanvasComponent>().GetCanvas() );
            }

            return sol::make_object( state, sol::nil );
        } );

        entity.set_function( "GetTransform", []( scene::Entity& e ) -> scene::TransformComponent& {
            return e.GetComponent<scene::TransformComponent>();
        } );
        auto transform{ state.new_usertype<scene::TransformComponent>( "TransformComponent", sol::constructors<
                                                                                                 TransformComponent(),
                                                                                                 TransformComponent( glm::vec3, glm::vec3, glm::vec3 )>() ) };

        transform.set_function( "GetTransform", []( const scene::TransformComponent& transform ) -> glm::mat4 {
            return transform.GetTransform();
        } );
        transform.set_function( "GetTranslation", []( scene::TransformComponent& t ) -> glm::vec3 {
            return t.GetTranslation();
        } );
        transform.set_function( "GetRotation", []( scene::TransformComponent& t ) -> glm::vec3 {
            return t.GetRotation();
        } );
        transform.set_function( "GetScale", []( scene::TransformComponent& t ) -> glm::vec3 {
            return t.GetScale();
        } );

        transform.set_function( "SetTranslation", []( scene::TransformComponent& t, const glm::vec3& value ) {
            t.SetTranslation( value );
        } );
        transform.set_function( "SetRotation", []( scene::TransformComponent& t, const glm::vec3& value ) {
            t.SetRotation( value );
        } );
        transform.set_function( "SetScale", []( scene::TransformComponent& t, const glm::vec3& value ) {
            t.SetScale( value );
        } );

        transform.set_function( "HasUniformScale", []( scene::TransformComponent& t ) {
            return t.HasUniformScale();
        } );
        transform.set_function( "SetUniformScale", []( scene::TransformComponent& t, bool value ) {
            t.SetUniformSale( value );
        } );
    }
}// namespace mikoto::scripting
