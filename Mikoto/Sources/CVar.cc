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

#include <cerrno>
#include <charconv>
#include <cmath>
#include <cstdlib>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Core/CVar.hh>

namespace mikoto::core {

    // Global instance CVarRegistry
    static CVarRegistry gCVarRegistry{};

    namespace {
        auto HasMutableValue( const CVarFlags flags ) -> bool {
            return ( flags & ( CVarFlags::eReadOnly | CVarFlags::eStartupOnly ) ) == CVarFlags::eNone;
        }
    }

    auto CVarTextCodec<bool>::Parse( const eastl::string_view value, bool& result ) -> bool {
        if ( value == "1" || value == "true" || value == "on" || value == "yes" ) { result = true; return true; }
        if ( value == "0" || value == "false" || value == "off" || value == "no" ) { result = false; return true; }
        return false;
    }

    auto CVarTextCodec<bool>::Format( const bool value ) -> eastl::string { return value ? "true" : "false"; }

    auto CVarTextCodec<i32>::Parse( const eastl::string_view value, i32& result ) -> bool {
        const auto [end, error]{ std::from_chars( value.data(), value.data() + value.size(), result ) };
        return error == std::errc{} && end == value.data() + value.size();
    }

    auto CVarTextCodec<i32>::Format( const i32 value ) -> eastl::string {
        return string::Format( "{}", value );
    }

    auto CVarTextCodec<f32>::Parse( const eastl::string_view value, f32& result ) -> bool {
        const eastl::string nullTerminatedValue{ value };
        char* end{};
        errno = 0;
        const f32 parsedValue{ std::strtof( nullTerminatedValue.c_str(), &end ) };
        if ( errno == ERANGE || end != nullTerminatedValue.c_str() + nullTerminatedValue.size() || !std::isfinite( parsedValue ) ) {
            return false;
        }
        result = parsedValue;
        return true;
    }

    auto CVarTextCodec<f32>::Format( const f32 value ) -> eastl::string {
        return string::Format( "{}", value );
    }

    auto CVarTextCodec<eastl::string>::Parse( const eastl::string_view value, eastl::string& result ) -> bool {
        result.assign( value.data(), value.size() );
        return true;
    }

    auto CVarTextCodec<eastl::string>::Format( const eastl::string& value ) -> eastl::string { return value; }

    ICVar::ICVar( CVarMetadata metadata )
        : mMetadata{ eastl::move( metadata ) } {

    }

    auto ICVar::GetMetadata() const -> const CVarMetadata& {
        return mMetadata;
    }

    auto CVarRegistry::SetFromAssignment( const eastl::string_view assignment ) -> bool {
        const eastl::string_view::size_type separator{ assignment.find( '=' ) };
        return separator != eastl::string_view::npos && separator != 0 && Set( assignment.substr( 0, separator ), assignment.substr( separator + 1 ) );
    }

    auto CVarRegistry::Set( const eastl::string_view name, const eastl::string_view value ) -> bool {
        ICVar* variable{};
        {
            std::lock_guard lock{ mMutex };
            const eastl::string key{ name };
            if ( const auto registered{ mVariables.find( key ) }; registered != mVariables.end() ) {
                variable = registered->second.get();
            } else {
                mPendingValues[key] = eastl::string{ value };
                return true;
            }
        }
        return variable->SetFromString( value );
    }

    auto CVarRegistry::Toggle( const eastl::string_view name ) -> bool {
        auto* const variable{ Find( name ) };
        if ( variable == nullptr || variable->GetMetadata().mType != CVarType::eBool || !HasMutableValue( variable->GetMetadata().mFlags ) ) {
            return false;
        }
        auto* const booleanVariable{ static_cast<CVar<bool>*>( variable ) };
        return booleanVariable->SetValue( !booleanVariable->GetValue() );
    }

    auto CVarRegistry::GetString( const eastl::string_view name, const eastl::string_view fallback ) const -> eastl::string {
        const auto* const variable{ Find( name ) };
        return variable != nullptr ? variable->ToString() : eastl::string{ fallback };
    }

    auto CVarRegistry::GetMetadata( const eastl::string_view name ) const -> const CVarMetadata* {
        const auto* const variable{ Find( name ) };
        return variable != nullptr ? &variable->GetMetadata() : nullptr;
    }

    auto CVarRegistry::Contains( const eastl::string_view name ) const -> bool { return Find( name ) != nullptr; }

    auto CVarRegistry::Find( const eastl::string_view name ) const -> ICVar* {
        std::lock_guard lock{ mMutex };
        if ( const auto variable{ mVariables.find( eastl::string{ name } ) }; variable != mVariables.end() ) {
            return variable->second.get();
        }
        return nullptr;
    }
}
