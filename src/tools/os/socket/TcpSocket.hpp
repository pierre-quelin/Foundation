/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file TcpSocket.hpp
 * @brief TCP socket wrapper.
 */
#pragma once

#include "tools/os/socket/Socket.hpp"

#include <cstdint>

namespace tools
{
namespace os
{
namespace socket
{

/** @brief TCP stream socket (listen / accept / connected endpoints). */
class TcpSocket : public Socket
{
public:
    /** @return TCP listener on @p port, or invalid socket on failure. */
    [[nodiscard]] static TcpSocket listen(std::uint16_t port, int backlog = 8);

    /** @return Accepted client, or invalid socket on failure. */
    [[nodiscard]] TcpSocket accept() const;

    void shutdownBoth();

private:
    using Socket::Socket;
};

} // namespace socket
} // namespace os
} // namespace tools