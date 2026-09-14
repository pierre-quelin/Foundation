/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ITransport.hpp
 * @brief Network façade under IEventBus (MQTT or loopback).
 */
#pragma once

#include "tools/design/ipc/Envelope.hpp"

#include <cstdint>
#include <functional>
#include <string_view>

namespace tools::design::ipc
{

using TransportHandler = std::function<void(const Envelope&)>;
using SubscriptionId   = std::uint64_t;

/**
 * @brief Lowest IPC transport layer — no business routing.
 *
 * MQTT wildcards on @c subscribe filters: @c + (one level), @c # (multi-level, trailing).
 */
class ITransport
{
public:
    virtual ~ITransport() = default;

    virtual void connect()                         = 0;
    virtual void disconnect()                      = 0;
    [[nodiscard]] virtual bool isConnected() const = 0;

    virtual void publish(const Envelope& envelope) = 0;

    [[nodiscard]] virtual SubscriptionId subscribe(std::string_view filter,
                                                   TransportHandler handler) = 0;
    virtual void unsubscribe(SubscriptionId id)                              = 0;
};

} // namespace tools::design::ipc
