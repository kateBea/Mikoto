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

#include <volk.h>

#include <EASTL/vector.h>

#include <Core/Core.hh>
#include <Core/Types.hh>

#include <Memory/Allocator.hh>

#include <Renderer/Vulkan/VulkanDevice.hh>
#include <Renderer/Vulkan/VulkanHelpers.hh>
#include <Renderer/Vulkan/VulkanFramebuffer.hh>

namespace mikoto::renderer::vulkan {

    using namespace mikoto::memory;

    Framebuffer::Framebuffer( const FramebufferDescription& createInfo )
        : IFramebuffer{ createInfo } {
    }

    auto Framebuffer::GetNativeHandle( ObjectType type ) -> Object {
        if (type != ObjectType::Vk_Framebuffer) {
            return Object(nullptr);
        }

        return Object(mFrameBuffer );
    }

    auto Framebuffer::Release() -> void {
        vkDestroyFramebuffer( checked_cast<Device*>( mDevice )->GetDevice(), mFrameBuffer, nullptr );
        mIsAllocated = false;
    }

    Framebuffer::~Framebuffer() {
        if ( mIsAllocated ) {
            Release();
        }
    }

    auto Framebuffer::Initialize() -> void {
        mCreateInfo = initializers::FramebufferCreateInfo();

        // TODO: No longer using this
        VkRenderPass renderPass{ VK_NULL_HANDLE };

        mCreateInfo.pNext = nullptr;
        mCreateInfo.renderPass = renderPass;

        mCreateInfo.width = mWidth;
        mCreateInfo.height = mHeight;
        mCreateInfo.layers = 1;

        eastl::vector<VkImageView> attachments{};
        for (auto& texture : mColorAttachments) {
            attachments.emplace_back( texture->GetNativeHandle( ObjectType::Vk_ImageView ) );
        }

        for (auto& texture : mDepthAttachment) {
            attachments.emplace_back( texture->GetNativeHandle( ObjectType::Vk_ImageView ) );
        }

        mCreateInfo.attachmentCount = as<u32>( attachments.size() );
        mCreateInfo.pAttachments = attachments.data();

        MKT_VK_CHECK( vkCreateFramebuffer(
            checked_cast<Device*>( mDevice )->GetDevice(),
            MKT_ADDRESSOF( mCreateInfo ),
            nullptr,
            MKT_ADDRESSOF( mFrameBuffer ) ) );

        mIsAllocated = true;
    }
}// namespace Mikoto