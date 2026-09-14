/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file IStatement.hpp
 * @brief Prepared statement with bound parameters.
 */
#pragma once

#include "tools/design/db/IResultSet.hpp"

#include <cstdint>
#include <memory>
#include <string_view>

namespace tools::design::db
{

/**
 * @brief Prepared SQL statement. Parameters are 1-based (SQL convention).
 */
class IStatement
{
public:
    virtual ~IStatement() = default;

    virtual void reset()         = 0;
    virtual void clearBindings() = 0;

    virtual void bindNull(int index)                         = 0;
    virtual void bindInt64(int index, std::int64_t value)    = 0;
    virtual void bindDouble(int index, double value)         = 0;
    virtual void bindText(int index, std::string_view value) = 0;

    /** @brief Execute a non-query statement (INSERT / UPDATE / DDL). */
    virtual void execute() = 0;

    /** @brief Execute a query and return a result cursor. */
    [[nodiscard]] virtual std::unique_ptr<IResultSet> query() = 0;
};

} // namespace tools::design::db
