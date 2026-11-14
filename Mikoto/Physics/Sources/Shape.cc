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

#include <EASTL/algorithm.h>

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/ConvexHullShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/TaperedCylinderShape.h>
#include <Jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.h>

#include <Core/Exception.hh>

#include <Physics/Shape.hh>
#include <Physics/Utility.hh>

namespace mikoto::physics {

    using namespace mikoto::core;


    // No scene, renderer or device dependency: cooking can be validated headlessly.
    static auto CookColliderShape( const ColliderShapeDescription& desc ) -> JPH::ShapeSettings::ShapeResult {
        MKT_BEGIN_PROFILER();
        using Result = JPH::ShapeSettings::ShapeResult;
        auto error = []( const char* message ) {
            Result result;
            result.SetError( message );

            return result;
        };

        const auto scale = glm::abs( desc.mScale );

        if ( glm::any( glm::lessThan( scale, glm::vec3{ 0.00001f } ) ) ) {
            return error( "Collider scale must be nonzero on every axis." );
        }

        auto vec = []( const glm::vec3& v ) {
            return JPH::Vec3{ v.x, v.y, v.z };
        };
        Result result;

        switch ( desc.mKind ) {
            case ColliderShapeKind::eCylinder:
            case ColliderShapeKind::eTaperedCapsule:
            case ColliderShapeKind::eTaperedCylinder: {
                const float radial{ eastl::max( scale.x, scale.z ) };
                const float top{ desc.mDimensions.x * radial }, bottom{ desc.mDimensions.z * radial };
                const float height{ desc.mDimensions.y * scale.y };

                if ( top <= 0 || height <= 0 || ( desc.mKind != ColliderShapeKind::eCylinder && bottom <= 0 ) ) {
                    return error( "Collider radii and half height must be positive." );
                }

                // Corresponding Jolt Samples/Tests/Shapes/*ShapeTest.cpp constructors.
                if ( desc.mKind == ColliderShapeKind::eCylinder ) {
                    result = JPH::CylinderShapeSettings{ height, top, eastl::min( .05f, eastl::min( height, top ) * .5f ) }.Create();
                }
                else if ( desc.mKind == ColliderShapeKind::eTaperedCapsule ) {
                    result = JPH::TaperedCapsuleShapeSettings{ height, top, bottom }.Create();
                }
                else {
                    result = JPH::TaperedCylinderShapeSettings{ height, top, bottom, 0 }.Create();
                }

                break;
            }

            case ColliderShapeKind::ePlane: {
                if ( glm::length2( desc.mDimensions ) < .0001f ) {
                    return error( "Plane normal must be nonzero." );
                }

                const auto normal{ glm::normalize( desc.mDimensions / desc.mScale ) };
                result = JPH::PlaneShapeSettings{ JPH::Plane{ vec( normal ), -desc.mPlaneDistance / glm::length( desc.mDimensions / desc.mScale ) } }.Create();
                break;
            }

            case ColliderShapeKind::eHeightField: {
                if ( !desc.mHeights || desc.mSampleCount < 4 || desc.mHeights->size() != core::usize{ desc.mSampleCount } * desc.mSampleCount ||
                     glm::any( glm::lessThanEqual( desc.mDimensions, core::float3{} ) ) ) {
                    return error( "Heightfield needs a square grid of at least 4 samples and positive spacing." );
                }

                result = JPH::HeightFieldShapeSettings{ desc.mHeights->data(), JPH::Vec3::sZero(), vec( desc.mDimensions * scale ), desc.mSampleCount }.Create();
                break;
            }

            case ColliderShapeKind::eCompound: {
                if ( desc.mParts.empty() ) {
                    return error( "Compound collider needs at least one valid child shape." );
                }

                JPH::StaticCompoundShapeSettings fixed{};
                JPH::MutableCompoundShapeSettings mutableShape{};
                JPH::CompoundShapeSettings& settings{ desc.mMutable ? static_cast<JPH::CompoundShapeSettings&>( mutableShape ) : static_cast<JPH::CompoundShapeSettings&>( fixed ) };

                for ( const auto& part: desc.mParts ) {
                    if ( !part.mShape ) {
                        return error( "Compound contains an invalid collider." );
                    }

                    settings.AddShape( vec( part.mPosition ), { part.mRotation.x, part.mRotation.y, part.mRotation.z, part.mRotation.w }, part.mShape->GetNativeShape() );
                }

                result = settings.Create();

                if ( !result.HasError() ) {
                    result = result.Get()->ScaleShape( vec( desc.mScale ) );
                }

                break;
            }

            case ColliderShapeKind::eBox: {
                const auto extents = desc.mDimensions * scale;

                if ( glm::any( glm::lessThanEqual( extents, glm::vec3{} ) ) ) {
                    return error( "Box half extents must be positive." );
                }

                const float convexRadius = eastl::min( { 0.05f, extents.x * 0.5f, extents.y * 0.5f, extents.z * 0.5f } );
                result = JPH::BoxShapeSettings{ vec( extents ), convexRadius }.Create();
                break;
            }

            case ColliderShapeKind::eSphere: {
                const float radius = desc.mDimensions.x * eastl::max( { scale.x, scale.y, scale.z } );

                if ( radius <= 0 ) {
                    return error( "Sphere radius must be positive." );
                }

                result = JPH::SphereShapeSettings{ radius }.Create();
                break;
            }

            case ColliderShapeKind::eCapsule: {
                const float radius = desc.mDimensions.x * eastl::max( scale.x, scale.z );
                const float height = desc.mDimensions.y * scale.y;

                if ( radius <= 0 || height < 0 ) {
                    return error( "Capsule radius must be positive and half height nonnegative." );
                }

                result = height == 0 ? JPH::SphereShapeSettings{ radius }.Create() : JPH::CapsuleShapeSettings{ height, radius }.Create();
                break;
            }

            case ColliderShapeKind::eMesh: {
                if ( desc.mVertices.empty() ) {
                    return error( "Mesh collider needs a mesh with retained CPU geometry." );
                }

                JPH::Array<JPH::Vec3> points;
                points.reserve( desc.mVertices.size() );

                for ( const auto& vertex: desc.mVertices ) {
                    const auto point = vertex * desc.mScale;
                    points.push_back( vec( point ) );
                }

                if ( desc.mIsConvex ) {
                    result = JPH::ConvexHullShapeSettings{ points, 0.0f }.Create();
                }
                else {
                    if ( desc.mIndices.empty() || desc.mIndices.size() % 3 != 0 ) {
                        return error( "Mesh collider needs triangle indices." );
                    }

                    JPH::TriangleList triangles;
                    const bool mirrored = desc.mScale.x * desc.mScale.y * desc.mScale.z < 0;

                    for ( size_t i = 0; i < desc.mIndices.size(); i += 3 ) {
                        const auto a = desc.mIndices[i];
                        const auto b = desc.mIndices[i + 1];
                        const auto c = desc.mIndices[i + 2];

                        if ( a >= points.size() || b >= points.size() || c >= points.size() ) {
                            return error( "Mesh collider index is out of range." );
                        }

                        triangles.emplace_back( points[a], points[mirrored ? c : b], points[mirrored ? b : c] );
                    }

                    result = JPH::MeshShapeSettings{ triangles }.Create();
                }

                break;
            }
        }

        if ( result.HasError() ) {
            return result;
        }

        // Convex bodies may switch to dynamic at runtime, so they need usable mass and inertia.
        // Jolt can successfully cook a planar hull even though its volume is zero.
        if ( desc.mKind != ColliderShapeKind::ePlane && desc.mKind != ColliderShapeKind::eHeightField && ( desc.mKind != ColliderShapeKind::eMesh || desc.mIsConvex ) ) {
            const auto mass = result.Get()->GetMassProperties();

            if ( mass.mMass <= 0 ) {
                return error( "Collider needs nonzero volume. For flat geometry, use a static triangle mesh collider." );
            }

            for ( int row = 0; row < 3; ++row ) {
                for ( int column = 0; column < 3; ++column ) {
                    const float value = mass.mInertia( row, column );

                    if ( row == column && value <= 0 ) {
                        return error( "Collider has invalid inertia. Check its dimensions and mesh volume." );
                    }
                }
            }
        }

        const auto offset = desc.mOffset * desc.mScale;

        if ( offset != glm::vec3{} ) {
            result = JPH::RotatedTranslatedShapeSettings{ vec( offset ), JPH::Quat::sIdentity(), result.Get().GetPtr() }.Create();
        }

        // Change handling without moving the collision surface or its visuals.
        if ( !result.HasError() && desc.mCenterOfMassOffset != glm::vec3{} ) {
            result = JPH::OffsetCenterOfMassShapeSettings{ vec( desc.mCenterOfMassOffset ), result.Get().GetPtr() }.Create();
        }

        return result;
    }

    struct Shape::State {
        JPH::RefConst<JPH::Shape> mNative;
    };

    static auto CookBox( const core::float3& halfExtents ) -> JPH::RefConst<JPH::Shape> {
        ColliderShapeDescription description{};
        description.mDimensions = halfExtents;
        const auto result{ CookColliderShape( description ) };

        if ( result.HasError() ) {
            MKT_THROW_RUNTIME_ERROR( result.GetError().c_str() );
        }

        return result.Get();
    }


    Shape::Shape( const JPH::Shape* shape )
        : mState{ eastl::make_unique<State>() } {
        mState->mNative = shape;
    }

    Shape::~Shape() = default;

    auto Shape::GetNativeShape() const -> const JPH::Shape* {
        return mState->mNative.GetPtr();
    }

    auto Shape::Create( const ColliderShapeDescription& description, eastl::string& error ) -> ShapeHandle {
        error.clear();
        const auto cooked{ CookColliderShape( description ) };

        if ( cooked.HasError() ) {
            error = cooked.GetError().c_str();

            return {};
        }

        // Native cooking results are temporary. Each wrapper takes its own
        // reference before the result is released; bodies take another on attach.
        if ( description.mKind == ColliderShapeKind::eBox ) {
            return ShapeHandle{ new BoxShape{ cooked.Get().GetPtr(), description.mDimensions * glm::abs( description.mScale ) } };
        }

        return ShapeHandle{ new Shape{ cooked.Get().GetPtr() } };
    }


    BoxShape::BoxShape( const core::float3& halfExtents )
        : BoxShape{ CookBox( halfExtents ).GetPtr(), halfExtents } {
    }

    BoxShape::BoxShape( const JPH::Shape* shape, const core::float3& halfExtents )
        : Shape{ shape }, mDimensions{ halfExtents } {
    }

    auto BoxShape::GetHalfExtents() const -> const core::float3& {
        return mDimensions;
    }


    bool ColliderShapeDescription::operator==( const ColliderShapeDescription& rhs ) const {
        MKT_BEGIN_PROFILER();

        // Decomposing a rotating world matrix introduces tiny scale roundoff; don't recook every frame.
        const auto scaleTolerance{ glm::max( glm::abs( mScale ), glm::abs( rhs.mScale ) ) * 0.0001f };

        const bool sameScale{ glm::all( glm::lessThanEqual( glm::abs( mScale - rhs.mScale ), scaleTolerance ) ) };

        return mKind == rhs.mKind && mDimensions == rhs.mDimensions && mOffset == rhs.mOffset && mCenterOfMassOffset == rhs.mCenterOfMassOffset &&
               sameScale && mIsConvex == rhs.mIsConvex && mMutable == rhs.mMutable && mParts == rhs.mParts && mHeights == rhs.mHeights && mSampleCount == rhs.mSampleCount && mPlaneDistance == rhs.mPlaneDistance && mVertices.data() == rhs.mVertices.data() &&
               mVertices.size() == rhs.mVertices.size() && mIndices.data() == rhs.mIndices.data() && mIndices.size() == rhs.mIndices.size();
    }
}// namespace mikoto::physics
