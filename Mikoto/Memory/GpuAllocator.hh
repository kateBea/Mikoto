//
// Created by zanet on 10/6/2025.
//

#ifndef GPUALLOCATOR_H
#define GPUALLOCATOR_H
#include <Library/Utility/Types.hh>
#include <Renderer/GpuDevice.hh>

#include "Allocator.hh"


namespace Mikoto {
    class GpuAllocator : public Allocator {
    public:
        ~GpuAllocator() override = default;
        explicit GpuAllocator(GpuDevice* device)
            : m_Device{ device }
        {}

        virtual auto Init() -> void = 0;
        virtual auto Shutdown() -> void = 0;

        static auto Create(GpuDevice* device) -> Unique<GpuAllocator>;

    protected:
        GpuDevice* m_Device{};
    };
}



#endif //GPUALLOCATOR_H
