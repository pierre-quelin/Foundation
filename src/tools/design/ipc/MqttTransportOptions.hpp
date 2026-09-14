/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file MqttTransportOptions.hpp
 * @brief Connection options for TransportByMqtt (Paho).
 */
#pragma once

#include <chrono>
#include <string>

namespace tools::design::ipc
{

/**
 * @brief Broker / credentials / optional TLS paths for TransportByMqtt.
 *
 * Interim secrets: clear @c password and/or @c passwordEnv (@c getenv).
 * Strict resolution via ISecretProvider is Phase 1.7.
 */
struct MqttTransportOptions
{
    std::string brokerUri{"tcp://127.0.0.1:1883"};
    std::string clientId;
    std::string username;
    std::string password;
    std::string passwordEnv;

    int keepAliveSec = 60;

    std::string caFile;
    std::string certFile;
    std::string keyFile;
    std::string keyPassword;

    std::chrono::seconds connectTimeout{10};
};

} // namespace tools::design::ipc
