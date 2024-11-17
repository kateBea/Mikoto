/**
 * VulkanObject.hh
 * Created by kate on 11/3/2024.
 * */
#ifndef MIKOTO_VULKAN_OBJECT_HH
#define MIKOTO_VULKAN_OBJECT_HH

namespace Mikoto {

    template <typename T, typename InitDataT>
    class VulkanObject {
    public:
        using Value_T = T;

        virtual auto Create(const InitDataT& data) -> void = 0;
        virtual auto Release() -> void = 0;

        auto Get() const -> const Value_T& { return m_Obj; }
    protected:
        Value_T m_Obj;
        InitDataT m_CreateInfo;
    };
}

#endif // MIKOTO_VULKAN_OBJECT_HH
