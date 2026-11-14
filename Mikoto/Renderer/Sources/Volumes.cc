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

#include <EASTL/utility.h>

#include <Renderer/Core/Volumes.hh>

namespace mikoto::renderer {
    using namespace mikoto::core;

    // Extinction combines absorption and out-scattering, as in PBRT's sigma_t.
    // https://pbr-book.org/4ed/Volume_Scattering/Volume_Scattering_Processes
    auto MediumProperties::GetExtinction() const -> float3 {
        return mAbsorption + mScattering;
    }

    MediumInterface::MediumInterface( MediumHandle inside, MediumHandle outside )
        : mInside{ eastl::move( inside ) }, mOutside{ eastl::move( outside ) } {
    }

    auto MediumInterface::GetInside() const -> MediumHandle {
        return mInside;
    }

    auto MediumInterface::GetOutside() const -> MediumHandle {
        return mOutside;
    }

    auto MediumInterface::IsTransition() const -> bool {
        return mInside.GetPtr() != mOutside.GetPtr();
    }

    // A boundary chooses the next medium without creating a visible surface.
    // https://pbr-book.org/4ed/Volume_Scattering/Media#MediumInterfaceDefinition
    auto MediumInterface::GetMedium( const float3& direction, const float3& outwardNormal ) const -> MediumHandle {
        return glm::dot( direction, outwardNormal ) > 0.0f ? mOutside : mInside;
    }

    auto MediumInterface::SetInside( MediumHandle medium ) -> void {
        mInside = eastl::move( medium );
    }

    auto MediumInterface::SetOutside( MediumHandle medium ) -> void {
        mOutside = eastl::move( medium );
    }

}// namespace mikoto::renderer
