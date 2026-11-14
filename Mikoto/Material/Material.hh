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

#ifndef MIKOTO_MATERIAL_MATERIAL_HH
#define MIKOTO_MATERIAL_MATERIAL_HH

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Serializer.hh>
#include <Core/ReferenceCounted.hh>

namespace mikoto::material {

    /**
     * @brief Base interface for material resource management.
     *
     * This interface defines the core contract for any material within the engine.
     * Materials are can't be moved, copying allows a material to be cloned.
     */
    class IMaterial : core::INonCopyable, public core::ISerializable {
    public:
        /**
         * @brief Sets a new name for the material.
         * @param newName A string view representing the new name.
         */
        virtual auto SetName( eastl::string_view newName ) -> void = 0;


        /**
         * @brief Retrieves the current name of the material.
         * @return A non-mutable reference to the containing the material's name.
         */
        MKT_NODISCARD virtual auto GetName() const -> const eastl::string& = 0;
    };

    using MaterialHandle = core::Ref<IMaterial>;
}// namespace mikoto::material

#endif// MIKOTO_MATERIAL_MATERIAL_HH
