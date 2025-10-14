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

#define MKT_SIZEOF( X ) sizeof( X )

#define MKT_NOTHROW_PLACEMENT_NEW( SIZE ) ::operator new( 1 * SIZE, std::nothrow )
#define MKT_NOTHROW_PLACEMENT_DELETE( PTR ) ::operator delete(PTR)

namespace Mikoto {

    class Allocator {
    public:

        explicit Allocator() = default;
        virtual ~Allocator() = default;

    };
}


#endif//ALLOCATOR_HH
