/**
 * Singleton.hh
 * Created by kate on 5/28/23.
 * */

#ifndef MIKOTO_SINGLETON_HH
#define MIKOTO_SINGLETON_HH

// Project Headers
#include <Logging/Assert.hh>

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
            MKT_ASSERT(!s_Instance, "Singleton - Instance already exists.");
            s_Instance = static_cast<ValuePtr_T>(this);
        }

        /**
         * Returns a reference to the single instance
         * @returns single instance
         * */
        static auto Get() -> ValueRef_T {
            return *s_Instance;
        }

        /**
         * Returns a pointer to the single instance
         * @returns pointer single instance
         * */
        static auto GetPtr() -> ValuePtr_T { if (!s_Instance) Get();  return s_Instance; }

        ValuePtr_T operator->() {
            return s_Instance;
        }

        /**
         * Performs destruction on this singleton instance
         * */
        virtual ~Singleton() = default;

    public:
        DISABLE_COPY_AND_MOVE_FOR(Singleton);

    protected:
        /**
         * Pointer to the single instance allowed for Singleton objects
         * */
        inline static ValuePtr_T s_Instance;
    };

}

#endif // MIKOTO_SINGLETON_HH
