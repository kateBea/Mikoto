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

#ifndef MIKOTO_CORE_RESOURCE_HH
#define MIKOTO_CORE_RESOURCE_HH

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>
#include <Core/Profiler.hh>
#include <Core/ReferenceCounted.hh>

namespace mikoto::core {

    // Ref<T> owns resources directly. Derived destructors release native state.
    class Resource : public core::ReferenceCounted<Resource> {
    public:
        MKT_NODISCARD auto IsUsed() const -> bool;
        MKT_NODISCARD auto IsReady() const -> bool;

    protected:
        virtual auto Initialize() -> void = 0;
        virtual auto Destroy() -> void = 0;

    protected:
        bool mIsAllocated{ false };
    };
}

#endif// MIKOTO_CORE_RESOURCE_HH
