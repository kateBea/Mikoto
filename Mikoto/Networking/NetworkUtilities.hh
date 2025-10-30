//
// Created by kate on 10/30/25.
//

#ifndef NETWORK_UTILITIES_HH
#define NETWORK_UTILITIES_HH

#include <string>
#include <string_view>

#include <Common/Common.hh>
#include <Library/Utility/Types.hh>

namespace Mikoto::Network {

    struct HttpResponse {
        std::string statusLine;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
    };

    class HttpRequestBuilder {
    public:

    };

    // Returns the host and the port
    MKT_NODISCARD auto GetHost(std::string_view apiResponse) -> std::pair<std::string, std::optional<std::string>>;

    MKT_NODISCARD auto GetHttpBody(std::string_view apiResponse) -> std::string;
    MKT_NODISCARD auto GetHttpResponse(std::string_view apiResponse) -> HttpResponse;

} // Mikoto

#endif
