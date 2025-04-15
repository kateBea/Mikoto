//
// Created by zanet on 1/26/2025.
//

#ifndef REGISTRY_HH
#define REGISTRY_HH

#include <type_traits>

#include <ankerl/unordered_dense.h>

#include <Core/Assert.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    /**
    * @brief A type-based registry for storing and accessing systems or services.
    *
    * This final class uses `typeid().hash_code()` to uniquely identify types
    * and stores them in an unordered_dense map, ensuring fast lookups and ownership
    * via `Scope_T<BaseType>` (a scoped pointer, likely alias for `std::unique_ptr<BaseType>`).
    *
    * This is a generic registry suitable for any kind of subsystem or service
    * you want to instantiate and fetch by type later.
    *
    * @tparam BaseType The common base class for all types that will be stored in the registry.
    */
    template<typename BaseType>
    class Registry final {
    public:
        /// Internal alias for the actual storage map using ankerl::unordered_dense
        using Registry_T = ankerl::unordered_dense<size_t, Scope_T<BaseType>>;

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
            const Size_T typeName{ typeid( RegisteredType ).hash_code() };

            MKT_ASSERT( !m_Registry.contains( typeName ), "Registry::Register - Error registering system more than once." );

            auto system{ CreateScope<RegisteredType>( std::forward<Args>( args )... ) };
            const auto [itInsert, success]{ m_Registry.try_emplace( typeName, std::move( system ) ) };

            if ( success ) {
                return dynamic_cast<RegisteredType*>( itInsert->second.get() );
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
            const Size_T typeName{ typeid( SystemType ).hash_code() };

            if ( m_Registry.contains( typeName ) ) {
                m_Registry.erase( typeName );
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

            if ( m_Registry.contains( typeName ) ) {
                return dynamic_cast<SystemType*>( m_Registry[typeName].get() );
            }

            return nullptr;
        }

        /**
        * @brief Const begin iterator for registry traversal.
        *
        * @return Iterator to the beginning of the internal map.
        */
        constexpr auto begin() const -> decltype( auto ) {
            return m_Registry.begin();
        }

        /**
        * @brief Const end iterator for registry traversal.
        *
        * @return Iterator to the end of the internal map.
        */
        constexpr auto end() const -> decltype( auto ) {
            return m_Registry.end();
        }

        /**
        * @brief Mutable begin iterator for registry traversal.
        *
        * @return Iterator to the beginning of the internal map.
        */
        constexpr auto begin() -> decltype( auto ) {
            return m_Registry.begin();
        }

        /**
        * @brief Mutable end iterator for registry traversal.
        *
        * @return Iterator to the end of the internal map.
        */
        constexpr auto end() -> decltype( auto ) {
            return m_Registry.end();
        }

        /**
        * @brief Clears all registered systems/services from the registry.
        */
        auto Clear() -> void {
            m_Registry.clear();
        }

    private:
        /// Internal storage for registered types keyed by their hash code
        Registry_T m_Registry{};
    };

}// namespace Mikoto

#endif//REGISTRY_HH
