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

#ifndef MIKOTO_NETWORKING_CLIENT_HH
#define MIKOTO_NETWORKING_CLIENT_HH

#include <chrono>
#include <memory>

#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/string_view.h>

#include <Core/Types.hh>

#include <Networking/HttpResponse.hh>
#include <Networking/NetworkService.hh>

namespace mikoto::network {

    /** @brief HTTP/1.1 client supporting blocking requests and polled asynchronous GET requests.
     *
     * Use Get/Post on a worker thread; they require an initialized NetworkService
     * and have no deadline or response-size limit. GetAsync uses the service worker
     * (or NetworkService::PollTasks in manual mode). Call Poll to collect results.
     * Initialize NetworkService before starting requests. Client APIs and service
     * lifecycle operations run on the owning thread; getters reflect the last Poll.
     * Asynchronous HTTPS
     * verifies the certificate and host name when OpenSSL support is available.
     * Responses are collected until connection close; redirects are not followed.
     */
    class HttpClient final : core::INonCopyable {
    public:
        enum class Status {
            Idle,
            Connecting,
            Sending,
            Receiving,
            Complete,
            Failed,
            Cancelled
        };

        /** @brief Configures a client without connecting to the server.
         *
         * @param url HTTP or HTTPS URL, including an optional port and request target.
         */
        explicit HttpClient( eastl::string_view url );


        /** @brief Configures an endpoint with a default request target of '/'.
         *
         * @param host Server hostname or unbracketed IP address.
         * @param port Nonzero server port.
         * @param security Plain TCP or verified TLS transport.
         */
        HttpClient( eastl::string_view host, core::u16 port, SecurityProtocol security = SecurityProtocol::eNone );


        /** @brief Cancels pending asynchronous I/O and releases owned connections. */
        ~HttpClient();


        /** @brief Starts a nonblocking GET, including DNS, connection and optional TLS handshake.
         *
         * @param path Request target, or empty to use the constructor URL's target.
         * @param timeout Total request deadline; must be positive.
         * @param maxResponseBytes Maximum raw response size, including headers; must be positive.
         */
        auto GetAsync(
                eastl::string_view path = {},
                std::chrono::milliseconds timeout = std::chrono::seconds{ 5 },
                core::usize maxResponseBytes = 1024 * 1024 ) -> void;


        /** @brief Collects progress and completed results from the networking worker.
         * Call on the owning thread. Terminal requests remain available through the getters.
         */
        auto Poll() -> void;


        /** @brief Schedules cancellation and transport closure on the networking executor.
         * Completed, failed and already-cancelled requests are unchanged.
         * Call Poll to observe the cancellation result.
         */
        auto Cancel() -> void;


        /** @brief Checks whether an asynchronous request still needs polling.
         *
         * @return True while connecting, sending or receiving.
         */
        MKT_NODISCARD auto IsPending() const -> bool;


        /** @brief Reads asynchronous request progress.
         *
         * @return Current status, or Idle before the first asynchronous request.
         */
        MKT_NODISCARD auto GetStatus() const -> Status;


        /** @brief Returns the latest asynchronous response; HTTP errors such as 404 are valid responses.
         *
         * @return Client-owned response, valid until the next GetAsync or destruction.
         * Status '0' denotes no valid response. Blocking Get/Post return their results directly.
         */
        MKT_NODISCARD auto GetResponse() const -> const HttpResponse &;


        /** @brief Reads the asynchronous transport, validation or timeout error.
         *
         * @return Client-owned diagnostic string; empty when no error was recorded.
         */
        MKT_NODISCARD auto GetError() const -> const eastl::string &;


        /** @brief Reports buffered asynchronous response bytes, including headers and transfer framing.
         *
         * @return Accepted raw byte count, bounded by maxResponseBytes.
         */
        MKT_NODISCARD auto GetReceivedBytes() const -> core::usize;


        /** @brief Measures asynchronous request duration, frozen on completion, failure or cancellation.
         *
         * @return Elapsed milliseconds, or zero before a request starts.
         */
        MKT_NODISCARD auto GetElapsedMilliseconds() const -> double;


        /** @brief Performs a blocking GET; intended for worker-thread use.
         *
         * @param path Request target, or empty to use the constructor URL's target.
         * @param contentType Optional Content-Type request header.
         * @return Parsed response, with status '0' on failure.
         */
        MKT_NODISCARD auto Get( eastl::string_view path, eastl::string_view contentType = {} ) -> HttpResponse;


        /** @brief Performs a blocking POST; intended for worker-thread use.
         *
         * @param path Request target, or empty to use the constructor URL's target.
         * @param body Request payload; embedded null bytes are preserved.
         * @param contentType Payload MIME type.
         * @return Parsed response, with status '0' on failure.
         */
        MKT_NODISCARD auto Post( eastl::string_view path, eastl::string_view body, eastl::string_view contentType = "application/octet-stream" ) -> HttpResponse;


        /** @brief Checks endpoint syntax and availability of the requested transport security.
         *
         * @return True for a usable configuration; does not check server reachability.
         */
        MKT_NODISCARD auto IsValid() const -> bool;


    private:
        /** @brief Sends a blocking request and reads until the server closes the connection.
         *
         * @param request Complete serialized HTTP request.
         * @return Parsed response, or status '0' on failure.
         */
        MKT_NODISCARD auto SendRawRequest( eastl::string_view request ) -> HttpResponse;


        /** @brief Establishes the blocking transport when necessary.
         *
         * @return True when a connected socket is available.
         */
        MKT_NODISCARD auto EnsureSocket() -> bool;


        /** @brief Serializes an HTTP/1.1 request with Connection: close.
         *
         * @param method Request verb.
         * @param path Request target, or empty to use the configured target.
         * @param body Payload bytes.
         * @param contentType Optional payload MIME type.
         * @return Serialized request, or an empty string for an invalid target.
         */
        MKT_NODISCARD auto BuildRequest(
            eastl::string_view method,
            eastl::string_view path,
            eastl::string_view body,
            eastl::string_view contentType ) const -> eastl::string;

    private:
        struct AsyncRequest;
        std::shared_ptr<AsyncRequest> mAsyncRequest{};

        SocketHandle mSocket{};

        // Public results belong exclusively to the calling thread.
        HttpResponse mResponse{};
        eastl::string mError{};
        core::usize mReceivedBytes{};
        double mElapsedMilliseconds{};

        core::u16 mPort{};
        eastl::string mHost{};
        eastl::string mDefaultTarget{ "/" };

        Status mStatus{ Status::Idle };
        SecurityProtocol mSecurity{ SecurityProtocol::eNone };

        bool mIsValid{};
    };
}// namespace mikoto::network

#endif// MIKOTO_NETWORKING_CLIENT_HH
