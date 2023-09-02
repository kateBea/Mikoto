/**
 * Singleton.hh
 * Created by kate on 5/28/23.
 * */

#ifndef MIKOTO_SINGLETON_HH
#define MIKOTO_SINGLETON_HH

// Project Headers
#include <Core/Assert.hh>

namespace Mikoto {
    /**
     * Defines a general interface for classes that require global single instance.
     * @tparam Derived The type that requires a single instance
     * */
    template<typename Derived>
    class Singleton {
    public:
        using Value_T = Derived;
        using ValueRef_T = Derived&;
        using ValuePtr_T = Value_T*;

    public:
        explicit Singleton() {
            MKT_ASSERT(!s_Instance, "Singleton instance already exists!");
            s_Instance = static_cast<ValuePtr_T>(this);
        }

        /**
         * Returns a reference to the single instance
         * @returns single instance
         * */
        static auto Get() -> ValueRef_T { static Value_T obj{}; return *s_Instance; }

        /**
         * Returns a pointer to the single instance
         * @returns pointer single instance
         * */
        static auto GetPtr() -> ValuePtr_T { if (!s_Instance) Get();  return s_Instance; }

        /**
         * Performs destruction on this singleton instance (defaulted)
         * */
        ~Singleton() = default;

    public:
        /*************************************************************
         * FORBIDDEN OPERATIONS
         *
         * Defines a list of operations not allowed for Singletons
         * ***********************************************************/
        Singleton(const Singleton&) = delete;
        auto operator=(const Singleton&) -> ValueRef_T& = delete;

        Singleton(Singleton&&) = delete;
        auto operator=(Singleton&&) -> Singleton& = delete;

    protected:
        /*************************************************************
         * DATA MEMBERS
         * ***********************************************************/

        /**
         * Pointer to the single instance allowed for Singleton objects
         * */
        inline static ValuePtr_T s_Instance;
    };

}

#endif // MIKOTO_SINGLETON_HH
