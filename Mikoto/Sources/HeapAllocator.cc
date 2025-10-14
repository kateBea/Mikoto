//
// Created by zanet on 3/27/2025.
//

#include <Memory/Allocator.hh>
#include <Memory/HeapAllocator.hh>
#include <new>

namespace Mikoto {

    HeapAllocator::HeapAllocator( const Size maxSize )
        : m_AllocatedSize{ maxSize } {}

    auto HeapAllocator::Allocate( const Size size ) -> Block {
        const Pointer block{
            MKT_NOTHROW_PLACEMENT_NEW( size )
        };

        if ( block ) {
            m_AllocatedSize += size;
        }

        return { size, block };
    }

    auto HeapAllocator::Deallocate( Block block ) -> void {
        if ( block.Address ) {
            // Deallocate the memory and update the allocated size.
            MKT_NOTHROW_PLACEMENT_DELETE( block.Address );
            m_AllocatedSize -= block.AllocatedSize;
        }
    }

    MKT_NODISCARD auto HeapAllocator::MaxSize() const noexcept -> Size {
        return 0;// TODO
    }

}// namespace Mikoto
