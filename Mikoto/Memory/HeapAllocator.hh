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
    class HeapAllocator final : public Allocator {
    public:
        struct AllocatedBlock {
            Size AllocatedSize{ 0 };
            void* Address{ nullptr };
        };

        using Block = AllocatedBlock;
        using Pointer = decltype(AllocatedBlock::Address);
    public:

        /**
         * @brief Constructs a HeapAllocator with the specified maximum allocation size.
         * The constructor initializes the allocator with a predefined maximum size for memory allocation.
         * @param maxSize The maximum size (in bytes) that can be allocated by this allocator.
         */
        explicit HeapAllocator( Size maxSize );

        /**
         * @brief Allocates memory for the specified size.
         * This method allocates a block of memory of the specified size from the pre-allocated memory block.
         * @param size The size (in bytes) of memory to allocate.
         * @return A pointer to the allocated memory.
         */
        auto Allocate( Size size ) -> Block;

        /**
         * @brief Deallocates memory that was previously allocated.
         * This method releases a previously allocated memory block back to the allocator.
         * @param block Block to be deallocated
         */
        auto Deallocate( Block block ) -> void ;

        /**
         * @brief Returns the size of memory that has been allocated so far.
         * This method returns the total amount of memory that has been allocated by the `HeapAllocator` so far.
         * @return The amount of memory allocated in bytes.
         */
        MKT_NODISCARD auto GetAllocatedSize() const noexcept -> Size { return m_AllocatedSize; }

    private:

        //Size m_MaxSize{ 0 };
        Size m_AllocatedSize{ 0 };
    };
}// namespace Mikoto


#endif//HEAPALLOCATOR_HH
