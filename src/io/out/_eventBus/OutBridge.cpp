/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file OutBridge.cpp
 */
#include "io/out/OutBridge.hpp"

#include "tools/design/config/Reference.hpp"
#include "tools/design/factory/InstanceRegistry.hpp"
#include "tools/design/factory/Register.hpp"
#include "tools/design/ipc/IpcException.hpp"
#include "util/json/Json.hpp"

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
        throw std::logic_error("OutBridge: ApplicationServices::eventBus not wired");
    }
    return *app.eventBus;
}

[[nodiscard]] std::string bridgeObjectName(const tools::design::config::Node& node)
{
    return tools::design::config::instancePath(node.path());
}

[[nodiscard]] std::shared_ptr<IOut> requireBridgedInstance(tools::design::ApplicationServices& app,
                                                           const tools::design::config::Node& node)
{
    if (app.instances == nullptr)
    {
        throw std::logic_error("OutBridge: InstanceRegistry not configured");
    }
    const std::string key = tools::design::config::instancePath(node.path());
    if (auto instance = app.instances->find<IOut>(key))
    {
        return instance;
    }
    throw std::logic_error(
        "OutBridge: explicit config no longer supported; use Bridged with the bridge InstanceOf on the local object at '" +
        key + "'");
}

} // namespace

OutBridge::OutBridge(tools::design::ApplicationServices& app, tools::design::config::Node node) : OutBridge(app, node, requireBridgedInstance(app, node))
{
}

OutBridge::OutBridge(tools::design::ApplicationServices& app,
                     tools::design::config::Node node,
                     std::shared_ptr<IOut> instance) : _bus(requireEventBus(app)), _transport(_bus.transport()), _topics(_bus.topicScheme(), bridgeObjectName(node)), _ownedInstance(std::move(instance)), _instance(*_ownedInstance)
{
}

OutBridge::~OutBridge()
{
    try
    {
        stop();
    }
    catch (...)
    {
    }
}

void OutBridge::publishValue(unsigned int value)
{
    tools::design::ipc::Envelope env;
    env.topic    = _topics.value();
    env.payload  = util::json::Json{{"v", value}}.dump();
    env.retained = true;
    _transport.publish(env);
}

void OutBridge::publishStatus(bool ready)
{
    tools::design::ipc::Envelope env;
    env.topic    = _topics.status();
    env.payload  = util::json::Json{{"ready", ready}}.dump();
    env.retained = true;
    _transport.publish(env);
}

void OutBridge::launch()
{
    start();
}

void OutBridge::start()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_started)
        {
            return;
        }
        _started = true;
    }

    _setSub = _transport.subscribe(_topics.set(), [this](const tools::design::ipc::Envelope& env)
                                   { onSet(env); });
    _cmdSub = _transport.subscribe(_topics.cmd(), [this](const tools::design::ipc::Envelope& env)
                                   { onCmd(env); });

    unsigned int current = 0;
    if (_instance.get(current) == 0)
    {
        publishValue(current);
    }
    publishStatus(true);
}

void OutBridge::stop()
{
    bool was = false;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        was      = _started;
        _started = false;
    }
    if (!was)
    {
        return;
    }
    if (_setSub != 0)
    {
        _transport.unsubscribe(_setSub);
        _setSub = 0;
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

void OutBridge::onSet(const tools::design::ipc::Envelope& env)
{
    try
    {
        const unsigned int v = util::json::Json::parse(env.payload).value("v", 0u);
        if (_instance.set(v) == 0)
        {
            publishValue(v);
        }
    }
    catch (...)
    {
    }
}

void OutBridge::onCmd(const tools::design::ipc::Envelope& env)
{
    util::json::Json rep{{"ok", false}};
    try
    {
        const auto req         = util::json::Json::parse(env.payload);
        const std::string corr = req.value("corr", "");
        const std::string op   = req.value("op", "");
        rep["corr"]            = corr;
        if (op == "init")
        {
            rep["ok"] = (_instance.init() == 0);
        }
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

} // namespace io::out

FOUNDATION_FACTORY_REGISTER(io::out::OutBridge, "io::out::OutBridge", io_out_OutBridge)
