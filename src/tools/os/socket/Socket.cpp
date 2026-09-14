/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/os/socket/Socket.hpp"

#include "tools/os/socket/TcpSocket.hpp"
#include "tools/os/socket/UdpSocket.hpp"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
using native_socket_t                   = SOCKET;
constexpr native_socket_t InvalidNative = INVALID_SOCKET;
inline int platformLastError()
{
    return WSAGetLastError();
}
inline void platformClose(native_socket_t socket)
{
    closesocket(socket);
}
inline void platformShutdown(native_socket_t socket)
{
    shutdown(socket, SD_BOTH);
}
#else
#include <arpa/inet.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using native_socket_t                   = int;
constexpr native_socket_t InvalidNative = -1;
inline int platformLastError()
{
    return errno;
}
inline void platformClose(native_socket_t socket)
{
    close(socket);
}
inline void platformShutdown(native_socket_t socket)
{
    shutdown(socket, SHUT_RDWR);
}
#endif

namespace tools
{
namespace os
{
namespace socket
{

namespace
{

using Handle = Socket::Handle;

Handle fromNative(native_socket_t socket)
{
    return static_cast<Handle>(socket);
}

native_socket_t toNative(Handle handle)
{
    return static_cast<native_socket_t>(handle);
}

bool sendNative(native_socket_t socket, const char* data, std::size_t size)
{
    std::size_t remaining = size;
    const char* buffer    = data;
    while (remaining > 0)
    {
#if defined(_WIN32)
        const int sent = send(socket, buffer, static_cast<int>(remaining), 0);
#else
        const ssize_t sent = send(socket, buffer, remaining, MSG_NOSIGNAL);
#endif
        if (sent <= 0)
        {
            return false;
        }
        buffer += sent;
        remaining -= static_cast<std::size_t>(sent);
    }
    return true;
}

native_socket_t createUdpSocket()
{
    return ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}

bool setReuseAddr(native_socket_t socket)
{
    int reuse = 1;
    return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse)) == 0;
}

bool fillSockaddrIn(sockaddr_in& address, const char* host, std::uint16_t port)
{
    address                 = {};
    address.sin_family      = AF_INET;
    address.sin_port        = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (host == nullptr || host[0] == '\0')
    {
        return true;
    }

#if defined(_WIN32)
    return InetPtonA(AF_INET, host, &address.sin_addr) == 1;
#else
    return inet_pton(AF_INET, host, &address.sin_addr) == 1;
#endif
}

} // namespace

Socket::Socket(Handle handle) :
    _handle(handle)
{
}

Socket::Socket(Socket&& other) noexcept :
    _handle(other._handle)
{
    other._handle = Invalid;
}

Socket& Socket::operator=(Socket&& other) noexcept
{
    if (this != &other)
    {
        close();
        _handle       = other._handle;
        other._handle = Invalid;
    }
    return *this;
}

Socket::~Socket()
{
    close();
}

bool Socket::valid() const
{
#if defined(_WIN32)
    return _handle != Invalid;
#else
    return _handle >= 0;
#endif
}

bool Socket::sendAll(std::string_view data) const
{
    return sendAll(data.data(), data.size());
}

bool Socket::sendAll(const char* data, std::size_t size) const
{
    if (!valid())
    {
        return false;
    }
    return sendNative(toNative(_handle), data, size);
}

int Socket::recv(void* buffer, std::size_t size) const
{
    if (!valid())
    {
        return -1;
    }
#if defined(_WIN32)
    const int received =
        ::recv(toNative(_handle), static_cast<char*>(buffer), static_cast<int>(size), 0);
#else
    const ssize_t received = ::recv(toNative(_handle), buffer, size, 0);
#endif
    if (received == 0)
    {
        return 0;
    }
    if (received < 0)
    {
        return -1;
    }
    return static_cast<int>(received);
}

void Socket::close()
{
    if (valid())
    {
        platformClose(toNative(_handle));
        _handle = Invalid;
    }
}

int Socket::lastError()
{
    return platformLastError();
}

TcpSocket TcpSocket::listen(std::uint16_t port, int backlog)
{
    native_socket_t socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (socket == InvalidNative)
    {
        return TcpSocket{};
    }

    setReuseAddr(socket);

    sockaddr_in address{};
    address.sin_family      = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port        = htons(port);

    if (bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
        ::listen(socket, backlog) != 0)
    {
        platformClose(socket);
        return TcpSocket{};
    }

    return TcpSocket(fromNative(socket));
}

TcpSocket TcpSocket::accept() const
{
    if (!valid())
    {
        return TcpSocket{};
    }

    const native_socket_t client = ::accept(toNative(_handle), nullptr, nullptr);
    if (client == InvalidNative)
    {
        return TcpSocket{};
    }
    return TcpSocket(fromNative(client));
}

void TcpSocket::shutdownBoth()
{
    if (valid())
    {
        platformShutdown(toNative(_handle));
    }
}

UdpSocket UdpSocket::bind(std::uint16_t port)
{
    native_socket_t socket = createUdpSocket();
    if (socket == InvalidNative)
    {
        return UdpSocket{};
    }

    setReuseAddr(socket);

    sockaddr_in address{};
    if (!fillSockaddrIn(address, nullptr, port) ||
        ::bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        platformClose(socket);
        return UdpSocket{};
    }

    return UdpSocket(fromNative(socket));
}

UdpSocket UdpSocket::open()
{
    native_socket_t socket = createUdpSocket();
    if (socket == InvalidNative)
    {
        return UdpSocket{};
    }
    return UdpSocket(fromNative(socket));
}

int UdpSocket::sendTo(std::string_view data, const char* host, std::uint16_t port) const
{
    return sendTo(data.data(), data.size(), host, port);
}

int UdpSocket::sendTo(const char* data, std::size_t size, const char* host, std::uint16_t port) const
{
    if (!valid() || data == nullptr)
    {
        return -1;
    }

    sockaddr_in address{};
    if (!fillSockaddrIn(address, host, port))
    {
        return -1;
    }

#if defined(_WIN32)
    const int sent = ::sendto(toNative(_handle), data, static_cast<int>(size), 0, reinterpret_cast<sockaddr*>(&address), sizeof(address));
#else
    const ssize_t sent = ::sendto(toNative(_handle), data, size, 0, reinterpret_cast<sockaddr*>(&address), sizeof(address));
#endif
    if (sent < 0)
    {
        return -1;
    }
    return static_cast<int>(sent);
}

int UdpSocket::recvFrom(void* buffer, std::size_t size, char* hostOut, std::size_t hostOutSize, std::uint16_t* portOut) const
{
    if (!valid() || buffer == nullptr)
    {
        return -1;
    }

    sockaddr_in from{};
#if defined(_WIN32)
    int fromLen = static_cast<int>(sizeof(from));
    const int received =
        ::recvfrom(toNative(_handle), static_cast<char*>(buffer), static_cast<int>(size), 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
#else
    socklen_t fromLen = sizeof(from);
    const ssize_t received =
        ::recvfrom(toNative(_handle), buffer, size, 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
#endif
    if (received < 0)
    {
        return -1;
    }

    if (hostOut != nullptr && hostOutSize > 0)
    {
#if defined(_WIN32)
        InetNtopA(AF_INET, &from.sin_addr, hostOut, static_cast<DWORD>(hostOutSize));
#else
        inet_ntop(AF_INET, &from.sin_addr, hostOut, hostOutSize);
#endif
    }

    if (portOut != nullptr)
    {
        *portOut = ntohs(from.sin_port);
    }

    return static_cast<int>(received);
}

} // namespace socket
} // namespace os
} // namespace tools
