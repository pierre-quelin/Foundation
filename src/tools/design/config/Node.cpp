/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/config/Node.hpp"

#include <sstream>
#include <vector>

namespace tools::design::config::detail
{

namespace
{

[[nodiscard]] std::vector<std::string> splitPath(std::string_view path)
{
    std::vector<std::string> segments;
    std::string current;

    auto flush = [&]()
    {
        if (!current.empty())
        {
            segments.push_back(std::move(current));
            current.clear();
        }
    };

    for (char ch : path)
    {
        if (ch == '/' || ch == '.')
        {
            flush();
        }
        else
        {
            current.push_back(ch);
        }
    }
    flush();
    return segments;
}

} // namespace

std::string joinPath(std::string_view base, std::string_view segment)
{
    if (base.empty())
    {
        return std::string(segment);
    }
    if (segment.empty())
    {
        return std::string(base);
    }
    return std::string(base) + "/" + std::string(segment);
}

util::json::Json& navigate(util::json::Json& root, std::string_view path, bool createMissing)
{
    if (path.empty())
    {
        return root;
    }

    util::json::Json* current = &root;
    for (const std::string& segment : splitPath(path))
    {
        if (!current->is_object())
        {
            throw std::runtime_error("config: not an object at '" + segment + "'");
        }
        if (!current->contains(segment))
        {
            if (!createMissing)
            {
                throw std::runtime_error("config: missing key '" + segment + "'");
            }
            (*current)[segment] = util::json::Json::object();
        }
        current = &(*current)[segment];
    }
    return *current;
}

} // namespace tools::design::config::detail

namespace tools::design::config
{

std::vector<std::pair<std::string, Node>> Node::items() const
{
    const std::lock_guard<tools::os::sync::SemM> lock(*_mutex);
    if (_json == nullptr)
    {
        throw std::runtime_error("config::Node: null node");
    }
    if (!_json->is_object())
    {
        throw std::runtime_error(_path + ": not an object");
    }
    std::vector<std::pair<std::string, Node>> result;
    result.reserve(_json->size());
    for (auto it = _json->begin(); it != _json->end(); ++it)
    {
        auto& child = *it;
        result.emplace_back(it.key(),
                            Node(&child, detail::joinPath(_path, it.key()), _mutex));
    }
    return result;
}

util::json::Json Node::toJson() const
{
    const std::lock_guard<tools::os::sync::SemM> lock(*_mutex);
    if (_json == nullptr)
    {
        throw std::runtime_error("config::Node: null node");
    }
    return *_json;
}

} // namespace tools::design::config
