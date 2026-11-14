//    Copyright 2026 ケイト
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <new>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <exception>

#include <EASTL/sort.h>
#include <EASTL/algorithm.h>

#include <malloc.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Memory/Allocator.hh>

// https://github.com/vblanco20-1/Project-Ascendant.git
// https://github.com/electronicarts/EASTL/blob/master/doc/FAQ.md

using mikoto::core::cstr;
using mikoto::core::usize;

#if MIKOTO_PLATFORM_WINDOWS
#include <crtdbg.h>

#endif

using mikoto::core::cstr;
using mikoto::core::usize;

// Define the aligned overload first so the other hook can delegate to it.
auto operator new[](
    mikoto::core::usize size,
    mikoto::core::usize alignment,
    mikoto::core::usize alignmentOffset,
    MKT_UNUSED cstr pName,
    MKT_UNUSED int flags,
    MKT_UNUSED unsigned debugFlags,
    cstr file,
    int line ) -> void* {
    if ( alignment == 0 || ( alignment & ( alignment - 1 ) ) != 0 ) {
        throw std::bad_alloc{};
    }

    // Align the requested element inside the allocation:
    // ( address + alignmentOffset ) % alignment == 0.
    // Removing whole alignment multiples preserves this condition.
    const mikoto::core::usize offset{ alignmentOffset & ( alignment - 1 ) };

    // The CRT requires a nonzero allocation with the offset inside it.
    const mikoto::core::usize allocationSize{ size > offset ? size : offset + 1 };

    for ( ;; ) {
        // In Debug, retain the allocation's file and line and enable CRT
        // heap guards. In Release, this maps to _aligned_offset_malloc.
        void* memory{
            _aligned_offset_malloc_dbg(
                allocationSize, alignment, offset, file, line )
        };

        if ( memory ) {
#if defined( _DEBUG ) && defined( MIKOTO_EASTL_TRACE_ALLOCATIONS ) && !defined( MIKOTO_PLATFORM_WINDOWS )
            // Optional diagnostics for metadata the CRT heap cannot store.
            // Keep disabled during normal profiling: reporting every
            // allocation can substantially distort timings.
            _CrtDbgReport(
                _CRT_WARN,
                file,
                line,
                nullptr,
                "EASTL allocation: pointer=%p, name=%s, size=%zu, "
                "alignment=%zu, offset=%zu, flags=%d, debugFlags=%u\n",
                memory,
                pName ? pName : "(unnamed)",
                size,
                alignment,
                alignmentOffset,
                flags,
                debugFlags );
#endif
            return memory;
        }

        // Allow the application's new-handler to release memory or throw.
        const auto handler{ std::get_new_handler() };

        if ( !handler ) {
            throw std::bad_alloc{};
        }

        handler();
    }
}

auto operator new[](
    mikoto::core::usize size,
    cstr pName,
    int flags,
    unsigned debugFlags,
    cstr file,
    int line ) -> void* {
    return ::operator new[](
        size,
        __STDCPP_DEFAULT_NEW_ALIGNMENT__,
        0,
        pName,
        flags,
        debugFlags,
        file,
        line );
}

// EASTL releases its allocations through ordinary delete[].
// Ordinary arrays must use the same allocation family so that this
// shared deletion path is valid for both EASTL and other arrays.
auto operator new[]( mikoto::core::usize size ) -> void* {
    return ::operator new[](
        size,
        __STDCPP_DEFAULT_NEW_ALIGNMENT__,
        0,
        "Array",
        0,
        0,
        nullptr,
        0 );
}

auto operator delete[]( void* memory ) noexcept -> void {
    _aligned_free_dbg( memory );
}

auto operator delete[]( void* memory, mikoto::core::usize ) noexcept -> void {
    ::operator delete[]( memory );
}

namespace mikoto::memory {

    auto AlignUp( core::usize value, core::usize alignment ) -> core::usize {
        return ( value + alignment - 1 ) & ~( alignment - 1 );
    }


    auto IAllocator::Reset() -> void {
    }

}// namespace mikoto::memory
