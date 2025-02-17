/**
 * VulkanObject.hh
 * Created by kate on 11/3/2024.
 * */
#ifndef MIKOTO_VULKAN_OBJECT_HH
#define MIKOTO_VULKAN_OBJECT_HH

#include <Common/Common.hh>

namespace Mikoto {

    class VulkanObject {
    public:
        explicit VulkanObject() = default;
        virtual ~VulkanObject() = default;

        MKT_NODISCARD auto IsReleased() const -> bool { return m_IsReleased; }

        virtual auto Release() -> void = 0;

    protected:
        auto Invalidate() -> void { m_IsReleased = true; }

        bool m_IsReleased{ false };
    };
}

#endif // MIKOTO_VULKAN_OBJECT_HH
