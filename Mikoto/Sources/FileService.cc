//
// Created by zanet on 1/26/2025.
//

// C++ Standard Library
#include <algorithm>
#include <fstream>
#include <initializer_list>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Third-Party Libraries
#include <nfd.hpp>

// Project Headers
#include <Core/Profiler.hh>
#include <Filesystem/FileService.hh>
#include <Filesystem/FileWatcherService.hh>
#include <Logging/Assert.hh>
#include <Logging/Logger.hh>
#include <Core/Exception.hh>

namespace Mikoto {

    FileService::FileService( const FileServiceCreateInfo& ) {}

    auto FileService::SaveDialog( const std::string& defaultName, const std::initializer_list<std::pair<std::string, std::string>>& filters ) -> Path {
        std::string saveFilePath{};

        // Process filters
        std::vector<nfdfilteritem_t> filterItems{};

        for ( const auto& [filterName, filterExtensions]: filters ) {
            filterItems.emplace_back( nfdfilteritem_t{ filterName.data(), filterExtensions.data() } );
        }

        // initialize NFD
        NFD::Guard nfdGuard{};

        // auto-freeing memory
        NFD::UniquePath outPath{};

        // show the dialog
        nfdresult_t result{ NFD::SaveDialog( outPath, filterItems.data(), filterItems.size(), nullptr, defaultName.data() ) };

        if ( result == NFD_OKAY ) {
            saveFilePath = outPath.get();
        } else if ( result == NFD_CANCEL ) {
            MKT_CORE_LOGGER_INFO( "Filesystem::SaveDialog - User canceled File open dialog" );
        } else {
            MKT_CORE_LOGGER_ERROR( "Filesystem::SaveDialog - Error in  File open dialog: {}", NFD::GetError() );
        }

        // NFD::Guard will automatically quit NFD.

        return saveFilePath;
    }

    auto FileService::OpenDialog( const std::initializer_list<std::pair<std::string, std::string>>& filters ) -> Path {
        std::string filePath{};

        // Process filters
        std::vector<nfdfilteritem_t> filterItems{};

        for ( const auto& [filterName, filterExtensions]: filters ) {
            filterItems.emplace_back( nfdfilteritem_t{ filterName.data(), filterExtensions.data() } );
        }

        // initialize NFD
        NFD::Guard nfdGuard{};

        // auto-freeing memory
        NFD::UniquePath outPath{};

        // show the dialog
        nfdresult_t result{ NFD::OpenDialog( outPath, filterItems.data(), filterItems.size() ) };

        if ( result == NFD_OKAY ) {
            filePath = outPath.get();
        } else if ( result == NFD_CANCEL ) {
            MKT_CORE_LOGGER_INFO( "User canceled File open dialog" );
        } else {
            MKT_CORE_LOGGER_ERROR( "Error in  File open dialog: {}", NFD::GetError() );
        }

        // NFD::Guard will automatically quit NFD.

        return filePath;
    }

    auto FileService::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO( "Initializing FileService..." );

        if ( const auto result{ NFD::Init() == NFD_OKAY }; !result ) {
            MKT_THROW_RUNTIME_ERROR( "FileManager - Failed to initialized File dialog library NFD." );
        }

        m_IsInitialized = true;
    }

    auto FileService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if ( !m_IsInitialized ) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down FileService..." );

        NFD::Quit();
    }

    auto FileService::LoadFile( const Path& path, FileMode mode ) -> File* {
        MKT_BEGIN_PROFILER_NAMED();

        File* result{ nullptr };

        // Use string because m_Files key is std::string, Path contained type is implementation defined
        // char on linux and wchar_t on windows
        const auto findIt{ m_Files.find( path.string() ) };

        if ( findIt != m_Files.end() ) {
            result = findIt->second.get();
        } else {
            // File does not exist
            auto newFile{ File::Load( path, mode ) };
            if ( newFile ) {
                result = newFile.get();

                {
                    std::lock_guard lock{ m_FileLoadMutex };
                    const auto [insertIt, success]{
                        m_Files.try_emplace( path.string(), std::move( newFile ) )
                    };
                }

                //If we managed to load the file listen on update notifications to update the file contents
                FileWatcherService::Get()->Watch( result->GetPath(), [result](const Path& pathCallable, FileWatchEvent event) mutable -> void {
                    if (event == FileWatchEvent::MODIFIED) {
                        result->UpdateContents();
                        MKT_CORE_LOGGER_INFO( "File at path {} was modified. Updating it's contents", pathCallable.string());
                    }
                } );
            } else {
                MKT_CORE_LOGGER_ERROR( "Could not load file at {}", path.string() );
            }
        }

        return result;
    }

    auto FileService::GetFile( const Path& path ) -> File* {
        auto it{ m_Files.find( path.string() ) };
        if ( it != m_Files.end() ) {
            return it->second.get();
        }

        return nullptr;
    }

    auto FileService::GetFile( const Path& path ) const -> const File* {
        return const_cast<FileService*>( this )->GetFile( path );
    }

    auto FileService::LoadFileAsync( const Path& path, const FileMode mode ) -> void {
        File* result{ LoadFile( path, mode ) };

        //return nullptr;
    }

    auto FileService::CreateNewFile( const Path& path ) -> File* {
        File* result{ nullptr };

        // File does not exist
        auto newFile{ File::Load( path, MKT_FILE_OPEN_MODE_TRUNCATE ) };
        if ( newFile ) {
            result = newFile.get();

            const auto [insertIt, success]{
                m_Files.try_emplace( path.string(), std::move( newFile ) )
            };
        } else {
            MKT_CORE_LOGGER_ERROR( "Could not create file at {}", path.string() );
        }

        return result;
    }

    auto FileService::CreateFileAsync( const Path& path ) -> File* {
        return nullptr;
    }

    auto FileService::SaveFile( const File* file ) -> void {
    }

    auto FileService::SaveFileAsync( const File* file ) -> void {
    }

}// namespace Mikoto