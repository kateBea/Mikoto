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

#include <glm/glm.hpp>
#include <sol/sol.hpp>

#include <Logging/Logger.hh>

#include <Scripting/MathBindings.hh>

namespace mikoto::scripting {

#define MKT_SET_GLM_MATH( var, type, scalar )                                           \
    ( var ).set_function( sol::meta_function::addition,                                 \
                          sol::overload(                                                \
                                  []( const type& a, const type& b ) { return a + b; }, \
                                  []( const type& a, scalar b ) { return a + b; },      \
                                  []( scalar a, const type& b ) { return a + b; } ) );  \
    ( var ).set_function( sol::meta_function::subtraction,                              \
                          sol::overload(                                                \
                                  []( const type& a, const type& b ) { return a - b; }, \
                                  []( const type& a, scalar b ) { return a - b; },      \
                                  []( scalar a, const type& b ) { return a - b; } ) );  \
    ( var ).set_function( sol::meta_function::multiplication,                           \
                          sol::overload(                                                \
                                  []( const type& a, const type& b ) { return a * b; }, \
                                  []( const type& a, scalar b ) { return a * b; },      \
                                  []( scalar a, const type& b ) { return a * b; } ) );  \
    ( var ).set_function( sol::meta_function::division,                                 \
                          sol::overload(                                                \
                                  []( const type& a, scalar b ) { return a / b; } ) );  \
    ( var ).set_function( sol::meta_function::unary_minus,                              \
                          []( const type& v ) { return -v; } );                         \
    ( var ).set_function( sol::meta_function::equal_to,                                 \
                          []( const type& a, const type& b ) { return a == b; } )

#define MKT_SET_GLM_MATRIX_MATH( var, type )                                                \
    ( var ).set_function( sol::meta_function::multiplication,                               \
                          sol::overload(                                                    \
                                  []( const type& a, const type& b ) { return a * b; } ) ); \
    ( var ).set_function( sol::meta_function::equal_to,                                     \
                          []( const type& a, const type& b ) { return a == b; } )


    auto MathBinding::Init( sol::state& state ) -> void {
        SetupMathTypes( state );
        SetupMathFunctions( state );
    }

    auto MathBinding::SetupMathTypes( sol::state& state ) -> void {
        auto vec2 = state.new_usertype<glm::vec2>(
                "Vec2F",
                sol::constructors<
                        glm::vec2(),
                        glm::vec2( float, float )>(),
                "x", &glm::vec2::x,
                "y", &glm::vec2::y );

        auto vec3 = state.new_usertype<glm::vec3>(
                "Vec3F",
                sol::constructors<
                        glm::vec3(),
                        glm::vec3( float, float, float )>(),
                "x", &glm::vec3::x,
                "y", &glm::vec3::y,
                "z", &glm::vec3::z );

        auto vec4 = state.new_usertype<glm::vec4>(
                "Vec4F",
                sol::constructors<
                        glm::vec4(),
                        glm::vec4( float, float, float, float )>(),
                "x", &glm::vec4::x,
                "y", &glm::vec4::y,
                "z", &glm::vec4::z,
                "w", &glm::vec4::w );

        MKT_SET_GLM_MATH( vec2, glm::vec2, float );
        MKT_SET_GLM_MATH( vec3, glm::vec3, float );
        MKT_SET_GLM_MATH( vec4, glm::vec4, float );


        auto mat2 = state.new_usertype<glm::mat2>(
                "Mat2F",
                sol::constructors<glm::mat2()>() );

        auto mat3 = state.new_usertype<glm::mat3>(
                "Mat3F",
                sol::constructors<glm::mat3()>() );

        auto mat4 = state.new_usertype<glm::mat4>(
                "Mat4F",
                sol::constructors<glm::mat4()>() );

        MKT_SET_GLM_MATRIX_MATH( mat2, glm::mat2 );
        MKT_SET_GLM_MATRIX_MATH( mat3, glm::mat3 );
        MKT_SET_GLM_MATRIX_MATH( mat4, glm::mat4 );

        // ToString
        vec2.set_function( sol::meta_function::to_string,
                           []( const glm::vec2& v ) {
                               return fmt::format( "Vec2F({}, {})", v.x, v.y );
                           } );

        vec3.set_function( sol::meta_function::to_string,
                           []( const glm::vec3& v ) {
                               return fmt::format( "Vec3F({}, {}, {})", v.x, v.y, v.z );
                           } );

        vec4.set_function( sol::meta_function::to_string,
                           []( const glm::vec4& v ) {
                               return fmt::format( "Vec4F({}, {}, {}, {})", v.x, v.y, v.z, v.w );
                           } );
    }

    auto MathBinding::SetupMathFunctions( sol::state& state ) -> void {
        auto mathTable{ state.create_named_table( "Math" ) };
    }
}// namespace Mikoto