/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file EventBus.cpp
 * @brief EventBus implementation.
 */
#include "tools/design/ipc/EventBus.hpp"

#include "tools/design/config/ResolvePlatform.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/factory/Register.hpp"
#include "tools/design/ipc/IpcException.hpp"
#include "tools/design/scheduler/SchedulerService.hpp"
#include "tools/os/sync/SemM.hpp"
#include "tools/os/thread/Thread.h"
#include "util/chrono/Delay.hpp"
#include "util/json/Json.hpp"

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

namespace tools::design::ipc
{
namespace
{

using util::chrono::literals::operator""_ms;

constexpr const char* KindReq = "req";
constexpr const char* KindRep = "rep";
constexpr const char* KindPub = "pub";

[[nodiscard]] std::string encodePub(std::string_view payload)
{
    util::json::Json j;
    j["kind"] = KindPub;
    j["data"] = std::string(payload);
    return j.dump();
}

[[nodiscard]] std::string encodeReq(std::string_view corr, std::string_view payload)
{
    util::json::Json j;
    j["kind"] = KindReq;
    j["corr"] = std::string(corr);
    j["data"] = std::string(payload);
    return j.dump();
}

[[nodiscard]] std::string encodeRep(std::string_view corr, std::string_view payload)
{
    util::json::Json j;
    j["kind"] = KindRep;
    j["corr"] = std::string(corr);
    j["data"] = std::string(payload);
    return j.dump();
}

[[nodiscard]] util::json::Json copyObjectFields(const config::Node& node,
                                                std::initializer_list<const char*> keys)
{
    util::json::Json out = util::json::Json::object();
    for (const char* key : keys)
    {
        if (!node.contains(key))
        {
            continue;
        }
        const config::Node field = node[key];
        if (field.is_string())
        {
            out[key] = field.value<std::string>();
        }
        else if (field.is_object())
        {
            util::json::Json nested = util::json::Json::object();
            for (const auto& [childKey, childNode] : field.items())
            {
                if (childNode.is_string())
                {
                    nested[childKey] = childNode.value<std::string>();
                }
            }
            out[key] = std::move(nested);
        }
        else
        {
            try
            {
                out[key] = field.value<int>();
            }
            catch (const std::exception&)
            {
            }
        }
    }
    return out;
}

[[nodiscard]] std::string resolveEventBusPlatformName(ApplicationServices& app, config::Node node)
{
    if (node.contains("PlatformName"))
    {
        return node["PlatformName"].value<std::string>();
    }
    if (!app.platformName.empty())
    {
        return app.platformName;
    }
    throw IpcException("EventBus: PlatformName missing and ApplicationServices::platformName empty");
}

[[nodiscard]] std::shared_ptr<ITransport> makeTransport(ApplicationServices& app,
                                                        config::Node node,
                                                        const std::string& platformName)
{
    using factory::createAs;
    using factory::createShared;
    using factory::ensureInstances;
    using factory::shareOwned;

    if (node.contains("Transport"))
    {
        const config::Node transportNode = node["Transport"];
        config::PlatformResolvedNode resolved(transportNode, platformName);
        util::json::Json props     = resolved.get().toJson();
        const std::string existing = props.value("ClientId", std::string{});
        if (existing.empty())
        {
            const std::string appName = node.value_or("AppName", std::string{"app"});
            props["ClientId"]         = appName + "-" + platformName;
        }

        ensureInstances(app);
        std::string ephemeralPath = transportNode.path();
        const std::string key     = config::instancePath(ephemeralPath);
        if (app.instances->contains(key))
        {
            throw std::runtime_error("factory: instance already registered at '" + key + "'");
        }

        tools::os::sync::SemM mutex;
        config::Node ephemeral(&props, std::move(ephemeralPath), &mutex);
        return createShared<ITransport>(app, ephemeral);
    }

    ensureInstances(app);

    util::json::Json props = util::json::Json::object();
    const bool mqttish =
        node.contains("Broker") || node.contains("Credentials") || node.contains("Tls");
    const char* typeName = mqttish ? "tools::design::ipc::TransportByMqtt"
                                   : "tools::design::ipc::TransportLoopback";
    if (mqttish)
    {
        props                      = copyObjectFields(node, {"Broker", "ClientId", "Credentials", "Tls", "KeepAliveSec"});
        const std::string existing = props.value("ClientId", std::string{});
        if (existing.empty())
        {
            const std::string appName = node.value_or("AppName", std::string{"app"});
            props["ClientId"]         = appName + "-" + platformName;
        }
    }

    std::string ephemeralPath = node.path();
    if (!ephemeralPath.empty())
    {
        ephemeralPath.push_back('/');
    }
    ephemeralPath += "Objects/Transport";
    const std::string key = config::instancePath(ephemeralPath);
    if (app.instances->contains(key))
    {
        throw std::runtime_error("factory: instance already registered at '" + key + "'");
    }

    tools::os::sync::SemM mutex;
    config::Node ephemeral(&props, std::move(ephemeralPath), &mutex);
    auto obj = shareOwned(createAs<ITransport>(app, ephemeral, typeName));
    app.instances->put(key, obj);
    return obj;
}

} // namespace

EventBus::EventBus(ITransport& transport,
                   TopicScheme topics,
                   std::string platformName,
                   std::shared_ptr<scheduler::EventScheduler> scheduler) : _ownedTransport(), _transport(transport), _topics(std::move(topics)), _platform(std::move(platformName)), _scheduler(std::move(scheduler))
{
    if (_scheduler == nullptr)
    {
        throw IpcException("EventBus: EventScheduler is required");
    }
}

EventBus::EventBus(ApplicationServices& app, config::Node node) : _ownedTransport(makeTransport(app, node, resolveEventBusPlatformName(app, node))), _transport(*_ownedTransport), _topics(TopicScheme{node.value_or("AppName", std::string{"app"})}), _platform(resolveEventBusPlatformName(app, node)), _scheduler(app.schedulerServiceRef().acquireShared())
{
    if (_ownedTransport == nullptr)
    {
        throw IpcException("EventBus: transport creation failed");
    }
    if (_scheduler == nullptr)
    {
        throw IpcException("EventBus: EventScheduler is required");
    }
}

EventBus::~EventBus()
{
    try
    {
        stop();
    }
    catch (...)
    {
    }
}

std::string EventBus::handlerKey(std::string_view objectPath, std::string_view event)
{
    return std::string(objectPath) + '\n' + std::string(event);
}

std::string EventBus::makeCorrId()
{
    return _platform + "-" + std::to_string(_corrCounter.fetch_add(1));
}

void EventBus::setLinkState(LinkState state)
{
    LinkState previous = LinkState::Offline;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_linkState == state)
        {
            return;
        }
        previous   = _linkState;
        _linkState = state;
        (void)previous;
    }
    linkStateChanged(state);
}

void EventBus::setPeerOnline(const std::string& platform, bool online)
{
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        const auto it  = _peers.find(platform);
        const bool was = (it != _peers.end()) && it->second;
        if (was == online)
        {
            return;
        }
        _peers[platform] = online;
        changed          = true;
    }
    if (changed)
    {
        peerOnlineChanged(platform, online);
    }
}

LinkState EventBus::linkState() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _linkState;
}

bool EventBus::peerOnline(std::string_view platform) const
{
    std::lock_guard<std::mutex> lock(_mutex);
    const auto it = _peers.find(std::string(platform));
    return it != _peers.end() && it->second;
}

void EventBus::start()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_linkState != LinkState::Offline)
        {
            return;
        }
    }
    setLinkState(LinkState::Connecting);

    _transport.connect();

    _statusSubId = _transport.subscribe(
        _topics.appTopic({"platforms", "+", "status"}),
        [this](const Envelope& env)
        {
            _scheduler->schedule([this, env]()
                                 { onTransportEnvelope(env); });
        });

    _evtSubId = _transport.subscribe(
        _topics.appTopic({"evt", "+", "+", "+"}),
        [this](const Envelope& env)
        {
            _scheduler->schedule([this, env]()
                                 { onTransportEnvelope(env); });
        });

    Envelope presence;
    presence.topic    = _topics.platformStatusTopic(_platform);
    presence.payload  = "online";
    presence.retained = true;
    _transport.publish(presence);

    setLinkState(LinkState::Online);
}

void EventBus::stop()
{
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_linkState == LinkState::Offline && _evtSubId == 0 && _statusSubId == 0)
        {
            return;
        }
    }

    // Drop our transport handlers before any publish. Loopback delivers synchronously and
    // those handlers schedule work with raw `this` — publishing while still subscribed
    // queues UAF jobs that run during ~EventBus / scheduler join (glibc double-free).
    if (_evtSubId != 0)
    {
        _transport.unsubscribe(_evtSubId);
        _evtSubId = 0;
    }
    if (_statusSubId != 0)
    {
        _transport.unsubscribe(_statusSubId);
        _statusSubId = 0;
    }

    if (_transport.isConnected())
    {
        Envelope presence;
        presence.topic    = _topics.platformStatusTopic(_platform);
        presence.payload  = "offline";
        presence.retained = true;
        try
        {
            _transport.publish(presence);
        }
        catch (...)
        {
        }
    }

    // Drain callbacks already queued (from traffic before unsubscribe).
    if (_scheduler != nullptr)
    {
        (void)_scheduler->synchronise();
    }

    // Do not disconnect the transport — it may be shared by several EventBus instances.
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _peers.clear();
    }
    setLinkState(LinkState::Offline);
}

void EventBus::synchronize(const std::vector<std::string>& expectedPlatforms,
                           util::chrono::Delay timeout)
{
    if (linkState() == LinkState::Offline)
    {
        start();
    }

    std::unordered_set<std::string> expected(expectedPlatforms.begin(), expectedPlatforms.end());
    expected.erase(_platform);

    const auto deadline = std::chrono::steady_clock::now() + timeout.toNanoseconds();
    while (true)
    {
        bool allOnline = true;
        for (const auto& p : expected)
        {
            if (!peerOnline(p))
            {
                allOnline = false;
                break;
            }
        }
        if (allOnline)
        {
            return;
        }
        if (std::chrono::steady_clock::now() >= deadline)
        {
            throw IpcException("EventBus::synchronize: timeout waiting for peers");
        }
        (void)_scheduler->synchronise(util::chrono::Delay(std::chrono::milliseconds(20)));
        tools::os::thread::Thread::sleep_for(5_ms);
    }
}

void EventBus::publish(std::string_view objectPath,
                       std::string_view event,
                       std::string_view payload,
                       bool retained)
{
    if (linkState() != LinkState::Online || !_transport.isConnected())
    {
        throw IpcException("EventBus::publish: bus is offline");
    }
    EventId id;
    id.srcPlatform = _platform;
    id.objectPath  = std::string(objectPath);
    id.event       = std::string(event);

    Envelope env;
    env.topic    = _topics.eventTopic(id);
    env.payload  = encodePub(payload);
    env.retained = retained;
    _transport.publish(env);
}

EventBusSubscriptionId EventBus::subscribe(std::string_view objectPath,
                                           std::string_view event,
                                           EventHandler handler)
{
    std::lock_guard<std::mutex> lock(_mutex);
    const auto id = _nextBusSubId++;
    _busSubs.emplace(id, BusSubscription{std::string(objectPath), std::string(event), std::move(handler)});
    return id;
}

void EventBus::unsubscribe(EventBusSubscriptionId id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _busSubs.erase(id);
}

void EventBus::onRequest(std::string_view objectPath,
                         std::string_view event,
                         RequestHandler handler)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _requestHandlers[handlerKey(objectPath, event)] = std::move(handler);
}

std::string EventBus::request(std::string_view objectPath,
                              std::string_view event,
                              std::string_view payload,
                              util::chrono::Delay timeout)
{
    if (linkState() != LinkState::Online || !_transport.isConnected())
    {
        throw IpcException("EventBus::request: bus is offline");
    }

    const std::string corr = makeCorrId();
    auto pending           = std::make_shared<PendingRequest>();
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _pending[corr] = pending;
    }

    EventId id;
    id.srcPlatform = _platform;
    id.objectPath  = std::string(objectPath);
    id.event       = std::string(event);

    Envelope env;
    env.topic   = _topics.eventTopic(id);
    env.payload = encodeReq(corr, payload);
    _transport.publish(env);

    std::unique_lock<std::mutex> lock(pending->mutex);
    const bool ok =
        pending->cv.wait_for(lock, timeout.toNanoseconds(), [&]
                             { return pending->done; });

    {
        std::lock_guard<std::mutex> mapLock(_mutex);
        _pending.erase(corr);
    }

    if (!ok)
    {
        throw IpcException("EventBus::request: timeout");
    }
    return pending->reply;
}

void EventBus::onTransportEnvelope(const Envelope& envelope)
{
    if (const auto platform = _topics.parsePlatformStatus(envelope.topic))
    {
        if (*platform != _platform)
        {
            setPeerOnline(*platform, envelope.payload == "online");
        }
        return;
    }

    const auto eventId = _topics.parseEvent(envelope.topic);
    if (!eventId)
    {
        return;
    }

    util::json::Json msg;
    try
    {
        msg = util::json::Json::parse(envelope.payload);
    }
    catch (...)
    {
        // Raw payload → treat as pub data for compatibility.
        dispatchEvent(*eventId, envelope.payload);
        return;
    }

    const auto kind = msg.value("kind", std::string(KindPub));
    const auto data = msg.value("data", std::string{});
    if (kind == KindReq)
    {
        handleRequestMessage(*eventId, msg.value("corr", std::string{}), data);
    }
    else if (kind == KindRep)
    {
        handleReplyMessage(msg.value("corr", std::string{}), data);
    }
    else
    {
        dispatchEvent(*eventId, data);
    }
}

void EventBus::dispatchEvent(const EventId& id, const std::string& payload)
{
    std::vector<EventHandler> handlers;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        for (const auto& [sid, sub] : _busSubs)
        {
            (void)sid;
            if (sub.objectPath == id.objectPath && sub.event == id.event)
            {
                handlers.push_back(sub.handler);
            }
        }
    }
    for (const auto& h : handlers)
    {
        h(id, payload);
    }
}

void EventBus::handleRequestMessage(const EventId& id,
                                    const std::string& corr,
                                    const std::string& data)
{
    if (corr.empty())
    {
        return;
    }
    RequestHandler handler;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        const auto it = _requestHandlers.find(handlerKey(id.objectPath, id.event));
        if (it == _requestHandlers.end())
        {
            return;
        }
        handler = it->second;
    }

    std::string replyPayload;
    try
    {
        replyPayload = handler(id, data);
    }
    catch (...)
    {
        return;
    }

    EventId replyId;
    replyId.srcPlatform = _platform;
    replyId.objectPath  = id.objectPath;
    replyId.event       = id.event;

    Envelope env;
    env.topic   = _topics.eventTopic(replyId);
    env.payload = encodeRep(corr, replyPayload);
    try
    {
        if (_transport.isConnected())
        {
            _transport.publish(env);
        }
    }
    catch (...)
    {
    }
}

void EventBus::handleReplyMessage(const std::string& corr, const std::string& data)
{
    std::shared_ptr<PendingRequest> pending;
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
        pending->reply = data;
        pending->done  = true;
    }
    pending->cv.notify_all();
}

} // namespace tools::design::ipc

FOUNDATION_FACTORY_REGISTER(tools::design::ipc::EventBus,
                            "tools::design::ipc::EventBus",
                            tools_design_ipc_EventBus)
