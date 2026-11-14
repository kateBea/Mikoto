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

#include <cctype>

#include <EASTL/utility.h>
#include <EASTL/array.h>

#include <Core/CVar.hh>
#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>

#include <Logging/Logger.hh>

#include <Platform/Event.hh>
#include <Platform/Utility.hh>
#include <Platform/CoreEvents.hh>
#include <Platform/ActionManager.hh>

namespace mikoto::platform {

    MKT_NODISCARD static auto MakeShortcutKey( platform::KeyCode key, platform::ModKey mods ) -> ShortcutKey {
        return { key, core::as<core::i32>( mods & ( ModKey::eShift | ModKey::eControl | ModKey::eAlt | ModKey::eSuper ) ) };
    }

    ActionManager::ActionManager( const ActionManagerCreateInfo& )
    {
    }

    auto ActionManager::Initialize() -> void {
        MKT_BEGIN_PROFILER();
        MKT_CORE_LOGGER_DEBUG( "Initializing ActionManager" );

        mIsInitialized = true;
    }

    auto ActionManager::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !mIsInitialized ) {
            return;
        }

        MKT_CORE_LOGGER_DEBUG( "Shutting down ActionManager" );

        mActions.clear();
    }

    auto ActionManager::Dispatch( platform::KeyCode key, platform::ModKey mods ) const -> bool {
        MKT_BEGIN_PROFILER();
        const ShortcutKey keyCombo{ MakeShortcutKey( key, mods ) };
        auto it{ mActions.find( keyCombo ) };

        if ( it != mActions.end() ) {
            it->second.mAction();
            return true;
        }

        return false;
    }

    auto ActionManager::Bind( platform::KeyCode key, platform::ModKey mods, ShortcutAction action, eastl::string_view description ) -> void {
        MKT_BEGIN_PROFILER();
        mActions[MakeShortcutKey( key, mods )] = ShortcutBinding{ std::move( action ), eastl::string{ description } };
    }

    auto ActionManager::GetBindings() const -> const ShortcutMap& {
        return mActions;
    }

    auto ActionManager::BindCVarToggle( const platform::KeyCode key, const platform::ModKey mods, const eastl::string_view cvarName, const eastl::string_view description ) -> void {
        MKT_BEGIN_PROFILER();
        Bind( key, mods, [name = eastl::string{ cvarName }]() -> void {
            core::CVarRegistry::Get().Toggle( name );
        }, description );
    }

    auto ShortcutKey::GetName() const -> eastl::string {
        eastl::string name{};
        const eastl::array modifierNames{
            eastl::pair<ModKey, core::cstr>{ ModKey::eAlt, "Alt+" },
            eastl::pair<ModKey, core::cstr>{ ModKey::eShift, "Shift+" },
            eastl::pair<ModKey, core::cstr>{ ModKey::eSuper, "Super+" },
            eastl::pair<ModKey, core::cstr>{ ModKey::eControl, "Ctrl+" },
        };

        for ( const auto& [modifier, text]: modifierNames ) {
            if ( modifiers & core::as<core::i32>( modifier ) ) {
                name += text;
            }
        }

        // Printable keys use their character, named keys use the stable platform
        // names without the implementation prefix (e.g. DELETE becomes Delete).
        if ( key > Key_Space && key <= Key_Grave_Accent ) {
            name += core::as<char>( key );
            return name;
        }

        eastl::string_view keyName{ GetStringRepresentation( key ) };
        constexpr eastl::string_view prefix{ "MKT_KEY_" };

        if ( !keyName.starts_with( prefix ) ) {
            name.append( keyName.data(), keyName.size() );
            return name;
        }

        keyName.remove_prefix( prefix.size() );

        if ( key >= Key_F1 && key <= Key_F25 ) {
            name.append( keyName.data(), keyName.size() );
            return name;
        }

        bool wordStart{ true };

        for ( const char character: keyName ) {
            if ( character == '_' ) {
                name += ' ';
                wordStart = true;
            }
            else {
                name += wordStart ? character : core::as<char>( std::tolower( core::as<unsigned char>( character ) ) );
                wordStart = false;
            }
        }

        return name;
    }

    bool ShortcutKey::operator==( const ShortcutKey& other ) const {
        MKT_BEGIN_PROFILER();
        return key == other.key && modifiers == other.modifiers;
    }


    auto ShortcutKeyHash::operator()( const ShortcutKey& sk ) const -> core::usize {
        MKT_BEGIN_PROFILER();
        core::usize h1{ eastl::hash<int>{}( sk.key ) };
        core::usize h2{ eastl::hash<core::i32>{}( core::as<core::i32>( sk.modifiers ) ) };
        return h1 ^ ( h2 << 1 );// Displace bits to combine hashes
    }
}