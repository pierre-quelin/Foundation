/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Signal.hpp
 * @brief boost::signals2::signal facade
 */
#pragma once

#include <boost/signals2.hpp>

namespace tools
{
namespace design
{
namespace signal
{
template <typename Signature>
using Signal = boost::signals2::signal<Signature>;

using Connection = boost::signals2::connection;

using ScopedConnection = boost::signals2::scoped_connection;
} // namespace signal
} // namespace design
} // namespace tools
