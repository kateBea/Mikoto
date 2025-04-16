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
#include <Core/Assert.hh>
#include <Core/Logger.hh>
#include <FileSystem/FileService.hh>
#include <Scene/Entity.hh>

namespace Mikoto {

    FileService::FileService( const FileServiceCreateInfo& options )
        : m_CurrentWorkingDir{ std::filesystem::current_path() }
    {}

    auto FileService::SaveDialog(const std::string& defaultName, const std::initializer_list<std::pair<std::string, std::string>>& filters) -> Path_T {
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
            MKT_CORE_LOGGER_INFO("Filesystem::SaveDialog - User canceled File open dialog");
        }
        else {
            MKT_CORE_LOGGER_ERROR("Filesystem::SaveDialog - Error in  File open dialog: {}", NFD::GetError());
        }

        // NFD::Guard will automatically quit NFD.

        return saveFilePath;
    }

    auto FileService::OpenDialog(const std::initializer_list<std::pair<std::string, std::string>>& filters) -> Path_T {
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

    auto FileService::Init() -> void {
        if ( const auto result{ NFD::Init() == NFD_OKAY }; !result) {
            MKT_THROW_RUNTIME_ERROR("FileManager - Failed to initialized File dialog library NFD.");
        }
    }

    auto FileService::Shutdown() -> void {
        NFD::Quit();
    }

    auto  FileService::GetCurrentWorkingDirectory() const -> std::string {
        return m_CurrentWorkingDir.string();
    }


    auto FileService::LoadFileAsync( const Path_T& path, const FileMode mode ) -> Task<File>& {
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