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

#ifndef MIKOTO_SCRIPTING_DOTNET_INTEROP_HH
#define MIKOTO_SCRIPTING_DOTNET_INTEROP_HH

#include <Core/Types.hh>

namespace mikoto::scripting {
    // Versioned, blittable C ABI shared with NativeBridge.cs. Never pass STL,
    // EASTL, GLM or managed object layouts over this boundary.
    struct ManagedVector3 {
        core::f32 mX{};
        core::f32 mY{};
        core::f32 mZ{};
    };

    // These operation values and float-buffer layouts match MathTypes.cs.
    enum class ManagedVectorOperation : core::u32 {
        eConstruct,
        eAdd,
        eSubtract,
        eMultiply,
        eDivide,
        eNegate,
        eEqual,
        eLength,
        eDot,
        eNormalize,
        eCross
    };

    enum class ManagedMatrixOperation : core::u32 {
        eConstruct,
        eMultiply,
        eEqual
    };

    /**
     * @brief Stateless GLM entry points available to managed value types.
     * @details Vector buffers contain dimension floats; square matrix buffers
     * contain dimension * dimension floats in column-major order. Scalar results
     * occupy output[0]. No GLM object layout crosses the native boundary.
     * Callers supply non-null buffers of the matching size and dimension 2, 3 or 4.
     * Returns one on success, zero for an invalid dimension, operation or pointer.
     */
    struct ManagedMathApi {
        core::i32 ( *mVector )( ManagedVectorOperation, core::u32, const core::f32*, const core::f32*, core::f32* ){};
        core::i32 ( *mMatrix )( ManagedMatrixOperation, core::u32, const core::f32*, const core::f32*, core::f32* ){};
    };

    struct ManagedCompilationReport {
        core::u32 mSize{};
        // 0: compiling, 1: success, 2: cache hit, 3: failed.
        core::u32 mStatus{};
        core::cstr mSource{};
        core::cstr mOutput{};
        core::cstr mDiagnostics{};
        core::cstr mRuntime{};
        core::f64 mMilliseconds{};
        core::u32 mFiles{};
        core::u32 mErrors{};
        core::u32 mWarnings{};
        core::u32 mReserved{};
    };

    // Borrowed UTF-8 strings are copied by registration callbacks before returning.
    struct ManagedScriptClassInfo {
        core::u32 mSize{};
        core::u32 mClassID{};
        core::cstr mName{};
        core::cstr mNamespace{};
        core::u32 mFieldCount{};
    };

    struct ManagedScriptFieldInfo {
        core::u32 mSize{};
        core::u32 mClassID{};
        core::u32 mFieldID{};
        core::u32 mType{};
        core::cstr mName{};
        core::cstr mAssetKind{};
        core::cstr mDefaultJson{};
        core::u32 mReadOnly{};
    };

    // Keep these values synchronized with ComponentType in Component.cs.
    enum class ManagedComponentType : core::u32 {
        eTransform = 0,
        eLight = 1,
        eCanvas = 2,
        eVehicle = 3,
        eCharacter = 4,
    };

    struct ManagedNativeApi {
        core::u32 mSize{};
        core::u32 mVersion{ 9 };
        core::i32 ( *mGetTransform )( core::u64, core::u64, core::i32, ManagedVector3* ){};
        core::i32 ( *mSetTransform )( core::u64, core::u64, core::i32, const ManagedVector3* ){};
        core::i32 ( *mIsKeyPressed )( core::i32 ){};
        // Severity: 0 info, 1 warning, 2 error, 3 debug.
        void ( *mLog )( core::i32, core::cstr ){};
        void ( *mReportCompilation )( const ManagedCompilationReport* ){};
        void ( *mRegisterScriptClass )( const ManagedScriptClassInfo* ){};
        void ( *mRegisterScriptField )( const ManagedScriptFieldInfo* ){};
        void ( *mUnregisterScriptClass )( core::u32 ){};
        core::i32 ( *mPlayAudio )( core::u64, core::u64, core::cstr ){};
        const ManagedMathApi* mMath{};

        core::i32 ( *mHasComponent )( core::u64, core::u64, ManagedComponentType ){};
        core::i32 ( *mGetLightIntensity )( core::u64, core::u64, core::f32* ){};
        core::i32 ( *mSetLightIntensity )( core::u64, core::u64, core::f32 ){};
        core::i32 ( *mFindCanvasElement )( core::u64, core::u64, core::cstr, core::u64* ){};
        core::i32 ( *mGetCanvasValue )( core::u64, core::u64, core::u64, core::f32* ){};
        core::i32 ( *mSetCanvasValue )( core::u64, core::u64, core::u64, core::f32 ){};
        core::i32 ( *mFindEntity )( core::u64, core::u64, core::cstr, core::u64* ){};
        core::i32 ( *mSetVehicleInputs )( core::u64, core::u64, core::f32, core::f32, core::f32, core::f32 ){};
        core::i32 ( *mGetVehicleSpeed )( core::u64, core::u64, core::f32* ){};
        core::i32 ( *mSetCanvasText )( core::u64, core::u64, core::u64, core::cstr ){};
        core::i32 ( *mSetCharacterInputs )( core::u64, core::u64, const ManagedVector3*, core::f32 ){};
        core::i32 ( *mGetCharacterGrounded )( core::u64, core::u64 ){};
        core::i32 ( *mIsMouseButtonPressed )( core::i32 ){};
        core::i32 ( *mGetMousePosition )( core::f32*, core::f32* ){};
        core::i32 ( *mIsInputFocused )(){};
        core::i32 ( *mIsPointerOverInput )(){};
        core::i32 ( *mGetMouseScrollDelta )( core::f32*, core::f32* ){};
    };

    enum class ManagedOperation : core::u32 {
        eBind,
        eCreate,
        eUpdate,
        eDestroy,
        eStopScene,
        eCompile,
        eReloadScene,
        eInspect,
        eSetFields
    };

    struct ManagedCommand {
        core::u32 mSize{};
        ManagedOperation mOperation{};
        core::u64 mScene{};
        core::u64 mEntity{};
        core::cstr mAssembly{};
        core::cstr mClass{};
        const ManagedNativeApi* mNativeApi{};
        core::f32 mTimeStep{};
        core::u32 mReserved{};
        core::u32* mClassID{};
        core::cstr mValues{};
    };
}// namespace mikoto::scripting

#endif// MIKOTO_SCRIPTING_DOTNET_INTEROP_HH
