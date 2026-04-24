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

#ifndef MIKOTO_FLAG_HH
#define MIKOTO_FLAG_HH

#include <Core/Core.hh>

namespace mikoto::core {

    template<typename Properties>
    struct Flags final {
        typename Properties::Data mData;

        constexpr auto operator|=( Flags const &other ) -> Flags & {
            mData |= other.mData;
            return *this;
        }

        constexpr auto operator&=( Flags const &other ) -> Flags & {
            mData &= other.mData;
            return *this;
        }

        constexpr auto operator^=( Flags const &other ) -> Flags & {
            mData ^= other.mData;
            return *this;
        }

        MKT_NODISCARD constexpr auto operator~() const -> Flags {
            return { ~mData };
        }

        MKT_NODISCARD constexpr auto operator|( Flags const &other ) const -> Flags {
            return { mData | other.mData };
        }

        MKT_NODISCARD constexpr auto operator&( Flags const &other ) const -> Flags {
            return { mData & other.mData };
        }

        MKT_NODISCARD constexpr auto operator^( Flags const &other ) const -> Flags {
            return { mData ^ other.mData };
        }

        MKT_NODISCARD constexpr auto operator<=>( Flags const &other ) const = default;

        constexpr operator bool() const {
            return mData != 0;
        }

        MKT_NODISCARD constexpr auto Has( Flags const &other ) const -> bool {
            return mData & other.mData;
        }
    };
}// namespace mikoto

#endif//MIKOTO_FLAG_HH
