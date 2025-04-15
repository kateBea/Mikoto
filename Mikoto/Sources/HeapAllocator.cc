//
// Created by zanet on 3/27/2025.
//

#include <new>
#include <Memory/HeapAllocator.hh>

namespace Mikoto {

    HeapAllocator::HeapAllocator( const Size_T maxSize )
        : m_AllocatedSize{ maxSize } {}

    auto HeapAllocator::Allocate( const Size_T size ) -> void* {
        const Pointer_T block{
            ( ::operator new( 1 * size, std::nothrow ) )
        };

        if (block) {
            m_AllocatedSize += size;
        }

        return block;
    }

    auto HeapAllocator::Deallocate( void* ptr, Size_T size ) -> void {
        if (ptr) {
            // Deallocate the memory and update the allocated size.
            ::operator delete(ptr);
            m_AllocatedSize -= size;
        }
    }

    MKT_NODISCARD auto HeapAllocator::MaxSize() const noexcept -> Size_T {
        return m_MaxSize;
    }

}
