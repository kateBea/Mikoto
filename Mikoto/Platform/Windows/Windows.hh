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

// Never include windows.h anywhere explicitly
// Use this header instead if you need to access Win32 API

#ifndef MIKOTO_PLATFORM_PLATFORM_WINDOWS_HH
#define MIKOTO_PLATFORM_PLATFORM_WINDOWS_HH

#include <Core/Platform.hh>

#if defined( MIKOTO_PLATFORM_WINDOWS )

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef NODRAWTEXT
#define NODRAWTEXT
#endif

// Specify windows target for asio
#define _WIN32_WINDOWS 0x0A00

#include <guiddef.h>
#include <windows.h>
#include <combaseapi.h>

#include <EASTL/string.h>

#include <Core/Core.hh>

#endif

#endif// MIKOTO_PLATFORM_PLATFORM_WINDOWS_HH
