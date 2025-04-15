//
// Created by zanet on 3/27/2025.
//

#ifndef ALLOCATOR_HH
#define ALLOCATOR_HH

#include <memory>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

// Macro for bytes
#define MKT_BYTES( X ) ( X )

// Macro for kilobytes
#define MKT_KILOBYTES( X ) ( ( X ) * 1024 )

// Macro for megabytes
#define MKT_MEGABYTES( X ) ( ( X ) * 1024 * 1024 )

// Macro for gigabytes
#define MKT_GIGABYTES( X ) ( ( X ) * 1024 * 1024 * 1024 )

namespace Mikoto {

    /**
    * @class Allocator
    * @brief A base class for custom memory allocation strategies.
    *
    * This template class defines the interface for a memory allocator. It provides methods for allocating,
    * deallocating, constructing, and destroying objects of type `T`. Derived classes must implement the allocation
    * and deallocation logic specific to their memory management strategy.
    *
    * The `Allocator` class supports object construction with placement new and destruction via the destructor.
    * Additionally, it includes a helper method to retrieve the address of an object, even if the `operator&` is overloaded.
    *
    * @tparam T The type of object the allocator is responsible for managing.
    */
    template<typename T>
    class Allocator {
    public:
        using Pointer_T = T*;

        /**
        * @brief Destructor for the Allocator class.
        *
        * The destructor is virtual to ensure proper cleanup in derived classes.
        */
        virtual ~Allocator() = default;

        /**
        * @brief Allocates memory for a given number of objects of type `T`.
        *
        * This method must be implemented by derived classes to provide the memory allocation strategy.
        *
        * @param size The number of objects to allocate memory for.
        * @return A pointer to the allocated memory.
        */
        virtual auto Allocate( Size_T size ) -> Pointer_T = 0;

        /**
        * @brief Allocates at least the specified number of objects of type `T`.
        *
        * This method provides a default implementation for allocating memory. Derived classes can override it
        * to provide a more specialized allocation strategy.
        *
        * @param size The number of objects to allocate memory for.
        * @return A pointer to the allocated memory.
        */
        virtual auto AllocateAtLeast( const Size_T size ) -> Pointer_T {
            return Allocate( size );
        }

        /**
        * @brief Deallocates the memory for the specified pointer.
        *
        * This method must be implemented by derived classes to provide the memory deallocation strategy.
        *
        * @param ptr A pointer to the memory to deallocate.
        * @param size The number of objects to deallocate memory for.
        */
        virtual auto Deallocate( T* ptr, Size_T size ) -> void = 0;

        /**
        * @brief Returns the maximum size that can be allocated by this allocator.
        *
        * This method must be implemented by derived classes to specify the maximum allocatable size.
        *
        * @return The maximum allocatable size in bytes.
        */
        MKT_NODISCARD virtual auto MaxSize() const noexcept -> Size_T = 0;

        /**
        * @brief Constructs an object of type `T` in the specified memory location.
        *
        * This method uses placement new to construct an object of type `T` at the given memory location.
        *
        * @param ptr A pointer to the memory location where the object should be constructed.
        * @param args The arguments to forward to the constructor of `T`.
        */
        template<typename... Args>
        auto Construct( T* ptr, Args&&... args ) -> void {
            new ( ptr ) T( std::forward<Args>( args )... );
        }

        /**
         * @brief Destroys an object of type `T` at the specified memory location.
         *
         * This method calls the destructor of the object at the given memory location.
         *
         * @param ptr A pointer to the object to destroy.
         */
        auto Destroy( T* ptr ) -> void {
            ptr->~T();
        }
    };
}


#endif//ALLOCATOR_HH
