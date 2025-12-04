//
// Created by kate on 11/8/25.
//

#include <array>

#include <Logging/Logger.hh>
#include <Networking/Client.hh>

namespace Mikoto {
    HttpClient::HttpClient( std::string_view url ) {
        ParseUrl(url);
        InitSocket();
    }

    HttpClient::HttpClient( std::string_view host, UInt16 port, SecurityProtocol sp )
    : m_Host{ host }, m_Security{ sp }, m_Port{ port }
    {
        InitSocket();
    }

    auto HttpClient::Get( std::string_view path, std::string_view contentType ) -> HttpResponse {
        std::string req{};

        req += "GET ";
        req += path;
        req += " HTTP/1.1\r\n";

        req += "Host: " + m_Host + "\r\n";
        req += "User-Agent: Mikoto-HttpClient/1.0\r\n";
        req += "Accept: */*\r\n";

        req += "Content-Type: ";
        req += contentType;
        req += "\r\n";

        req += "Connection: close\r\n\r\n";

        return SendRawRequest(req);
    }

    auto HttpClient::Post( std::string_view path, std::string_view body, std::string_view contentType ) -> HttpResponse {
        std::string req{};

        req += "POST ";
        req += path;
        req += " HTTP/1.1\r\n";

        req += "Host: " + m_Host + "\r\n";
        req += "User-Agent: Mikoto-HttpClient/1.0\r\n";
        req += "Accept: */*\r\n";
        req += "Content-Type: ";
        req += contentType;
        req += "\r\n";

        req += "Content-Length: ";
        req += std::to_string(body.size());
        req += "\r\n";

        req += "Connection: close\r\n\r\n";

        req += body;

        return SendRawRequest(req);
    }

    auto HttpClient::SendRawRequest( const std::string_view raw ) -> HttpResponse {
        if (m_Socket.IsEmpty() || !m_Socket->IsConnected()) {
            MKT_CORE_LOGGER_ERROR("HttpClient::SendRawRequest – socket is not connected");
            return {};
        }

        // Send request
        m_Socket->SendSync(raw);

        std::string apiResponse{};
        std::array<char, 4096>buffer{};

        // Receive until EOF
        while (true) {
            const Size n{ m_Socket->ReceiveSync(buffer.data(), sizeof(buffer)) };
            if (n == 0) {
                break;
            }

            apiResponse.append(buffer.data(), n);
        }

        return GetHttpResponse(apiResponse);
    }

    auto HttpClient::ParseUrl( std::string_view url ) -> void {
        // Detect scheme
        if (url.starts_with("https://")) {
            m_Security = SecurityProtocol::TLS;
            url.remove_prefix(strlen("https://"));
            m_Port = 443;
        }
        else if (url.starts_with("http://")) {
            m_Security = SecurityProtocol::NONE;
            url.remove_prefix(strlen("http://"));
            m_Port = 80;
        }
        else {
            // Default: assume plain http://
            m_Security = SecurityProtocol::NONE;
            m_Port = 80;
        }

        // Extract host and port
        auto [host, port]{ GetHost(url) };
        m_Host = host;

        if (port.has_value()) {
            m_Port = static_cast<UInt16>(std::stoi(*port));
        }
    }

    auto HttpClient::InitSocket() -> void {
        switch (m_Security) {
            case SecurityProtocol::NONE:
                // if host and port specified
                if (m_Port != 0) {
                    m_Socket = NetworkService::Get()->CreateSocketSync( SocketType::SOCKET_TCP, m_Host, m_Port );
                } else {
                    m_Socket = NetworkService::Get()->CreateSocketHttp( m_Host, true );
                }
                break;
            case SecurityProtocol::TLS:
                m_Socket = NetworkService::Get()->CreateSocketHttps( m_Host, true );
                break;
        }
    }

    HttpClient::~HttpClient() {
        m_Socket.Reset();
    }
}