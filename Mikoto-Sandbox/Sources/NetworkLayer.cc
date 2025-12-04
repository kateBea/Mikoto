//
// Created by kate on 10/30/25.
//

#include "NetworkLayer.hh"

#include <imgui.h>

#include <Networking/NetworkService.hh>
#include <Networking/NetworkUtilities.hh>
#include <Threading/TaskService.hh>
#include <array>
#include <nlohmann/json.hpp>
#include <string>

namespace Mikoto {

    static auto HttpGet( SocketHandle socket, const std::string &address, const std::string &path ) -> std::string {
        // Build HTTP GET request using fmt
        // Use keep-alive
        // If we use close server will probably close the connection so we can no longer keep reading
        const std::string request = fmt::format(
                "GET {} HTTP/1.1\r\n"
                "Host: {}\r\n"
                "User-Agent: MikotoSandbox/1.0\r\n"
                "Accept: application/json\r\n"
                "Connection: close\r\n"
                "\r\n",
                path, address );

        // avoid compiler warning void cast
        ( void )socket->SendSync( request.c_str(), request.size() );

        std::string response{};
        std::array<char, 1024> buffer{};
        Size readCharsCount{};

        do {
            readCharsCount = socket->ReceiveSync( buffer.data(), buffer.size() );
            response.append( buffer.data(), readCharsCount );
        } while ( readCharsCount > 0 );

        return response;
    }

    static auto HttpGetImgage( SocketHandle socket, const std::string &address, const std::string &path ) -> std::string {
        // Build HTTP GET request using fmt
        // Use keep-alive
        // If we use close server will probably close the connection so we can no longer keep reading
        std::string request;
        request += "GET " + path + " HTTP/1.1\r\n";
        request += "Host: " + address + "\r\n";
        request += "User-Agent: MikotoSandbox/1.0\r\n";
        request += "Accept: image/*\r\n";
        request += "Connection: close\r\n";
        request += "\r\n";

        // avoid compiler warning void cast
        ( void )socket->SendSync( request.c_str(), request.size() );

        std::string response{};
        std::array<char, 1024> buffer{};
        Size readCharsCount{};

        do {
            readCharsCount = socket->ReceiveSync( buffer.data(), buffer.size() );
            response.append( buffer.data(), readCharsCount );
        } while ( readCharsCount > 0 );

        return response;
    }

    NetworkLayer::NetworkLayer( const std::string_view name )
        : ILayer{ name } {
    }
    auto NetworkLayer::DrawAnimeWindow() -> void {
        ImGui::Begin( "Anime Browser" );

        // Dropdown
        if ( !m_AnimeList.empty() ) {
            std::vector<const char *> titles{};
            for ( auto &a: m_AnimeList ) {
                titles.push_back( a.title.c_str() );
            }

            if ( ImGui::Combo( "Select Anime", &m_SelectedAnimeIndex, titles.data(), titles.size() ) ) {
                // Mock fetching selected anime
            }
        }

        // Display selected anime info
        if ( !m_SelectedAnimeJsons.empty() ) {

            for ( const auto &[id, contents ]: m_SelectedAnimeJsons ) {
                if ( contents.empty() || id != m_AnimeList[m_SelectedAnimeIndex].id  )
                    continue;

                try {
                    nlohmann::json root = nlohmann::json::parse( contents );

                    // Access the "data" object
                    if ( !root.contains( "data" ) )
                        continue;

                    const auto &data = root["data"];

                    // Title
                    if ( data.contains( "title" ) )
                        ImGui::Text( "Title: %s", data["title"].get<std::string>().c_str() );

                    // English title (optional)
                    if ( data.contains( "title_english" ) && !data["title_english"].is_null() )
                        ImGui::Text( "English Title: %s", data["title_english"].get<std::string>().c_str() );

                    // Synopsis (wrap text)
                    if ( data.contains( "synopsis" ) && !data["synopsis"].is_null() )
                        ImGui::TextWrapped( "Synopsis: %s", data["synopsis"].get<std::string>().c_str() );

                    // Episodes
                    if ( data.contains( "episodes" ) && !data["episodes"].is_null() )
                        ImGui::Text( "Episodes: %d", data["episodes"].get<int>() );

                    // Score and rank
                    if ( data.contains( "score" ) && !data["score"].is_null() )
                        ImGui::Text( "Score: %.2f", data["score"].get<double>() );

                    if ( data.contains( "rank" ) && !data["rank"].is_null() )
                        ImGui::Text( "Rank: %d", data["rank"].get<int>() );

                    // Genres
                    if ( data.contains( "genres" ) && data["genres"].is_array() ) {
                        std::string genreStr;
                        for ( const auto &g: data["genres"] )
                            genreStr += g["name"].get<std::string>() + ", ";

                        if ( !genreStr.empty() )
                            genreStr.erase( genreStr.size() - 2 );// remove last comma
                        ImGui::Text( "Genres: %s", genreStr.c_str() );
                    }

                    // Studios
                    if ( data.contains( "studios" ) && data["studios"].is_array() ) {
                        std::string studiosStr;
                        for ( const auto &s: data["studios"] )
                            studiosStr += s["name"].get<std::string>() + ", ";

                        if ( !studiosStr.empty() )
                            studiosStr.erase( studiosStr.size() - 2 );
                        ImGui::Text( "Studios: %s", studiosStr.c_str() );
                    }

                    ImGui::Separator();// nice visual separator between entries

                } catch ( const std::exception &e ) {
                    ImGui::TextColored( ImVec4( 1, 0, 0, 1 ), "Failed to parse JSON: %s", e.what() );
                }
            }
        }

        ImGui::End();
    }

    auto NetworkLayer::OnUpdate( float deltaTime ) -> void {
        static bool localHostFirst{ true };
        if ( m_LocalHostSocket->IsConnected() && localHostFirst ) {
            ( void )m_LocalHostSocket->SendSync( "Hello world!" );
            localHostFirst = false;
        }

        DrawAnimeWindow();
    }

    auto NetworkLayer::OnCreate() -> void {
        m_LocalHostSocket = NetworkService::Get()->CreateSocket( SocketType::SOCKET_TCP, "localhost", 8000 );

        // Test getting image from web
        // Gotten from Jikan API https://api.jikan.moe/v4/anime/20/pictures
        auto values{ GetHost( "https://cdn.myanimelist.net/images/anime/1/20.jpg" ) };
        SocketHandle imageSocket{ NetworkService::Get()->CreateSocketHttps( values.first, true ) };

        if (!imageSocket.IsEmpty()) {
            auto result{ HttpGetImgage( imageSocket, imageSocket->GetHost(), "/images/anime/1141/142503.jpg" ) };
        }

        m_AnimeList = {
            { 1, "Cowboy Bebop" },
            { 20, "Naruto" },
            { 5114, "Fullmetal Alchemist: Brotherhood" },
            { 30276, "One Punch Man" },
        };

        UInt16 offset{ 1 };
        for ( const auto &[id, title]: m_AnimeList ) {
            // I first add the entry
            m_SelectedAnimeJsons[id] = "";

            // Create the task to load with new socket
            TaskService::Get()->Submit( [offset, this, &id, &title]() -> void {
                // To avoid 429, not sure the rate limit on jikan
                std::this_thread::sleep_for( std::chrono::seconds( 2 * offset ) );

                SocketHandle socket{ NetworkService::Get()->CreateSocketHttps( "api.jikan.moe", true ) };
                if ( socket.IsEmpty() ) {
                    return;
                }

                try {
                    const std::string response{ HttpGet( socket, socket->GetHost(), fmt::format( "/v4/anime/{}/full", id ) ) };

                    m_SelectedAnimeJsons[id] = GetHttpBody( response );

                    MKT_CORE_LOGGER_DEBUG( "Loaded successfully {}", title );
                } catch ( const std::exception &e ) {
                    MKT_CORE_LOGGER_ERROR( "Error processing anime {}. e.what(): {}", title, e.what() );
                }
            } );

            offset++;
        }
    }

    auto NetworkLayer::OnDestroy() -> void {
    }

    auto NetworkLayer::OnEvent( Event &event ) -> void {
    }
}// namespace Mikoto
