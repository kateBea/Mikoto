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

#ifndef MIKOTO_IMAGE_PROCESSOR_HH
#define MIKOTO_IMAGE_PROCESSOR_HH

#include <EASTL/vector.h>
#include <EASTL/fixed_hash_map.h>

#include <Core/Core.hh>
#include <Core/String.hh>
#include <Core/Types.hh>

#include <Filesystem/File.hh>
#include <Filesystem/Path.hh>

#include <Assets/Image.hh>

namespace mikoto::asset {
    using namespace mikoto::filesystem;

    // Pending transition to Google Wuffs
    // Using STB_Image for the time being

    auto ProcessImage2D( FileHandle file ) -> ImageHandle;
    auto ProcessImage2D( const Path& filepath ) -> ImageHandle;

    // Attempts to construct the cube image from the equirectangular file
    auto ProcessImageCube( FileHandle file ) -> ImageHandle;

    auto ProcessImageCube( const eastl::fixed_hash_map<ImageCubeFace, FileHandle, kCubeFaceCount>& files ) -> ImageHandle;


    // TODO(kate): HDR image processor
    // Will take an HDR equirectangular image from file path and project it into a cube
    // You can use it to then generate your cube map
    // Initial implementation from: https://github.com/ivarout/HdriToCubemap


}// namespace Mikoto

#endif//MIKOTO_IMAGE_PROCESSOR_HH
