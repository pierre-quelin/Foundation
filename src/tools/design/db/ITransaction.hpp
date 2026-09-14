/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ITransaction.hpp
 * @brief Database transaction handle.
 */
#pragma once

namespace tools::design::db
{

class ITransaction
{
public:
    virtual ~ITransaction() = default;

    virtual void commit()   = 0;
    virtual void rollback() = 0;

    [[nodiscard]] virtual bool isOpen() const = 0;
};

} // namespace tools::design::db
