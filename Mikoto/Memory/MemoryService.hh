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

#ifndef MIKOTO_MEMORY_SERVICE_HH
#define MIKOTO_MEMORY_SERVICE_HH

#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Common/Singleton.hh>
#include <Library/Utility/Types.hh>
#include <Memory/HeapAllocator.hh>

namespace Mikoto {

    /**
    * @struct MemoryServiceCreateInfo
    * @brief Struct used for configuring and initializing a MemoryService instance.
    *
    * This structure holds the configuration options for the `MemoryService`, specifically the initial amount of memory
    * that should be allocated when the service is initialized. It provides a fluent interface for setting its properties.
    */
    struct MemoryServiceCreateInfo {
        Size InitialMemoryPoolSize{};

        /**
         * @brief Sets the initial size of the memory pool.
         * @param size The size of the memory pool in bytes.
         * @return Reference to the modified MemoryServiceCreateInfo.
         */
        auto WithInitialSize(Size size) -> MemoryServiceCreateInfo&;
    };


    /**
     * @class MemoryService
     * @brief Manages memory allocation and deallocation for the system.
     *
     * The `MemoryService` class is responsible for managing system memory, including providing access to the `HeapAllocator`
     * and handling initialization and shutdown of the memory subsystem.
     *
     * This service can be used to allocate and deallocate memory, as well as to manage different types of memory pools.
     */
    class MemoryService final : public IService, public Singleton<MemoryService> {
    public:
        /**
         * @brief Constructs a MemoryService instance with the provided configuration.
         *
         * The constructor takes the initial memory allocation size from the `MemoryServiceCreateInfo` struct.
         *
         * @param options The configuration for initializing the MemoryService.
         */
        explicit MemoryService( const MemoryServiceCreateInfo& options );

        /**
         * @brief Initializes the MemoryService.
         *
         * This method is called to set up the memory system with the initial allocation.
         */
        auto Init() -> void override;

        /**
         * @brief Shuts down the MemoryService.
         *
         * This method is called to clean up the memory resources when the service is no longer needed.
         */
        auto Shutdown() -> void override;

        auto GetHeapAllocator() -> HeapAllocator* { return std::addressof(m_HeapAllocator); }

    private:

        HeapAllocator m_HeapAllocator{ 1000 };

    };
}

#endif//MIKOTO_MEMORY_SERVICE_HH
