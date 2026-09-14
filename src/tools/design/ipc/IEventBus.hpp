/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file IEventBus.hpp
 * @brief Inter-platform event bus API (no MQTT types).
 */
#pragma once

#include "tools/design/ipc/EventId.hpp"
#include "tools/design/ipc/ITransport.hpp"
#include "tools/design/ipc/LinkState.hpp"
#include "tools/design/ipc/TopicScheme.hpp"
#include "tools/design/signal/Signal.hpp"
#include "util/chrono/Delay.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace tools::design::ipc
{

using EventBusSubscriptionId = std::uint64_t;
using EventHandler =
    std::function<void(const EventId& id, const std::string& payload)>;
using RequestHandler = std::function<std::string(const EventId& id, const std::string& payload)>;

/**
 * @brief Application-facing bus: pub/sub, request/reply, link / presence.
 */
class IEventBus
{
public:
    virtual ~IEventBus() = default;

    virtual void start() = 0;
    virtual void stop()  = 0;

    [[nodiscard]] virtual LinkState linkState() const                      = 0;
    [[nodiscard]] virtual bool peerOnline(std::string_view platform) const = 0;

    /**
     * @brief Publish presence and wait until all @p expectedPlatforms are online
     *        (or timeout → @c IpcException).
     */
    virtual void synchronize(const std::vector<std::string>& expectedPlatforms,
                             util::chrono::Delay timeout) = 0;

    /** @brief Publish an event from this platform (@c EventId::srcPlatform filled). */
    virtual void publish(std::string_view objectPath,
                         std::string_view event,
                         std::string_view payload,
                         bool retained = false) = 0;

    [[nodiscard]] virtual EventBusSubscriptionId subscribe(std::string_view objectPath,
                                                           std::string_view event,
                                                           EventHandler handler) = 0;
    virtual void unsubscribe(EventBusSubscriptionId id)                          = 0;

    /** @brief Handle inbound request/reply for @p objectPath / @p event. */
    virtual void onRequest(std::string_view objectPath,
                           std::string_view event,
                           RequestHandler handler) = 0;

    /**
     * @brief Synchronous request; waits for a matching reply or throws on timeout / offline.
     */
    [[nodiscard]] virtual std::string request(std::string_view objectPath,
                                              std::string_view event,
                                              std::string_view payload,
                                              util::chrono::Delay timeout) = 0;

    /** @brief Underlying transport (same instance the bus uses). */
    [[nodiscard]] virtual ITransport& transport() = 0;

    /** @brief Topic naming scheme used by this bus. */
    [[nodiscard]] virtual const TopicScheme& topicScheme() const = 0;

    tools::design::signal::Signal<void(LinkState)> linkStateChanged;
    tools::design::signal::Signal<void(std::string, bool)> peerOnlineChanged;
};

} // namespace tools::design::ipc
