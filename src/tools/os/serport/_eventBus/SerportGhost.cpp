/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SerportGhost.cpp
 * @brief Remote ISerport client.
 */
#include "tools/os/serport/SerportGhost.hpp"

#include "tools/design/config/Reference.hpp"
#include "tools/design/factory/Register.hpp"
#include "tools/design/ipc/IpcException.hpp"
#include "tools/design/ipc/LinkState.hpp"
#include "tools/os/thread/Thread.h"
#include "util/chrono/Delay.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace tools::os::serport
{
namespace
{

using util::chrono::literals::operator""_ms;

[[nodiscard]] tools::design::ipc::IEventBus& requireEventBus(tools::design::ApplicationServices& app)
{
    if (app.eventBus == nullptr)
    {
        throw std::logic_error("SerportGhost: ApplicationServices::eventBus not wired");
    }
    return *app.eventBus;
}

} // namespace

SerportGhost::SerportGhost(tools::design::ApplicationServices& app, tools::design::config::Node node) : _bus(requireEventBus(app)), _transport(_bus.transport()), _topics(_bus.topicScheme(), tools::design::config::instancePath(node.path()))
{
}

SerportGhost::~SerportGhost()
{
    try
    {
        stop();
    }
    catch (...)
    {
    }
}

bool SerportGhost::busOnline() const
{
    return _bus.linkState() == tools::design::ipc::LinkState::Online &&
           _transport.isConnected();
}

std::string SerportGhost::nextCorr()
{
    return std::to_string(_corr.fetch_add(1));
}

void SerportGhost::launch()
{
    start();
}

void SerportGhost::start()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_started)
        {
            return;
        }
        _started = true;
    }

    const auto rx = _transport.subscribe(_topics.rx(), [this](const tools::design::ipc::Envelope& env)
                                         { onRx(env); });
    const auto rep =
        _transport.subscribe(_topics.rep(), [this](const tools::design::ipc::Envelope& env)
                             { onRep(env); });
    const auto status =
        _transport.subscribe(_topics.status(), [this](const tools::design::ipc::Envelope& env)
                             { onStatus(env); });

    std::lock_guard<std::mutex> lock(_mutex);
    _rxSub     = rx;
    _repSub    = rep;
    _statusSub = status;
}

void SerportGhost::stop()
{
    tools::design::ipc::SubscriptionId rx     = 0;
    tools::design::ipc::SubscriptionId rep    = 0;
    tools::design::ipc::SubscriptionId status = 0;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_started)
        {
            return;
        }
        rx         = _rxSub;
        rep        = _repSub;
        status     = _statusSub;
        _rxSub     = 0;
        _repSub    = 0;
        _statusSub = 0;
        _started   = false;
        _ready     = false;
        _rx.clear();
    }
    if (rx != 0)
    {
        _transport.unsubscribe(rx);
    }
    if (rep != 0)
    {
        _transport.unsubscribe(rep);
    }
    if (status != 0)
    {
        _transport.unsubscribe(status);
    }
}

void SerportGhost::onRx(const tools::design::ipc::Envelope& env)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _rx.insert(_rx.end(), env.payload.begin(), env.payload.end());
}

void SerportGhost::onStatus(const tools::design::ipc::Envelope& env)
{
    try
    {
        const auto j     = util::json::Json::parse(env.payload);
        const bool ready = j.value("ready", false);
        std::lock_guard<std::mutex> lock(_mutex);
        _ready = ready;
    }
    catch (...)
    {
    }
}

void SerportGhost::onRep(const tools::design::ipc::Envelope& env)
{
    try
    {
        const auto j           = util::json::Json::parse(env.payload);
        const std::string corr = j.value("corr", "");
        if (corr.empty())
        {
            return;
        }
        std::shared_ptr<PendingCmd> pending;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            const auto it = _pending.find(corr);
            if (it == _pending.end())
            {
                return;
            }
            pending = it->second;
        }
        {
            std::lock_guard<std::mutex> lock(pending->mutex);
            pending->ok   = j.value("ok", false);
            pending->body = env.payload;
            pending->done = true;
        }
        pending->cv.notify_all();
    }
    catch (...)
    {
    }
}

bool SerportGhost::waitCmd(const std::string& corr,
                           util::chrono::Delay timeout,
                           std::string* bodyOut)
{
    std::shared_ptr<PendingCmd> pending;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        const auto it = _pending.find(corr);
        if (it == _pending.end())
        {
            return false;
        }
        pending = it->second;
    }

    std::unique_lock<std::mutex> lock(pending->mutex);
    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::duration_cast<std::chrono::milliseconds>(timeout.toNanoseconds());
    while (!pending->done)
    {
        if (pending->cv.wait_until(lock, deadline) == std::cv_status::timeout)
        {
            break;
        }
    }
    const bool ok = pending->done && pending->ok;
    if (bodyOut != nullptr)
    {
        *bodyOut = pending->body;
    }

    std::lock_guard<std::mutex> mapLock(_mutex);
    _pending.erase(corr);
    return ok;
}

int SerportGhost::sendCmd(const util::json::Json& body, util::chrono::Delay timeout)
{
    if (!busOnline())
    {
        return -1;
    }

    const std::string corr = nextCorr();
    util::json::Json msg   = body;
    msg["corr"]            = corr;

    auto pending = std::make_shared<PendingCmd>();
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _pending[corr] = pending;
    }

    try
    {
        tools::design::ipc::Envelope env;
        env.topic   = _topics.cmd();
        env.payload = msg.dump();
        _transport.publish(env);
    }
    catch (const tools::design::ipc::IpcException&)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _pending.erase(corr);
        return -1;
    }

    return waitCmd(corr, timeout, nullptr) ? 0 : -1;
}

int SerportGhost::read(char* buffer, unsigned int size)
{
    return read(buffer, size, 0_ms);
}

int SerportGhost::read(char* buffer, unsigned int size, util::chrono::Delay delay)
{
    if (buffer == nullptr || size == 0)
    {
        return -1;
    }

    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::duration_cast<std::chrono::milliseconds>(delay.toNanoseconds());

    for (;;)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!_rx.empty())
            {
                const unsigned int n =
                    static_cast<unsigned int>(std::min<std::size_t>(_rx.size(), size));
                for (unsigned int i = 0; i < n; ++i)
                {
                    buffer[i] = _rx.front();
                    _rx.pop_front();
                }
                return static_cast<int>(n);
            }
        }
        if (std::chrono::steady_clock::now() >= deadline)
        {
            return 0;
        }
        tools::os::thread::Thread::sleep_for(5_ms);
    }
}

int SerportGhost::write(const char* buffer, unsigned int size)
{
    if (!busOnline() || buffer == nullptr)
    {
        return -1;
    }
    try
    {
        tools::design::ipc::Envelope env;
        env.topic = _topics.tx();
        env.payload.assign(buffer, buffer + size);
        _transport.publish(env);
        return static_cast<int>(size);
    }
    catch (const tools::design::ipc::IpcException&)
    {
        return -1;
    }
}

int SerportGhost::getNRead()
{
    std::lock_guard<std::mutex> lock(_mutex);
    return static_cast<int>(_rx.size());
}

int SerportGhost::getNWrite()
{
    return 0;
}

int SerportGhost::flush()
{
    return sendCmd(util::json::Json{{"op", "flush"}}, 500_ms);
}

int SerportGhost::wflush()
{
    return sendCmd(util::json::Json{{"op", "wflush"}}, 500_ms);
}

int SerportGhost::rflush()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _rx.clear();
    }
    return sendCmd(util::json::Json{{"op", "rflush"}}, 500_ms);
}

int SerportGhost::cancel()
{
    return 0;
}

int SerportGhost::setParams(BitRate bRate, DataBit nData, Parity parity, StopBit nStop)
{
    _bitrate = bRate;
    _databit = nData;
    _parity  = parity;
    _stopBit = nStop;
    return sendCmd(util::json::Json{{"op", "setParams"},
                                    {"bitrate", static_cast<int>(bRate)},
                                    {"databit", static_cast<int>(nData)},
                                    {"parity", static_cast<int>(parity)},
                                    {"stopbit", static_cast<int>(nStop)}},
                   500_ms);
}

int SerportGhost::setFlowCtrl(FlowCtrl flowCtrl)
{
    _flowctrl = flowCtrl;
    return sendCmd(util::json::Json{{"op", "setFlowCtrl"},
                                    {"flow", static_cast<int>(flowCtrl)}},
                   500_ms);
}

bool SerportGhost::isReady() const
{
    if (!busOnline())
    {
        return false;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    return _ready;
}

int SerportGhost::reset()
{
    return sendCmd(util::json::Json{{"op", "reset"}}, 500_ms);
}

} // namespace tools::os::serport

FOUNDATION_FACTORY_REGISTER(tools::os::serport::SerportGhost,
                            "tools::os::serport::SerportGhost",
                            tools_os_serport_SerportGhost)
