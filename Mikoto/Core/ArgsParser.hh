//
// Created by zanet on 1/25/2025.
//

#ifndef COMMANDLINEPARSER_HH
#define COMMANDLINEPARSER_HH

#include <ranges>

#include <Common/Common.hh>

namespace Mikoto {
    struct ArgsParserCreateInfo {
        std::string_view Description{};
        std::string_view ProgramName{};
    };

    enum class ArgumentType {
        ARGUMENT_TYPE_INT,
        ARGUMENT_TYPE_CHAR,
        ARGUMENT_TYPE_FLOAT,
        ARGUMENT_TYPE_STRING
    };

    class ArgsParser final {
    public:
        struct Command {
            std::string Parameter{};
            std::string Description{};

            bool IsRequired{ false };

            std::function<void()> Action{};

            ArgumentType Type{ ArgumentType::ARGUMENT_TYPE_STRING };
        };

        explicit ArgsParser(const ArgsParserCreateInfo& createInfo)
            : m_Description{ createInfo.Description }, m_ProgramName{ createInfo.ProgramName } {}

        auto Emplace(const Command& command) -> void {
            if ( auto [it, success]{ m_Commands.try_emplace( command.Parameter, command ) }; !success ) {
                MKT_CORE_LOGGER_WARN( "ArgsParser:Emplace - Command already exists." );
            }
        }

        auto Execute(const std::string_view parameter) -> void {
            if ( auto commandIt{ m_Commands.find( parameter.data() ) }; commandIt != m_Commands.end() ) {
                commandIt->second.Action();
            }
        }

        auto Validate(Int32_T argc, char** argv) -> bool {
            // TODO: Implement the validation logic
            return true;
        }

        static auto Create(const ArgsParserCreateInfo& createInfo) -> Scope_T<ArgsParser> {
            return CreateScope<ArgsParser>( createInfo );
        }

    private:

        static auto Validate(const std::string_view command) -> bool {
            return command.starts_with( "--" ) ||
                std::ranges::any_of( command, []( const auto c ) { return std::isspace( c ); } );
        }
    private:
        std::string m_ProgramName{};
        std::string m_Description{};

        std::unordered_map<std::string, Command> m_Commands{};
    };
}

#endif //COMMANDLINEPARSER_HH
