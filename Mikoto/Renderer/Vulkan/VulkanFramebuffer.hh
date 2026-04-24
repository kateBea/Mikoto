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

#ifndef MIKOTO_VULKAN_FRAMEBUFFER_HH
#define MIKOTO_VULKAN_FRAMEBUFFER_HH

#include <volk.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

#include <Renderer/Core/Rhi.hh>

namespace mikoto::renderer::vulkan {

    class Framebuffer final : public IFramebuffer {
    public:
        explicit Framebuffer(const FramebufferDescription& createInfo);

        MKT_NODISCARD auto GetNativeHandle( ObjectType type ) -> Object override;
        MKT_NODISCARD auto GetCreateInfo() const -> const VkFramebufferCreateInfo& { return mCreateInfo; }

        ~Framebuffer() override;

    protected:
        auto Initialize() -> void override;
        auto Release() -> void override;

    private:
        VkFramebuffer mFrameBuffer{};
        VkFramebufferCreateInfo mCreateInfo{};
    };
}

#endif // MIKOTO_VULKAN_FRAMEBUFFER_HH