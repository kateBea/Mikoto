//
// Created by kate on 1/17/26.
//

#include <Scripting/SceneBinding.hh>

#include "Scene/Component.hh"
#include "Scene/Entity.hh"

namespace Mikoto {

    auto SceneBinding::Init( sol::state& state ) -> void {
        auto entity{ state.new_usertype<Entity>( "Entity" ) };

        entity.set_function( "GetTag", []( Entity& e ) -> TagComponent& { return e.GetComponent<TagComponent>(); } );
        auto tag{ state.new_usertype<TagComponent>( "TagComponent", sol::constructors<
                                                                                              TagComponent(),
                                                                                              TransformComponent( std::string_view )>() ) };

        tag.set_function( "GetGuid", []( const TagComponent& tag ) { return tag.GetGUID(); } );
        tag.set_function( "GetTag", []( const TagComponent& tag ) { return tag.GetTag(); } );
        tag.set_function( "IsActive", []( const TagComponent& tag ) { return tag.IsActive(); } );

        tag.set_function( "SetActive", []( TagComponent& tag, bool active ) { tag.SetActive(active); } );
        tag.set_function( "SetTag", []( TagComponent& tag, std::string_view name ) { tag.SetTag(name); } );



        entity.set_function("GetTransform", []( Entity& e ) -> TransformComponent& { return e.GetComponent<TransformComponent>(); } );
        auto transform{ state.new_usertype<TransformComponent>( "TransformComponent", sol::constructors<
                                                                                              TransformComponent(),
                                                                                              TransformComponent( glm::vec3, glm::vec3 ),
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
