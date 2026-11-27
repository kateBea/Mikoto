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

#include <nlohmann/json.hpp>

#include <sol/sol.hpp>

#include <Core/Core.hh>
#include <Core/Profiler.hh>
#include <Core/Exception.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileService.hh>

#include <Assets/AssetRegistryService.hh>

#include <Scene/Scene.hh>
#include <Scene/Entity.hh>
#include <Scene/Component.hh>

#include <Physics/ContactEvents.hh>

#include <Scripting/Lua/LuaScript.hh>

#define MKT_SOL_CALL( fn, ... )                                                    \
    do {                                                                           \
        if ( ( fn ).valid() ) {                                                    \
            sol::protected_function _protected_fn = ( fn );                        \
            sol::protected_function_result _result = _protected_fn( __VA_ARGS__ ); \
            if ( !_result.valid() ) {                                              \
                sol::error err = _result;                                          \
                MKT_CORE_LOGGER_ERROR( "Lua Error: {}", err.what() );              \
            }                                                                      \
        }                                                                          \
    } while ( false )

namespace mikoto::scripting {
    struct LuaScript::LuaInstance {
        // The VM outlives its table references. Each script file runs in its own environment.
        eastl::shared_ptr<sol::state> mState{};
        sol::environment mEnvironment{};
        sol::table mTable{};
        eastl::string mAppliedProperties{};
    };


    static auto PropertyType( const std::string& name ) -> ScriptValueType {
        if ( name == "Bool" ) {
            return ScriptValueType::eBool;
        }

        if ( name == "Int32" ) {
            return ScriptValueType::eInt32;
        }

        if ( name == "UInt32" ) {
            return ScriptValueType::eUInt32;
        }

        if ( name == "Float" || name == "Float32" ) {
            return ScriptValueType::eFloat32;
        }

        if ( name == "Double" || name == "Float64" ) {
            return ScriptValueType::eFloat64;
        }

        if ( name == "String" ) {
            return ScriptValueType::eString;
        }

        if ( name == "Vector2" || name == "Vec2" ) {
            return ScriptValueType::eVec2;
        }

        if ( name == "Vector3" || name == "Vec3" || name == "Color3" ) {
            return ScriptValueType::eVec3;
        }

        if ( name == "Vector4" || name == "Vec4" || name == "Color4" ) {
            return ScriptValueType::eVec4;
        }

        if ( name == "Entity" ) {
            return ScriptValueType::eEntity;
        }

        if ( name == "Asset" ) {
            return ScriptValueType::eAsset;
        }

        return ScriptValueType::eUnknown;
    }

    static auto DefaultValue( const sol::object& value, ScriptValueType type ) -> nlohmann::json {
        if ( !value.valid() || value == sol::nil ) {
            return nullptr;
        }

        switch ( type ) {
            case ScriptValueType::eBool:
                return value.as<bool>();
            case ScriptValueType::eInt32:
                return value.as<core::i32>();
            case ScriptValueType::eUInt32:
                return value.as<core::u32>();
            case ScriptValueType::eEntity:
                return value.as<core::u64>();
            case ScriptValueType::eFloat32:
            case ScriptValueType::eFloat64:
                return value.as<double>();
            case ScriptValueType::eString:
            case ScriptValueType::eAsset:
                return value.as<std::string>();
            case ScriptValueType::eVec2: {
                auto v{ value.as<core::float2>() };
                return { v.x, v.y };
            }

            case ScriptValueType::eVec3: {
                auto v{ value.as<core::float3>() };
                return { v.x, v.y, v.z };
            }

            case ScriptValueType::eVec4: {
                auto v{ value.as<core::float4>() };
                return { v.x, v.y, v.z, v.w };
            }

            default:
                return nullptr;
        }
    }

    static auto AssignProperty( sol::state& state, sol::table& table, const ScriptFieldMetadata& field,
        const nlohmann::json& value, scene::Scene* scene ) -> void {
        const auto* name{ field.mName.c_str() };

        if ( value.is_null() ) {
            table[name] = sol::nil;
            return;
        }

        switch ( field.mType ) {
            case ScriptValueType::eBool:
                table[name] = value.get<bool>();
                break;
            case ScriptValueType::eInt32:
                table[name] = value.get<core::i32>();
                break;
            case ScriptValueType::eUInt32:
                table[name] = value.get<core::u32>();
                break;
            case ScriptValueType::eEntity:
                table[name] = scene ? scene->FindByID( value.get<core::u64>() ) : nullptr;
                break;
            case ScriptValueType::eFloat32:
            case ScriptValueType::eFloat64:
                table[name] = value.get<double>();
                break;
            case ScriptValueType::eString:
                table[name] = value.get<std::string>();
                break;
            case ScriptValueType::eVec2:
                table[name] = core::float2{ value.at( 0 ).get<float>(), value.at( 1 ).get<float>() };
                break;
            case ScriptValueType::eVec3:
                table[name] = core::float3{ value.at( 0 ).get<float>(), value.at( 1 ).get<float>(), value.at( 2 ).get<float>() };
                break;
            case ScriptValueType::eVec4:
                table[name] = core::float4{ value.at( 0 ).get<float>(), value.at( 1 ).get<float>(), value.at( 2 ).get<float>(), value.at( 3 ).get<float>() };
                break;
            case ScriptValueType::eAsset: {
                auto asset{ state.create_table() };
                asset["Path"] = value.get<std::string>();
                asset["Kind"] = std::string{ field.mAssetKind.c_str() };

                if ( field.mAssetKind == "Audio" ) {
                    asset.set_function( "Play", []( sol::table clip, scene::Entity& entity ) {
                        if ( !entity.HasComponent<scene::AudioSourceComponent>() ) {
                            return;
                        }

                        const auto path{ clip.get<std::string>( "Path" ) };
                        auto& audio{ entity.GetComponent<scene::AudioSourceComponent>() };
                        auto file{ filesystem::FileService::Get()->LoadFile( filesystem::Path{ path.c_str() } ) };

                        if ( !file ) {
                            return;
                        }

                        auto handle{ asset::AssetRegistryService::Get()->LoadAsset<audio::Audio>( asset::AudioLoadDescription{ .mFile = file } ) };

                        if ( !handle ) {
                            return;
                        }

                        audio.SetClip( handle );

                        if ( auto source{ audio.GetSource() } ) {
                            source->Play();
                        }
                    } );
                }

                table[name] = asset;
                break;
            }

            default:
                break;
        }
    }


    LuaScript::LuaScript( filesystem::FileHandle file, eastl::shared_ptr<sol::state> state, scene::Entity* entity )
        : mLua{ eastl::make_unique<LuaInstance>() }, mFile{ eastl::move( file ) }, mEntity{ entity } {
        mLua->mState = eastl::move( state );
        Initialize();
    }

    LuaScript::~LuaScript() {
        Stop();
    }

    auto LuaScript::Start( scene::Scene& scene ) -> void {
        if ( mRunning ) {
            return;
        }

        mScene = &scene;
        mLua->mAppliedProperties.clear();
        mFailed = !ApplyProperties();

        if ( mFailed ) {
            return;
        }

        mRunning = true;
        auto callback{ mLua->mTable.get<sol::protected_function>( "OnCreate" ) };
        MKT_SOL_CALL( callback, mLua->mTable );
    }

    auto LuaScript::Stop() -> void {
        if ( !mRunning ) {
            mScene = nullptr;
            return;
        }

        mRunning = false;
        auto callback{ mLua->mTable.get<sol::protected_function>( "OnDestroy" ) };
        MKT_SOL_CALL( callback, mLua->mTable );
        mScene = nullptr;
    }

    auto LuaScript::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        auto& state{ *mLua->mState };
        sol::environment environment{ state, sol::create, state.globals() };
        // Retain existing bindings for legacy helpers; lifecycle methods receive self.Entity too.
        environment["Entity"] = mEntity;
        sol::protected_function_result result{ state.script_file( mFile->GetPath().GetC_Str(), environment, sol::script_default_on_error ) };

        if ( !result.valid() ) {
            const sol::error error = result;
            MKT_THROW_RUNTIME_ERROR( string::Format( "Lua error in {}: {}", mFile->GetPath().GetC_Str(), error.what() ) );
        }

        if ( result.return_count() == 0 || result.get_type() != sol::type::table ) {
            MKT_THROW_RUNTIME_ERROR( "Lua scripts must return a named table containing their lifecycle methods and Properties." );
        }

        sol::table table{ result.get<sol::table>() };
        table["Entity"] = mEntity;
        ScriptClassMetadata metadata{};
        metadata.mName = mFile->GetPath().GetStem();
        metadata.mLanguage = ScriptLanguage::eLua;
        const auto properties{ table.get<sol::optional<sol::table>>( "Properties" ) };

        if ( properties ) {
            core::u32 id{};

            for ( const auto& [key, definition]: *properties ) {
                const auto name{ key.as<std::string>() };

                if ( name == "Entity" || name == "Properties" || name == "OnCreate" || name == "OnUpdate" || name == "OnDestroy" ||
                     name == "OnCollisionEnter" ||
                     name == "OnCollisionStay" ||
                     name == "OnCollisionExit" ||
                     name == "OnTriggerEnter" ||
                     name == "OnTriggerStay" ||
                     name == "OnTriggerExit" ) {
                    MKT_THROW_RUNTIME_ERROR( "Lua property names cannot replace Entity, Properties or lifecycle methods." );
                }

                const auto property{ definition.as<sol::table>() };
                const std::string type{ property.get_or( "Type", std::string{} ) };
                ScriptFieldMetadata field{};
                field.mFieldID = ++id;
                field.mName = name.c_str();
                field.mType = PropertyType( type );

                if ( field.mType == ScriptValueType::eUnknown ) {
                    MKT_THROW_RUNTIME_ERROR( string::Format( "Unknown Lua property type '{}' for '{}'", type.c_str(), name.c_str() ) );
                }

                field.mAssetKind = property.get_or( "Kind", std::string{} ).c_str();

                if ( type == "Color3" || type == "Color4" ) {
                    field.mAssetKind = "Color";
                }

                field.mReadOnly = property.get_or( "ReadOnly", false );
                const auto value{ DefaultValue( property.get<sol::object>( "Default" ), field.mType ) };
                field.mDefaultJson = value.dump().c_str();
                AssignProperty( state, table, field, value, mScene );
                metadata.mFieldLookup.emplace( id, eastl::move( field ) );
            }
        }
        // Only replace the current table after loading and validating all property definitions.
        Stop();
        mLua->mEnvironment = eastl::move( environment );
        mLua->mTable = eastl::move( table );
        mLua->mAppliedProperties.clear();
        mScriptMetadata = eastl::move( metadata );
    }

    auto LuaScript::ApplyProperties() -> bool {
        if ( !mEntity || !mEntity->HasComponent<scene::ScriptComponent>() ) {
            return true;
        }

        const auto& text{ mEntity->GetComponent<scene::ScriptComponent>().GetProperties() };

        if ( text == mLua->mAppliedProperties ) {
            return true;
        }

        try {
            const auto values{ nlohmann::json::parse( text.c_str() ) };

            if ( !values.is_object() ) {
                MKT_THROW_RUNTIME_ERROR( "Script properties must be an object." );
            }

            for ( const auto& [id, field]: mScriptMetadata.mFieldLookup ) {
                if ( !field.mReadOnly && values.contains( field.mName.c_str() ) ) {
                    AssignProperty( *mLua->mState, mLua->mTable, field, values.at( field.mName.c_str() ), mScene );
                }
                else if ( field.mType == ScriptValueType::eEntity ) {
                    AssignProperty( *mLua->mState, mLua->mTable, field, nlohmann::json::parse( field.mDefaultJson.c_str() ), mScene );
                }
            }

            mLua->mAppliedProperties = text;
            return true;
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_ERROR( "Lua properties failed for {}: {}. Fix the values and reload the script.",
                mFile->GetPath().GetC_Str(), error.what() );
            return false;
        }
    }

    auto LuaScript::Update( float timeStep ) -> void {
        if ( !mRunning || mFailed ) {
            return;
        }

        mFailed = !ApplyProperties();

        if ( mFailed ) {
            return;
        }

        auto callback{ mLua->mTable.get<sol::protected_function>( "OnUpdate" ) };
        MKT_SOL_CALL( callback, mLua->mTable, timeStep );
    }

    auto LuaScript::OnPhysicsEvent( const physics::ContactEvent &event, scene::Entity &other ) -> void {
        if ( !mRunning || mFailed ) {
            return;
        }
        auto callback{ mLua->mTable.get<sol::protected_function>( event.GetCallbackName() ) };
        if ( callback.valid() ) {
            const sol::protected_function_result result{ callback( mLua->mTable, &other ) };
            if ( !result.valid() ) {
                const sol::error error = result;
                mFailed = true;
                MKT_CORE_LOGGER_ERROR( "Lua {} failed in '{}': {}", event.GetCallbackName(), mFile->GetPath().GetC_Str(), error.what() );
            }
        }
    }

    auto LuaScript::ReloadScript() -> void {
        try {
            auto* scene{ mScene };
            Initialize();

            if ( scene ) {
                Start( *scene );
            }
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_ERROR( "Failed to reload script {}: {}", mFile->GetPath().GetC_Str(), error.what() );
        }
    }

    auto LuaScript::GetFile() const -> filesystem::FileHandle {
        return mFile;
    }

    auto LuaScript::GetEntity() const -> scene::Entity* {
        return mEntity;
    }

    auto LuaScript::GetLanguage() const -> ScriptLanguage {
        return ScriptLanguage::eLua;
    }

    auto LuaScript::GetMetaData() const -> const ScriptClassMetadata& {
        return mScriptMetadata;
    }
}// namespace mikoto::scripting
