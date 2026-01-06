
#include <array>
#include <ranges>
#include <filesystem>
#include <string_view>

#include <nlohmann/json.hpp>

#include <Core/Profiler.hh>
#include <Library/IO/File.hh>
#include <Library/String/String.hh>
#include <Filesystem/FileService.hh>

#include <Core/LocalizationService.hh>

namespace Mikoto {

    LocalizationService::LocalizationService( const LocalizationServiceCreateInfo &createInfo )
        : m_DefaultLanguage{ createInfo.DefaultLanguage },
          m_CurrentLanguage{ m_DefaultLanguage },
          m_LocDefaultPath{ createInfo.LocalizationRoot } {

    }

    auto LocalizationService::Init() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        MKT_CORE_LOGGER_INFO( "Initializing LocalizationService..." );

        LoadAllLanguages();

        m_IsInitialized = true;
    }

    auto LocalizationService::Shutdown() -> void {
        MKT_BEGIN_PROFILER_NAMED();

        if (!m_IsInitialized) { return; }

        // The Log comes after so we know the service was
        // initialized before attempting to shut it down
        MKT_CORE_LOGGER_INFO( "Shutting down LocalizationService..." );

        m_Translations.clear();

        m_IsInitialized = false;
    }

    auto LocalizationService::SetLanguage( ISOLanguage language ) -> void {
        m_CurrentLanguage = language;
    }

    auto LocalizationService::SetLanguage( const std::string &language ) -> void {
        SetLanguage( InferISO( language ) );
    }

    auto LocalizationService::GetCurrentLanguage() const -> ISOLanguage {
        return m_CurrentLanguage;
    }

    auto LocalizationService::IsCurrentLanguage( const ISOLanguage language ) const -> bool {
        return m_CurrentLanguage == language;
    }

    auto LocalizationService::IsDefaultLanguage( const ISOLanguage language ) const -> bool {
        return m_DefaultLanguage == language;
    }

    auto LocalizationService::GetDefaultLanguage() const -> ISOLanguage {
        return m_DefaultLanguage;
    }

    auto LocalizationService::HasKey( const std::string &key, ISOLanguage language ) const -> bool {
        if (!m_Translations.contains( language )) {
            MKT_CORE_LOGGER_ERROR( "LocalizationService::HasKey - No entries for {}", GetISOName(language) );
            return false;
        }

        const TranslationMap& map{ m_Translations.at(language) };

        return map.contains( key );
    }

    auto LocalizationService::Translate( const std::string &key ) const -> const std::string & {
        // 1. Current language
        if (auto langIt{ m_Translations.find( m_CurrentLanguage ) };
            langIt != m_Translations.end()) {
            if (auto it{ langIt->second.find( key ) };
                it != langIt->second.end())
                return it->second;
        }

        // 2. Default language
        if (auto defIt{ m_Translations.find( m_DefaultLanguage ) };
            defIt != m_Translations.end()) {
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
        for (const auto &entry: std::views::all( fs::directory_iterator( m_LocDefaultPath ) ) |
                                std::views::filter( [&locationExtension]( auto &e ) { return e.is_regular_file() && e.path().extension() == locationExtension; } )) {
            // Just load entries for default languages
            LoadDefaultLanguageFile( entry.path() );
        }
    }

    auto LocalizationService::LoadDefaultLanguageFile( const Path &path ) -> void {
        const File *file{ FileService::Get()->LoadFile( path ) };
        MKT_ASSERT( file, "File is NULL" );

        nlohmann::json data{ nlohmann::json::parse( file->GetFileContents() ) };

        const ISOLanguage language{ InferISO( path.filename().string() ) };

        TranslationMap& map{ m_Translations[language] };
        map.reserve( data.size() );

        for (auto &[key, value]: data.items()) {
            if (value.is_string()) {
                map[key] = value.get<std::string>();
            }
        }
    }
}