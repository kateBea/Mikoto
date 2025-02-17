//
// Created by zanet on 1/26/2025.
//

#ifndef REGISTRY_HH
#define REGISTRY_HH

#include <type_traits>
#include <unordered_map>

#include <Core/Logging/Assert.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    template<typename BaseType>
    class Registry final {
    public:
        using Registry_T = std::unordered_map<size_t, Scope_T<BaseType>>;

        template<typename SystemType, typename... Args>
        auto Register( Args&&... args ) -> SystemType* {
            const Size_T typeName{ typeid( SystemType ).hash_code() };

            MKT_ASSERT( !m_Registry.contains( typeName ), "Registry::Register - Error registering system more than once." );

            auto system{ CreateScope<SystemType>( std::forward<Args>( args )... ) };
            const auto [itInsert, success]{ m_Registry.try_emplace( typeName, std::move( system ) ) };

            if (success) {
                return dynamic_cast<SystemType*>(itInsert->second.get());
            }

            return nullptr;
        }

        template<typename SystemType>
        auto Unregister() -> void {
            const Size_T typeName{ typeid( SystemType ).hash_code() };

            if ( m_Registry.contains( typeName ) ) {
                m_Registry.erase( typeName );
            }
        }

        template<typename SystemType>
        auto Get() -> SystemType* {
            const auto typeName{ typeid( SystemType ).hash_code() };

            if ( m_Registry.contains( typeName ) ) {
                return dynamic_cast<SystemType*>( m_Registry[typeName].get() );
            }

            return nullptr;
        }

        constexpr auto begin() const -> decltype(auto) {
            return m_Registry.begin();
        }

        constexpr auto end() const -> decltype(auto) {
            return m_Registry.end();
        }

        constexpr auto begin() -> decltype(auto) {
            return m_Registry.begin();
        }

        constexpr auto end() -> decltype(auto) {
            return m_Registry.end();
        }

        auto Clear() -> void {
            m_Registry.clear();
        }

    private:
        Registry_T m_Registry{};
    };
}

#endif//REGISTRY_HH
