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

#include <Renderer/Core/Rhi.hh>
#include <Renderer/Core/GpuDevice.hh>

namespace mikoto::renderer {

    using namespace mikoto::platform;

    enum class RefreshRate {
        eSync,
        eUnlimited
    };

    struct RenderContextCreateInfo {
        Window* mWindow{ nullptr };
        RefreshRate mRefreshRate{ RefreshRate::eSync };
        GraphicsAPI mApi{ GraphicsAPI::eInvalid };
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

        virtual auto SetRefreshRate( RefreshRate rate ) -> void = 0;

        virtual auto SetPresentTarget(TextureHandle texture) -> void = 0;

        MKT_NODISCARD auto GetGpuDevice() -> GpuDevice*;
        MKT_NODISCARD auto GetGpuDevice() const -> const GpuDevice*;
        MKT_NODISCARD auto GetRefreshRate() const  -> RefreshRate;
        MKT_NODISCARD auto IsRefreshType(RefreshRate type) const  -> bool;

        static auto Create(const RenderContextCreateInfo& config) -> eastl::unique_ptr<RenderContext>;

    protected:

        explicit RenderContext(const RenderContextCreateInfo& createInfo)
            :   mWindow{ createInfo.mWindow }, mRefreshRate{ createInfo.mRefreshRate }
        { }

    protected:
        Window* mWindow{ nullptr };
        eastl::unique_ptr<GpuDevice> mDevice{ nullptr };

        RefreshRate mRefreshRate{ RefreshRate::eSync };
    };
}

#endif//MIKOTO_RENDER_CONTEXT_HH
