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

#ifndef MIKOTO_SCRIPTING_NATIVE_SCRIPT_API_HH
#define MIKOTO_SCRIPTING_NATIVE_SCRIPT_API_HH

#include <array>
#include <string>
#include <cstdint>
#include <type_traits>

// Self-contained C++ script SDK. Only value records and callbacks cross the
// shared-library boundary. Instance allocation and destruction stay in the module.
// https://docs.godotengine.org/en/stable/engine_details/engine_api/gdextension/what_is_gdextension.html
namespace mikoto::scripting::native {
    inline constexpr std::uint32_t kApiVersion{ 2 };
    inline constexpr char kModuleEntryPoint[]{ "MikotoGetNativeScriptModule" };

    enum class TransformField : std::uint32_t {
        ePosition,
        eRotation,
        eScale
    };

    struct Vector2 {
        float x{};
        float y{};
    };

    struct Vector3 {
        float x{};
        float y{};
        float z{};

        auto operator+( const Vector3& other ) const noexcept -> Vector3 {
            return { x + other.x, y + other.y, z + other.z };
        }
    };

    struct Vector4 {
        float x{};
        float y{};
        float z{};
        float w{};
    };

    struct EntityId {
        std::uint64_t mValue{};
    };

    // Values match ContactEvent::GetCallbackIndex() in the engine.
    enum class PhysicsEvent : std::uint32_t {
        eCollisionEnter,
        eCollisionStay,
        eCollisionExit,
        eTriggerEnter,
        eTriggerStay,
        eTriggerExit
    };

    struct HostApi {
        void ( *mReadTransform )( void*, TransformField, Vector3* ) noexcept;
        void ( *mWriteTransform )( void*, TransformField, const Vector3* ) noexcept;
        bool ( *mIsKeyPressed )( std::uint32_t ) noexcept;
        void ( *mLogInfo )( const char* ) noexcept;
        std::uint64_t ( *mGetEntityId )( void* ) noexcept;
    };

    class EntityScript {
    public:
        auto Bind( const HostApi& api, void* entity ) noexcept -> void {
            mApi = &api;
            mEntity = entity;
        }

        virtual auto OnCreate() -> void {}
        virtual auto OnUpdate( [[maybe_unused]] float timeStep ) -> void {}
        virtual auto OnCollisionEnter( [[maybe_unused]] EntityId other ) -> void {
        }
        virtual auto OnCollisionStay( [[maybe_unused]] EntityId other ) -> void {
        }
        virtual auto OnCollisionExit( [[maybe_unused]] EntityId other ) -> void {
        }
        virtual auto OnTriggerEnter( [[maybe_unused]] EntityId other ) -> void {
        }
        virtual auto OnTriggerStay( [[maybe_unused]] EntityId other ) -> void {
        }
        virtual auto OnTriggerExit( [[maybe_unused]] EntityId other ) -> void {
        }
        virtual auto OnDestroy() -> void {}
        virtual ~EntityScript() = default;

    protected:
        auto GetPosition() const noexcept -> Vector3 { return ReadTransform( TransformField::ePosition ); }
        auto GetRotation() const noexcept -> Vector3 { return ReadTransform( TransformField::eRotation ); }
        auto GetScale() const noexcept -> Vector3 { return ReadTransform( TransformField::eScale ); }
        auto SetPosition( const Vector3& value ) noexcept -> void { mApi->mWriteTransform( mEntity, TransformField::ePosition, &value ); }
        auto SetRotation( const Vector3& value ) noexcept -> void { mApi->mWriteTransform( mEntity, TransformField::eRotation, &value ); }
        auto SetScale( const Vector3& value ) noexcept -> void { mApi->mWriteTransform( mEntity, TransformField::eScale, &value ); }
        auto IsKeyPressed( std::uint32_t key ) const noexcept -> bool { return mApi->mIsKeyPressed( key ); }
        auto GetEntityId() const noexcept -> EntityId { return { mApi->mGetEntityId( mEntity ) }; }
        auto LogInfo( const char* text ) const noexcept -> void { mApi->mLogInfo( text ); }

    private:
        auto ReadTransform( TransformField field ) const noexcept -> Vector3 {
            Vector3 value{};
            mApi->mReadTransform( mEntity, field, &value );
            return value;
        }

        const HostApi* mApi{};
        void* mEntity{};
    };

    enum class FieldType : std::uint32_t {
        eBool,
        eInt32,
        eUInt32,
        eFloat32,
        eFloat64,
        eString,
        eVector2,
        eVector3,
        eVector4,
        eEntity
    };

    struct FieldValue {
        double mNumber{};
        Vector4 mVector{};
        std::uint64_t mEntity{};
        const char* mString{};
    };

    struct FieldDescription {
        const char* mName{};
        FieldType mType{};
        void ( *mRead )( const void*, FieldValue* ) noexcept;
        bool ( *mWrite )( void*, const FieldValue* ) noexcept;
    };

    // Pointer-to-member registration follows the same model as O3DE reflection.
    // https://docs.o3de.org/docs/user-guide/programming/components/reflection/reflecting-for-serialization/
    template<typename T, auto Member>
    constexpr auto Field( const char* name ) -> FieldDescription {
        using Type = std::remove_cvref_t<decltype( static_cast<T*>( nullptr )->*Member )>;

        constexpr auto type = [] {
            if constexpr ( std::is_same_v<Type, bool> ) return FieldType::eBool;
            else if constexpr ( std::is_same_v<Type, std::int32_t> ) return FieldType::eInt32;
            else if constexpr ( std::is_same_v<Type, std::uint32_t> ) return FieldType::eUInt32;
            else if constexpr ( std::is_same_v<Type, float> ) return FieldType::eFloat32;
            else if constexpr ( std::is_same_v<Type, double> ) return FieldType::eFloat64;
            else if constexpr ( std::is_same_v<Type, std::string> ) return FieldType::eString;
            else if constexpr ( std::is_same_v<Type, Vector2> ) return FieldType::eVector2;
            else if constexpr ( std::is_same_v<Type, Vector3> ) return FieldType::eVector3;
            else if constexpr ( std::is_same_v<Type, Vector4> ) return FieldType::eVector4;
            else {
                static_assert( std::is_same_v<Type, EntityId>, "Unsupported native script field type." );
                return FieldType::eEntity;
            }
        }();

        return {
            .mName = name,
            .mType = type,
            .mRead = []( const void* instance, FieldValue* result ) noexcept {
                const auto& value{ static_cast<const T*>( instance )->*Member };
                if constexpr ( std::is_arithmetic_v<Type> ) result->mNumber = static_cast<double>( value );
                else if constexpr ( type == FieldType::eString ) result->mString = value.c_str();
                else if constexpr ( type == FieldType::eVector2 ) result->mVector = { value.x, value.y };
                else if constexpr ( type == FieldType::eVector3 ) result->mVector = { value.x, value.y, value.z };
                else if constexpr ( type == FieldType::eVector4 ) result->mVector = value;
                else result->mEntity = value.mValue;
            },
            .mWrite = []( void* instance, const FieldValue* source ) noexcept {
                auto& value{ static_cast<T*>( instance )->*Member };
                try {
                    if constexpr ( std::is_arithmetic_v<Type> ) value = static_cast<Type>( source->mNumber );
                    else if constexpr ( type == FieldType::eString ) value = source->mString;
                    else if constexpr ( type == FieldType::eVector2 ) value = { source->mVector.x, source->mVector.y };
                    else if constexpr ( type == FieldType::eVector3 ) value = { source->mVector.x, source->mVector.y, source->mVector.z };
                    else if constexpr ( type == FieldType::eVector4 ) value = source->mVector;
                    else value.mValue = source->mEntity;
                    return true;
                } catch ( ... ) {
                    return false;
                }
            }
        };
    }

    struct ScriptModule {
        const char* mName{};
        const FieldDescription* mFields{};
        std::uint32_t mFieldCount{};
        void* ( *mCreate )( const HostApi*, void* ) noexcept;
        void ( *mDestroy )( void* ) noexcept;
        bool ( *mStart )( void* ) noexcept;
        bool ( *mUpdate )( void*, float ) noexcept;
        bool ( *mStop )( void* ) noexcept;
        bool ( *mPhysicsEvent )( void *, PhysicsEvent, EntityId ) noexcept;
    };

    using GetScriptModule = const ScriptModule* ( * )( std::uint32_t ) noexcept;

    template<typename T>
    auto Module( const char* name ) noexcept -> const ScriptModule* {
        static constexpr auto fields = [] {
            if constexpr ( requires { T::NativeFields(); } ) {
                return T::NativeFields();
            }
            else {
                return std::array<FieldDescription, 0>{};
            }
        }();

        static const ScriptModule module{
            .mName = name,
            .mFields = fields.data(),
            .mFieldCount = static_cast<std::uint32_t>( fields.size() ),
            .mCreate = []( const HostApi *api, void *entity ) noexcept -> void * {
                try {
                    auto* script{ new T{} };
                    script->Bind( *api, entity );
                    return script;
                } catch ( ... ) {
                    return nullptr;
                }
            },
            .mDestroy = []( void *script ) noexcept {
                delete static_cast<T *>( script );
            },
            .mStart = []( void *script ) noexcept {
                try {
                    static_cast<T *>( script )->OnCreate();
                    return true;
                } catch ( ... ) {
                    return false;
                }
            },
            .mUpdate = []( void *script, float delta ) noexcept {
                try {
                    static_cast<T *>( script )->OnUpdate( delta );
                    return true;
                } catch ( ... ) {
                    return false;
                }
            },
            .mStop = []( void *script ) noexcept {
                try {
                    static_cast<T *>( script )->OnDestroy();
                    return true;
                } catch ( ... ) {
                    return false;
                }
            },
            .mPhysicsEvent = []( void *instance, PhysicsEvent event, EntityId other ) noexcept {
                try {
                    auto *script{ static_cast<T *>( instance ) };
                    switch ( event ) {
                        case PhysicsEvent::eCollisionEnter:
                            script->OnCollisionEnter( other );
                            break;
                        case PhysicsEvent::eCollisionStay:
                            script->OnCollisionStay( other );
                            break;
                        case PhysicsEvent::eCollisionExit:
                            script->OnCollisionExit( other );
                            break;
                        case PhysicsEvent::eTriggerEnter:
                            script->OnTriggerEnter( other );
                            break;
                        case PhysicsEvent::eTriggerStay:
                            script->OnTriggerStay( other );
                            break;
                        case PhysicsEvent::eTriggerExit:
                            script->OnTriggerExit( other );
                            break;
                        default:
                            return false;
                    }
                    return true;
                } catch ( ... ) {
                    return false;
                }
            }
        };

        return &module;
    }
}

#if defined( _WIN32 )
#define MKT_NATIVE_SCRIPT_EXPORT __declspec( dllexport )
#else
#define MKT_NATIVE_SCRIPT_EXPORT __attribute__(( visibility( "default" ) ))
#endif

#define MKT_NATIVE_SCRIPT_FIELD( Type, Member ) ::mikoto::scripting::native::Field<Type, &Type::Member>( #Member )
#define MKT_NATIVE_SCRIPT_FIELDS( ... ) static constexpr auto NativeFields() { return std::array{ __VA_ARGS__ }; }

#define MKT_NATIVE_SCRIPT( Type )                                                                                     \
    extern "C" MKT_NATIVE_SCRIPT_EXPORT auto MikotoGetNativeScriptModule( std::uint32_t version ) noexcept             \
        -> const ::mikoto::scripting::native::ScriptModule* {                                                          \
        return version == ::mikoto::scripting::native::kApiVersion ? ::mikoto::scripting::native::Module<Type>( #Type ) : nullptr; \
    }

#endif // MIKOTO_SCRIPTING_NATIVE_SCRIPT_API_HH
