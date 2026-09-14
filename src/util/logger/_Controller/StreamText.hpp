/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
  * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file StreamText.hpp
 * @brief Text helpers for logger control streams.
 */
#pragma once

#include <string>
#include <string_view>

namespace tools
{
namespace os
{
namespace socket
{
class Socket;
}
} // namespace os
} // namespace tools

namespace util
{
namespace logger
{

/** @brief LF → CRLF for Telnet / TCP log stream clients (existing CRLF unchanged). */
[[nodiscard]] std::string normalizeCrLf(std::string_view data);

/** @brief Send line-oriented logger control or log stream text over a TCP socket. */
[[nodiscard]] bool sendStreamText(const tools::os::socket::Socket& socket, std::string_view data);

} // namespace logger
} // namespace util