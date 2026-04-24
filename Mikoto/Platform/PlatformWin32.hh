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

#ifndef MIKOTOROOT_PLATFORM_WIN32_HH
#define MIKOTOROOT_PLATFORM_WIN32_HH


// TODO:
// https://youtu.be/D-PC-huX-l8?list=PLqCJpWy5Fohd3S7ICFXwUomYW0Wv67pDD

// This file is supposed to be included at the top of you file before anything if
// you need to interact with the Win32 API otherwise it clutters the namespace with bunch
// of crap that might cause problems later

#include <Core/Platform.hh>

#if defined(MIKOTO_PLATFORM_WINDOWS)

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

// Specify windows target for asio
#define _WIN32_WINDOWS 0x0A00

#include <windows.h>

#include <guiddef.h>
#include <combaseapi.h>

#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>

namespace mikoto::platform::windows {

    MKT_NODISCARD auto GuidToString(const GUID& guid) -> eastl::string;

}// namespace mikoto::platform::windows

#endif


#endif//MIKOTOROOT_PLATFORM_WIN32_HH
