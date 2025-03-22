//
// Created by zanet on 3/8/2025.
//

#ifndef VULKANFONT_HH
#define VULKANFONT_HH

#include <unordered_map>

#include <volk.h>

#include <Assets/Font.hh>
#include <Common/Common.hh>

#include "Renderer/Text/FreeTypeFont.hh"
#include "VulkanObject.hh"


namespace Mikoto {
    class VulkanFont final : public VulkanObject, public FreeTypeFont {
    public:
        explicit VulkanFont( const FontLoadInfo &loadInfo )
            : FreeTypeFont{ loadInfo } {}

        MKT_NODISCARD auto GetGlyphDescriptorSet(UInt64_T characterCode) -> VkDescriptorSet;
        MKT_NODISCARD auto GetAtlasDescriptorSet() const -> VkDescriptorSet { return m_AtlasDset; }

        auto Release() -> void override;

        ~VulkanFont() override;

    private:

        VkDescriptorSet m_AtlasDset{};

        // Temporary
        std::unordered_map<UInt64_T, VkDescriptorSet> m_GlyphDescriptorSet{};
    };

}


#endif //VULKANFONT_HH
