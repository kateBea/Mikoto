//
// Created by zanet on 1/25/2025.
//

#ifndef COMMANDLINEPARSER_HH
#define COMMANDLINEPARSER_HH

#include <ranges>

#include <Common/Common.hh>

namespace Mikoto {
    class CommandLineParser {
    public:
        struct Command {
            std::string CommandDescription{};
            std::function<void()> CommandFunction{};
        };

        auto Insert(const std::string_view commandName, const std::string_view commandDescription, std::function<void()>&& commandFunction) -> bool {
            Command newCommand{ commandDescription.data(), std::move( commandFunction ) };
            auto [insertIt, inserted]{ m_Commands.try_emplace( commandName.data(), newCommand) };

            return inserted;
        }

        auto Execute(const std::string_view command) -> void {
            if ( auto commandIt{ m_Commands.find( command.data() ) }; commandIt != m_Commands.end() ) {
                commandIt->second.CommandFunction();
            }
        }

    private:

        static auto Validate(const std::string_view command) -> bool {
            return command.starts_with( "--" ) ||
                std::ranges::any_of( command, []( const auto c ) { return std::isspace( c ); } );
        }

        std::unordered_map<std::string, Command> m_Commands{};
    };
}

#endif //COMMANDLINEPARSER_HH
