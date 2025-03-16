//
// Created by zanet on 3/8/2025.
//

#include "Renderer/Vulkan/VulkanFont.hh"

#include <Renderer/Vulkan/VulkanContext.hh>
#include <Renderer/Vulkan/VulkanTexture2D.hh>


namespace Mikoto {

    auto VulkanFont::GetGlyphDescriptorSet( UInt64_T characterCode ) -> VkDescriptorSet {
        auto findIt{ m_GlyphDescriptorSet.find( characterCode ) };

        if (findIt != m_GlyphDescriptorSet.end()) {
            return findIt->second;
        }

        VkDescriptorSet result{};

        FreeTypeGlyph* glyph{ GetGlyph( characterCode ) };

        if ( glyph != nullptr ) {
            const VkDescriptorSetLayout& descriptorSetLayout{ VulkanContext::Get().GetDescriptorSetLayouts( DESCRIPTOR_SET_LAYOUT_TEXT ) };

            const VulkanDevice& device{ VulkanContext::Get().GetDevice() };
            VulkanDescriptorAllocator& descriptorAllocator{ VulkanContext::Get().GetDescriptorAllocator() };

            result = *descriptorAllocator.Allocate( device.GetLogicalDevice(), descriptorSetLayout );

            VulkanDescriptorWriter descWriters{};

            VulkanTexture2D* glyphTexture{ dynamic_cast<VulkanTexture2D*>( glyph->GetTexture() ) };

            descWriters
                    .WriteImage( 0, glyphTexture->GetImage().GetView(), glyphTexture->GetSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER )
                    .UpdateSet( device.GetLogicalDevice(), result );

            m_GlyphDescriptorSet.try_emplace( characterCode, result );
        }

        return result;
    }

    auto VulkanFont::Release() -> void {
    }

    VulkanFont::~VulkanFont() {
    }
}// namespace Mikoto