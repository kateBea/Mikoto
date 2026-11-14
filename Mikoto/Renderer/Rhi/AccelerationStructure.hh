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

#ifndef MIKOTO_RENDERER_RHI_ACCELERATION_STRUCTURE_HH
#define MIKOTO_RENDERER_RHI_ACCELERATION_STRUCTURE_HH

#include <EASTL/array.h>
#include <EASTL/vector.h>

#include <Renderer/Rhi/Buffer.hh>

namespace mikoto::renderer::rhi {

    enum class AccelStructureType {
        eBottomLevel,
        eTopLevel
    };

    class IAccelerationStructure : public DeviceObject {
    public:
        using DeviceObject::Initialize;

        // Record initial immutable builds on the graphics queue before any trace.
        // Build and trace may share a submission. Do not discard that recording.
        // Input data must be valid and ready; modifying geometry requires a new AS.
        // TLAS builds its previously unbuilt BLAS dependencies automatically.
        virtual auto GetAddress() const -> DeviceAddress = 0;

        virtual auto GetType() const -> AccelStructureType = 0;

        ~IAccelerationStructure() override = default;
    };

    using AccelStructureHandle = core::Ref<IAccelerationStructure>;

    struct RayTracingInstance {
        AccelStructureHandle mBottomLevel{};
        // Row-major 3x4 object-to-world transform, identical on both APIs.
        eastl::array<float, 12> mTransform{ 1, 0, 0, 0,
            0, 1, 0, 0, 0, 0, 1, 0 };
        core::u32 mInstanceID{};
        core::u8 mMask{ 0xFF };
    };

    struct AccelStructureCreateDescription {
        AccelStructureType mType{ AccelStructureType::eBottomLevel };
        // One opaque triangle geometry. Position is float3 at vertex offset 0;
        // indices are uint32. Input buffers must declare AccelStructureBuildInput usage.
        BufferHandle mVertices{};
        BufferHandle mIndices{};
        core::u32 mVertexCount{};
        core::u32 mVertexStride{ 12 };
        core::u32 mIndexCount{};
        core::usize mIndexOffset{}; // Byte offset of this mesh in the index buffer.
        eastl::vector<RayTracingInstance> mInstances{};
    };
}

#endif// MIKOTO_RENDERER_RHI_ACCELERATION_STRUCTURE_HH

