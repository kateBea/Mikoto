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

#ifndef MIKOTO_PHYSICS_PHYSX_BACKEND_HH
#define MIKOTO_PHYSICS_PHYSX_BACKEND_HH

#include <EASTL/unique_ptr.h>

#include <PxPhysics.h>

#include <Core/Core.hh>

namespace mikoto::physics {
    // Owns the PhysX SDK and the callbacks it borrows for its lifetime.
    class PhysXBackend final {
    public:
        PhysXBackend();
        ~PhysXBackend();

        auto Initialize() -> void;
        auto Shutdown() -> void;

        // Borrows the SDK instance until Shutdown. Returns nullptr before initialization.
        MKT_NODISCARD auto GetPhysics() const -> physx::PxPhysics*;

    private:
        struct Runtime;
        eastl::unique_ptr<Runtime> mRuntime{};
    };
}

#endif// MIKOTO_PHYSICS_PHYSX_BACKEND_HH
