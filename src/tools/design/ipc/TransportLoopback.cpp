/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file TransportLoopback.cpp
 * @brief In-process transport: local publish → matching subscribers.
 */
#include "tools/design/ipc/TransportLoopback.hpp"

#include "tools/design/factory/Register.hpp"

#include <stdexcept>
#include <vector>

namespace tools::design::ipc
{
namespace
{

[[nodiscard]] std::vector<std::string_view> splitLevels(std::string_view topic)
{
    std::vector<std::string_view> levels;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= topic.size(); ++i)
    {
        if (i == topic.size() || topic[i] == '/')
        {
            levels.emplace_back(topic.data() + start, i - start);
            start = i + 1;
        }
    }
    return levels;
}

} // namespace

bool TransportLoopback::topicMatches(std::string_view filter, std::string_view topic)
{
    const auto fLevels = splitLevels(filter);
    const auto tLevels = splitLevels(topic);

    std::size_t fi = 0;
    std::size_t ti = 0;
    while (fi < fLevels.size() && ti < tLevels.size())
    {
        const auto& f = fLevels[fi];
        if (f == "#")
        {
            return fi + 1 == fLevels.size();
        }
        if (f != "+" && f != tLevels[ti])
        {
            return false;
        }
        ++fi;
        ++ti;
    }
    if (fi < fLevels.size() && fLevels[fi] == "#")
    {
        return fi + 1 == fLevels.size();
    }
    return fi == fLevels.size() && ti == tLevels.size();
}

void TransportLoopback::connect()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _connected = true;
}

void TransportLoopback::disconnect()
{
    std::lock_guard<std::mutex> lock(_mutex);
    _connected = false;
}

bool TransportLoopback::isConnected() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _connected;
}

void TransportLoopback::publish(const Envelope& envelope)
{
    std::vector<TransportHandler> handlers;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_connected)
        {
            throw std::runtime_error("TransportLoopback::publish: not connected");
        }
        if (envelope.retained)
        {
            _retained[envelope.topic] = envelope;
        }
        for (const auto& [id, sub] : _subs)
        {
            (void)id;
            if (topicMatches(sub.filter, envelope.topic))
            {
                handlers.push_back(sub.handler);
            }
        }
    }
    for (const auto& handler : handlers)
    {
        handler(envelope);
    }
}

SubscriptionId TransportLoopback::subscribe(std::string_view filter, TransportHandler handler)
{
    std::vector<Envelope> retainedReplay;
    SubscriptionId id = 0;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_connected)
        {
            throw std::runtime_error("TransportLoopback::subscribe: not connected");
        }
        id = _nextId++;
        _subs.emplace(id, Subscription{std::string(filter), std::move(handler)});
        for (const auto& [topic, env] : _retained)
        {
            if (topicMatches(filter, topic))
            {
                retainedReplay.push_back(env);
            }
        }
    }
    // Replay retained outside lock using the subscription just registered — call via copy.
    // Handlers were moved into _subs; call them from the map under a second lock briefly,
    // or replay by looking up id.
    for (const auto& env : retainedReplay)
    {
        TransportHandler h;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            const auto it = _subs.find(id);
            if (it == _subs.end())
            {
                return id;
            }
            h = it->second.handler;
        }
        h(env);
    }
    return id;
}

void TransportLoopback::unsubscribe(SubscriptionId id)
{
    std::lock_guard<std::mutex> lock(_mutex);
    _subs.erase(id);
}

TransportLoopback::TransportLoopback(ApplicationServices& app, config::Node node)
{
    (void)app;
    (void)node;
}

} // namespace tools::design::ipc

FOUNDATION_FACTORY_REGISTER(tools::design::ipc::TransportLoopback,
                            "tools::design::ipc::TransportLoopback",
                            tools_design_ipc_TransportLoopback)
