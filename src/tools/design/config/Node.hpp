/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Node.hpp
 * @brief Non-owning view on a configuration subtree.
 */
#pragma once

#include "tools/os/sync/SemM.hpp"
#include "util/chrono/Delay.hpp"
#include "util/json/Json.hpp"

#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace tools::design::config
{

namespace detail
{

[[nodiscard]] std::string joinPath(std::string_view base, std::string_view segment);

[[nodiscard]] util::json::Json& navigate(util::json::Json& root,
                                         std::string_view path,
                                         bool createMissing);

template <typename T>
struct ValueTraits;

template <>
struct ValueTraits<std::string>
{
    [[nodiscard]] static std::string get(const util::json::Json& j)
    {
        return j.get<std::string>();
    }

    static void set(util::json::Json& j, const std::string& value) { j = value; }
};

template <>
struct ValueTraits<bool>
{
    [[nodiscard]] static bool get(const util::json::Json& j) { return j.get<bool>(); }

    static void set(util::json::Json& j, bool value) { j = value; }
};

template <>
struct ValueTraits<int>
{
    [[nodiscard]] static int get(const util::json::Json& j) { return j.get<int>(); }

    static void set(util::json::Json& j, int value) { j = value; }
};

template <>
struct ValueTraits<unsigned int>
{
    [[nodiscard]] static unsigned int get(const util::json::Json& j)
    {
        return j.get<unsigned int>();
    }

    static void set(util::json::Json& j, unsigned int value) { j = value; }
};

template <>
struct ValueTraits<std::uint8_t>
{
    [[nodiscard]] static std::uint8_t get(const util::json::Json& j)
    {
        return static_cast<std::uint8_t>(j.get<unsigned int>());
    }

    static void set(util::json::Json& j, std::uint8_t value)
    {
        j = static_cast<unsigned int>(value);
    }
};

template <>
struct ValueTraits<double>
{
    [[nodiscard]] static double get(const util::json::Json& j) { return j.get<double>(); }

    static void set(util::json::Json& j, double value) { j = value; }
};

template <>
struct ValueTraits<util::chrono::Delay>
{
    [[nodiscard]] static util::chrono::Delay get(const util::json::Json& j)
    {
        if (!j.is_string())
        {
            throw std::runtime_error(
                "expected duration string (e.g. \"5s\", \"200ms\", \"2min\", \"1h\")");
        }
        return util::chrono::Delay::parse(j.get<std::string>());
    }

    static void set(util::json::Json& j, util::chrono::Delay value)
    {
        const auto ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(value.toNanoseconds());
        j = std::to_string(ms.count()) + "ms";
    }
};

} // namespace detail

/**
 * @brief View on a JSON configuration node (mutable through the shared center).
 */
class Node
{
public:
    Node() = default;

    Node(util::json::Json* json, std::string path, tools::os::sync::SemM* mutex) noexcept
        : _json(json), _path(std::move(path)), _mutex(mutex)
    {
    }

    /** @brief True when this view does not reference a live config document. */
    [[nodiscard]] bool isNull() const noexcept
    {
        return _json == nullptr || _mutex == nullptr;
    }

    [[nodiscard]] bool contains(std::string_view key) const
    {
        if (isNull())
        {
            return false;
        }
        const std::lock_guard<tools::os::sync::SemM> lock(*_mutex);
        if (!_json->is_object())
        {
            return false;
        }
        return _json->contains(std::string(key));
    }

    [[nodiscard]] Node operator[](std::string_view key) const
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
        auto& child = (*_json)[std::string(key)];
        return Node(&child, detail::joinPath(_path, key), _mutex);
    }

    [[nodiscard]] Node operator[](std::size_t index) const
    {
        const std::lock_guard<tools::os::sync::SemM> lock(*_mutex);
        if (_json == nullptr)
        {
            throw std::runtime_error("config::Node: null node");
        }
        if (!_json->is_array())
        {
            throw std::runtime_error(_path + ": not an array");
        }
        if (index >= _json->size())
        {
            throw std::runtime_error(_path + ": index out of range");
        }
        auto& element = (*_json)[index];
        return Node(&element, _path + "[" + std::to_string(index) + "]", _mutex);
    }

    [[nodiscard]] bool is_string() const
    {
        if (isNull())
        {
            return false;
        }
        const std::lock_guard<tools::os::sync::SemM> lock(*_mutex);
        return _json->is_string();
    }

    [[nodiscard]] bool is_object() const
    {
        if (isNull())
        {
            return false;
        }
        const std::lock_guard<tools::os::sync::SemM> lock(*_mutex);
        return _json->is_object();
    }

    [[nodiscard]] bool is_array() const
    {
        if (isNull())
        {
            return false;
        }
        const std::lock_guard<tools::os::sync::SemM> lock(*_mutex);
        return _json->is_array();
    }

    [[nodiscard]] std::size_t size() const
    {
        const std::lock_guard<tools::os::sync::SemM> lock(*_mutex);
        if (_json == nullptr)
        {
            throw std::runtime_error("config::Node: null node");
        }
        if (_json->is_array() || _json->is_object())
        {
            return _json->size();
        }
        throw std::runtime_error(_path + ": not a container");
    }

    [[nodiscard]] std::vector<std::pair<std::string, Node>> items() const;

    [[nodiscard]] Node at(std::string_view path) const
    {
        const std::lock_guard<tools::os::sync::SemM> lock(*_mutex);
        if (_json == nullptr)
        {
            throw std::runtime_error("config::Node: null node");
        }
        const std::string fullPath =
            _path.empty() ? std::string(path) : detail::joinPath(_path, path);
        util::json::Json& child = detail::navigate(*_json, path, false);
        return Node(&child, fullPath, _mutex);
    }

    template <typename T>
    [[nodiscard]] T value() const
    {
        const std::lock_guard<tools::os::sync::SemM> lock(*_mutex);
        if (_json == nullptr)
        {
            throw std::runtime_error("config::Node: null node");
        }
        if (_json->is_null())
        {
            throw std::runtime_error(_path + ": null value");
        }
        try
        {
            return detail::ValueTraits<T>::get(*_json);
        }
        catch (const util::json::Json::exception& ex)
        {
            throw std::runtime_error(_path + ": " + ex.what());
        }
    }

    /** JSON-native types (enums, etc.) not covered by ValueTraits. */
    template <typename T>
    [[nodiscard]] T as() const
    {
        const std::lock_guard<tools::os::sync::SemM> lock(*_mutex);
        if (_json == nullptr)
        {
            throw std::runtime_error("config::Node: null node");
        }
        if (_json->is_null())
        {
            throw std::runtime_error(_path + ": null value");
        }
        try
        {
            return _json->get<T>();
        }
        catch (const util::json::Json::exception& ex)
        {
            throw std::runtime_error(_path + ": " + ex.what());
        }
        catch (const std::runtime_error&)
        {
            throw;
        }
    }

    template <typename T>
    [[nodiscard]] T value_or(T defaultValue) const
    {
        const std::lock_guard<tools::os::sync::SemM> lock(*_mutex);
        if (_json == nullptr || _json->is_null())
        {
            return defaultValue;
        }
        try
        {
            return detail::ValueTraits<T>::get(*_json);
        }
        catch (const util::json::Json::exception&)
        {
            return defaultValue;
        }
    }

    /** @brief @p key present → @c value&lt;T&gt;() ; sinon @p defaultValue (sans modifier le JSON). */
    template <typename T>
    [[nodiscard]] T value_or(std::string_view key, T defaultValue) const
    {
        if (!contains(key))
        {
            return defaultValue;
        }
        return (*this)[key].value<T>();
    }

    template <typename T>
    void set_value(T value)
    {
        const std::lock_guard<tools::os::sync::SemM> lock(*_mutex);
        if (_json == nullptr)
        {
            throw std::runtime_error("config::Node: null node");
        }
        detail::ValueTraits<T>::set(*_json, std::move(value));
    }

    [[nodiscard]] std::string path() const noexcept { return _path; }

    /** @brief Deep copy of the underlying JSON (for ephemeral Platform merge, etc.). */
    [[nodiscard]] util::json::Json toJson() const;

private:
    util::json::Json* _json = nullptr;
    std::string _path;
    tools::os::sync::SemM* _mutex = nullptr;
};

} // namespace tools::design::config
