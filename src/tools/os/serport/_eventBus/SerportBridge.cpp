/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SerportBridge.cpp
 * @brief Local ISerport host on the bus.
 */
#include "tools/os/serport/SerportBridge.hpp"

#include "tools/design/config/Reference.hpp"
#include "tools/design/factory/InstanceRegistry.hpp"
#include "tools/design/factory/Register.hpp"
#include "tools/design/ipc/IpcException.hpp"
#include "tools/os/thread/Thread.h"
#include "util/chrono/Delay.hpp"
#include "util/json/Json.hpp"

#include <chrono>
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
        throw std::logic_error("SerportBridge: ApplicationServices::eventBus not wired");
    }
    return *app.eventBus;
}

[[nodiscard]] std::string bridgeObjectName(const tools::design::config::Node& node)
{
    return tools::design::config::instancePath(node.path());
}

[[nodiscard]] std::shared_ptr<ISerport> requireBridgedInstance(tools::design::ApplicationServices& app,
                                                               const tools::design::config::Node& node)
{
    if (app.instances == nullptr)
    {
        throw std::logic_error("SerportBridge: InstanceRegistry not configured");
    }
    const std::string key = tools::design::config::instancePath(node.path());
    if (auto port = app.instances->find<ISerport>(key))
    {
        return port;
    }
    throw std::logic_error(
        "SerportBridge: explicit config no longer supported; use Bridged with the bridge InstanceOf on the local object at '" +
        key + "'");
}

} // namespace

SerportBridge::SerportBridge(tools::design::ApplicationServices& app, tools::design::config::Node node) : SerportBridge(app, node, requireBridgedInstance(app, node))
{
}

SerportBridge::SerportBridge(tools::design::ApplicationServices& app,
                             tools::design::config::Node node,
                             std::shared_ptr<ISerport> instance) : _bus(requireEventBus(app)), _transport(_bus.transport()), _topics(_bus.topicScheme(), bridgeObjectName(node)), _ownedInstance(std::move(instance)), _instance(*_ownedInstance)
{
}

SerportBridge::~SerportBridge()
{
    try
    {
        stop();
    }
    catch (...)
    {
    }
}

void SerportBridge::publishStatus(bool ready)
{
    tools::design::ipc::Envelope env;
    env.topic    = _topics.status();
    env.payload  = util::json::Json{{"ready", ready}}.dump();
    env.retained = true;
    _transport.publish(env);
}

void SerportBridge::launch()
{
    start();
}

void SerportBridge::start()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_started)
        {
            return;
        }
        _started = true;
    }

    _txSub  = _transport.subscribe(_topics.tx(), [this](const tools::design::ipc::Envelope& env)
                                   { onTx(env); });
    _cmdSub = _transport.subscribe(_topics.cmd(), [this](const tools::design::ipc::Envelope& env)
                                   { onCmd(env); });

    publishStatus(_instance.isReady());

    _rxRunning = true;
    _rxThread  = std::make_unique<tools::os::thread::Thread>([this]()
                                                             { rxLoop(); },
                                                             "SerportBridge.Rx");
    _rxThread->start();
}

void SerportBridge::stop()
{
    bool wasStarted = false;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        wasStarted = _started;
        _started   = false;
    }
    if (!wasStarted)
    {
        return;
    }

    _rxRunning = false;
    if (_rxThread)
    {
        _rxThread->join();
        _rxThread.reset();
    }

    if (_txSub != 0)
    {
        _transport.unsubscribe(_txSub);
        _txSub = 0;
    }
    if (_cmdSub != 0)
    {
        _transport.unsubscribe(_cmdSub);
        _cmdSub = 0;
    }

    try
    {
        publishStatus(false);
    }
    catch (...)
    {
    }
}

void SerportBridge::onTx(const tools::design::ipc::Envelope& env)
{
    if (env.payload.empty())
    {
        return;
    }
    (void)_instance.write(env.payload.data(), static_cast<unsigned int>(env.payload.size()));
}

void SerportBridge::onCmd(const tools::design::ipc::Envelope& env)
{
    util::json::Json rep{{"ok", false}};
    try
    {
        const auto req         = util::json::Json::parse(env.payload);
        const std::string corr = req.value("corr", "");
        const std::string op   = req.value("op", "");
        rep["corr"]            = corr;

        int rc = -1;
        if (op == "flush")
        {
            rc = _instance.flush();
        }
        else if (op == "wflush")
        {
            rc = _instance.wflush();
        }
        else if (op == "rflush")
        {
            rc = _instance.rflush();
        }
        else if (op == "reset")
        {
            rc = _instance.reset();
        }
        else if (op == "setParams")
        {
            rc = _instance.setParams(static_cast<ISerport::BitRate>(req.value("bitrate", 9600)),
                                     static_cast<ISerport::DataBit>(req.value("databit", 8)),
                                     static_cast<ISerport::Parity>(req.value("parity", 0)),
                                     static_cast<ISerport::StopBit>(req.value("stopbit", 1)));
        }
        else if (op == "setFlowCtrl")
        {
            rc = _instance.setFlowCtrl(static_cast<ISerport::FlowCtrl>(req.value("flow", 0)));
        }
        rep["ok"] = (rc == 0);
        publishStatus(_instance.isReady());
    }
    catch (...)
    {
        rep["ok"] = false;
    }

    tools::design::ipc::Envelope out;
    out.topic   = _topics.rep();
    out.payload = rep.dump();
    try
    {
        _transport.publish(out);
    }
    catch (const tools::design::ipc::IpcException&)
    {
    }
}

void SerportBridge::rxLoop()
{
    char buf[256];
    while (_rxRunning.load())
    {
        const int n = _instance.read(buf, sizeof(buf), 50_ms);
        if (n > 0)
        {
            tools::design::ipc::Envelope env;
            env.topic = _topics.rx();
            env.payload.assign(buf, buf + n);
            try
            {
                _transport.publish(env);
            }
            catch (...)
            {
            }
        }
        else if (n < 0)
        {
            tools::os::thread::Thread::sleep_for(20_ms);
        }
    }
}

} // namespace tools::os::serport

FOUNDATION_FACTORY_REGISTER(tools::os::serport::SerportBridge,
                            "tools::os::serport::SerportBridge",
                            tools_os_serport_SerportBridge)
