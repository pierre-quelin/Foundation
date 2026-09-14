/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file ScopedTransaction.hpp
 * @brief RAII wrapper: rollback on destruction unless @c commit() was called.
 */
#pragma once

#include "tools/design/db/ITransaction.hpp"

#include <memory>
#include <utility>

namespace tools::design::db
{

class ScopedTransaction
{
public:
    explicit ScopedTransaction(std::unique_ptr<ITransaction> tx) : _tx(std::move(tx))
    {
    }

    ~ScopedTransaction()
    {
        if (_tx != nullptr && _tx->isOpen())
        {
            try
            {
                _tx->rollback();
            }
            catch (...)
            {
                // Best-effort rollback during unwind.
            }
        }
    }

    ScopedTransaction(const ScopedTransaction&)            = delete;
    ScopedTransaction& operator=(const ScopedTransaction&) = delete;

    ScopedTransaction(ScopedTransaction&& other) noexcept
        : _tx(std::move(other._tx))
    {
    }

    ScopedTransaction& operator=(ScopedTransaction&& other) noexcept
    {
        if (this != &other)
        {
            if (_tx != nullptr && _tx->isOpen())
            {
                try
                {
                    _tx->rollback();
                }
                catch (...)
                {
                }
            }
            _tx = std::move(other._tx);
        }
        return *this;
    }

    void commit()
    {
        if (_tx != nullptr)
        {
            _tx->commit();
        }
    }

    [[nodiscard]] ITransaction* get() const noexcept { return _tx.get(); }

private:
    std::unique_ptr<ITransaction> _tx;
};

} // namespace tools::design::db
