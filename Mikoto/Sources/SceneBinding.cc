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

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Scripting/SceneBinding.hh>

#include <Scene/Entity.hh>
#include <Scene/Component.hh>

namespace mikoto::scripting {

    using namespace mikoto::core;
    using namespace mikoto::scene;
    using namespace mikoto::filesystem;

    auto SceneBinding::Init( sol::state& state ) -> void {
        auto entity{ state.new_usertype<Entity>( "Entity" ) };

        entity.set_function( "GetTag", []( Entity& e ) -> TagComponent& { return e.GetComponent<TagComponent>(); } );
        auto tag{ state.new_usertype<TagComponent>( "TagComponent", sol::constructors<
                                                                                              TagComponent(),
                                                                                              TransformComponent( const char* )>() ) };

        tag.set_function( "GetGuid", []( const TagComponent& tag ) { return tag.GetGUID(); } );
        tag.set_function( "GetTag", []( const TagComponent& tag ) { return tag.GetTag().c_str(); } );
        tag.set_function( "IsActive", []( const TagComponent& tag ) { return tag.IsActive(); } );

        tag.set_function( "SetActive", []( TagComponent& tag, bool active ) { tag.SetActive(active); } );
        tag.set_function( "SetTag", []( TagComponent& tag, const char* name ) { tag.SetTag(name); } );



        entity.set_function("GetTransform", []( Entity& e ) -> TransformComponent& { return e.GetComponent<TransformComponent>(); } );
        auto transform{ state.new_usertype<TransformComponent>( "TransformComponent", sol::constructors<
                                                                                              TransformComponent(),
                                                                                              TransformComponent( glm::vec3, glm::vec3, glm::vec3 )>() ) };

        transform.set_function( "GetTransform", []( const TransformComponent& transform ) -> glm::mat4 { return transform.GetTransform(); } );
        transform.set_function( "GetTranslation", []( TransformComponent& t ) -> glm::vec3 { return t.GetTranslation(); } );
        transform.set_function( "GetRotation", []( TransformComponent& t ) -> glm::vec3 { return t.GetRotation(); } );
        transform.set_function( "GetScale", []( TransformComponent& t ) -> glm::vec3 { return t.GetScale(); } );

        transform.set_function( "SetTranslation", []( TransformComponent& t, const glm::vec3& value ) { t.SetTranslation( value ); } );
        transform.set_function( "SetRotation", []( TransformComponent& t, const glm::vec3& value ) { t.SetRotation( value ); } );
        transform.set_function( "SetScale", []( TransformComponent& t, const glm::vec3& value ) { t.SetScale( value ); } );

        transform.set_function( "HasUniformScale", []( TransformComponent& t ) { return t.HasUniformScale(); } );
        transform.set_function( "SetUniformScale", []( TransformComponent& t, bool value ) { t.SetUniformSale( value ); } );
    }
}// namespace Mikoto
