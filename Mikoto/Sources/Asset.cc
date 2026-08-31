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

#include <Assets/Asset.hh>

namespace mikoto::asset {

    using namespace mikoto::core;
    using namespace mikoto::filesystem;

    auto GetHashedAssetID(const Path& path) -> AssetID {
        return eastl::hash<eastl::string>{}(path.GetAbsolute());
    }

    auto IsFileImage(const Path& path) -> bool {
        const auto& ext{ path.GetNormalizedExtension() };

        return ext == "png" ||
               ext == "jpg" ||
               ext == "jpeg" ||
               ext == "tga" ||
               ext == "bmp" ||
               ext == "hdr";
    }

    auto IsFileImage( filesystem::FileType type ) -> bool {
        return type == FileType::ePng ||
                type == FileType::eJpg ||
                type == FileType::eJpeg ||
                type == FileType::eBmp ||
                type == FileType::eTarga ||
                type == FileType::eHdr ||
                type == FileType::eGlb;
    }

    auto IsFileModel(const Path& path) -> bool {
        const auto& ext{ path.GetNormalizedExtension() };

        return ext == "obj" ||
               ext == "fbx" ||
               ext == "gltf" ||
               ext == "glb";
    }

    auto IsFileModel( filesystem::FileType type ) -> bool {
        return type == FileType::eObj ||
               type == FileType::eFbx ||
               type == FileType::eGltf ||
               type == FileType::eGlb;
    }

    auto IsFileAudio(const Path& path) -> bool {
        const auto& ext{ path.GetNormalizedExtension() };

        return ext == "wav" ||
               ext == "mp3" ||
               ext == "ogg" ||
               ext == "flac";
    }

    auto IsFileFont(const Path& path) -> bool {
        const auto& ext{ path.GetNormalizedExtension() };

        return ext == "ttf" ||
               ext == "otf";
    }

    auto IsFileMaterial(const Path& path) -> bool {
        const auto& ext{ path.GetNormalizedExtension() };

        return ext == "mat" ||
               ext == "material" ||
               ext == "json";
    }
}// namespace mikoto::asset