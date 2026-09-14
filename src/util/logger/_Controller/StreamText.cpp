/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
*/

#include "util/logger/StreamText.hpp"

#include "tools/os/socket/Socket.hpp"

namespace util
{
namespace logger
{

std::string normalizeCrLf(std::string_view data)
{
    std::string normalized;
    normalized.reserve(data.size() + 16);
    for (std::size_t i = 0; i < data.size(); ++i)
    {
        if (data[i] == '\n' && (i == 0 || data[i - 1] != '\r'))
        {
            normalized += "\r\n";
        }
        else
        {
            normalized += data[i];
        }
    }
    return normalized;
}

bool sendStreamText(const tools::os::socket::Socket& socket, std::string_view data)
{
    return socket.sendAll(normalizeCrLf(data));
}

} // namespace logger
} // namespace util
