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

#ifndef MIKOTO_RENDERER_CORE_FRAME_GRAPH_RESOURCE_MANAGER_HH
#define MIKOTO_RENDERER_CORE_FRAME_GRAPH_RESOURCE_MANAGER_HH

#include <mutex>

#include <ankerl/unordered_dense.h>

#include <EASTL/atomic.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Renderer/Core/FrameGraphTypes.hh>
#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::renderer {

    class FGResourceManager final {
    public:
        static constexpr FGResourceHandle kInvalidResourceHandle{ 0 };

        explicit FGResourceManager( renderer::rhi::IGpuDevice *device );

        /** Replaces storage without recording GPU work; initialize it in the next pass. */
        auto ReplaceBufferStorage( FGBufferHandle handle, core::usize bytes ) -> void;

        auto SetResource( FGResourceHandle handle, core::Ref<core::Resource> resource ) -> void;

        /** Returns and clears the replaced allocations for fence-based descriptor retirement. */
        MKT_NODISCARD auto GetReplacedResources() -> eastl::vector<core::Ref<core::Resource>>;
        MKT_NODISCARD auto IsResourceReferenced( const core::Resource *resource ) const -> bool;

        MKT_NODISCARD auto Get( FGResourceHandle handle ) -> FGResource &;
        MKT_NODISCARD auto Get( FGResourceHandle handle ) const -> const FGResource &;

        MKT_NODISCARD auto GetBufferMappedAddress( FGBufferHandle handle ) const -> const void *;

        MKT_NODISCARD auto Free( FGResourceHandle name ) -> bool;
        MKT_NODISCARD auto Allocate( FGResourceType type, core::Resource *resource = nullptr ) -> FGResource &;

        /** @brief Allocates larger storage in a preparation pass, returning whether it changed.
         * Does not copy contents or record commands. Restore the complete CPU data in
         * a subsequent upload pass, whose resource declaration transitions the new buffer.
         * Submitted recordings retain the previous allocation until completion.
         */
        MKT_NODISCARD auto GrowBuffer( FGBufferHandle handle, core::usize bytes ) -> bool;

        MKT_NODISCARD auto GetBufferRevision() const -> core::u64;
        MKT_NODISCARD auto GetGpuDeviceAddress( FGBufferHandle handle ) -> core::u64;

        MKT_NODISCARD auto ImportTexture( renderer::rhi::TextureHandle handle ) -> FGTextureHandle;
        MKT_NODISCARD auto ImportBuffer( renderer::rhi::BufferHandle handle ) -> FGBufferHandle;

        /** Imports a runtime sampled dependency, deduplicated within this graph.
         * Upload completion and shader-readable state remain the caller's responsibility.
         * Importing retains the texture without allocating a descriptor. The handle
         * remains valid until explicitly freed or this manager is destroyed.
         */
        MKT_NODISCARD auto ImportSampledTexture( renderer::rhi::TextureHandle texture ) -> FGTextureHandle;

    private:
        renderer::rhi::IGpuDevice *mDevice{};

        mutable std::mutex mResourceMutex{};

        core::u64 mBufferRevision{};
        eastl::atomic<core::u32> mResourceCount{};

        ankerl::unordered_dense::map<core::Resource *, FGResourceHandle> mImportedResources{};
        ankerl::unordered_dense::map<FGResourceHandle, eastl::unique_ptr<FGResource>> mResources{};
        eastl::vector<core::Ref<core::Resource>> mReplacedResources{};
    };
}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CORE_FRAME_GRAPH_RESOURCE_MANAGER_HH
