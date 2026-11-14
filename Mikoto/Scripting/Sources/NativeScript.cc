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

#include <cmath>
#include <limits>
#include <filesystem>

#include <EASTL/unique_ptr.h>
#include <SDL3/SDL_loadso.h>
#include <nlohmann/json.hpp>

#include <Core/RuntimeConsole.hh>

#include <Logging/Logger.hh>

#include <Platform/InputSystem.hh>

#include <Scene/Entity.hh>
#include <Scene/Component.hh>

#include <Scripting/ScriptingService.hh>
#include <Scripting/Native/NativeScript.hh>

namespace mikoto::scripting {

    static auto ReadTransform( void* owner, native::TransformField field, native::Vector3* value ) noexcept -> void {
        auto* entity{ static_cast<scene::Entity*>( owner ) };
        if ( !entity || !entity->HasComponent<scene::TransformComponent>() ) {
            return;
        }

        const auto& transform{ entity->GetComponent<scene::TransformComponent>() };
        const auto& vector{ field == native::TransformField::ePosition ? transform.GetTranslation() :
            field == native::TransformField::eRotation ? transform.GetRotation() : transform.GetScale() };
        *value = { vector.x, vector.y, vector.z };
    }

    static auto WriteTransform( void* owner, native::TransformField field, const native::Vector3* value ) noexcept -> void {
        auto* entity{ static_cast<scene::Entity*>( owner ) };
        if ( !entity || !entity->HasComponent<scene::TransformComponent>() ) {
            return;
        }

        auto& transform{ entity->GetComponent<scene::TransformComponent>() };
        const core::float3 vector{ value->x, value->y, value->z };
        switch ( field ) {
            case native::TransformField::ePosition: transform.SetTranslation( vector ); break;
            case native::TransformField::eRotation: transform.SetRotation( vector ); break;
            case native::TransformField::eScale: transform.SetScale( vector ); break;
        }
    }

    static constexpr native::HostApi kHostApi{
        .mReadTransform = &ReadTransform,
        .mWriteTransform = &WriteTransform,
        .mIsKeyPressed = []( core::u32 key ) noexcept {
            return ScriptingService::HasInstance() && ScriptingService::Get()->HasInputFocus() &&
                platform::InputSystem::HasInstance() && platform::InputSystem::Get()->IsKeyPressed( core::as<platform::KeyCode>( key ) );
        },
        .mLogInfo = []( const char* text ) noexcept {
            if ( text && core::RuntimeConsole::HasInstance() ) {
                try { core::RuntimeConsole::Get()->Info( text ); } catch ( ... ) {}
            }
        },
        .mGetEntityId = []( void* owner ) noexcept -> core::u64 {
            auto* entity{ static_cast<scene::Entity*>( owner ) };
            return entity && entity->HasComponent<scene::TagComponent>() ? entity->GetComponent<scene::TagComponent>().GetGuid() : 0;
        }
    };

    static auto NativeFieldType( native::FieldType type ) -> ScriptValueType {
        using Field = native::FieldType;
        using Value = ScriptValueType;
        switch ( type ) {
            case Field::eBool: return Value::eBool;
            case Field::eInt32: return Value::eInt32;
            case Field::eUInt32: return Value::eUInt32;
            case Field::eFloat32: return Value::eFloat32;
            case Field::eFloat64: return Value::eFloat64;
            case Field::eString: return Value::eString;
            case Field::eVector2: return Value::eVec2;
            case Field::eVector3: return Value::eVec3;
            case Field::eVector4: return Value::eVec4;
            case Field::eEntity: return Value::eEntity;
        }

        return Value::eUnknown;
    }

    static auto NativeDefault( native::FieldType type, const native::FieldValue& value ) -> nlohmann::json {
        using Field = native::FieldType;
        switch ( type ) {
            case Field::eBool: return value.mNumber != 0;
            case Field::eInt32: return core::as<core::i32>( value.mNumber );
            case Field::eUInt32: return core::as<core::u32>( value.mNumber );
            case Field::eFloat32:
            case Field::eFloat64: return value.mNumber;
            case Field::eString: return value.mString ? value.mString : "";
            case Field::eVector2: return { value.mVector.x, value.mVector.y };
            case Field::eVector3: return { value.mVector.x, value.mVector.y, value.mVector.z };
            case Field::eVector4: return { value.mVector.x, value.mVector.y, value.mVector.z, value.mVector.w };
            case Field::eEntity: return value.mEntity;
        }

        return nullptr;
    }

    static auto WriteNativeField( const native::FieldDescription& field, void* instance, const nlohmann::json& json ) -> bool {
        using Field = native::FieldType;
        native::FieldValue value{};
        std::string text{};
        switch ( field.mType ) {
            case Field::eBool:
                value.mNumber = json.get<bool>() ? 1.0 : 0.0;
                break;
            case Field::eInt32:
            case Field::eUInt32: {
                const auto number{ json.get<core::f64>() };
                const auto lower{ field.mType == Field::eInt32 ? core::as<core::f64>( std::numeric_limits<core::i32>::min() ) : 0.0 };
                const auto upper{ field.mType == Field::eInt32 ? core::as<core::f64>( std::numeric_limits<core::i32>::max() ) :
                    core::as<core::f64>( std::numeric_limits<core::u32>::max() ) };
                if ( !json.is_number_integer() || number < lower || number > upper ) {
                    return false;
                }
                value.mNumber = number;
                break;
            }
            case Field::eFloat32:
            case Field::eFloat64:
                value.mNumber = json.get<core::f64>();
                if ( !std::isfinite( value.mNumber ) ||
                     ( field.mType == Field::eFloat32 && std::abs( value.mNumber ) > std::numeric_limits<core::f32>::max() ) ) {
                    return false;
                }
                break;
            case Field::eString:
                text = json.get<std::string>();
                value.mString = text.c_str();
                break;
            case Field::eVector2:
            case Field::eVector3:
            case Field::eVector4:
                value.mVector.x = json.at( 0 ).get<core::f32>();
                value.mVector.y = json.at( 1 ).get<core::f32>();
                if ( field.mType != Field::eVector2 ) {
                    value.mVector.z = json.at( 2 ).get<core::f32>();
                }
                if ( field.mType == Field::eVector4 ) {
                    value.mVector.w = json.at( 3 ).get<core::f32>();
                }
                if ( !std::isfinite( value.mVector.x ) || !std::isfinite( value.mVector.y ) ||
                     !std::isfinite( value.mVector.z ) || !std::isfinite( value.mVector.w ) ) {
                    return false;
                }
                break;
            case Field::eEntity:
                value.mEntity = json.get<core::u64>();
                break;
        }

        return field.mWrite( instance, &value );
    }

    NativeScriptModule::~NativeScriptModule() {
        if ( mLibrary ) {
            SDL_UnloadObject( mLibrary );
        }

        if ( !mLoadedPath.IsEmpty() ) {
            std::error_code error{};
            std::filesystem::remove( mLoadedPath.GetPathTyped<std::filesystem::path>(), error );
        }
    }

    auto NativeScriptModule::ReadMetadata() -> bool {
        const eastl::unique_ptr<void, decltype( mApi->mDestroy )> defaults{ mApi->mCreate( &kHostApi, nullptr ), mApi->mDestroy };
        if ( !defaults ) {
            return false;
        }

        mMetadata.mName = mApi->mName;
        mMetadata.mLanguage = ScriptLanguage::eNative;
        for ( core::u32 index{}; index < mApi->mFieldCount; ++index ) {
            const auto& field{ mApi->mFields[index] };
            const auto type{ NativeFieldType( field.mType ) };
            if ( !field.mName || !*field.mName || !field.mRead || !field.mWrite || type == ScriptValueType::eUnknown ) {
                return false;
            }

            native::FieldValue value{};
            field.mRead( defaults.get(), &value );
            mMetadata.mFieldLookup.emplace( index, ScriptFieldMetadata{
                .mFieldID = index,
                .mName = field.mName,
                .mType = type,
                .mDefaultJson = NativeDefault( field.mType, value ).dump().c_str()
            } );
        }

        return true;
    }

    NativeScript::NativeScript( filesystem::FileHandle file, scene::Entity *entity )
        : mEntity{ entity }, mFile{ eastl::move( file ) } {
        mModule = ScriptingService::Get()->GetNativeScriptingService()->GetModule( mFile->GetPath() );
    }

    NativeScript::~NativeScript() {
        Stop();
    }

    auto NativeScript::Start( scene::Scene & ) -> void {
        if ( !mPlaying ) {
            mPlaying = true;
            mModule = ScriptingService::Get()->GetNativeScriptingService()->GetModule( mFile->GetPath() );
            StartInstance();
        }
    }

    auto NativeScript::StartInstance() -> void {
        if ( !mModule || !mEntity ) {
            return;
        }

        mAppliedProperties.clear();
        mUpdateFailed = false;
        mInstance = mModule->mApi->mCreate( &kHostApi, mEntity );
        if ( mInstance && ApplyProperties() && mModule->mApi->mStart( mInstance ) ) {
            mRunning = true;
        }
        else {
            Stop();
            mPlaying = true;
            MKT_CORE_LOGGER_ERROR( "Could not start native script '{}'.", mFile->GetPath().GetC_Str() );
        }
    }

    auto NativeScript::Stop() -> void {
        mPlaying = false;
        if ( !mInstance ) {
            return;
        }

        const auto running{ mRunning };
        mRunning = false;
        if ( running && !mModule->mApi->mStop( mInstance ) ) {
            MKT_CORE_LOGGER_ERROR( "OnDestroy failed in native script '{}'.", mFile->GetPath().GetC_Str() );
        }

        mModule->mApi->mDestroy( mInstance );
        mInstance = nullptr;
    }

    auto NativeScript::Update( float timeStep ) -> void {
        AdoptModule( ScriptingService::Get()->GetNativeScriptingService()->GetModule( mFile->GetPath() ) );
        if ( mRunning && !mUpdateFailed && ApplyProperties() && !mModule->mApi->mUpdate( mInstance, timeStep ) ) {
            mUpdateFailed = true;
            MKT_CORE_LOGGER_ERROR( "OnUpdate failed in native script '{}'.", mFile->GetPath().GetC_Str() );
        }
    }

    auto NativeScript::ReloadScript() -> void {
        AdoptModule( ScriptingService::Get()->GetNativeScriptingService()->GetModule( mFile->GetPath(), true ) );
    }

    auto NativeScript::AdoptModule( eastl::shared_ptr<NativeScriptModule> module ) -> void {
        if ( !module || module == mModule ) {
            return;
        }

        const auto playing{ mPlaying };
        Stop();
        mModule = eastl::move( module );

        mPlaying = playing;
        if ( mPlaying ) {
            StartInstance();
        }
    }

    auto NativeScript::ApplyProperties() -> bool {
        if ( !mEntity->HasComponent<scene::ScriptComponent>() ) {
            return true;
        }

        const auto& text{ mEntity->GetComponent<scene::ScriptComponent>().GetProperties() };
        if ( text == mAppliedProperties ) {
            return true;
        }

        try {
            const auto values{ nlohmann::json::parse( text.begin(), text.end() ) };
            if ( !values.is_object() ) {
                return false;
            }

            for ( core::u32 index{}; index < mModule->mApi->mFieldCount; ++index ) {
                const auto& field{ mModule->mApi->mFields[index] };
                const auto override{ values.find( field.mName ) };
                const auto& metadata{ mModule->mMetadata.mFieldLookup.at( index ) };
                const auto value{ override != values.end() ? *override : nlohmann::json::parse( metadata.mDefaultJson.c_str() ) };
                if ( !WriteNativeField( field, mInstance, value ) ) {
                    MKT_CORE_LOGGER_ERROR( "Invalid native script field '{}'.", field.mName );
                    return false;
                }
            }

            mAppliedProperties = text;
            return true;
        } catch ( const std::exception& error ) {
            MKT_CORE_LOGGER_ERROR( "Could not apply native script properties: {}", error.what() );
            return false;
        }
    }

    auto NativeScript::GetFile() const -> filesystem::FileHandle {
        return mFile;
    }

    auto NativeScript::GetEntity() const -> scene::Entity * {
        return mEntity;
    }

    auto NativeScript::GetLanguage() const -> ScriptLanguage {
        return ScriptLanguage::eNative;
    }

    auto NativeScript::GetMetaData() const -> const ScriptClassMetadata & {
        static const ScriptClassMetadata empty{};
        mMetadataModule = mModule;
        if ( ScriptingService::HasInstance() ) {
            if ( auto* service{ ScriptingService::Get()->GetNativeScriptingService() } ) {
                mMetadataModule = service->GetModule( mFile->GetPath() );
            }
        }
        return mMetadataModule ? mMetadataModule->mMetadata : empty;
    }
}// namespace mikoto::scripting
