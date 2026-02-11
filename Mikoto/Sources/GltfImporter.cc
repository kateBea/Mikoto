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

#include <fstream>
#include <iostream>
#include <mutex>

#include <Assets/GltfImporter.hh>

#include <tiny_gltf.h>

namespace Mikoto {

    static std::mutex mutex;

    GLTFImporter::GLTFImporter( GpuDevice *device )
    : ModelImporter{ device } {
        m_Loaders.resize( 10 );
    }
    auto GLTFImporter::Import( const ModelLoadDescription &description ) -> Model * {
        std::lock_guard lock{ mutex };

        // Acquire free loader and use it, right now there is only one
        auto& loaderData{ m_Loaders.front() };
        tinygltf::Model model{};

        bool res = loaderData.Loader.LoadASCIIFromFile(&model, &loaderData.Err, &loaderData.Warn, description.ModelFile->GetPath());
        if (!loaderData.Warn.empty()) {
            std::cout << "WARN: " << loaderData.Warn << std::endl;
        }

        if (!loaderData.Err.empty()) {
            std::cout << "ERR: " << loaderData.Err << std::endl;
        }

        if (!res)
            std::cout << "Failed to load glTF: " << description.ModelFile->GetPath() << std::endl;
        else
            std::cout << "Loaded glTF: " << description.ModelFile->GetPath() << std::endl;

        return nullptr;
    }

}// namespace Mikoto