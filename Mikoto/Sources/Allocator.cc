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

// Ref:: https://github.com/vblanco20-1/Project-Ascendant.git

#include <new>
#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <exception>

#include <EASTL/sort.h>
#include <EASTL/algorithm.h>

#include <Core/Core.hh>

#include <Memory/Allocator.hh>

void* operator new[](size_t size, const char*, int, unsigned, const char*, int) {
    return std::malloc(size);
}

void* operator new[](size_t size, size_t, size_t, const char*, int, unsigned, const char*, int) {
    return std::malloc(size);
}

void operator delete[](void* p, const char*, int, unsigned, const char*, int) noexcept {
    std::free(p);
}

void operator delete[](void* p, size_t, size_t, const char*, int, unsigned, const char*, int) noexcept {
    std::free(p);
}

auto operator new( std::size_t size ) -> void* {
    if ( size == 0 ) {
        size = 1;
    }

    if ( void* ptr{ std::malloc( size ) } ) {
        return ptr;
    }

    throw std::bad_alloc{};
}

auto operator new[]( std::size_t size ) -> void* {
    if ( size == 0 ) {
        size = 1;
    }

    if ( void* ptr{ std::malloc( size ) } ) {
        return ptr;
    }

    throw std::bad_alloc{};
}

auto operator new( std::size_t size, std::align_val_t alignment ) -> void* {
    if ( size == 0 ) {
        size = 1;
    }

    const auto align{ static_cast<std::size_t>( alignment ) };

#if defined( _MSC_VER )
    if ( void* ptr{ _aligned_malloc( size, align ) } ) {
        return ptr;
    }
#else
    const std::size_t alignedSize{ ( ( size + align - 1 ) / align ) * align };
    if ( void* ptr{ std::aligned_alloc( align, alignedSize ) } ) {
        return ptr;
    }
#endif

    throw std::bad_alloc{};
}

auto operator new[]( std::size_t size, std::align_val_t alignment ) -> void* {
    return ::operator new( size, alignment );
}

auto operator delete( void* ptr ) noexcept -> void {
    std::free( ptr );
}

auto operator delete[]( void* ptr ) noexcept -> void {
    std::free( ptr );
}

auto operator delete( void* ptr, std::size_t ) noexcept -> void {
    std::free( ptr );
}

auto operator delete[]( void* ptr, std::size_t ) noexcept -> void {
    std::free( ptr );
}

auto operator delete( void* ptr, std::align_val_t ) noexcept -> void {
#if defined( _MSC_VER )
    _aligned_free( ptr );
#else
    std::free( ptr );
#endif
}

auto operator delete[]( void* ptr, std::align_val_t alignment ) noexcept -> void {
    ::operator delete( ptr, alignment );
}

auto operator delete( void* ptr, std::size_t, std::align_val_t alignment ) noexcept -> void {
    ::operator delete( ptr, alignment );
}

auto operator delete[]( void* ptr, std::size_t, std::align_val_t alignment ) noexcept -> void {
    ::operator delete( ptr, alignment );
}

namespace mikoto::memory {

    auto MallocAlloc( size_t size ) -> void* {
        return nullptr;
    }

    auto MallocCalloc( size_t size ) -> void* {
        return nullptr;
    }

    auto MallocRealloc( void* p, size_t size ) -> void* {
        return nullptr;
    }

    auto MallocFree( void* p ) -> void {

    }

    auto AlignUp( size_t value, size_t alignment ) -> size_t {
        return (value + alignment - 1) & ~(alignment - 1);
    }
}// namespace mikoto::memory