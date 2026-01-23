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

#include <any>
#include <utility>
#include <typeindex>

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Logging/Assert.hh>

namespace Mikoto {

    class FrameGraphBlackboard final {
    public:
        explicit FrameGraphBlackboard() = default;

        FrameGraphBlackboard(const FrameGraphBlackboard &) = default;
        FrameGraphBlackboard(FrameGraphBlackboard &&) noexcept = default;

        auto operator=(const FrameGraphBlackboard &) -> FrameGraphBlackboard & = default;
        auto operator=(FrameGraphBlackboard &&) noexcept -> FrameGraphBlackboard & = default;

        template <typename T, typename... Args>
        auto Add(Args &&...args) -> T&{
            MKT_ASSERT(!Contains<T>(), "Cannot register already existing type.");
            return m_Storage[typeid(T)].emplace<T>(std::forward<Args>(args)...);
        }

        template <typename T>
        MKT_NODISCARD auto Contains() const -> bool {
            return m_Storage.contains(typeid(T));
        }

        template <typename T>
        MKT_NODISCARD auto Get() -> T& {
            MKT_ASSERT(Contains<T>(), "Type not found in Blackboard");
            return std::any_cast<T&>(m_Storage.at(typeid(T)));
        }

        template <typename T>
        MKT_NODISCARD auto TryGet() -> T* {
            const auto it{ m_Storage.find( typeid(T) ) };
            return it != m_Storage.cend() ? std::any_cast<T>(&it->second) : nullptr;
        }

        template <typename T>
        MKT_NODISCARD auto Get() const -> const T& {
            MKT_ASSERT(Contains<T>(), "Type not found in Blackboard");
            return std::any_cast<const T&>(m_Storage.at(typeid(T)));
        }

        template <typename T>
        MKT_NODISCARD auto TryGet() const -> const T* {
            const auto it{ m_Storage.find( typeid(T) ) };
            return it != m_Storage.cend() ? std::any_cast<const T>(&it->second) : nullptr;
        }

        ~FrameGraphBlackboard() = default;

    private:
        ankerl::unordered_dense::map<std::type_index, std::any> m_Storage{};
    };
}

#endif //MIKOTO_FRAME_GRAPH_BLACKBOARD_HH