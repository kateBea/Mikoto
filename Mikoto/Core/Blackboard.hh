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

#ifndef MIKOTO_FRAME_GRAPH_BLACKBOARD_HH
#define MIKOTO_FRAME_GRAPH_BLACKBOARD_HH

#include <utility>
#include <typeinfo>
#include <typeindex>

// eastl::any requires utility to be included before
#include <EASTL/utility.h>
#include <EASTL/any.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Logging/Assert.hh>

namespace mikoto::core {

    class Blackboard final {
    public:
        explicit Blackboard() = default;

        Blackboard(const Blackboard &) = default;
        Blackboard(Blackboard &&) noexcept = default;

        auto operator=(const Blackboard &) -> Blackboard & = default;
        auto operator=(Blackboard &&) noexcept -> Blackboard & = default;

        template <typename T, typename... Args>
        auto Add(Args &&...args) -> T&{
            MKT_ASSERT(!Contains<T>(), "Cannot register already existing type.");
            return mStorage[typeid(T)].emplace<T>(eastl::forward<Args>(args)...);
        }

        template <typename T>
        MKT_NODISCARD auto Contains() const -> bool {
            return mStorage.contains(typeid(T));
        }

        template <typename T>
        MKT_NODISCARD auto Get() -> T& {
            MKT_ASSERT(Contains<T>(), "Type not found in Blackboard.");
            return eastl::any_cast<T&>(mStorage.at(typeid(T)));
        }

        template <typename T>
        MKT_NODISCARD auto TryGet() -> T* {
            const auto it{ mStorage.find( typeid(T) ) };
            return it != mStorage.cend() ? eastl::any_cast<T>(&it->second) : nullptr;
        }

        template <typename T>
        MKT_NODISCARD auto Get() const -> const T& {
            MKT_ASSERT(Contains<T>(), "Type not found in Blackboard.");
            return eastl::any_cast<const T&>(mStorage.at(typeid(T)));
        }

        template <typename T>
        MKT_NODISCARD auto TryGet() const -> const T* {
            const auto it{ mStorage.find( typeid(T) ) };
            return it != mStorage.cend() ? eastl::any_cast<const T>(&it->second) : nullptr;
        }

        ~Blackboard() = default;

    private:
        ankerl::unordered_dense::map<std::type_index, eastl::any> mStorage{};
    };
}

#endif //MIKOTO_FRAME_GRAPH_BLACKBOARD_HH