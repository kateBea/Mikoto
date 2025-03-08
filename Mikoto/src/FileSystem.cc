//
// Created by zanet on 1/26/2025.
//

// C++ Standard Library
#include <memory>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <initializer_list>

// Third-Party Libraries
#include <nfd.hpp>
#include <entt/entt.hpp>
#include <yaml-cpp/yaml.h>

// Project Headers
#include <Core/Logging/Assert.hh>
#include <Core/Logging/Logger.hh>
#include <Core/System/FileSystem.hh>
#include <Scene/Scene/Entity.hh>

namespace Mikoto {

    auto FileSystem::SaveDialog(const std::string& defaultName, const std::initializer_list<std::pair<std::string, std::string>>& filters) -> Path_T {
        std::string saveFilePath{};

        // Process filters
        std::vector<nfdfilteritem_t> filterItems{};

        for (const auto& [filterName, filterExtensions] : filters) {
            filterItems.emplace_back(nfdfilteritem_t{ filterName.data(), filterExtensions.data() });
        }

        // initialize NFD
        NFD::Guard nfdGuard{};

        // auto-freeing memory
        NFD::UniquePath outPath{};

        // show the dialog
        nfdresult_t result{ NFD::SaveDialog(outPath, filterItems.data(), filterItems.size(), nullptr, defaultName.data()) };

        if (result == NFD_OKAY) {
            saveFilePath = outPath.get();
        }
        else if (result == NFD_CANCEL) {
            MKT_CORE_LOGGER_INFO("FileSystem::SaveDialog - User canceled File open dialog");
        }
        else {
            MKT_CORE_LOGGER_ERROR("FileSystem::SaveDialog - Error in  File open dialog: {}", NFD::GetError());
        }

        // NFD::Guard will automatically quit NFD.

        return saveFilePath;
    }

    auto FileSystem::OpenDialog(const std::initializer_list<std::pair<std::string, std::string>>& filters) -> Path_T {
        std::string filePath{};

        // Process filters
        std::vector<nfdfilteritem_t> filterItems{};

        for (const auto& [filterName, filterExtensions] : filters) {
            filterItems.emplace_back(nfdfilteritem_t{ filterName.data(), filterExtensions.data() });
        }

        // initialize NFD
        NFD::Guard nfdGuard{};

        // auto-freeing memory
        NFD::UniquePath outPath{};

        // show the dialog
        nfdresult_t result{ NFD::OpenDialog(outPath, filterItems.data(), filterItems.size()) };

        if (result == NFD_OKAY) {
            filePath = outPath.get();
        }
        else if (result == NFD_CANCEL) {
            MKT_CORE_LOGGER_INFO("User canceled File open dialog");
        }
        else {
            MKT_CORE_LOGGER_ERROR("Error in  File open dialog: {}", NFD::GetError());
        }

        // NFD::Guard will automatically quit NFD.

        return filePath;
    }

    auto FileSystem::Init() -> void {
        if ( const auto result{ NFD::Init() == NFD_OKAY }; !result) {
            MKT_THROW_RUNTIME_ERROR("FileManager - Failed to initialized File dialog library NFD.");
        }
    }

    auto FileSystem::Shutdown() -> void {
        NFD::Quit();
    }

    auto FileSystem::Update() -> void {

    }

    auto FileSystem::LoadFile( const Path_T& path, const FileMode mode ) -> File* {
        File* result{ nullptr };

        const auto findIt{ m_Files.find( path.string() ) };

        if (findIt != m_Files.end()) {
            result = findIt->second.get();
        } else {
            const auto[insertIt, success]{ m_Files.try_emplace( path.string(), CreateScope<File>( path, mode ) ) };

            if (success) {
                result = insertIt->second.get();
            }
        }

        return result;
    }

}// namespace Mikoto