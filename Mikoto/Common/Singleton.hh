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
        using Value = Derived;
        using ValueRef = Derived&;
        using ValuePtr = Value*;

    public:
        explicit Singleton() {
            MKT_ASSERT(!s_Instance, "Singleton - Instance already exists.");
            s_Instance = static_cast<ValuePtr>(this);
        }

        /**
         * Returns a reference to the single instance
         * @returns Reference to the single instance
         * */
        static auto Get() -> ValueRef {
            return *s_Instance;
        }

        /**
         * Returns a pointer to the single instance
         * @returns Pointer to the single instance
         * */
        static auto GetPtr() -> ValuePtr { if (!s_Instance) Get();  return s_Instance; }

        ValuePtr operator->() {
            return s_Instance;
        }

        /**
         * Called when this instance is destroyed.
         * */
        virtual ~Singleton() = default;

    public:
        DISABLE_COPY_AND_MOVE_FOR(Singleton);

    protected:
        /**
         * Pointer to the single instance allowed for Singleton objects
         * */
        inline static ValuePtr s_Instance;
    };

}

#endif // MIKOTO_SINGLETON_HH
