/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Reference.hpp
 * @brief Resolve dotted config references (e.g. "parent.child").
 */
#pragma once

#include "tools/design/config/Node.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace tools::design::config
{

/**
 * @brief True when @p value is a dotted path reference, not an InstanceOf type name.
 *
 * References use dots between config keys ("parent.child"); factory type names use "::".
 */
[[nodiscard]] inline bool isReference(std::string_view value) noexcept
{
    return value.find('.') != std::string_view::npos && value.find("::") == std::string_view::npos;
}

/**
 * @brief Logical instance path: strip @c Objects @e container segments only.
 *
 * Walks @c owner [/ Objects / entry]… — each @c Objects after an owner/entry is
 * the registry keyword and is dropped; an entry may itself be named @c Objects.
 * Examples: @c IOBoard/Objects/MCP2221 → @c IOBoard.MCP2221 ;
 * @c IOBoard/Objects/Objects → @c IOBoard.Objects ;
 * @c Board/Objects/Objects/Objects/Child → @c Board.Objects.Child.
 */
[[nodiscard]] inline std::string instancePath(std::string_view path)
{
    std::string result;
    std::string segment;
    std::vector<std::string> segments;

    auto flush = [&]()
    {
        if (!segment.empty())
        {
            segments.push_back(std::move(segment));
            segment.clear();
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
            segment.push_back(ch);
        }
    }
    flush();

    for (std::size_t i = 0; i < segments.size();)
    {
        if (!result.empty())
        {
            result.push_back('.');
        }
        result += segments[i];
        ++i;
        if (i < segments.size() && segments[i] == "Objects")
        {
            ++i; // skip registry container under the owner/entry just kept
        }
    }
    return result;
}

/** @brief Last segment of @c instancePath(path) — short instance name for loggers. */
[[nodiscard]] inline std::string instanceName(std::string_view path)
{
    const std::string logical = instancePath(path);
    const std::size_t pos     = logical.rfind('.');
    return pos == std::string::npos ? logical : logical.substr(pos + 1);
}

namespace detail
{

[[nodiscard]] inline std::string referenceToPath(std::string_view reference)
{
    std::string path;
    path.reserve(reference.size());
    for (char ch : reference)
    {
        path.push_back(ch == '.' ? '/' : ch);
    }
    return path;
}

[[nodiscard]] inline std::string parentPath(std::string_view nodePath)
{
    const std::size_t pos = nodePath.rfind('/');
    if (pos == std::string_view::npos)
    {
        return {};
    }
    return std::string(nodePath.substr(0, pos));
}

/** @brief Follow short aliases through parent.Objects — refs hide Objects in paths. */
[[nodiscard]] inline Node dereference(Node root, Node node);

} // namespace detail

[[nodiscard]] inline Node resolveReference(Node root, std::string_view reference);

[[nodiscard]] inline Node resolveObject(Node node, std::string_view objectName);

namespace detail
{

[[nodiscard]] inline Node dereference(Node root, Node node)
{
    constexpr int maxDepth = 16;
    for (int depth = 0; depth < maxDepth; ++depth)
    {
        if (!node.is_string())
        {
            return node;
        }
        const std::string ref = node.value<std::string>();
        if (isReference(ref))
        {
            node = resolveReference(root, ref);
            continue;
        }
        const std::string parent = parentPath(node.path());
        if (parent.empty())
        {
            throw std::runtime_error("config: cannot resolve alias '" + ref + "' at " + node.path());
        }
        node = resolveObject(root.at(parent), ref);
    }
    throw std::runtime_error("config: alias resolution depth exceeded at " + node.path());
}

} // namespace detail

/** @brief Definition node in @p parent's local Objects registry. */
[[nodiscard]] inline Node resolveObject(Node node, std::string_view objectName)
{
    if (!node.contains("Objects"))
    {
        throw std::runtime_error("config: no Objects at " + node.path());
    }
    const Node objects = node["Objects"];
    if (!objects.contains(objectName))
    {
        throw std::runtime_error("config: Objects/" + std::string(objectName) + " not found at " +
                                 node.path());
    }
    return objects[objectName];
}

/** @brief Absolute dotted path from the configuration document root. */
[[nodiscard]] inline Node resolveReference(Node root, std::string_view reference)
{
    if (!isReference(reference))
    {
        throw std::runtime_error("config: not a reference path '" + std::string(reference) + "'");
    }
    const std::string path = detail::referenceToPath(reference);
    Node node;
    try
    {
        node = root.at(path);
    }
    catch (const std::runtime_error&)
    {
        const std::size_t slash = path.rfind('/');
        if (slash == std::string::npos)
        {
            throw;
        }
        const Node parent = root.at(path.substr(0, slash));
        node              = resolveObject(parent, path.substr(slash + 1));
    }
    return detail::dereference(root, node);
}

/**
 * @brief Object node for @p node[key]: dotted external ref, short local ref, or inline object.
 *
 * A string without a dot names an entry in @p node.Objects; a dotted string is resolved
 * from the document root; an inline object is returned as-is.
 */
[[nodiscard]] inline Node follow(Node root, Node node, std::string_view key)
{
    const Node field = node[key];
    if (field.is_string())
    {
        const std::string ref = field.value<std::string>();
        if (isReference(ref))
        {
            return resolveReference(root, ref);
        }
        return resolveObject(node, ref);
    }
    return field;
}

/**
 * @brief Resolve a field key or bare Objects entry name under @p node.
 *
 * Used for `obtain`/`create` by name and for Item-list iteration.
 */
[[nodiscard]] inline Node resolveNamed(Node root, Node node, std::string_view name)
{
    if (isReference(name))
    {
        return resolveReference(root, name);
    }
    if (node.contains(name))
    {
        const Node field = node[name];
        if (field.is_string())
        {
            const std::string ref = field.value<std::string>();
            if (isReference(ref))
            {
                return resolveReference(root, ref);
            }
            return resolveObject(node, ref);
        }
        if (field.is_object())
        {
            return field;
        }
    }
    return resolveObject(node, name);
}

} // namespace tools::design::config
