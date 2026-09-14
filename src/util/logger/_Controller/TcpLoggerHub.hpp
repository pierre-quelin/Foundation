/**
 * Copyright (c) 2021–2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file TcpLoggerHub.hpp
 * @brief Single TCP port: live log stream + logger control commands.
 */
#pragma once

#include "tools/os/socket/Runtime.hpp"
#include "tools/os/socket/TcpSocket.hpp"
#include "tools/os/thread/Thread.h"
#include "util/logger/ILoggerCtrl.hpp"
#include "util/logger/Logger.hpp"

#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace util
{
namespace logger
{

/**
 * @brief spdlog sink + @c ILoggerCtrl on one listening port.
 *
 * Each client receives live traces and can issue control commands (e.g. @c sll).
 * Connect with telnet/netcat, e.g. @c telnet localhost 8023
 */
class TcpLoggerHub : public spdlog::sinks::base_sink<std::mutex>, public ILoggerCtrl
{
public:
    static constexpr std::uint16_t DefaultPort = 8023;

    explicit TcpLoggerHub(std::uint16_t port = DefaultPort);
    ~TcpLoggerHub() override;

    TcpLoggerHub(const TcpLoggerHub&)            = delete;
    TcpLoggerHub& operator=(const TcpLoggerHub&) = delete;

    /** @brief Enables command processing (optional until set). */
    void setLogService(std::shared_ptr<LogService> logs);

    [[nodiscard]] std::uint16_t port() const { return _port; }
    [[nodiscard]] std::size_t clientCount() const;

    void start() override;
    void stop() override;
    [[nodiscard]] bool isRunning() const override;

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
    void flush_() override;

private:
    struct Session;

    void acceptLoop();
    void runSession(const std::shared_ptr<Session>& session);
    void removeSession(const std::shared_ptr<Session>& session);
    void broadcast(const char* data, std::size_t size);
    [[nodiscard]] bool sendToSession(Session& session, std::string_view data);

    tools::os::socket::Runtime _socketRuntime;
    std::uint16_t _port;
    std::shared_ptr<LogService> _logs;
    std::atomic<bool> _running{false};
    std::unique_ptr<tools::os::thread::Thread> _acceptThread;
    tools::os::socket::TcpSocket _listener;
    mutable std::mutex _sessionsMutex;
    std::vector<std::shared_ptr<Session>> _sessions;
};

namespace sinks
{

/** @return TCP logger hub sink (stream + commands; call setLogService after LogService exists). */
inline spdlog::sink_ptr tcpStream(std::uint16_t port = TcpLoggerHub::DefaultPort)
{
    return std::make_shared<TcpLoggerHub>(port);
}

} // namespace sinks
} // namespace logger
} // namespace util
