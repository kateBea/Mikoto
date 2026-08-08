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

#ifndef MIKOTO_RENDER_CONTEXT_HH
#define MIKOTO_RENDER_CONTEXT_HH

#include <slang.h>
#include <slang-com-ptr.h>

#include <EASTL/unique_ptr.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Platform/Window.hh>

#include <Renderer/Rhi/Types.hh>
#include <Renderer/Rhi/Texture.hh>
#include <Renderer/Rhi/GpuDevice.hh>

namespace mikoto::renderer {

    struct RenderContextCreateInfo {
        platform::Window* mWindow{ nullptr };

        rhi::RefreshRate mRefreshRate{ rhi::RefreshRate::eSync };
        rhi::GraphicsAPI mApi{ rhi::GraphicsAPI::eInvalid };

        auto SetWindow(platform::Window* window) noexcept -> RenderContextCreateInfo&;
        auto SetRefreshRate(rhi::RefreshRate refreshRate) noexcept -> RenderContextCreateInfo&;
        auto SetGraphicsAPI(rhi::GraphicsAPI api) noexcept -> RenderContextCreateInfo&;
    };

    class RenderContext {
    public:
        virtual ~RenderContext() = default;

        virtual auto Init() -> bool = 0;
        virtual auto Shutdown() -> void = 0;

        virtual auto SubmitFrame() -> void = 0;
        virtual auto PrepareFrame() -> void = 0;

        virtual auto Update() -> void = 0;
        virtual auto Present() -> void = 0;

        virtual auto SetRefreshRate( rhi::RefreshRate rate ) -> void = 0;

        virtual auto SetPresentTarget( rhi::TextureHandle texture ) -> void = 0;

        MKT_NODISCARD auto GetGpuDevice() -> rhi::IGpuDevice*;
        MKT_NODISCARD auto GetGpuDevice() const -> const rhi::IGpuDevice*;
        MKT_NODISCARD auto GetRefreshRate() const -> rhi::RefreshRate;
        MKT_NODISCARD auto IsRefreshType( rhi::RefreshRate type ) const -> bool;

        static auto Create( const RenderContextCreateInfo& config ) -> eastl::unique_ptr<RenderContext>;

    protected:
        explicit RenderContext( const RenderContextCreateInfo& createInfo );

    protected:
        platform::Window* mWindow{ nullptr };
        eastl::unique_ptr<rhi::IGpuDevice> mDevice{ nullptr };

        rhi::RefreshRate mRefreshRate{ rhi::RefreshRate::eSync };
    };
}// namespace mikoto::renderer

#endif//MIKOTO_RENDER_CONTEXT_HH
