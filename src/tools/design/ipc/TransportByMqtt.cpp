/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file TransportByMqtt.cpp
 * @brief Paho-backed ITransport with auto-reconnect and filter resubscribe.
 */
#include "tools/design/ipc/TransportByMqtt.hpp"

#include "tools/design/config/Reference.hpp"
#include "tools/design/factory/Register.hpp"
#include "tools/design/ipc/IpcException.hpp"

#include <cstdlib>
#include <string_view>
#include <utility>
#include <vector>

#include "mqtt/async_client.h"

namespace tools::design::ipc
{

namespace
{

mqtt::connect_options buildConnectOptions(const MqttTransportOptions& options,
                                          const std::string& password)
{
    auto builder = mqtt::connect_options_builder()
                       .clean_session(true)
                       .keep_alive_interval(std::chrono::seconds(options.keepAliveSec))
                       .connect_timeout(options.connectTimeout)
                       .automatic_reconnect(std::chrono::seconds(1),
                                            std::chrono::seconds(30));

    if (!options.username.empty())
    {
        builder.user_name(options.username);
    }
    if (!password.empty())
    {
        builder.password(password);
    }

    if (!options.caFile.empty() || !options.certFile.empty() || !options.keyFile.empty())
    {
        auto ssl = mqtt::ssl_options_builder();
        if (!options.caFile.empty())
        {
            ssl.trust_store(options.caFile);
        }
        if (!options.certFile.empty())
        {
            ssl.key_store(options.certFile);
        }
        if (!options.keyFile.empty())
        {
            ssl.private_key(options.keyFile);
        }
        if (!options.keyPassword.empty())
        {
            ssl.private_keypassword(options.keyPassword);
        }
        builder.ssl(ssl.finalize());
    }

    return builder.finalize();
}

} // namespace

[[nodiscard]] MqttTransportOptions optionsFromNode(config::Node node)
{
    MqttTransportOptions opt;
    opt.brokerUri = node.value_or("Broker", std::string{"tcp://127.0.0.1:1883"});
    opt.clientId  = node.value_or("ClientId", std::string{});
    if (opt.clientId.empty())
    {
        opt.clientId = config::instanceName(node.path());
    }
    if (opt.clientId.empty())
    {
        opt.clientId = "foundation-mqtt";
    }
    opt.keepAliveSec = node.value_or("KeepAliveSec", 60);
    if (node.contains("Credentials"))
    {
        const config::Node cred = node["Credentials"];
        opt.username            = cred.value_or("User", std::string{});
        opt.password            = cred.value_or("Password", std::string{});
        opt.passwordEnv         = cred.value_or("PasswordEnv", std::string{});
    }
    if (node.contains("Tls"))
    {
        const config::Node tls = node["Tls"];
        opt.caFile             = tls.value_or("CaFile", std::string{});
        opt.certFile           = tls.value_or("CertFile", std::string{});
        opt.keyFile            = tls.value_or("KeyFile", std::string{});
        opt.keyPassword        = tls.value_or("KeyPassword", std::string{});
        if (tls.contains("KeyPasswordEnv"))
        {
            const char* env = std::getenv(tls["KeyPasswordEnv"].value<std::string>().c_str());
            if (env != nullptr)
            {
                opt.keyPassword = env;
            }
        }
    }
    return opt;
}

TransportByMqtt::TransportByMqtt(MqttTransportOptions options) : _options(std::move(options))
{
    if (_options.clientId.empty())
    {
        throw IpcException("TransportByMqtt: clientId is required");
    }
    if (_options.brokerUri.empty())
    {
        throw IpcException("TransportByMqtt: brokerUri is required");
    }
}

TransportByMqtt::TransportByMqtt(ApplicationServices& app, config::Node node) : TransportByMqtt(optionsFromNode(std::move(node)))
{
    (void)app;
}

TransportByMqtt::~TransportByMqtt()
{
    try
    {
        disconnect();
    }
    catch (...)
    {
    }
}

std::string TransportByMqtt::resolvePassword() const
{
    if (!_options.password.empty())
    {
        return _options.password;
    }
    if (_options.passwordEnv.empty())
    {
        return {};
    }
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996) // getenv — interim until ISecretProvider (1.7)
#endif
    const char* value = std::getenv(_options.passwordEnv.c_str());
#ifdef _MSC_VER
#pragma warning(pop)
#endif
    return value != nullptr ? std::string(value) : std::string{};
}

void TransportByMqtt::installHandlers()
{
    _client->set_connected_handler([this](const std::string& /*cause*/)
                                   { onConnected(); });
    _client->set_connection_lost_handler([this](const std::string& /*cause*/)
                                         { onConnectionLost(); });
    _client->set_message_callback([this](mqtt::const_message_ptr msg)
                                  {
        if (!msg)
        {
            return;
        }
        onMessage(msg->get_topic(), msg->to_string(), msg->is_retained()); });
}

bool TransportByMqtt::topicMatches(std::string_view filter, std::string_view topic)
{
    auto splitLevels = [](std::string_view value)
    {
        std::vector<std::string_view> levels;
        std::size_t start = 0;
        for (std::size_t i = 0; i <= value.size(); ++i)
        {
            if (i == value.size() || value[i] == '/')
            {
                levels.emplace_back(value.data() + start, i - start);
                start = i + 1;
            }
        }
        return levels;
    };

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

void TransportByMqtt::onConnected()
{
    std::vector<std::string> filters;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        filters.reserve(_subs.size());
        for (const auto& entry : _subs)
        {
            filters.push_back(entry.second.filter);
        }
    }
    for (const auto& filter : filters)
    {
        try
        {
            if (_client)
            {
                _client->subscribe(filter, Qos);
            }
        }
        catch (const mqtt::exception&)
        {
            // Best-effort on reconnect; next reconnect will retry.
        }
    }
}

void TransportByMqtt::onConnectionLost()
{
    // Auto-reconnect is enabled on connect_options; subscriptions restored in onConnected.
}

void TransportByMqtt::onMessage(const std::string& topic,
                                const std::string& payload,
                                bool retained)
{
    std::vector<TransportHandler> handlers;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        for (const auto& entry : _subs)
        {
            if (entry.second.handler && topicMatches(entry.second.filter, topic))
            {
                handlers.push_back(entry.second.handler);
            }
        }
    }

    Envelope envelope;
    envelope.topic    = topic;
    envelope.payload  = payload;
    envelope.retained = retained;

    for (const auto& handler : handlers)
    {
        try
        {
            handler(envelope);
        }
        catch (...)
        {
        }
    }
}

void TransportByMqtt::connect()
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (_userConnected && _client && _client->is_connected())
    {
        return;
    }

    const std::string password = resolvePassword();
    _client                    = std::make_unique<mqtt::async_client>(_options.brokerUri, _options.clientId);
    installHandlers();

    const auto connOpts = buildConnectOptions(_options, password);
    lock.unlock();

    try
    {
        auto tok = _client->connect(connOpts);
        tok->wait();
    }
    catch (const mqtt::exception& ex)
    {
        std::lock_guard<std::mutex> resetLock(_mutex);
        _client.reset();
        _userConnected = false;
        throw IpcException(std::string("TransportByMqtt connect failed: ") + ex.what());
    }

    std::lock_guard<std::mutex> doneLock(_mutex);
    _userConnected = true;
}

void TransportByMqtt::disconnect()
{
    std::unique_ptr<mqtt::async_client> client;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _userConnected = false;
        client         = std::move(_client);
    }
    if (!client)
    {
        return;
    }

    try
    {
        if (client->is_connected())
        {
            // Disable further auto-reconnect by disconnecting cleanly.
            auto tok = client->disconnect();
            tok->wait();
        }
    }
    catch (const mqtt::exception& ex)
    {
        throw IpcException(std::string("TransportByMqtt disconnect failed: ") + ex.what());
    }
}

bool TransportByMqtt::isConnected() const
{
    std::lock_guard<std::mutex> lock(_mutex);
    return _userConnected && _client && _client->is_connected();
}

void TransportByMqtt::publish(const Envelope& envelope)
{
    std::unique_lock<std::mutex> lock(_mutex);
    if (!_userConnected || !_client || !_client->is_connected())
    {
        throw IpcException("TransportByMqtt: not connected");
    }
    auto* client = _client.get();
    lock.unlock();

    try
    {
        auto msg = mqtt::make_message(envelope.topic, envelope.payload, Qos, envelope.retained);
        auto tok = client->publish(msg);
        tok->wait();
    }
    catch (const mqtt::exception& ex)
    {
        throw IpcException(std::string("TransportByMqtt publish failed: ") + ex.what());
    }
}

SubscriptionId TransportByMqtt::subscribe(std::string_view filter, TransportHandler handler)
{
    if (filter.empty())
    {
        throw IpcException("TransportByMqtt: empty subscribe filter");
    }
    if (!handler)
    {
        throw IpcException("TransportByMqtt: null subscribe handler");
    }

    const std::string filterStr(filter);
    SubscriptionId id          = 0;
    mqtt::async_client* client = nullptr;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        id        = _nextId++;
        _subs[id] = Subscription{filterStr, std::move(handler)};
        if (_userConnected && _client && _client->is_connected())
        {
            client = _client.get();
        }
    }

    if (client)
    {
        try
        {
            auto tok = client->subscribe(filterStr, Qos);
            tok->wait();
        }
        catch (const mqtt::exception& ex)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _subs.erase(id);
            throw IpcException(std::string("TransportByMqtt subscribe failed: ") + ex.what());
        }
    }
    return id;
}

void TransportByMqtt::unsubscribe(SubscriptionId id)
{
    std::string filter;
    mqtt::async_client* client = nullptr;
    bool lastForFilter         = true;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        const auto it = _subs.find(id);
        if (it == _subs.end())
        {
            return;
        }
        filter = it->second.filter;
        _subs.erase(it);
        for (const auto& entry : _subs)
        {
            if (entry.second.filter == filter)
            {
                lastForFilter = false;
                break;
            }
        }
        if (lastForFilter && _userConnected && _client && _client->is_connected())
        {
            client = _client.get();
        }
    }

    if (client)
    {
        try
        {
            auto tok = client->unsubscribe(filter);
            tok->wait();
        }
        catch (const mqtt::exception& ex)
        {
            throw IpcException(std::string("TransportByMqtt unsubscribe failed: ") + ex.what());
        }
    }
}

} // namespace tools::design::ipc

FOUNDATION_FACTORY_REGISTER(tools::design::ipc::TransportByMqtt,
                            "tools::design::ipc::TransportByMqtt",
                            tools_design_ipc_TransportByMqtt)
