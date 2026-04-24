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

#ifndef MIKOTO_LOCALIZATION_SERVICE_HH
#define MIKOTO_LOCALIZATION_SERVICE_HH

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/string_view.h>

#include <ankerl/unordered_dense.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/String.hh>
#include <Core/Service.hh>
#include <Core/Singleton.hh>

#include <Filesystem/Path.hh>

namespace mikoto::core {

    using namespace mikoto::core;
    using namespace mikoto::filesystem;

    enum class ISOLanguage {
        ES_ES, // Spanish (Spain)
        EN_US, // English (United States)
        EN_GB, // English (United Kingdom)
        JA_JP, // Japanese (Japan)
        ZH_CN, // Chinese (Simplified)
    };

    struct LocalizationServiceCreateInfo {
        Path mLocalizationBasePath{};// Assets/Localization
        ISOLanguage mDefaultLanguage{ ISOLanguage::EN_US };
    };

    struct LanguageAlias {
        eastl::string_view mAlias{};
        ISOLanguage mValue{ ISOLanguage::EN_US };
    };

    MKT_NODISCARD constexpr auto StripExtension( eastl::string_view sv ) -> eastl::string_view {
        const auto dot{ sv.find( '.' ) };
        return dot == eastl::string_view::npos ? sv : sv.substr( 0, dot );
    }

    constexpr eastl::array LanguageAliases{
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

    MKT_NODISCARD static constexpr auto InferISO( eastl::string_view language ) -> ISOLanguage {
        language = StripExtension( language );

        for (const auto &entry: LanguageAliases) {
            if (string::Equal( language, entry.mAlias, string::ComparePolicy::eIgnoreCase )) {
                return entry.mValue;
            }
        }

        return ISOLanguage::EN_US;
    }

    MKT_NODISCARD static constexpr auto GetISOName( ISOLanguage language ) -> eastl::string_view {
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

    class LocalizationService final : public IService, public Singleton<LocalizationService> {
    public:
        explicit LocalizationService( const LocalizationServiceCreateInfo& createInfo );

        auto Initialize() -> void override;
        auto Shutdown() -> void override;

        auto SetLanguage( ISOLanguage language ) -> void;
        auto SetLanguage( const eastl::string &language ) -> void;

        MKT_NODISCARD auto GetDefaultLanguage() const -> ISOLanguage;
        MKT_NODISCARD auto GetCurrentLanguage() const -> ISOLanguage;

        MKT_NODISCARD auto IsCurrentLanguage(ISOLanguage language) const -> bool;
        MKT_NODISCARD auto IsDefaultLanguage(ISOLanguage language) const -> bool;

        MKT_NODISCARD auto HasKey( const eastl::string &key, ISOLanguage language ) const -> bool;
        MKT_NODISCARD auto Translate( const eastl::string &key ) const -> const eastl::string &;

        auto ParseLocalizationEntries(const Path &file) -> void;

    private:
        auto LoadAllLanguages() -> void;
        auto LoadDefaultLanguageFile( const Path &path ) -> void;

    private:
        ISOLanguage m_DefaultLanguage{ ISOLanguage::EN_US };
        ISOLanguage mCurrentLanguage{ ISOLanguage::EN_US };

        Path mLocDefaultPath{};

        // Entry -> Translation
        using TranslationMap = ankerl::unordered_dense::map<eastl::string, eastl::string>;

        ankerl::unordered_dense::map<ISOLanguage, TranslationMap> mTranslations{};
    };

    #define MKT_LOC(key) LocalizationService::Get()->Translate(key)
    #define MKT_IS_CURRENT_ISO(ISO) LocalizationService::Get()->IsCurrentLanguage(ISO)
    #define MKT_IS_CURRENT_ISO_STR(ISO_STR) LocalizationService::Get()->IsCurrentLanguage(InferISO(ISO_STR))
}

#endif //MIKOTO_LOCALIZATION_SERVICE_HH