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

#ifndef MIKOTO_SCRIPTING_DOTNET_BINDINGS_HH
#define MIKOTO_SCRIPTING_DOTNET_BINDINGS_HH

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Scripting/Dotnet/DotnetInterop.hh>

namespace mikoto::scene {
    class Scene;
}

namespace mikoto::scripting {
    /** @brief Registers a group of native callbacks in the managed bridge. */
    class DotnetBinding {
    public:
        virtual ~DotnetBinding() = default;

        /** @brief Publishes callbacks without invoking managed code.
         *
         * @param api Native callback table owned by the scripting service.
         */
        virtual auto Init( ManagedNativeApi& api ) -> void = 0;
    };

    /** @brief Binds scene entities, transforms, vehicles, lights and canvas controls with callback access validation. */
    class DotnetSceneBinding final : public DotnetBinding {
    public:
        auto Init( ManagedNativeApi& api ) -> void override;

        /** @brief Limits engine access to the current scene callback. */
        class CallbackScope final : public core::INonCopyable, public core::INonMoveable {
        public:
            /** @brief Enters an engine-owned scene callback.
             *
             * @param scene Scene whose entities may be accessed.
             * @param session Managed session identifier for this scene.
             */
            explicit CallbackScope( scene::Scene& scene, core::u64 session );

            ~CallbackScope();

        private:
            scene::Scene* mPreviousScene{};
            core::u64 mPreviousSession{};
        };

        /** @brief Checks whether the current thread is running a scene callback.
         *
         * @return True while engine-owned managed scene callbacks are active.
         */
        MKT_NODISCARD static auto HasActiveCallback() -> bool;

    private:
        static auto FindEntity( core::u64 session, core::u64 owner, core::cstr name, core::u64* output ) -> core::i32;
        static auto SetCharacterInputs( core::u64 session, core::u64 entity, const ManagedVector3* velocity, core::f32 jumpSpeed ) -> core::i32;
        static auto GetCharacterGrounded( core::u64 session, core::u64 entity ) -> core::i32;
        static auto SetVehicleInputs( core::u64 session, core::u64 entity, core::f32 throttle, core::f32 steer,
                                      core::f32 brake, core::f32 handBrake ) -> core::i32;
        static auto GetVehicleSpeed( core::u64 session, core::u64 entity, core::f32* output ) -> core::i32;
        static auto SetCanvasText( core::u64 session, core::u64 entity, core::u64 element, core::cstr text ) -> core::i32;
        static auto HasComponent( core::u64 session, core::u64 entity, ManagedComponentType type ) -> core::i32;
        static auto GetLightIntensity( core::u64 session, core::u64 entity, core::f32* output ) -> core::i32;
        static auto SetLightIntensity( core::u64 session, core::u64 entity, core::f32 value ) -> core::i32;
        static auto FindCanvasElement( core::u64 session, core::u64 entity, core::cstr name, core::u64* output ) -> core::i32;
        static auto GetCanvasValue( core::u64 session, core::u64 entity, core::u64 element, core::f32* output ) -> core::i32;
        static auto SetCanvasValue( core::u64 session, core::u64 entity, core::u64 element, core::f32 value ) -> core::i32;

        static auto PlayAudio( core::u64 session, core::u64 entity, core::cstr path ) -> core::i32;

        static auto GetTransform( core::u64 session, core::u64 entity, core::i32 kind,
                                  ManagedVector3* output ) -> core::i32;

        static auto SetTransform( core::u64 session, core::u64 entity, core::i32 kind,
                                  const ManagedVector3* input ) -> core::i32;
    };

    /** @brief Binds focused gameplay keyboard and mouse input for scene-owned callbacks. */
    class DotnetInputBinding final : public DotnetBinding {
    public:
        auto Init( ManagedNativeApi& api ) -> void override;

    private:
        static auto IsKeyPressed( core::i32 key ) -> core::i32;
        static auto IsMouseButtonPressed( core::i32 button ) -> core::i32;
        static auto GetMousePosition( core::f32* x, core::f32* y ) -> core::i32;
        static auto GetMouseScrollDelta( core::f32* x, core::f32* y ) -> core::i32;
        static auto IsInputFocused() -> core::i32;
        static auto IsPointerOverInput() -> core::i32;
    };

    /** @brief Binds logging and compilation diagnostics across the native ABI. */
    class DotnetSystemBinding final : public DotnetBinding {
    public:
        auto Init( ManagedNativeApi& api ) -> void override;

    private:
        static auto Log( core::i32 severity, core::cstr message ) -> void;
        static auto ReportCompilation( const ManagedCompilationReport* report ) -> void;
    };

    /**
     * @brief Binds managed vector and matrix operations to GLM.
     * @details Like MathBinding for Lua, arithmetic runs in native GLM types.
     * This binding owns no scene state and is safe to call from managed workers.
     * The returned table has process lifetime and is published with the host API.
     */
    class DotnetMathBinding final : public DotnetBinding {
    public:
        auto Init( ManagedNativeApi& api ) -> void override;

        MKT_NODISCARD static auto GetApi() -> const ManagedMathApi&;

    private:
        static auto VectorOperation( ManagedVectorOperation operation, core::u32 dimension,
                                     const core::f32* left, const core::f32* right, core::f32* output ) noexcept -> core::i32;

        static auto MatrixOperation( ManagedMatrixOperation operation, core::u32 dimension,
                                     const core::f32* left, const core::f32* right, core::f32* output ) noexcept -> core::i32;
    };
}// namespace mikoto::scripting

#endif// MIKOTO_SCRIPTING_DOTNET_BINDINGS_HH
