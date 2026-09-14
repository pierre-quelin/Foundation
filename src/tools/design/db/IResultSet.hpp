/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file IResultSet.hpp
 * @brief Query result cursor and row access.
 */
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace tools::design::db
{

/**
 * @brief One row of a result set (valid until the next @c IResultSet::next).
 */
class IRow
{
public:
    virtual ~IRow() = default;

    [[nodiscard]] virtual std::size_t columnCount() const = 0;

    [[nodiscard]] virtual bool isNull(std::size_t index) const     = 0;
    [[nodiscard]] virtual bool isNull(std::string_view name) const = 0;

    [[nodiscard]] virtual std::int64_t getInt64(std::size_t index) const     = 0;
    [[nodiscard]] virtual std::int64_t getInt64(std::string_view name) const = 0;

    [[nodiscard]] virtual double getDouble(std::size_t index) const     = 0;
    [[nodiscard]] virtual double getDouble(std::string_view name) const = 0;

    [[nodiscard]] virtual std::string getText(std::size_t index) const     = 0;
    [[nodiscard]] virtual std::string getText(std::string_view name) const = 0;
};

/**
 * @brief Forward-only result cursor.
 */
class IResultSet
{
public:
    virtual ~IResultSet() = default;

    /** @brief Advance to the next row; false when exhausted. */
    virtual bool next() = 0;

    [[nodiscard]] virtual const IRow& row() const = 0;
};

} // namespace tools::design::db
