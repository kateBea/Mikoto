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

#ifndef MIKOTO_RHI_UTILITY_HH
#define MIKOTO_RHI_UTILITY_HH

#include <EASTL/string.h>
#include <EASTL/utility.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Memory/BufferSpan.hh>
#include <Core/ResourcePool.hh>

#include <Renderer/Rhi/Types.hh>

namespace mikoto::renderer::rhi {

    MKT_NODISCARD auto InferAPI( eastl::string_view apiName ) -> GraphicsAPI;

    MKT_NODISCARD auto GetFormatInfo( Format format ) -> const FormatInfo&;

    MKT_NODISCARD auto InferDimensions( RenderResolution resolution ) -> eastl::pair<float, float>;
    MKT_NODISCARD auto InferDimensions( RenderResolution resolution, core::u32 mipLevel ) -> eastl::pair<core::u32, core::u32>;
    MKT_NODISCARD auto InferDimensions( float width, float height, core::u32 mipLevel ) -> eastl::pair<core::u32, core::u32>;

    MKT_NODISCARD auto InferElementCount( Format dataType, core::size_t sizeBytes ) -> core::size_t;

    MKT_NODISCARD auto IsBuffer( ResourceType type ) noexcept -> bool;
    MKT_NODISCARD auto IsTexture( ResourceType type ) noexcept -> bool;
    MKT_NODISCARD auto IsSampler( ResourceType type ) noexcept -> bool;

    MKT_NODISCARD auto IsMultiple( core::usize value, core::usize compare ) -> bool;
    MKT_NODISCARD auto NextMultiple( core::usize value, core::usize multiple ) -> core::usize;
}// namespace mikoto::renderer::rhi

#endif//MIKOTO_RHI_UTILITY_HH
