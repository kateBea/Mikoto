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

#ifndef MIKOTO_RENDERER_CORE_VOLUMES_HH
#define MIKOTO_RENDERER_CORE_VOLUMES_HH

#include <EASTL/optional.h>

#include <Core/Types.hh>
#include <Core/ReferenceCounted.hh>

namespace mikoto::renderer {

    // Design reference: PBRT v4, Media and Phase Functions.
    // https://pbr-book.org/4ed/Volume_Scattering/Media
    // https://pbr-book.org/4ed/Volume_Scattering/Phase_Functions
    // CPU-side transport contracts. VolumeModule uploads these optical coefficients;
    // GPU transport evaluates the corresponding medium and phase operations in Slang.
    // RGB coefficients use inverse world units. Spectral transport is deferred.

    struct PhaseFunctionSample {
        core::f32 mValue{};
        core::float3 mDirection{};
        core::f32 mPdf{};
    };

    class PhaseFunction : public core::ReferenceCounted<PhaseFunction> {
    public:
        // Both unit directions point away from the scattering point, as in PBRT.
        MKT_NODISCARD virtual auto Evaluate( const core::float3& outgoing, const core::float3& incoming ) const -> core::f32 = 0;
        MKT_NODISCARD virtual auto Sample( const core::float3& outgoing, const core::float2& sample ) const -> eastl::optional<PhaseFunctionSample> = 0;
        MKT_NODISCARD virtual auto Pdf( const core::float3& outgoing, const core::float3& incoming ) const -> core::f32 = 0;

        ~PhaseFunction() override = default;
    };

    using PhaseFunctionHandle = core::Ref<PhaseFunction>;

    // Planned phase implementation: Henyey-Greenstein, shared by fog and clouds.
    // Positive anisotropy favors forward scattering; zero is isotropic.
    class HenyeyGreensteinPhaseFunction;

    struct MediumProperties {
        core::float3 mAbsorption{};
        core::float3 mScattering{};
        core::float3 mEmission{};
        PhaseFunctionHandle mPhase{};

        MKT_NODISCARD auto GetExtinction() const -> core::float3;
    };

    struct RayMajorantSegment {
        core::f32 mTMin{};
        core::f32 mTMax{};
        core::float3 mExtinctionMajorant{};
    };

    // Emit non-overlapping segments front to back; nullopt means traversal ended.
    // The majorant bounds extinction throughout its segment, for null scattering.
    // https://pbr-book.org/4ed/Volume_Scattering/Media#RayMajorantIteratorDefinition
    class RayMajorantIterator {
    public:
        MKT_NODISCARD virtual auto Next() -> eastl::optional<RayMajorantSegment> = 0;

        virtual ~RayMajorantIterator() = default;
    };

    class Medium : public core::ReferenceCounted<Medium> {
    public:
        MKT_NODISCARD virtual auto IsEmissive() const -> bool = 0;

        // Evaluate local coefficients, independently of the enclosing shape.
        MKT_NODISCARD virtual auto SamplePoint( const core::float3& position ) const -> MediumProperties = 0;

        // TODO: add PBRT's SampleRay contract when ray and caller-owned iterator
        // storage are defined. Homogeneous media emit one segment; grids use DDA.
        // Density evaluation, majorant construction and transport are deferred.

        ~Medium() override = default;
    };

    using MediumHandle = core::Ref<Medium>;

    // Future implementations resolve VolumeMaterial data into a transport medium.
    // Components retain shape/placement; materials retain reusable optical data.
    class HomogeneousMedium;
    class GridMedium;
    class CloudMedium;

    class MediumInterface {
    public:
        MediumInterface() = default;
        MediumInterface( MediumHandle inside, MediumHandle outside );

        MediumInterface( const MediumInterface& other ) = default;
        MediumInterface( MediumInterface&& other ) = default;

        auto operator=( const MediumInterface& other ) -> MediumInterface& = default;
        auto operator=( MediumInterface&& other ) -> MediumInterface& = default;

        MKT_NODISCARD auto GetInside() const -> MediumHandle;
        MKT_NODISCARD auto GetOutside() const -> MediumHandle;
        MKT_NODISCARD auto IsTransition() const -> bool;

        // Null handles denote vacuum. The normal points out of the boundary.
        MKT_NODISCARD auto GetMedium( const core::float3& direction, const core::float3& outwardNormal ) const -> MediumHandle;

        auto SetInside( MediumHandle medium ) -> void;
        auto SetOutside( MediumHandle medium ) -> void;

        ~MediumInterface() = default;

    private:
        MediumHandle mInside{};
        MediumHandle mOutside{};
    };

    // A volume interaction has no surface normal or surface BSDF.
    struct MediumInteraction {
        core::float3 mPosition{};
        core::float3 mOutgoing{};
        MediumHandle mMedium{};
        MediumProperties mProperties{};
    };

}// namespace mikoto::renderer

#endif// MIKOTO_RENDERER_CORE_VOLUMES_HH
