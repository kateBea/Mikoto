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

#include <exception>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Platform.hh>

#include <Logging/Assert.hh>

#include <Renderer/D3D12/Direct3D12Helpers.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#include <Renderer/D3D11/Direct3D11Helpers.hh>

namespace mikoto::renderer::d3d12 {
    auto ThrowIfFailed( HRESULT hr ) -> void {
        if (FAILED(hr)) {
            MKT_THROW_RUNTIME_ERROR( "D3D12 Error" ); // TODO: improved errors
        }
    }
}// namespace mikoto::renderer::d3d12

#endif