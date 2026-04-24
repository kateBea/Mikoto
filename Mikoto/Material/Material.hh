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

#ifndef MIKOTO_MATERIAL_HH
#define MIKOTO_MATERIAL_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/ReferenceCounted.hh>

namespace mikoto::material {

    class Material : public core::ReferenceCounted {
    public:
        explicit Material(const eastl::string_view name = "Base Material")
            :  mName{ name }
        {}

        MKT_NODISCARD auto GetName() const -> const eastl::string& { return mName; }
        auto SetName(const eastl::string_view newName) -> void { mName = newName; }

        ~Material() override = default;

    protected:
        eastl::string mName{};
    };

    using MaterialHandle = core::Ref<Material>;
}


#endif // MIKOTO_MATERIAL_HH
