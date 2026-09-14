/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/os/socket/Runtime.hpp"

#include <atomic>
#include <mutex>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#endif

namespace tools
{
namespace os
{
namespace socket
{

namespace
{

std::mutex g_runtimeMutex;
std::atomic<int> g_runtimeUsers{0};

} // namespace

Runtime::Runtime()
{
#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(g_runtimeMutex);
    if (g_runtimeUsers.fetch_add(1, std::memory_order_acq_rel) == 0)
    {
        WSADATA wsaData{};
        _initialized = (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0);
    }
    else
    {
        _initialized = true;
    }
#else
    _initialized = true;
#endif
}

Runtime::~Runtime()
{
#if defined(_WIN32)
    std::lock_guard<std::mutex> lock(g_runtimeMutex);
    if (g_runtimeUsers.fetch_sub(1, std::memory_order_acq_rel) == 1)
    {
        WSACleanup();
    }
#endif
}

} // namespace socket
} // namespace os
} // namespace tools
