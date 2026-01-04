//    Copyright 2025 ケイト
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

#ifndef MIKOTO_LOCALIZATION_SERVICE_HH
#define MIKOTO_LOCALIZATION_SERVICE_HH

#include <ankerl/unordered_dense.h>

#include <Common/Common.hh>
#include <Common/Service.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto {

    enum class ISOLanguage {
        ES_ES, // Spanish (Spain)
        EN_US, // English (United States)
        EN_GB, // English (United Kingdom)
        JA_JP, // Japanese (Japan)
        ZH_CN, // Chinese (Simplified)
    };

    struct LocalizationServiceCreateInfo {
        Path LocalizationRoot{};// Assets/Localization
        ISOLanguage DefaultLanguage{ ISOLanguage::EN_US };
    };

    struct LanguageAlias {
        std::string_view Alias;
        ISOLanguage Value;
    };

    MKT_NODISCARD constexpr auto StripExtension( std::string_view sv ) -> std::string_view {
        const auto dot{ sv.find( '.' ) };
        return dot == std::string_view::npos ? sv : sv.substr( 0, dot );
    }

    constexpr std::array LanguageAliases{
        // Japanese
        LanguageAlias{ "ja", ISOLanguage::JA_JP },
        LanguageAlias{ "ja-jp", ISOLanguage::JA_JP },
        LanguageAlias{ "japanese", ISOLanguage::JA_JP },

        // Spanish
        LanguageAlias{ "es", ISOLanguage::ES_ES },
        LanguageAlias{ "es-es", ISOLanguage::ES_ES },
        LanguageAlias{ "spanish", ISOLanguage::ES_ES },

        // English (US)
        LanguageAlias{ "en", ISOLanguage::EN_US },
        LanguageAlias{ "en-us", ISOLanguage::EN_US },
        LanguageAlias{ "english", ISOLanguage::EN_US },

        // English (UK)
        LanguageAlias{ "en-gb", ISOLanguage::EN_GB },
        LanguageAlias{ "uk", ISOLanguage::EN_GB },

        // Chinese (Simplified)
        LanguageAlias{ "zh", ISOLanguage::ZH_CN },
        LanguageAlias{ "zh-cn", ISOLanguage::ZH_CN },
        LanguageAlias{ "chinese_simplified", ISOLanguage::ZH_CN },
    };

    MKT_NODISCARD static constexpr auto InferISO( std::string_view language ) -> ISOLanguage {
        language = StripExtension( language );

        for (const auto &entry: LanguageAliases) {
            if (StringUtils::Equal( language, entry.Alias, StringUtils::StringComparisonPolicy::CASE_INSENSITIVE )) {
                return entry.Value;
            }
        }

        return ISOLanguage::EN_US;
    }

    MKT_NODISCARD static constexpr auto GetISOName( ISOLanguage language ) -> std::string_view {
        using enum ISOLanguage;

        switch (language) {
            case ES_ES: return "Spanish (Spain) (es-ES)";
            case EN_US: return "English (United States) (en-US)";
            case EN_GB: return "English (United Kingdom) (en-GB)";
            case JA_JP: return "Japanese (Japan) (ja-JP)";
            case ZH_CN: return "Chinese (Simplified) (zh-CN)";
        }

        return "Unknown Language";
    }

    class LocalizationService final : public Singleton<LocalizationService>, public IService {
    public:
        explicit LocalizationService( const LocalizationServiceCreateInfo& createInfo );

        auto Init() -> void override;
        auto Shutdown() -> void override;

        auto SetLanguage( ISOLanguage language ) -> void;
        auto SetLanguage( const std::string &language ) -> void;

        MKT_NODISCARD auto GetDefaultLanguage() const -> ISOLanguage;
        MKT_NODISCARD auto GetCurrentLanguage() const -> ISOLanguage;

        MKT_NODISCARD auto IsCurrentLanguage(ISOLanguage language) const -> bool;
        MKT_NODISCARD auto IsDefaultLanguage(ISOLanguage language) const -> bool;

        MKT_NODISCARD auto HasKey( const std::string &key, ISOLanguage language ) const -> bool;
        MKT_NODISCARD auto Translate( const std::string &key ) const -> const std::string &;

        auto ParseLocalizationEntries(const Path &file) -> void;

    private:
        auto LoadAllLanguages() -> void;
        auto LoadDefaultLanguageFile( const Path &path ) -> void;

    private:
        ISOLanguage m_DefaultLanguage{ ISOLanguage::EN_US };
        ISOLanguage m_CurrentLanguage{ ISOLanguage::EN_US };

        Path m_LocDefaultPath{};

        // Entry -> Translation
        using TranslationMap = ankerl::unordered_dense::map<std::string, std::string>;

        ankerl::unordered_dense::map<ISOLanguage, TranslationMap> m_Translations{};
    };

    #define MKT_LOC(key) LocalizationService::Get()->Translate(key)
    #define MKT_IS_CURRENT_ISO(ISO) LocalizationService::Get()->IsCurrentLanguage(ISO)
    #define MKT_IS_CURRENT_ISO_STR(ISO_STR) LocalizationService::Get()->IsCurrentLanguage(InferISO(ISO_STR))
}


#endif //MIKOTO_LOCALIZATION_SERVICE_HH