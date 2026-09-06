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

    struct SwapChainDescription {
        core::u32 mBackBufferCount{};
        Format mForma{ Format::eBGRA8_UNORM };
        ColorSpace mColorSpace{ ColorSpace::eSrgbNonlinear };
        Extent2D mDimension{};
        eastl::string mName{};
    };

    class ISwapChain : public DeviceObject {
    public:
        virtual auto OnResize( const Extent2D& dimensions ) -> void = 0;
        virtual auto SetPresentMode( PresentMode presentMode ) -> void = 0;

        MKT_NODISCARD virtual auto GetImageFormat() const -> Format = 0;
        MKT_NODISCARD virtual auto GetNextBackBuffer() const -> TextureHandle = 0;
        MKT_NODISCARD virtual auto GetBackBuffer( core::usize imageIndex) const -> TextureHandle = 0;


        MKT_NODISCARD virtual auto GetBackBufferCount() const -> core::u32 = 0;

        ~ISwapChain() override = default;
    };

    using SwapChainHandle = core::Ref<ISwapChain>;
}


#endif//MIKOTO_RHI_SWAP_CHAIN_HH
