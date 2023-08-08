/**
 * Singleton.hh
 * Created by kate on 5/28/23.
 * */

#ifndef KATE_ENGINE_SINGLETON_HH
#define KATE_ENGINE_SINGLETON_HH

#include <Core/Assert.hh>

namespace kaTe {
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
            KT_ASSERT(!s_Instance, "Singleton instance already exists!");
            s_Instance = static_cast<ValuePtr_T>(this);
        }

        /**
         * Return a reference to the single instance
         * @returns single instance
         * */
        static auto Get() -> ValueRef_T { static Value_T obj{}; return *s_Instance; }

        /**
         * Return a pointer to the single instance
         * @returns pointer single instance
         * */
        static auto GetPtr() -> ValuePtr_T { return s_Instance; }

    public:
        // Forbidden operations for Singleton
        Singleton(const Singleton&) = delete;
        auto operator=(const Singleton&) -> ValueRef_T& = delete;

        Singleton(Singleton&&) = delete;
        auto operator=(Singleton&&) -> Singleton& = delete;

    protected:
        virtual ~Singleton() = default;

        inline static ValuePtr_T s_Instance;
    };

}   // END NAMESPACE kaTe

#endif // KATE_ENGINE_SINGLETON_HH
