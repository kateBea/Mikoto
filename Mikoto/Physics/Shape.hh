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

#ifndef MIKOTO_PHYSICS_SHAPE_HH
#define MIKOTO_PHYSICS_SHAPE_HH

#include <EASTL/span.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/shared_ptr.h>

#include <glm/glm.hpp>
#include <glm/gtx/compatibility.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/ReferenceCounted.hh>

namespace mikoto::physics {
    class Shape;
    using ShapeHandle = core::RefConst<Shape>;
    struct CompoundShapePart {
        ShapeHandle mShape{};
        core::float3 mPosition{};
        core::quat mRotation{ 1, 0, 0, 0 };
        auto operator==( const CompoundShapePart& ) const -> bool = default;
    };
    enum class ColliderShapeKind {
        eBox,
        eSphere,
        eCapsule,
        eMesh,
        eCylinder,
        eTaperedCapsule,
        eTaperedCylinder,
        ePlane,
        eHeightField,
        eCompound
    };

    struct ColliderShapeDescription {
        ColliderShapeKind mKind{ ColliderShapeKind::eBox };

        core::float3 mDimensions{ 0.5f };// box half extents; sphere radius x; capsule radius x / half height y
        core::float3 mOffset{};
        core::float3 mCenterOfMassOffset{};
        core::float3 mScale{ 1.0f };
        bool mIsConvex{ true };
        bool mMutable{};
        core::f32 mPlaneDistance{};
        core::u32 mSampleCount{};
        eastl::shared_ptr<const eastl::vector<core::f32>> mHeights{};
        eastl::vector<CompoundShapePart> mParts{};

        eastl::span<const core::float3> mVertices{};
        eastl::span<const core::u32> mIndices{};

        auto operator==( const ColliderShapeDescription& rhs ) const -> bool;
    };

    enum class BodyType {
        eStatic,
        eKinematic,
        eDynamic
    };

    enum class ColliderType {
        eBox,
        eSphere,
        eCapsule,
        eMesh,
        eHeightField,
        eCompound,
        eCylinder,
        eTaperedCapsule,
        eTaperedCylinder,
        ePlane,
        eCount,
    };

    enum class GravityBody {
        eEarth,
        eMoon,
        eMars,
        eJupiter,
    };
}

namespace JPH {
    class Shape;
}

namespace mikoto::physics {

    /** Immutable cooked collider shared through Mikoto's intrusive references.
     * Native bodies retain their own Jolt reference, so replacing or releasing
     * a handle cannot invalidate a shape still used by the physics solver.
     */
    class Shape : public core::ReferenceCounted<Shape> {
    public:
        ~Shape() override;

        /** Cook a collider. On invalid geometry, return an empty handle and an
         * error suitable for the inspector; no partially initialized shape escapes.
         * Mesh spans are consumed during this call and are not retained.
         */
        MKT_NODISCARD static auto Create( const ColliderShapeDescription& description, eastl::string& error ) -> ShapeHandle;

        /** Borrow the immutable backend shape when attaching it to a Jolt body. */
        MKT_NODISCARD auto GetNativeShape() const -> const JPH::Shape*;

    protected:
        explicit Shape( const JPH::Shape* shape );

    private:
        struct State;
        eastl::unique_ptr<State> mState;
    };

    /** A box with positive half extents in metres; constructed with RefConst<BoxShape>::New. */
    class BoxShape final : public Shape {
    public:
        explicit BoxShape( const core::float3& halfExtents );

        MKT_NODISCARD auto GetHalfExtents() const -> const core::float3&;

    private:
        friend class Shape;
        BoxShape( const JPH::Shape* shape, const core::float3& halfExtents );
        core::float3 mDimensions{};
    };

}// namespace mikoto::physics

#endif// MIKOTO_PHYSICS_SHAPE_HH
