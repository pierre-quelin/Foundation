/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file OutGhost.cpp
 */
#include "io/out/OutGhost.hpp"

#include "tools/design/config/Reference.hpp"
#include "tools/design/factory/Register.hpp"
#include "tools/design/ipc/IpcException.hpp"
#include "tools/design/ipc/LinkState.hpp"
#include "util/chrono/Delay.hpp"

#include <chrono>
#include <stdexcept>
#include <utility>

namespace io::out
{
namespace
{

[[nodiscard]] tools::design::ipc::IEventBus& requireEventBus(tools::design::ApplicationServices& app)
{
    if (app.eventBus == nullptr)
    {
        throw std::logic_error("OutGhost: ApplicationServices::eventBus not wired");
    }
    return *app.eventBus;
}

} // namespace

OutGhost::OutGhost(tools::design::ApplicationServices& app, tools::design::config::Node node) : _bus(requireEventBus(app)), _transport(_bus.transport()), _topics(_bus.topicScheme(), tools::design::config::instancePath(node.path()))
{
}

OutGhost::~OutGhost()
{
    try
    {
        stop();
    }
    catch (...)
    {
    }
}

bool OutGhost::busOnline() const
{
    return _bus.linkState() == tools::design::ipc::LinkState::Online &&
           _transport.isConnected();
}

std::string OutGhost::nextCorr()
{
    return std::to_string(_corr.fetch_add(1));
}

void OutGhost::launch()
{
    start();
}

void OutGhost::start()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_started)
        {
            return;
        }
        _started = true;
    }

    const auto value =
        _transport.subscribe(_topics.value(), [this](const tools::design::ipc::Envelope& env)
                             { onValue(env); });
    const auto rep =
        _transport.subscribe(_topics.rep(), [this](const tools::design::ipc::Envelope& env)
                             { onRep(env); });
    const auto status =
        _transport.subscribe(_topics.status(), [this](const tools::design::ipc::Envelope& env)
                             { onStatus(env); });

    std::lock_guard<std::mutex> lock(_mutex);
    _valueSub  = value;
    _repSub    = rep;
    _statusSub = status;
}

void OutGhost::stop()
{
    tools::design::ipc::SubscriptionId value  = 0;
    tools::design::ipc::SubscriptionId rep    = 0;
    tools::design::ipc::SubscriptionId status = 0;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_started)
        {
            return;
        }
        value      = _valueSub;
        rep        = _repSub;
        status     = _statusSub;
        _valueSub  = 0;
        _repSub    = 0;
        _statusSub = 0;
        _started   = false;
        _ready     = false;
    }
    if (value != 0)
    {
        _transport.unsubscribe(value);
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

void OutGhost::onValue(const tools::design::ipc::Envelope& env)
{
    try
    {
        const unsigned int v = util::json::Json::parse(env.payload).value("v", 0u);
        std::lock_guard<std::mutex> lock(_mutex);
        _value    = v;
        _hasValue = true;
    }
    catch (...)
    {
    }
}

void OutGhost::onStatus(const tools::design::ipc::Envelope& env)
{
    try
    {
        const bool ready = util::json::Json::parse(env.payload).value("ready", false);
        std::lock_guard<std::mutex> lock(_mutex);
        _ready = ready;
    }
    catch (...)
    {
    }
}

void OutGhost::onRep(const tools::design::ipc::Envelope& env)
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
            pending->done = true;
        }
        pending->cv.notify_all();
    }
    catch (...)
    {
    }
}

int OutGhost::sendCmd(const util::json::Json& body)
{
    if (!busOnline())
    {
        return -1;
    }
    const std::string corr = nextCorr();
    util::json::Json msg   = body;
    msg["corr"]            = corr;
    auto pending           = std::make_shared<PendingCmd>();
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

    std::unique_lock<std::mutex> lock(pending->mutex);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (!pending->done)
    {
        if (pending->cv.wait_until(lock, deadline) == std::cv_status::timeout)
        {
            break;
        }
    }
    const bool ok = pending->done && pending->ok;
    lock.unlock();
    std::lock_guard<std::mutex> mapLock(_mutex);
    _pending.erase(corr);
    return ok ? 0 : -1;
}

int OutGhost::init()
{
    return sendCmd(util::json::Json{{"op", "init"}});
}

int OutGhost::set(unsigned int value)
{
    if (!busOnline())
    {
        return -1;
    }
    try
    {
        tools::design::ipc::Envelope env;
        env.topic   = _topics.set();
        env.payload = util::json::Json{{"v", value}}.dump();
        _transport.publish(env);
        std::lock_guard<std::mutex> lock(_mutex);
        _value    = value;
        _hasValue = true;
        return 0;
    }
    catch (const tools::design::ipc::IpcException&)
    {
        return -1;
    }
}

int OutGhost::get(unsigned int& value) const
{
    if (!busOnline())
    {
        return -1;
    }
    std::lock_guard<std::mutex> lock(_mutex);
    if (!_hasValue)
    {
        return -1;
    }
    value = _value;
    return 0;
}

} // namespace io::out

FOUNDATION_FACTORY_REGISTER(io::out::OutGhost, "io::out::OutGhost", io_out_OutGhost)
