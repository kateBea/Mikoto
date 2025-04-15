//
// Created by zanet on 4/9/2025.
//

#ifndef FRAMEBUFFER_HH
#define FRAMEBUFFER_HH

#include <Library/Utility/Types.hh>
#include <Library/Data/ResourcePool.hh>
#include <Renderer/DeviceObject.hh>

namespace Mikoto {
/**
 * @brief Represents a framebuffer object used for off-screen rendering.
 *
 * This class encapsulates the functionality of a framebuffer, allowing for
 * rendering to textures instead of directly to the screen. It provides methods
 * for creating, binding, and managing the framebuffer and its associated
 * textures.
 */
    class Framebuffer : public DeviceObject  {
    public:

    };
}



#endif //FRAMEBUFFER_HH
