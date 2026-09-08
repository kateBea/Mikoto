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

#ifndef MIKOTO_RHI_SWAP_CHAIN_HH
#define MIKOTO_RHI_SWAP_CHAIN_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Memory/BufferSpan.hh>
#include <Core/ResourcePool.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Utility.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/DeviceObject.hh>

namespace mikoto::renderer::rhi {

    enum struct PresentMode : core::i32 {
        eTearing,
        eLowLatency,
        eVSync,
        eVSyncAdaptive
    };

    /**
     * Describes the presentation images and dimensions of a swap chain.
     */
    struct SwapChainDescription {
        core::u32 mBackBufferCount{};
        Format mFormat{ Format::eBGRA8_UNORM };
        ColorSpace mColorSpace{ ColorSpace::eSrgbNonlinear };
        Extent2D mDimension{};
        eastl::string mName{};
    };

    /**
     * Backend-independent interface for the presentation back-buffer chain.
     */
    class ISwapChain : public DeviceObject {
    public:

        /**
         * Recreates presentation images for @p dimensions.
         *
         * @param dimensions Input value used by this operation.
         */
        virtual auto OnResize( const Extent2D& dimensions ) -> void = 0;

        /**
         * Selects how presentation is synchronized with the display.
         *
         * @param presentMode Requested presentation mode.
         */
        virtual auto SetPresentMode( PresentMode presentMode ) -> void = 0;

        /**
         * Returns the format of presentation images.
         *
         * @returns The presentation-image format.
         */
        MKT_NODISCARD virtual auto GetImageFormat() const -> Format = 0;

        /**
         * Returns the presentation image selected for the next frame.
         *
         * @returns The next back buffer.
         */
        MKT_NODISCARD virtual auto GetNextBackBuffer() const -> TextureHandle = 0;

        /**
         * Returns the back buffer at @p imageIndex.
         *
         * @param imageIndex Back-buffer index.
         * @returns The requested back buffer.
         */
        MKT_NODISCARD virtual auto GetBackBuffer( core::usize imageIndex) const -> TextureHandle = 0;

        /**
         * Returns the number of presentation back buffers.
         *
         * @returns The number of back buffers.
         */
        MKT_NODISCARD virtual auto GetBackBufferCount() const -> core::u32 = 0;

        /**
         * Destroys the swap chain.
         */
        ~ISwapChain() override = default;
    };

    using SwapChainHandle = core::Ref<ISwapChain>;
}


#endif//MIKOTO_RHI_SWAP_CHAIN_HH
