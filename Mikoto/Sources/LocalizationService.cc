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

#include <ranges>
#include <string>
#include <filesystem>

#include <EASTL/array.h>
#include <EASTL/string_view.h>

#include <nlohmann/json.hpp>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/Profiler.hh>
#include <Core/LocalizationService.hh>

#include <Logging/Logger.hh>

#include <Filesystem/FileService.hh>

namespace mikoto::core {

    using namespace mikoto::filesystem;

    LocalizationService::LocalizationService( const LocalizationServiceCreateInfo &createInfo )
        : m_DefaultLanguage{ createInfo.mDefaultLanguage },
          mCurrentLanguage{ m_DefaultLanguage },
          mLocDefaultPath{ createInfo.mLocalizationBasePath } {

    }

    auto LocalizationService::Initialize() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO( "Initializing LocalizationService..." );

        LoadAllLanguages();

        mIsInitialized = true;
    }

    auto LocalizationService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!mIsInitialized) {
            return;
        }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down LocalizationService..." );

        mTranslations.clear();

        mIsInitialized = false;
    }

    auto LocalizationService::SetLanguage( ISOLanguage language ) -> void {
        mCurrentLanguage = language;
    }

    auto LocalizationService::SetLanguage( const eastl::string &language ) -> void {
        SetLanguage( InferISO( language ) );
    }

    auto LocalizationService::GetCurrentLanguage() const -> ISOLanguage {
        return mCurrentLanguage;
    }

    auto LocalizationService::IsCurrentLanguage( const ISOLanguage language ) const -> bool {
        return mCurrentLanguage == language;
    }

    auto LocalizationService::IsDefaultLanguage( const ISOLanguage language ) const -> bool {
        return m_DefaultLanguage == language;
    }

    auto LocalizationService::GetDefaultLanguage() const -> ISOLanguage {
        return m_DefaultLanguage;
    }

    auto LocalizationService::HasKey( const eastl::string &key, ISOLanguage language ) const -> bool {
        if (!mTranslations.contains( language )) {
            MKT_CORE_LOGGER_ERROR( "LocalizationService::HasKey - No entries for {}", GetISOName(language) );
            return false;
        }

        const TranslationMap& map{ mTranslations.at(language) };

        return map.contains( eastl::string{ key } );
    }

    auto LocalizationService::Translate( const eastl::string &key ) const -> const eastl::string & {
        // 1. Current language
        if (auto langIt{ mTranslations.find( mCurrentLanguage ) };
            langIt != mTranslations.end()) {
            if (auto it{ langIt->second.find( key ) };
                it != langIt->second.end())
                return it->second;
        }

        // 2. Default language
        if (auto defIt{ mTranslations.find( m_DefaultLanguage ) };
            defIt != mTranslations.end()) {
            if (auto it{ defIt->second.find( key ) };
                it != defIt->second.end())
                return it->second;
        }

        // 3. Fallback to key itself
        return key;
    }

    auto LocalizationService::ParseLocalizationEntries( const Path &file ) -> void {
        LoadDefaultLanguageFile( file );
    }

    auto LocalizationService::LoadAllLanguages() -> void {
        namespace fs = std::filesystem;

        constexpr std::string_view locationExtension{ ".json" };
        for (const auto &entry: std::views::all( fs::directory_iterator( mLocDefaultPath.GetPathTyped<std::filesystem::path>() ) ) |
                                std::views::filter( [&locationExtension]( auto &e ) { return e.is_regular_file() && e.path().extension() == locationExtension; } )) {
            // Just load entries for default languages
            LoadDefaultLanguageFile( Path{ entry.path() } );
        }
    }

    auto LocalizationService::LoadDefaultLanguageFile( const Path &path ) -> void {
        FileHandle file{ FileService::Get()->LoadFile( path ) };
        if (file.IsEmpty() || !file->HasContents()) {
            return;
        }

        nlohmann::json data{ nlohmann::json::parse( file->GetContentsString().c_str() ) };

        const ISOLanguage language{ InferISO( path.GetFilename() ) };

        TranslationMap& map{ mTranslations[language] };
        map.reserve( data.size() );

        for (auto &[key, value]: data.items()) {
            if (value.is_string()) {
                map[string::ToEA_Stl(key)] = string::ToEA_Stl( value.get<std::string>() );
            }
        }
    }
}