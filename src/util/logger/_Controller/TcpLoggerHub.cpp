/**
 * Copyright (c) 2021–2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file TcpLoggerHub.cpp
 */

#include "util/logger/TcpLoggerHub.hpp"

#include "util/logger/LoggerCommands.hpp"
#include "util/logger/StreamText.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace util
{
namespace logger
{

namespace
{

using TcpSocket = tools::os::socket::TcpSocket;

bool sendTelnetOut(const TcpSocket& socket, const std::string& data)
{
    return socket.sendAll(normalizeCrLf(data));
}

bool redrawInputLine(const TcpSocket& socket, const std::string& prompt, const std::string& line)
{
    return socket.sendAll("\033[2K\r" + prompt + line);
}

bool sendTelnetOption(const TcpSocket& socket, unsigned char command, unsigned char option)
{
    const char bytes[3] = {static_cast<char>(255), static_cast<char>(command), static_cast<char>(option)};
    return socket.sendAll(bytes, 3);
}

void negotiateTelnet(const TcpSocket& socket, bool& serverEcho)
{
    (void)sendTelnetOption(socket, 252, 34); // WONT LINEMODE
    (void)sendTelnetOption(socket, 251, 1);  // WILL ECHO
    (void)sendTelnetOption(socket, 251, 3);  // WILL SGA
    serverEcho = true;
}

void handleTelnetOption(const TcpSocket& socket, unsigned char command, unsigned char option, bool& serverEcho)
{
    auto reply = [&](unsigned char replyCommand, unsigned char replyOption)
    {
        (void)sendTelnetOption(socket, replyCommand, replyOption);
    };

    if (command == 253) // DO
    {
        if (option == 1)
        {
            reply(251, 1);
            serverEcho = true;
        }
        else if (option == 3)
        {
            reply(251, 3);
        }
        else if (option == 34)
        {
            reply(252, 34);
        }
        else
        {
            reply(252, option);
        }
    }
    else if (command == 251) // WILL
    {
        if (option == 1)
        {
            reply(254, 1);
            serverEcho = true;
        }
        else if (option == 34)
        {
            reply(254, 34);
        }
        else
        {
            reply(254, option);
        }
    }
}

void historyUp(std::size_t& historyIndex, const std::vector<std::string>& history, std::string& line)
{
    if (historyIndex > 0)
    {
        --historyIndex;
        line = history[historyIndex];
    }
}

void historyDown(std::size_t& historyIndex, const std::vector<std::string>& history, std::string& line)
{
    if (historyIndex < history.size())
    {
        ++historyIndex;
        line = (historyIndex >= history.size()) ? std::string{} : history[historyIndex];
    }
}

bool readLine(const TcpSocket& socket, bool& connected, const std::string& prompt, std::vector<std::string>& history, std::string& line, bool& serverEcho)
{
    line.clear();
    connected = true;

    std::size_t historyIndex = history.size();

    enum class TelnetParse
    {
        Normal,
        Iac,
        IacOption,
        SubNeg,
        SubNegIac
    };
    TelnetParse telnetParse  = TelnetParse::Normal;
    unsigned char iacCommand = 0;

    enum class EscapeParse
    {
        Normal,
        Escape,
        Csi,
        Ss3
    };
    EscapeParse escapeParse = EscapeParse::Normal;
    enum class LeadIn
    {
        None,
        Nul,
        Ext
    };
    LeadIn leadIn = LeadIn::None;

    auto applyHistory = [&]() -> bool
    {
        return redrawInputLine(socket, prompt, line);
    };

    auto handleArrow = [&](unsigned char arrowByte)
    {
        if (arrowByte == 0x48 || arrowByte == 'H')
        {
            historyUp(historyIndex, history, line);
            applyHistory();
        }
        else if (arrowByte == 0x50 || arrowByte == 'P')
        {
            historyDown(historyIndex, history, line);
            applyHistory();
        }
    };

    auto echoBackspace = [&]() -> bool
    {
        return serverEcho && socket.sendAll("\b \b");
    };

    auto echoChar = [&](char ch) -> bool
    {
        return !serverEcho || socket.sendAll(std::string(1, ch));
    };

    std::array<char, 1> byte{};

    while (true)
    {
        const int received = socket.recv(byte.data(), byte.size());
        if (received <= 0)
        {
            connected = false;
            return false;
        }

        const unsigned char c = static_cast<unsigned char>(byte[0]);

        if (telnetParse != TelnetParse::Normal)
        {
            switch (telnetParse)
            {
                case TelnetParse::Iac:
                    if (c == 255)
                    {
                        telnetParse = TelnetParse::Normal;
                        break;
                    }
                    if (c == 250)
                    {
                        telnetParse = TelnetParse::SubNeg;
                        break;
                    }
                    if (c == 240)
                    {
                        telnetParse = TelnetParse::Normal;
                        break;
                    }
                    if (c == 251 || c == 252 || c == 253 || c == 254)
                    {
                        iacCommand  = c;
                        telnetParse = TelnetParse::IacOption;
                        break;
                    }
                    telnetParse = TelnetParse::Normal;
                    break;
                case TelnetParse::IacOption:
                    handleTelnetOption(socket, iacCommand, c, serverEcho);
                    telnetParse = TelnetParse::Normal;
                    break;
                case TelnetParse::SubNeg:
                    if (c == 255)
                    {
                        telnetParse = TelnetParse::SubNegIac;
                    }
                    break;
                case TelnetParse::SubNegIac:
                    telnetParse = (c == 240) ? TelnetParse::Normal : TelnetParse::SubNeg;
                    break;
                default:
                    telnetParse = TelnetParse::Normal;
                    break;
            }
            continue;
        }

        if (c == 255)
        {
            telnetParse = TelnetParse::Iac;
            continue;
        }

        if (escapeParse != EscapeParse::Normal)
        {
            switch (escapeParse)
            {
                case EscapeParse::Escape:
                    if (c == '[')
                    {
                        escapeParse = EscapeParse::Csi;
                    }
                    else if (c == 'O')
                    {
                        escapeParse = EscapeParse::Ss3;
                    }
                    else
                    {
                        escapeParse = EscapeParse::Normal;
                    }
                    break;
                case EscapeParse::Csi:
                    if (c >= 0x40 && c <= 0x7E)
                    {
                        if (c == 'A')
                        {
                            historyUp(historyIndex, history, line);
                            applyHistory();
                        }
                        else if (c == 'B')
                        {
                            historyDown(historyIndex, history, line);
                            applyHistory();
                        }
                        escapeParse = EscapeParse::Normal;
                    }
                    break;
                case EscapeParse::Ss3:
                    if (c == 'A')
                    {
                        historyUp(historyIndex, history, line);
                        applyHistory();
                    }
                    else if (c == 'B')
                    {
                        historyDown(historyIndex, history, line);
                        applyHistory();
                    }
                    escapeParse = EscapeParse::Normal;
                    break;
                default:
                    escapeParse = EscapeParse::Normal;
                    break;
            }
            continue;
        }

        if (c == 0x1B)
        {
            escapeParse = EscapeParse::Escape;
            continue;
        }

        if (leadIn == LeadIn::Nul)
        {
            leadIn = LeadIn::None;
            if (c == 0x48 || c == 0x50 || c == 'H' || c == 'P')
            {
                handleArrow(c);
                continue;
            }
        }
        else if (leadIn == LeadIn::Ext)
        {
            leadIn = LeadIn::None;
            continue;
        }

        if (c == 0)
        {
            leadIn = LeadIn::Nul;
            continue;
        }
        if (c == 0xE0)
        {
            leadIn = LeadIn::Ext;
            continue;
        }

        if (c == '\r')
        {
            continue;
        }
        if (c == '\n')
        {
            if (serverEcho)
            {
                (void)socket.sendAll("\r\n");
            }
            return true;
        }
        if (c == 0x7F || c == 0x08)
        {
            if (!line.empty())
            {
                line.pop_back();
                (void)echoBackspace();
            }
            continue;
        }
        if (c >= 32 && c < 127)
        {
            line.push_back(static_cast<char>(c));
            (void)echoChar(static_cast<char>(c));
        }
    }
}

} // namespace

struct TcpLoggerHub::Session
{
    TcpSocket socket;
    std::mutex writeMutex;
    std::unique_ptr<tools::os::thread::Thread> thread;
};

TcpLoggerHub::TcpLoggerHub(std::uint16_t port) : _port(port)
{
    start();
}

TcpLoggerHub::~TcpLoggerHub()
{
    stop();
}

void TcpLoggerHub::setLogService(std::shared_ptr<LogService> logs)
{
    std::lock_guard<std::mutex> lock(_sessionsMutex);
    _logs = std::move(logs);
}

std::size_t TcpLoggerHub::clientCount() const
{
    std::lock_guard<std::mutex> lock(_sessionsMutex);
    return _sessions.size();
}

void TcpLoggerHub::start()
{
    if (_running.load())
    {
        return;
    }

    if (!_socketRuntime.isInitialized())
    {
        std::cerr << "TcpLoggerHub: socket runtime init failed (error "
                  << tools::os::socket::Socket::lastError() << ")\n";
        return;
    }

    _listener = TcpSocket::listen(_port, 8);
    if (!_listener.valid())
    {
        std::cerr << "TcpLoggerHub: bind() failed on port " << _port << " (error "
                  << tools::os::socket::Socket::lastError() << ")\n";
        return;
    }

    _running.store(true);
    _acceptThread = std::make_unique<tools::os::thread::Thread>(
        [this]()
        { acceptLoop(); },
        "TcpLoggerHub.Accept");
    _acceptThread->start();
    std::cerr << "TcpLoggerHub: listening on port " << _port
              << " (logs + commands; connect: telnet/nc 127.0.0.1 " << _port << ")\n";
}

void TcpLoggerHub::stop()
{
    if (!_running.exchange(false))
    {
        return;
    }

    _listener.shutdownBoth();
    _listener.close();

    std::vector<std::shared_ptr<Session>> sessions;
    {
        std::lock_guard<std::mutex> lock(_sessionsMutex);
        sessions.swap(_sessions);
    }
    for (auto& session : sessions)
    {
        if (session)
        {
            session->socket.shutdownBoth();
            session->socket.close();
        }
    }

    if (_acceptThread)
    {
        _acceptThread->join();
        _acceptThread.reset();
    }

    for (auto& session : sessions)
    {
        if (session && session->thread)
        {
            session->thread->join();
            session->thread.reset();
        }
    }
}

bool TcpLoggerHub::isRunning() const
{
    return _running.load();
}

void TcpLoggerHub::acceptLoop()
{
    while (_running.load())
    {
        TcpSocket client = _listener.accept();
        if (!client.valid())
        {
            if (_running.load())
            {
                continue;
            }
            break;
        }

        auto session    = std::make_shared<Session>();
        session->socket = std::move(client);

        {
            std::lock_guard<std::mutex> lock(_sessionsMutex);
            _sessions.push_back(session);
        }

        session->thread = std::make_unique<tools::os::thread::Thread>(
            [this, session]()
            { runSession(session); },
            "TcpLoggerHub.Session");
        session->thread->start();
    }
}

void TcpLoggerHub::runSession(const std::shared_ptr<Session>& session)
{
    const std::string prompt = "> ";
    const std::string banner =
        "Logger hub ready on port " + std::to_string(_port) + ".\n"
                                                              "Live traces are streamed here. Type help for commands.\n" +
        prompt;

    bool serverEcho = false;
    negotiateTelnet(session->socket, serverEcho);

    if (!sendToSession(*session, banner))
    {
        removeSession(session);
        return;
    }

    std::vector<std::string> history;

    while (_running.load())
    {
        bool connected = true;
        std::string line;
        if (!readLine(session->socket, connected, prompt, history, line, serverEcho))
        {
            break;
        }

        if (line.empty())
        {
            if (!sendToSession(*session, prompt))
            {
                break;
            }
            continue;
        }

        if (history.empty() || history.back() != line)
        {
            history.push_back(line);
        }

        std::string upperLine = line;
        for (char& c : upperLine)
        {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }

        if (upperLine == "QUIT" || upperLine == "EXIT")
        {
            (void)sendToSession(*session, "Bye.\n");
            break;
        }

        std::shared_ptr<LogService> logs;
        {
            std::lock_guard<std::mutex> lock(_sessionsMutex);
            logs = _logs;
        }
        const std::string response =
            logs ? processLoggerCommand(*logs, line) : "LogService not configured.\n";
        if (!sendToSession(*session, response + '\n' + prompt))
        {
            break;
        }
    }

    removeSession(session);
}

void TcpLoggerHub::removeSession(const std::shared_ptr<Session>& session)
{
    std::lock_guard<std::mutex> lock(_sessionsMutex);
    _sessions.erase(std::remove(_sessions.begin(), _sessions.end(), session), _sessions.end());
}

bool TcpLoggerHub::sendToSession(Session& session, std::string_view data)
{
    std::lock_guard<std::mutex> lock(session.writeMutex);
    return sendTelnetOut(session.socket, std::string{data});
}

void TcpLoggerHub::broadcast(const char* data, std::size_t size)
{
    const std::string payload = normalizeCrLf(std::string_view(data, size));

    std::vector<std::shared_ptr<Session>> sessions;
    {
        std::lock_guard<std::mutex> lock(_sessionsMutex);
        sessions = _sessions;
    }

    for (auto& session : sessions)
    {
        if (!session)
        {
            continue;
        }
        std::lock_guard<std::mutex> lock(session->writeMutex);
        if (!session->socket.sendAll(payload))
        {
            session->socket.close();
        }
    }
}

void TcpLoggerHub::sink_it_(const spdlog::details::log_msg& msg)
{
    spdlog::memory_buf_t formatted;
    // pattern_formatter already appends its eol ("\n"); do not push another.
    formatter_->format(msg, formatted);
    broadcast(formatted.data(), formatted.size());
}

void TcpLoggerHub::flush_()
{
}

} // namespace logger
} // namespace util
