//
// Created by zanet on 3/27/2025.
//

#ifndef HEAPALLOCATOR_HH
#define HEAPALLOCATOR_HH

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>
#include <Memory/Allocator.hh>

namespace Mikoto {

    /**
     * @class HeapAllocator
     * @brief A memory allocator that manages memory using a large pre-allocated block.
     *
     * The `HeapAllocator` class manages memory allocation using a large block of memory allocated when the object
     * is created. It provides the implementation for memory allocation and deallocation, as well as tracking the
     * amount of memory allocated.
     */
    class HeapAllocator final : public Allocator<void> {
    public:
        /**
         * @brief Constructs a HeapAllocator with the specified maximum allocation size.
         *
         * The constructor initializes the allocator with a predefined maximum size for memory allocation.
         *
         * @param maxSize The maximum size (in bytes) that can be allocated by this allocator.
         */
        explicit HeapAllocator( Size_T maxSize );

        /**
         * @brief Allocates memory for the specified size.
         *
         * This method allocates a block of memory of the specified size from the pre-allocated memory block.
         *
         * @param size The size (in bytes) of memory to allocate.
         * @return A pointer to the allocated memory.
         */
        auto Allocate( Size_T size ) -> void* override;

        /**
         * @brief Deallocates memory that was previously allocated.
         *
         * This method releases a previously allocated memory block back to the allocator.
         *
         * @param ptr A pointer to the memory to deallocate.
         * @param size The size (in bytes) of the memory to deallocate.
         */
        auto Deallocate( void* ptr, Size_T size ) -> void override;

        /**
         * @brief Returns the maximum allocatable size.
         *
         * This method returns the maximum size of memory that can be allocated by the `HeapAllocator`.
         *
         * @return The maximum allocatable size in bytes.
         */
        MKT_NODISCARD auto MaxSize() const noexcept -> Size_T override;

        /**
         * @brief Returns the size of memory that has been allocated so far.
         *
         * This method returns the total amount of memory that has been allocated by the `HeapAllocator` so far.
         *
         * @return The amount of memory allocated in bytes.
         */
        MKT_NODISCARD auto GetAllocatedSize() const noexcept -> Size_T { return m_AllocatedSize; }

    private:
        // Allocate a huge chuck when this object is created
        // and manage that block internally. For now it will almost
        // stateless, but in the future the idea is to implement a way
        // to keep track of the allocated memory and minimazi actual
        // system allocations
        void* m_MemoryBlock{ nullptr };
        Size_T m_MaxSize{ 0 };
        Size_T m_AllocatedSize{ 0 };
    };
}// namespace Mikoto


#endif//HEAPALLOCATOR_HH
