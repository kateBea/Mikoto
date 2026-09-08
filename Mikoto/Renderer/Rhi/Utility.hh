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

    /**
     * Returns the value produced by InferAPI.
     *
     * @param apiName Input value used by this operation.
     * @returns The result of InferAPI.
     */
    MKT_NODISCARD auto InferAPI( eastl::string_view apiName ) -> GraphicsAPI;

    /**
     * Returns the value produced by GetFormatInfo.
     *
     * @param format Input value used by this operation.
     * @returns The result of GetFormatInfo.
     */
    MKT_NODISCARD auto GetFormatInfo( Format format ) -> const FormatInfo&;

    /**
     * Returns the value produced by InferDimensions.
     *
     * @param resolution Input value used by this operation.
     * @returns The result of InferDimensions.
     */
    MKT_NODISCARD auto InferDimensions( RenderResolution resolution ) -> eastl::pair<float, float>;

    /**
     * Returns the value produced by InferDimensions.
     *
     * @param resolution Input value used by this operation.
     * @param mipLevel Input value used by this operation.
     * @returns The result of InferDimensions.
     */
    MKT_NODISCARD auto InferDimensions( RenderResolution resolution, core::u32 mipLevel ) -> eastl::pair<core::u32, core::u32>;

    /**
     * Returns the value produced by InferDimensions.
     *
     * @param width Input value used by this operation.
     * @param height Input value used by this operation.
     * @param mipLevel Input value used by this operation.
     * @returns The result of InferDimensions.
     */
    MKT_NODISCARD auto InferDimensions( float width, float height, core::u32 mipLevel ) -> eastl::pair<core::u32, core::u32>;

    /**
     * Infers the number of format-sized elements contained in a byte count.
     *
     * @param dataType Input value used by this operation.
     * @param sizeBytes Input value used by this operation.
     * @returns The result of InferElementCount.
     */
    MKT_NODISCARD auto InferElementCount( Format dataType, core::usize sizeBytes ) -> core::usize;

    /**
     * Returns the value produced by IsBuffer.
     *
     * @param type Input value used by this operation.
     * @returns The result of IsBuffer.
     */
    MKT_NODISCARD auto IsBuffer( ResourceType type ) noexcept -> bool;

    /**
     * Returns the value produced by IsTexture.
     *
     * @param type Input value used by this operation.
     * @returns The result of IsTexture.
     */
    MKT_NODISCARD auto IsTexture( ResourceType type ) noexcept -> bool;

    /**
     * Returns the value produced by IsSampler.
     *
     * @param type Input value used by this operation.
     * @returns The result of IsSampler.
     */
    MKT_NODISCARD auto IsSampler( ResourceType type ) noexcept -> bool;

    /**
     * Returns the value produced by IsMultiple.
     *
     * @param value Input value used by this operation.
     * @param compare Input value used by this operation.
     * @returns The result of IsMultiple.
     */
    MKT_NODISCARD auto IsMultiple( core::usize value, core::usize compare ) -> bool;

    /**
     * Performs the operation represented by NextMultiple.
     *
     * @param value Input value used by this operation.
     * @param multiple Input value used by this operation.
     * @returns The result of NextMultiple.
     */
    MKT_NODISCARD auto NextMultiple( core::usize value, core::usize multiple ) -> core::usize;
}// namespace mikoto::renderer::rhi

#endif//MIKOTO_RHI_UTILITY_HH
