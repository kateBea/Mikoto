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

#ifndef MIKOTO_CVAR_HH
#define MIKOTO_CVAR_HH

#include <mutex>
#include <memory>
#include <concepts>

#include <EASTL/string.h>
#include <EASTL/functional.h>
#include <EASTL/string_view.h>
#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>
#include <Core/Singleton.hh>

namespace mikoto::core {

    /** @brief Controls persistence and mutation behaviour of a configuration variable. */
    enum class CVarFlags : u32 {
        eNone = 0,
        eArchive = 1u << 0,
        eReadOnly = 1u << 1,
        eStartupOnly = 1u << 2,
        eRestartRequired = 1u << 3,
    };

    /**
     * @brief Combines two CVar flag sets.
     * @param left First flag set.
     * @param right Second flag set.
     * @returns The combined flag set.
     */
    inline auto operator|( const CVarFlags left, const CVarFlags right ) -> CVarFlags {
        return as<CVarFlags>( as<u32>( left ) | as<u32>( right ) );
    }

    /**
     * @brief Returns flags that occur in both flag sets.
     * @param left First flag set.
     * @param right Second flag set.
     * @returns The intersecting flag set.
     */
    inline auto operator&( const CVarFlags left, const CVarFlags right ) -> CVarFlags {
        return as<CVarFlags>( as<u32>( left ) & as<u32>( right ) );
    }

    /** @brief Identifies the concrete value stored by a CVar. */
    enum class CVarType : u8 { eBool, eInt, eFloat, eString };

    /** @brief Describes a CVar independently of its current value. */
    struct CVarMetadata {
        eastl::string mName{};
        eastl::string mDescription{};
        CVarFlags mFlags{ CVarFlags::eNone };
        CVarType mType{ CVarType::eString };
    };

    template<typename Type>
    struct CVarTypeTraits;

    template<> struct CVarTypeTraits<bool> { static constexpr CVarType kType{ CVarType::eBool }; };
    template<> struct CVarTypeTraits<i32> { static constexpr CVarType kType{ CVarType::eInt }; };
    template<> struct CVarTypeTraits<f32> { static constexpr CVarType kType{ CVarType::eFloat }; };
    template<> struct CVarTypeTraits<eastl::string> { static constexpr CVarType kType{ CVarType::eString }; };

    /** @brief Restricts CVar APIs to value types with a built-in text codec. */
    template<typename Type>
    concept CVarValue = std::same_as<Type, bool> || std::same_as<Type, i32> || std::same_as<Type, f32> || std::same_as<Type, eastl::string>;

    template<CVarValue Type>
    struct CVarTextCodec;

    template<> struct CVarTextCodec<bool> {
        MKT_NODISCARD static auto Parse( eastl::string_view value, bool& result ) -> bool;
        MKT_NODISCARD static auto Format( bool value ) -> eastl::string;
    };
    template<> struct CVarTextCodec<i32> {
        MKT_NODISCARD static auto Parse( eastl::string_view value, i32& result ) -> bool;
        MKT_NODISCARD static auto Format( i32 value ) -> eastl::string;
    };
    template<> struct CVarTextCodec<f32> {
        MKT_NODISCARD static auto Parse( eastl::string_view value, f32& result ) -> bool;
        MKT_NODISCARD static auto Format( f32 value ) -> eastl::string;
    };
    template<> struct CVarTextCodec<eastl::string> {
        MKT_NODISCARD static auto Parse( eastl::string_view value, eastl::string& result ) -> bool;
        MKT_NODISCARD static auto Format( const eastl::string& value ) -> eastl::string;
    };

    /** @brief Type-erased common interface used by the central CVar registry. */
    class ICVar {
    public:
        virtual ~ICVar() = default;

        /**
         * @brief Returns immutable metadata for this variable.
         * @returns The variable metadata. */
        MKT_NODISCARD auto GetMetadata() const -> const CVarMetadata&;

        /** @brief Converts the current value to console/command-line text.
         * @returns The current value as text. */
        MKT_NODISCARD virtual auto ToString() const -> eastl::string = 0;

        /**
         * @brief Parses and applies a console/command-line value.
         * @param value Text to parse as this CVar's concrete type.
         * @returns True when parsing and setting succeeded.
         */
        virtual auto SetFromString( eastl::string_view value ) -> bool = 0;

    protected:
        /**
         * @brief Constructs immutable CVar metadata.
         * @param metadata Name, help text, flags, and concrete type. */
        explicit ICVar( CVarMetadata metadata );

    private:
        CVarMetadata mMetadata{};
    };

    template<CVarValue Type>
    class CVar final : public ICVar {
    public:
        using ChangedCallback = eastl::function<void( const Type& value )>;

        /**
         * @brief Creates a typed CVar owned by the registry.
         * @param metadata Immutable descriptive metadata.
         * @param defaultValue Value used when no command-line override exists.
         * @param value Initial value after applying a valid startup override.
         * @param callback Optional callback invoked after a successful value change.
         */
        CVar( CVarMetadata metadata, Type defaultValue, Type value, ChangedCallback callback ) :
            ICVar{ eastl::move( metadata ) }, mDefaultValue{ eastl::move( defaultValue ) },
            mValue{ eastl::move( value ) }, mCallback{ eastl::move( callback ) } { }

        /**
         * @brief Returns a copy of the current typed value.
         * @returns The current value. */
        MKT_NODISCARD auto GetValue() const -> Type {
            std::lock_guard lock{ mMutex };
            return mValue;
        }

        /**
         * @brief Returns the value registered before overrides were applied.
         * @returns The typed default value. */
        MKT_NODISCARD auto GetDefaultValue() const -> Type { return mDefaultValue; }

        /**
         * @brief Updates the value and invokes its callback after releasing the value lock.
         * @param value New typed value.
         * @returns False when the CVar cannot be changed at runtime.
         */
        auto SetValue( Type value ) -> bool {
            ChangedCallback callback{};
            Type callbackValue{};
            {
                std::lock_guard lock{ mMutex };
                const CVarFlags flags{ GetMetadata().mFlags };
                if ( ( flags & ( CVarFlags::eReadOnly | CVarFlags::eStartupOnly ) ) != CVarFlags::eNone ) {
                    return false;
                }
                if ( mValue == value ) {
                    return true;
                }
                mValue = eastl::move( value );
                callbackValue = mValue;
                callback = mCallback;
            }
            if ( callback ) {
                callback( callbackValue );
            }
            return true;
        }

        /**
         * @brief Converts the current value to text.
         * @returns The current formatted value. */
        MKT_NODISCARD auto ToString() const -> eastl::string override { return CVarTextCodec<Type>::Format( GetValue() ); }

        /**
         * @brief Parses text as this CVar's type and applies it.
         * @param value Text supplied by a console or command line.
         * @returns True when parsing and setting succeeded.
         */
        auto SetFromString( const eastl::string_view value ) -> bool override {
            Type parsedValue{};
            return CVarTextCodec<Type>::Parse( value, parsedValue ) && SetValue( eastl::move( parsedValue ) );
        }

    private:
        Type mDefaultValue{};
        mutable std::mutex mMutex{};
        Type mValue{};
        ChangedCallback mCallback{};
    };

    template<CVarValue Type>
    class CVarRef final {
    public:
        /**
         * @brief Creates an invalid typed CVar handle. */
        CVarRef() = default;

        /**
         * @brief Returns whether this handle refers to a registered typed CVar.
         * @returns True when valid. */
        MKT_NODISCARD explicit operator bool() const { return mVariable != nullptr; }

        /**
         * @brief Retrieves the current typed value.
         * @returns The value, or a value-initialized Type when invalid. */
        MKT_NODISCARD auto Get() const -> Type { return mVariable != nullptr ? mVariable->GetValue() : Type{}; }

        /**
         * @brief Attempts to update the referenced CVar.
         * @param value New typed value.
         * @returns True when the handle is valid and the change is allowed.
         */
        auto Set( Type value ) const -> bool { return mVariable != nullptr && mVariable->SetValue( eastl::move( value ) ); }

        /**
         * @brief Returns the CVar metadata.
         * @returns Metadata, or nullptr when invalid. */
        MKT_NODISCARD auto GetMetadata() const -> const CVarMetadata* { return mVariable != nullptr ? &mVariable->GetMetadata() : nullptr; }

    private:
        explicit CVarRef( CVar<Type>* variable ) : mVariable{ variable } { }

        CVar<Type>* mVariable{};

        friend class CVarRegistry;
    };

    /**
     * @brief Central registry that owns every configuration variable for process lifetime.
     *
     * Registry locking only protects lookup and registration. Typed entries own their own
     * locks, so CVarRef handles remain safe and inexpensive after registration. Text exists
     * only at the command-line and runtime-console boundaries.
     */
    class CVarRegistry final : public Singleton<CVarRegistry> {
    public:

        /**
         * @brief Registers a strongly typed variable or returns its existing typed handle.
         * @tparam Type Supported CVar value type.
         * @param name Stable dotted variable name.
         * @param description User-facing help text.
         * @param defaultValue Value used when no valid startup override exists.
         * @param flags Persistence and mutation flags.
         * @param callback Optional type-safe callback invoked after later changes.
         * @returns A typed handle, or an invalid one if @p name already has another type.
         */
        template<CVarValue Type>
        auto Register( eastl::string_view name, eastl::string_view description, Type defaultValue,
            CVarFlags flags = CVarFlags::eNone, typename CVar<Type>::ChangedCallback callback = {} ) -> CVarRef<Type>;

        /**
         * @brief Looks up a variable's current typed value.
         * @tparam Type Expected CVar value type.
         * @param name Stable dotted variable name.
         * @param fallback Value returned for missing or differently typed variables.
         * @returns The current typed value or @p fallback.
         */
        template<CVarValue Type>
        MKT_NODISCARD auto GetValue( eastl::string_view name, Type fallback = {} ) const -> Type;

        /**
         * @brief Sets a variable through a type-safe lookup.
         * @tparam Type Expected CVar value type.
         * @param name Stable dotted variable name.
         * @param value New typed value.
         * @returns False for missing, differently typed, or immutable variables.
         */
        template<CVarValue Type>
        auto SetValue( eastl::string_view name, Type value ) -> bool;

        /**
         * @brief Parses and applies a @c name=value console or command-line assignment.
         * @param assignment Assignment text.
         * @returns False for malformed assignments or a value invalid for a registered type.
         */
        auto SetFromAssignment( eastl::string_view assignment ) -> bool;

        /**
         * @brief Sets a CVar from text; unknown names become pending startup overrides.
         * @param name Stable dotted variable name.
         * @param value Text to parse as the variable's registered type.
         * @returns False when the value is invalid or the CVar is immutable.
         */
        auto Set( eastl::string_view name, eastl::string_view value ) -> bool;

        /**
         * @brief Toggles a boolean CVar.
         * @param name Stable dotted variable name.
         * @returns True on success. */
        auto Toggle( eastl::string_view name ) -> bool;

        /**
         * @brief Converts a registered CVar to text for runtime-console inspection.
         * @param name Stable dotted variable name.
         * @param fallback Value returned for unknown CVars.
         * @returns The formatted current value or @p fallback.
         */
        MKT_NODISCARD auto GetString( eastl::string_view name, eastl::string_view fallback = {} ) const -> eastl::string;

        /**
         * @brief Returns CVar metadata.
         * @param name Stable dotted variable name.
         * @returns Metadata, or nullptr when unknown. */
        MKT_NODISCARD auto GetMetadata( eastl::string_view name ) const -> const CVarMetadata*;

        /**
         * @brief Checks whether a CVar is registered.
         * @param name Stable dotted variable name.
         * @returns True when it exists. */
        MKT_NODISCARD auto Contains( eastl::string_view name ) const -> bool;

    private:
        MKT_NODISCARD auto Find( eastl::string_view name ) const -> ICVar*;

        mutable std::mutex mMutex{};
        ankerl::unordered_dense::map<eastl::string, std::unique_ptr<ICVar>> mVariables{};
        ankerl::unordered_dense::map<eastl::string, eastl::string> mPendingValues{};
    };

    /**
     * @brief Convenience object that registers a typed CVar and caches its stable handle.
     *
     * Prefer an AutoCVar for a subsystem-owned setting: it removes repeated name lookups
     * while the registry remains the single owner of its lifetime and metadata.
     * @tparam Type Supported CVar value type.
     */
    template<CVarValue Type>
    class AutoCVar final {
    public:
        /**
         * @brief Registers and caches a typed configuration variable.
         * @param name Stable dotted variable name.
         * @param description User-facing help text.
         * @param defaultValue Value used when no startup override exists.
         * @param flags Persistence and mutation flags.
         * @param callback Optional callback invoked after later changes.
         */
        AutoCVar( eastl::string_view name, eastl::string_view description, Type defaultValue,
            CVarFlags flags = CVarFlags::eNone, typename CVar<Type>::ChangedCallback callback = {} ) :
            mReference{ CVarRegistry::Get().Register<Type>( name, description, eastl::move( defaultValue ), flags, eastl::move( callback ) ) } { }

        /**
         * @brief Retrieves the current value without another registry lookup. @returns The current typed value. */
        MKT_NODISCARD auto Get() const -> Type { return mReference.Get(); }

        /**
         * @brief Attempts to update the cached variable. @param value New typed value. @returns True when allowed. */
        auto Set( Type value ) const -> bool { return mReference.Set( eastl::move( value ) ); }

        /**
         * @brief Returns the underlying stable typed handle. @returns The cached CVar handle. */
        MKT_NODISCARD auto GetReference() const -> CVarRef<Type> { return mReference; }

    private:
        CVarRef<Type> mReference{};
    };

    template<CVarValue Type>
    auto CVarRegistry::Register( const eastl::string_view name, const eastl::string_view description, Type defaultValue,
        const CVarFlags flags, typename CVar<Type>::ChangedCallback callback ) -> CVarRef<Type> {
        std::lock_guard lock{ mMutex };
        const eastl::string key{ name };
        if ( const auto existing{ mVariables.find( key ) }; existing != mVariables.end() ) {
            if ( existing->second->GetMetadata().mType != CVarTypeTraits<Type>::kType ) {
                return {};
            }
            return CVarRef<Type>{ static_cast<CVar<Type>*>( existing->second.get() ) };
        }

        Type initialValue{ defaultValue };
        if ( const auto pending{ mPendingValues.find( key ) }; pending != mPendingValues.end() ) {
            Type parsedValue{};
            if ( CVarTextCodec<Type>::Parse( pending->second, parsedValue ) ) {
                initialValue = eastl::move( parsedValue );
            }
            mPendingValues.erase( pending );
        }

        auto variable{ std::make_unique<CVar<Type>>( CVarMetadata{ .mName = key, .mDescription = eastl::string{ description },
            .mFlags = flags, .mType = CVarTypeTraits<Type>::kType }, eastl::move( defaultValue ), eastl::move( initialValue ), eastl::move( callback ) ) };
        auto* const rawVariable{ variable.get() };
        mVariables.emplace( key, eastl::move( variable ) );
        return CVarRef<Type>{ rawVariable };
    }

    template<CVarValue Type>
    auto CVarRegistry::GetValue( const eastl::string_view name, Type fallback ) const -> Type {
        const auto* const variable{ Find( name ) };
        if ( variable == nullptr || variable->GetMetadata().mType != CVarTypeTraits<Type>::kType ) {
            return fallback;
        }
        return static_cast<const CVar<Type>*>( variable )->GetValue();
    }

    template<CVarValue Type>
    auto CVarRegistry::SetValue( const eastl::string_view name, Type value ) -> bool {
        auto* const variable{ Find( name ) };
        return variable != nullptr && variable->GetMetadata().mType == CVarTypeTraits<Type>::kType &&
            static_cast<CVar<Type>*>( variable )->SetValue( eastl::move( value ) );
    }
}

#endif // MIKOTO_CVAR_HH
