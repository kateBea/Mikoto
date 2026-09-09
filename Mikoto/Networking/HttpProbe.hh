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


#ifndef MIKOTO_HTTP_PROBE_HH
#define MIKOTO_HTTP_PROBE_HH

#include <chrono>
#include <memory>

#include <Networking/NetworkUtilities.hh>

namespace mikoto::network {

    /**
     * A bounded, nonblocking HTTP GET for diagnosing a local server.
     * Call Poll on the owning thread each frame, even when the UI is hidden.
     * Supports localhost and IPv4/IPv6 literals over plaintext HTTP. It owns
     * its Asio context; no engine services, worker threads or socket pool are
     * needed. Destruction cancels outstanding I/O without waiting on a worker.
     */
    class HttpProbe final {
    public:
        enum class Status { Connecting, Sending, Receiving, Complete, Failed, Cancelled };

        explicit HttpProbe( eastl::string_view url,
                            std::chrono::milliseconds timeout = std::chrono::seconds{ 5 },
                            core::usize maxResponseBytes = 1024 * 1024 );
        ~HttpProbe();

        HttpProbe( const HttpProbe& ) = delete;
        auto operator=( const HttpProbe& ) -> HttpProbe& = delete;

        auto Poll() -> void;
        auto Cancel() -> void;

        MKT_NODISCARD auto IsPending() const -> bool;
        MKT_NODISCARD auto GetStatus() const -> Status;
        MKT_NODISCARD auto GetResponse() const -> const HttpResponse&;
        MKT_NODISCARD auto GetError() const -> const eastl::string&;
        MKT_NODISCARD auto GetUrl() const -> const eastl::string&;
        MKT_NODISCARD auto GetReceivedBytes() const -> core::usize;
        MKT_NODISCARD auto GetElapsedMilliseconds() const -> double;

    private:
        struct Impl;
        std::unique_ptr<Impl> mImpl;
    };
}

#endif // MIKOTO_HTTP_PROBE_HH
