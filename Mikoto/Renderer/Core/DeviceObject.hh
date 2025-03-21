//
// Created by zanet on 3/21/2025.
//

#ifndef IDEVICEOBJECT_HH
#define IDEVICEOBJECT_HH

namespace Mikoto {
    class DeviceObject {
    public:
        virtual ~DeviceObject() = default;

        virtual auto Init() -> void {}
        virtual auto Release() -> void = 0;
    };
}// namespace Mikoto

#endif //IDEVICEOBJECT_HH
