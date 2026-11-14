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

#ifndef MIKOTO_NETWORKING_SOCKET_HH
#define MIKOTO_NETWORKING_SOCKET_HH

#include <EASTL/string.h>
#include <EASTL/functional.h>
#include <EASTL/string_view.h>

#include <Core/Core.hh>
#include <Core/Types.hh>
#include <Core/ReferenceCounted.hh>

namespace mikoto::network {

    /** @brief Security applied when establishing the transport connection. */
    enum class SecurityProtocol : core::u8 {
        eNone, ///< Plaintext transport.
        eTls   ///< TLS with server certificate and hostname verification.
    };

    /** @brief Locally tracked connection state; not a probe of the remote peer. */
    enum class ConnectionStatus : core::u8 {
        ePending,      ///< Name resolution, transport connection or TLS handshake is in progress.
        eConnected,    ///< The connection attempt completed successfully.
        eDisconnected, ///< Initial state, or explicitly disconnected.
        eFailed        ///< A connection or transfer operation reported an error.
    };

    /**
     * @brief Reference-counted client interface for exchanging a stream of bytes.
     *
     * Retain ownership through SocketHandle. The transport does not define message
     * boundaries or add terminators; callers provide their own protocol framing.
     * Synchronous operations have no timeout parameter and can block the caller.
     *
     * Serialize transport operations and borrowed-string access with asynchronous
     * handlers. Connection-status queries provide a snapshot, not synchronization
     * for the remaining socket state.
     */
    class ISocket : public core::INonCopyable, public core::INonMoveable, public core::ReferenceCounted<ISocket> {
    public:
        /**
         * @brief Optional notification of a completed connection attempt.
         *
         * Receives true on success and a borrowed error description on failure.
         * Copy the error text if it must outlive the callback. Immediate rejection
         * may invoke this inline; other completions run on the networking context's
         * executor. Cancelled or superseded attempts may suppress notification.
         */
        using ConnectCallback = eastl::function<void( bool connected, eastl::string_view error )>;


        /**
         * @brief Cancels a pending connection, closes the transport and marks it disconnected.
         *
         * Has no effect on an already closed transport. Retains the last target
         * hostname and error text; pending connection callbacks may be suppressed.
         */
        virtual auto Disconnect() -> void = 0;


        /**
         * @brief Replaces the current connection and starts an asynchronous attempt.
         *
         * The networking context must dispatch handlers for the attempt to advance.
         * A pending connection is not ready for SendSync or ReceiveSync. For TLS,
         * success includes completion of the verified handshake.
         *
         * @param host Hostname or IP address without a URL scheme or path; copied before returning.
         * @param port Destination port; must be nonzero.
         * @param callback Optional completion handler, subject to ConnectCallback's lifetime and dispatch rules.
         */
        virtual auto ConnectAsync( eastl::string_view host, core::u16 port, ConnectCallback callback ) -> void = 0;


        /**
         * @brief Replaces the current connection and waits for resolution and connection.
         *
         * For TLS, also waits for the verified handshake. Use a worker thread when
         * blocking the application thread is unacceptable.
         *
         * @param host Hostname or IP address without a URL scheme or path; copied by the socket.
         * @param port Destination port; must be nonzero.
         * @return True when connected; false on failure or when the attempt cannot start.
         */
        MKT_NODISCARD virtual auto Connect( eastl::string_view host, core::u16 port ) -> bool = 0;


        /**
         * @brief Sends the view's bytes, including embedded zeros, without adding a terminator.
         * @param data Nonempty borrowed bytes, which must remain valid until this call returns.
         * @return True when all bytes were written; false under the same conditions as the buffer overload.
         */
        MKT_NODISCARD virtual auto SendSync( eastl::string_view data ) -> bool = 0;


        /**
         * @brief Writes the complete buffer to an established connection.
         *
         * A successful write does not acknowledge remote application processing.
         * Failure can occur after part of the buffer has already been transmitted.
         *
         * @param data Readable buffer, valid until the call returns; must not be null.
         * @param size Number of bytes to send; must be greater than zero.
         * @return True when all bytes were written; false for invalid arguments, a disconnected socket or a transfer error.
         */
        MKT_NODISCARD virtual auto SendSync( const void *data, core::usize size ) -> bool = 0;


        /**
         * @brief Receives up to maxSize bytes without appending a terminator.
         *
         * Reads may be shorter than the supplied capacity or a protocol message.
         * TcpSocket returns immediately when no plaintext TCP bytes are available;
         * its TLS read can block. A zero result alone does not prove disconnection,
         * and end-of-stream does not necessarily change the connection status.
         *
         * @param buffer Writable storage, valid until the call returns; must not be null.
         * @param maxSize Capacity in bytes; must be greater than zero.
         * @return Bytes received, or zero for no data, invalid arguments, end-of-stream or failure.
         */
        MKT_NODISCARD virtual auto ReceiveSync( void *buffer, core::usize maxSize ) -> core::usize = 0;


        /**
         * @brief Returns the hostname copied for the last started connection attempt.
         * @return Borrowed string, initially empty and retained after Disconnect.
         * Copy it before reconnecting or releasing the socket if it must remain available.
         */
        MKT_NODISCARD virtual auto GetHost() const -> const eastl::string & = 0;


        /**
         * @brief Returns the last recorded transport or handshake error.
         *
         * A new connection attempt clears the previous error when it starts;
         * Disconnect preserves it. Early rejection may leave the text empty or
         * unchanged, so it is not a substitute for checking operation results.
         *
         * @return Borrowed string; copy it before another operation or socket destruction.
         */
        MKT_NODISCARD virtual auto GetLastError() const -> const eastl::string & = 0;


        /**
         * @brief Returns a snapshot of the locally tracked connection state.
         * @return Current state; eConnected does not guarantee the peer is still reachable.
         */
        MKT_NODISCARD virtual auto GetConnectionStatus() const -> ConnectionStatus = 0;


        /**
         * @brief Tests the current connection-state snapshot against the requested value.
         * @param status State to compare with GetConnectionStatus().
         * @return True when the states match.
         */
        MKT_NODISCARD virtual auto IsConnectionStatus( ConnectionStatus status ) const -> bool = 0;


        /**
         * @brief Tests whether the local state is ConnectionStatus::eConnected.
         * @return True after connection success until a local operation changes the state.
         */
        MKT_NODISCARD virtual auto IsConnected() const -> bool = 0;
    };

    /** @brief Shared intrusive ownership of a socket through its transport-independent interface. */
    using SocketHandle = core::Ref<ISocket>;

}// namespace mikoto::network

#endif// MIKOTO_NETWORKING_SOCKET_HH
