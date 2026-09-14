/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file TopicScheme.cpp
 * @brief TopicScheme implementation.
 */
#include "tools/design/ipc/TopicScheme.hpp"

#include <vector>

namespace tools::design::ipc
{
namespace
{

[[nodiscard]] std::vector<std::string> splitLevels(std::string_view topic)
{
    std::vector<std::string> levels;
    std::string cur;
    for (char c : topic)
    {
        if (c == '/')
        {
            levels.push_back(std::move(cur));
            cur.clear();
        }
        else
        {
            cur.push_back(c);
        }
    }
    levels.push_back(std::move(cur));
    return levels;
}

} // namespace

TopicScheme::TopicScheme(std::string appName) : _app(std::move(appName))
{
}

std::string TopicScheme::appTopic(std::initializer_list<std::string_view> levels) const
{
    std::string out = "foundation/" + _app;
    for (const auto level : levels)
    {
        out.push_back('/');
        out.append(level);
    }
    return out;
}

std::string TopicScheme::eventTopic(const EventId& id) const
{
    return appTopic({"evt", id.srcPlatform, id.objectPath, id.event});
}

std::string TopicScheme::platformStatusTopic(std::string_view platform) const
{
    return appTopic({"platforms", platform, "status"});
}

std::optional<EventId> TopicScheme::parseEvent(std::string_view topic) const
{
    const auto levels = parseAppTopic(topic);
    if (!levels || levels->size() != 4 || (*levels)[0] != "evt")
    {
        return std::nullopt;
    }
    EventId id;
    id.srcPlatform = (*levels)[1];
    id.objectPath  = (*levels)[2];
    id.event       = (*levels)[3];
    return id;
}

std::optional<std::string> TopicScheme::parsePlatformStatus(std::string_view topic) const
{
    const auto levels = parseAppTopic(topic);
    if (!levels || levels->size() != 3 || (*levels)[0] != "platforms" || (*levels)[2] != "status")
    {
        return std::nullopt;
    }
    return (*levels)[1];
}

std::optional<std::vector<std::string>> TopicScheme::parseAppTopic(std::string_view topic) const
{
    const auto levels = splitLevels(topic);
    if (levels.size() < 2 || levels[0] != "foundation" || levels[1] != _app)
    {
        return std::nullopt;
    }
    return std::vector<std::string>(levels.begin() + 2, levels.end());
}

} // namespace tools::design::ipc
