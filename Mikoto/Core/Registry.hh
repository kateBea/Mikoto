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

#ifndef MIKOTO_REGISTRY_HH
#define MIKOTO_REGISTRY_HH

#include <EASTL/memory.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/utility.h>
#include <EASTL/type_traits.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Logging/Assert.hh>

namespace mikoto::core {

    /**
    * @brief A type-based registry for storing and accessing systems or services.
    *
    * This final class uses `typeid().hash_code()` to uniquely identify types
    * and stores them in an unordered_dense map, ensuring fast lookups and ownership
    * via `Scope_T<BaseType>` (a scoped pointer, likely alias for `eastl::unique_ptr<BaseType>`).
    *
    * This is a generic registry suitable for any kind of subsystem or service
    * you want to instantiate and fetch by type later.
    *
    * @tparam BaseType The common base class for all types that will be stored in the registry.
    */
    template<typename BaseType>
    class Registry final {
    public:

        /**
        * @brief Registers a new system/service of type RegisteredType into the registry.
        *
        * Constructs a unique instance using the provided arguments and stores it
        * under the hash of its type. Ensures the type hasn't been registered yet.
        *
        * @tparam RegisteredType The concrete type to register (must derive from BaseType).
        * @tparam Args Constructor argument types.
        *
        * @param args Arguments forwarded to RegisteredType constructor.
        *
        * @return Pointer to the newly registered instance, or nullptr if registration failed.
        */
        template<typename  RegisteredType, typename... Args>
        auto Register( Args&&... args ) -> RegisteredType* {
            const size_t typeName{ typeid( RegisteredType ).hash_code() };

            MKT_ASSERT( !mRegistry.contains( typeName ), "Registry::Register - Error registering system more than once." );

            auto system{ eastl::make_unique<RegisteredType>( eastl::forward<Args>( args )... ) };
            const auto [itInsert, success]{ mRegistry.try_emplace( typeName, eastl::move( system ) ) };

            if ( success ) {
                return as<RegisteredType*>( itInsert->second.get() );
            }

            return nullptr;
        }

        /**
        * @brief Unregisters a system/service of the specified type.
        *
        * Removes the associated instance from the registry if it exists.
        *
        * @tparam SystemType The type to unregister.
        */
        template<typename SystemType>
        auto Unregister() -> void {
            const size_t typeName{ typeid( SystemType ).hash_code() };

            if ( mRegistry.contains( typeName ) ) {
                mRegistry.erase( typeName );
            }
        }

        /**
        * @brief Retrieves a previously registered system/service by type.
        *
        * Returns a pointer to the instance if it exists in the registry,
        * or nullptr otherwise.
        *
        * @tparam SystemType The type to retrieve.
        *
        * @return Pointer to the instance, or nullptr if not found.
        */
        template<typename SystemType>
        auto Get() -> SystemType* {
            const auto typeName{ typeid( SystemType ).hash_code() };

            if ( mRegistry.contains( typeName ) ) {
                return as<SystemType*>( mRegistry[typeName].get() );
            }

            return nullptr;
        }

        template<typename SystemType>
        auto Get() const -> const SystemType* {
            const auto typeName{ typeid( SystemType ).hash_code() };

            if ( mRegistry.contains( typeName ) ) {
                return as<const SystemType*>( mRegistry.at(typeName).get() );
            }

            return nullptr;
        }

        // Iterators
        constexpr auto begin() -> decltype(auto) { return mRegistry.begin(); }
        constexpr auto end() -> decltype(auto) { return mRegistry.end(); }
        constexpr auto begin() const -> decltype(auto) { return mRegistry.begin(); }
        constexpr auto end() const -> decltype(auto) { return mRegistry.end(); }

        /**
        * @brief Clears all registered systems/services from the registry.
        */
        auto Clear() -> void {
            mRegistry.clear();
        }

    private:
        ankerl::unordered_dense::map<size_t, eastl::unique_ptr<BaseType>> mRegistry{};
    };

}// namespace Mikoto

#endif//MIKOTO_REGISTRY_HH
