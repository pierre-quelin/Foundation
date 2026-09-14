/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file TransportLoopback.hpp
 * @brief In-process ITransport for unit tests (no broker).
 */
#pragma once

#include "tools/design/config/Node.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/ipc/ITransport.hpp"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace tools::design::ipc
{

class TransportLoopback : public tools::design::factory::IObject, public ITransport
{
public:
    TransportLoopback() = default;
    TransportLoopback(tools::design::ApplicationServices& app, tools::design::config::Node node);

    void connect() override;
    void disconnect() override;
    [[nodiscard]] bool isConnected() const override;

    void publish(const Envelope& envelope) override;

    [[nodiscard]] SubscriptionId subscribe(std::string_view filter,
                                           TransportHandler handler) override;
    void unsubscribe(SubscriptionId id) override;

private:
    struct Subscription
    {
        std::string filter;
        TransportHandler handler;
    };

    [[nodiscard]] static bool topicMatches(std::string_view filter, std::string_view topic);

    mutable std::mutex _mutex;
    bool _connected        = false;
    SubscriptionId _nextId = 1;
    std::unordered_map<SubscriptionId, Subscription> _subs;
    std::unordered_map<std::string, Envelope> _retained;
};

} // namespace tools::design::ipc
