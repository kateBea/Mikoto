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


#include <EASTL/vector.h>

#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Filesystem/Path.hh>

#include <Scripting/Dotnet/DotnetBindings.hh>
#include <Scripting/Dotnet/DotnetRuntimeState.hh>

#if MIKOTO_ENABLE_DOTNET_SCRIPTING
#include <coreclr_delegates.h>
#include <hostfxr.h>
#include <nethost.h>

#if defined( _WIN32 )
#include <Platform/Windows/Windows.hh>
#else
#include <dlfcn.h>
#endif
#endif


namespace mikoto::scripting {
    using namespace core;


    static thread_local DotnetRuntimeState* sMetadataOwner{};


    auto DotnetRuntimeState::Initialize() -> void {
        MKT_BEGIN_PROFILER();

        if ( mIsInitialized ) {
            return;
        }

        try {
            InitBindings();
            mIsInitialized = true;
        } catch ( ... ) {
            mBindings.Clear();
            mNativeApi = {};
            throw;
        }
    }

    auto DotnetRuntimeState::Shutdown() -> void {
        MKT_BEGIN_PROFILER();

        // Managed sessions are stopped by the owner before dropping local state.
        // The CLR and host library remain loaded; another service may still use them.
        mExecute = nullptr;
        mAttempted = false;
        mNativeApi = {};
        mBindings.Clear();
        mScriptClasses.clear();
        mIsInitialized = false;
    }

    auto DotnetRuntimeState::IsStarted() const -> bool {
        return mExecute != nullptr;
    }

    auto DotnetRuntimeState::InitBindings() -> void {
        mBindings.Register<DotnetSceneBinding>();
        mBindings.Register<DotnetInputBinding>();
        mBindings.Register<DotnetMathBinding>();
        mBindings.Register<DotnetSystemBinding>();

        mNativeApi.mSize = sizeof( ManagedNativeApi );
        // ManagedNativeApi's default version is shared by every native runtime instance.
        // Keep it intact so extending the callback table cannot leave a stale override here.
        mNativeApi.mRegisterScriptClass = &RegisterScriptClass;
        mNativeApi.mRegisterScriptField = &RegisterScriptField;
        mNativeApi.mUnregisterScriptClass = &UnregisterScriptClass;

        for ( const auto& [id, binding]: mBindings ) {
            binding->Init( mNativeApi );
        }
    }

    auto DotnetRuntimeState::Call( ManagedCommand command ) -> bool {
        command.mSize = sizeof( ManagedCommand );
        // Callbacks belong to the invoking service, including independent compiler services.
        auto* previous{ sMetadataOwner };
        sMetadataOwner = this;
        const bool success{ mExecute && mExecute( &command ) == 0 };
        sMetadataOwner = previous;
        return success;
    }

    auto DotnetRuntimeState::GetMetadata( u32 classID, ScriptClassMetadata& output ) const -> bool {
        const auto found{ mScriptClasses.find( classID ) };

        if ( found == mScriptClasses.end() ) {
            return false;
        }

        output = found->second;
        return true;
    }

    auto DotnetRuntimeState::RegisterScriptClass( const ManagedScriptClassInfo* info ) -> void {
        try {
            if ( !sMetadataOwner || !info || info->mSize != sizeof( ManagedScriptClassInfo ) || !info->mName ) {
                return;
            }

            ScriptClassMetadata metadata{};
            metadata.mClassID = info->mClassID;
            metadata.mName = info->mName;
            metadata.mNamespace = info->mNamespace ? info->mNamespace : "";
            metadata.mLanguage = ScriptLanguage::eCSharp;
            metadata.mFieldLookup.reserve( info->mFieldCount );
            sMetadataOwner->mScriptClasses.insert_or_assign( info->mClassID, eastl::move( metadata ) );
        } catch ( ... ) {
            // Allocation failures must not unwind through the unmanaged callback boundary.
        }
    }

    auto DotnetRuntimeState::RegisterScriptField( const ManagedScriptFieldInfo* info ) -> void {
        try {
            if ( !sMetadataOwner || !info || info->mSize != sizeof( ManagedScriptFieldInfo ) || !info->mName ) {
                return;
            }

            auto found{ sMetadataOwner->mScriptClasses.find( info->mClassID ) };

            if ( found == sMetadataOwner->mScriptClasses.end() ) {
                return;
            }

            ScriptFieldMetadata field{};
            field.mFieldID = info->mFieldID;
            field.mName = info->mName;
            field.mType = static_cast<ScriptValueType>( info->mType );
            field.mAssetKind = info->mAssetKind ? info->mAssetKind : "";
            field.mDefaultJson = info->mDefaultJson ? info->mDefaultJson : "null";
            field.mReadOnly = info->mReadOnly != 0;
            found->second.mFieldLookup.insert_or_assign( field.mFieldID, eastl::move( field ) );
        } catch ( ... ) {
        }
    }

    auto DotnetRuntimeState::UnregisterScriptClass( u32 classID ) -> void {
        if ( sMetadataOwner ) {
            sMetadataOwner->mScriptClasses.erase( classID );
        }
    }

    auto DotnetRuntimeState::Start() -> bool {
        if ( !mIsInitialized ) {
            return false;
        }

        if ( mAttempted ) {
            return mExecute != nullptr;
        }

        mAttempted = true;
#if MIKOTO_ENABLE_DOTNET_SCRIPTING
        // Locate the installed host, then resolve its runtime-loading functions.
        usize length{};
        ( void )get_hostfxr_path( nullptr, &length, nullptr );

        if ( length == 0 ) {
            MKT_CORE_LOGGER_ERROR( "C#: .NET hostfxr was not found. Install the .NET 10 runtime." );
            return false;
        }

        eastl::vector<char_t> hostPath( length );

        if ( get_hostfxr_path( hostPath.data(), &length, nullptr ) != 0 ) {
            MKT_CORE_LOGGER_ERROR( "C#: could not resolve hostfxr." );
            return false;
        }

#if defined( _WIN32 )
        static const auto library{ LoadLibraryW( hostPath.data() ) };
        const auto symbol{ []( cstr name ) {
            return library ? GetProcAddress( library, name ) : nullptr;
        } };
#else
        static const auto library{ dlopen( hostPath.data(), RTLD_NOW | RTLD_LOCAL ) };
        const auto symbol{ []( cstr name ) {
            return library ? dlsym( library, name ) : nullptr;
        } };
#endif
        // CoreCLR is process-wide and cannot be restarted like a Lua VM.
        // Keep hostfxr loaded; stopping Play unloads managed script contexts.
        const auto initialize{ reinterpret_cast<hostfxr_initialize_for_runtime_config_fn>( symbol( "hostfxr_initialize_for_runtime_config" ) ) };
        const auto getDelegate{ reinterpret_cast<hostfxr_get_runtime_delegate_fn>( symbol( "hostfxr_get_runtime_delegate" ) ) };
        const auto close{ reinterpret_cast<hostfxr_close_fn>( symbol( "hostfxr_close" ) ) };

        if ( !initialize || !getDelegate || !close ) {
            MKT_CORE_LOGGER_ERROR( "C#: could not load the required hostfxr entry points." );
            return false;
        }

        // The deployed runtime configuration chooses CoreCLR and its framework version.
        const filesystem::Path config{ "Resources/Scripting/Mikoto.ScriptCore.runtimeconfig.json" };
        const filesystem::Path assembly{ "Resources/Scripting/Mikoto.ScriptCore.dll" };

        if ( !config.Exists() || !assembly.Exists() ) {
            MKT_CORE_LOGGER_ERROR( "C#: missing Resources/Scripting. Build the MikotoScriptCore target and run from the editor output directory." );
            return false;
        }
#if defined( _WIN32 )
        const auto configPath{ filesystem::Path{ config.GetAbsolute() }.GetPathTyped<eastl::wstring>() };
        const auto assemblyPath{ filesystem::Path{ assembly.GetAbsolute() }.GetPathTyped<eastl::wstring>() };
        constexpr const char_t* type{ L"Mikoto.Hosting.NativeBridge, Mikoto.ScriptCore" };
        constexpr const char_t* method{ L"Execute" };
#else
        const auto configPath{ config.GetAbsolute() };
        const auto assemblyPath{ assembly.GetAbsolute() };
        constexpr const char_t* type{ "Mikoto.Hosting.NativeBridge, Mikoto.ScriptCore" };
        constexpr const char_t* method{ "Execute" };
#endif

        // Closing this initialization handle does not unload the process-wide CLR.
        hostfxr_handle context{};
        const i32 status{ initialize( configPath.c_str(), nullptr, &context ) };

        if ( status < 0 || !context ) {
            if ( context ) {
                close( context );
            }

            MKT_CORE_LOGGER_ERROR( "C#: hostfxr initialization failed ({:#x}). A compatible .NET 10 runtime is required.", static_cast<u32>( status ) );
            return false;
        }

        void* delegate{};
        const i32 delegateStatus{ getDelegate( context, hdt_load_assembly_and_get_function_pointer, &delegate ) };
        close( context );

        if ( delegateStatus != 0 || !delegate ) {
            MKT_CORE_LOGGER_ERROR( "C#: failed to get the assembly loader ({:#x}).", static_cast<u32>( delegateStatus ) );
            return false;
        }

        // Load the managed bridge and publish this service's native callback table.
        const auto load{ reinterpret_cast<load_assembly_and_get_function_pointer_fn>( delegate ) };
        void* entry{};
        const i32 loadStatus{ load( assemblyPath.c_str(), type, method, UNMANAGEDCALLERSONLY_METHOD, nullptr, &entry ) };

        if ( loadStatus != 0 || !entry ) {
            MKT_CORE_LOGGER_ERROR( "C#: failed to load Mikoto.ScriptCore ({:#x}).", static_cast<u32>( loadStatus ) );
            return false;
        }

        // Store the single managed entry point to C#
        mExecute = reinterpret_cast<EntryPoint>( entry );

        if ( !Call( { .mOperation = ManagedOperation::eBind, .mNativeApi = &mNativeApi } ) ) {
            mExecute = nullptr;
            MKT_CORE_LOGGER_ERROR( "C#: native/managed bridge ABI mismatch." );
            return false;
        }

        return true;
#else
        MKT_CORE_LOGGER_ERROR( "C#: enable MIKOTO_ENABLE_DOTNET_SCRIPTING and rebuild to run managed scripts." );
        return false;
#endif
    }

}// namespace mikoto::scripting
