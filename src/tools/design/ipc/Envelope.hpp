/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file Envelope.hpp
 * @brief Transport message: topic + opaque payload (JSON or binary).
 */
#pragma once

#include <string>

namespace tools::design::ipc
{

struct Envelope
{
    std::string topic;
    std::string payload;
    bool retained = false;
};

} // namespace tools::design::ipc
