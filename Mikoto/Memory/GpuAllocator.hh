//
// Created by zanet on 10/6/2025.
//

#ifndef GPUALLOCATOR_H
#define GPUALLOCATOR_H

#include <Library/Utility/Types.hh>
#include <Memory/Allocator.hh>
#include <Renderer/Core/GpuDevice.hh>

namespace Mikoto {
    class GpuAllocator : public Allocator {
    public:
        ~GpuAllocator() override = default;
        explicit GpuAllocator(GpuDevice* device)
            : m_Device{ device }
        {}

        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        MKT_NODISCARD virtual auto GetMemoryUsage() const -> Size = 0;
        MKT_NODISCARD virtual auto GetMemoryTotal() const -> Size = 0;
        MKT_NODISCARD virtual auto GetMemoryAvailable() const -> Size = 0;

        static auto Create(GpuDevice* device) -> Unique<GpuAllocator>;

    protected:
        GpuDevice* m_Device{};
    };
}



#endif //GPUALLOCATOR_H
