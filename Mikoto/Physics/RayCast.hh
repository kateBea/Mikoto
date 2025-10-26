//
// Created by kate on 10/22/25.
//

#ifndef MIKOTO_RAYCAST_HH
#define MIKOTO_RAYCAST_HH

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {
    class RayCast {
    public:

        RayCast(const Vec3F& point, const Vec3F& direction);

        auto GetOrigin() const -> Vec3F;
        auto GetDirection() const -> Vec3F;

        DISABLE_COPY_AND_MOVE_FOR( RayCast );

    private:
        Vec3F m_Origin{};
        Vec3F m_Direction{};
    };
}



#endif //
