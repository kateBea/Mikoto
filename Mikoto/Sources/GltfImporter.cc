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
#include <algorithm>
#include <ranges>
#include <iostream>
#include <mutex>

#include <tiny_gltf.h>

#include <Assets/GltfImporter.hh>
#include <Threading/ThreadUtility.hh>

namespace Mikoto {

    static std::mutex mutex;

    GLTFImporter::GLTFImporter( GpuDevice *device )
    : ModelImporter{ device } {
        for (Int32 count{}; count < ThreadUtils::InferConcurrentThreads(); ++count) {
            m_Importers.emplace_back( CreateScope<LoaderData>( count ));
        }
    }
    auto GLTFImporter::Import( const ModelLoadDescription &description ) -> ModelData * {
        ModelData* result{ nullptr };

        auto iter{ m_Importers.end() };
        do {
            iter = TryAcquireImporter();

            // TODO: some control to avoid this thread to be waiting here forever to acquire and importer
        } while (iter == m_Importers.end());

        MKT_CORE_LOGGER_DEBUG( "Using GLTF importer {}", (*iter)->Index );

        result = Import( *(*iter), description );
        (*iter)->IsFree.store( true, std::memory_order_release );

        return result;
    }

    auto GLTFImporter::TryAcquireImporter() -> std::vector<Unique<LoaderData>>::iterator {
        return std::ranges::find_if( m_Importers, []( const auto &importer ) -> bool {
            bool expected{ true };
            if ( importer->IsFree.compare_exchange_strong( expected, false, std::memory_order_acquire ) ) {
                return true;
            }

            return false;
        } );
    }

    auto GLTFImporter::Import( LoaderData& loaderData, const ModelLoadDescription &description ) -> ModelData * {
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

}