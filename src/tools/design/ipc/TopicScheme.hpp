/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file TopicScheme.hpp
 * @brief Build / parse foundation/{app}/… MQTT-style topics (bus core only).
 */
#pragma once

#include "tools/design/ipc/EventId.hpp"

#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tools::design::ipc
{

/**
 * @brief Topic naming for bus primitives (events, platform presence).
 *
 * Domain-specific trees (e.g. remote serport) stay in their package and use
 * @c appTopic({…}) under @c foundation/{app}/….
 *
 * - evt: @c foundation/{app}/evt/{srcPlatform}/{objectPath}/{event}
 * - status: @c foundation/{app}/platforms/{platform}/status
 */
class TopicScheme
{
public:
    explicit TopicScheme(std::string appName);

    [[nodiscard]] const std::string& appName() const noexcept { return _app; }

    /** @brief @c foundation/{app}/ + joined @p levels. */
    [[nodiscard]] std::string appTopic(std::initializer_list<std::string_view> levels) const;

    [[nodiscard]] std::string eventTopic(const EventId& id) const;
    [[nodiscard]] std::string platformStatusTopic(std::string_view platform) const;

    [[nodiscard]] std::optional<EventId> parseEvent(std::string_view topic) const;
    [[nodiscard]] std::optional<std::string> parsePlatformStatus(std::string_view topic) const;

    /**
     * @brief If @p topic is under @c foundation/{app}/, return levels after the app name.
     */
    [[nodiscard]] std::optional<std::vector<std::string>> parseAppTopic(std::string_view topic) const;

private:
    std::string _app;
};

} // namespace tools::design::ipc
