//
// Created by zanet on 3/8/2025.
//

#ifndef VULKANFONT_HH
#define VULKANFONT_HH

#include <Assets/Font.hh>

#include "VulkanObject.hh"


namespace Mikoto {
    class VulkanFont final : public VulkanObject, public Font {
    public:
        explicit VulkanFont( const FontLoadInfo &loadInfo )
            : Font{ loadInfo } {}

        auto Release() -> void override;

        ~VulkanFont() override;

    private:


    };

}


#endif //VULKANFONT_HH
