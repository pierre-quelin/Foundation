/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file InTopics.hpp
 * @brief Domain topics for remote In over ITransport.
 */
#pragma once

#include "tools/design/ipc/TopicScheme.hpp"

#include <string>
#include <string_view>

namespace io::in
{

class InTopics
{
public:
    InTopics(tools::design::ipc::TopicScheme topics, std::string objectName) : _topics(std::move(topics)), _object(std::move(objectName))
    {
    }

    [[nodiscard]] const std::string& objectName() const noexcept { return _object; }

    [[nodiscard]] std::string value() const { return channel("value"); }
    [[nodiscard]] std::string cmd() const { return channel("cmd"); }
    [[nodiscard]] std::string rep() const { return channel("rep"); }
    [[nodiscard]] std::string status() const { return channel("status"); }

private:
    [[nodiscard]] std::string channel(std::string_view name) const
    {
        return _topics.appTopic({"in", _object, name});
    }

    tools::design::ipc::TopicScheme _topics;
    std::string _object;
};

} // namespace io::in
