/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file TransportByMqtt.hpp
 * @brief ITransport over Eclipse Paho MQTT C++ (auto-reconnect + resubscribe).
 */
#pragma once

#include "tools/design/config/Node.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/ipc/ITransport.hpp"
#include "tools/design/ipc/MqttTransportOptions.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mqtt
{
class async_client;
}

namespace tools::design::ipc
{

class TransportByMqtt : public tools::design::factory::IObject, public ITransport
{
public:
    explicit TransportByMqtt(MqttTransportOptions options);
    TransportByMqtt(tools::design::ApplicationServices& app, tools::design::config::Node node);
    ~TransportByMqtt() override;

    TransportByMqtt(const TransportByMqtt&)            = delete;
    TransportByMqtt& operator=(const TransportByMqtt&) = delete;

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

    [[nodiscard]] std::string resolvePassword() const;
    void installHandlers();
    [[nodiscard]] static bool topicMatches(std::string_view filter, std::string_view topic);
    void onMessage(const std::string& topic,
                   const std::string& payload,
                   bool retained);
    void onConnected();
    void onConnectionLost();

    MqttTransportOptions _options;
    mutable std::mutex _mutex;
    std::unique_ptr<mqtt::async_client> _client;
    bool _userConnected    = false;
    SubscriptionId _nextId = 1;
    std::unordered_map<SubscriptionId, Subscription> _subs;

    static constexpr int Qos = 1;
};

} // namespace tools::design::ipc
