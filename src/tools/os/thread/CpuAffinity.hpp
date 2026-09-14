/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file CpuAffinity.hpp
 * @brief Object API for CPU core selection (no bitmasks in public surface).
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace tools::os::thread
{

/**
 * @brief Selected logical CPU cores for a thread, or @ref any (OS decides).
 */
class CpuAffinity
{
public:
    [[nodiscard]] static CpuAffinity any();
    [[nodiscard]] static CpuAffinity cores(std::initializer_list<unsigned int> ids);
    [[nodiscard]] static CpuAffinity cores(std::vector<unsigned int> ids);
    [[nodiscard]] static CpuAffinity range(unsigned int first, unsigned int lastInclusive);

    [[nodiscard]] bool isAny() const noexcept { return _any; }

    [[nodiscard]] bool contains(unsigned int id) const;

    /**
     * @brief Bit i set ⇒ core i allowed (for OS backends). Empty / any ⇒ 0.
     * @note Clamped to the first 64 logical cores.
     */
    [[nodiscard]] std::uint64_t toBitMask() const;

    [[nodiscard]] const std::vector<unsigned int>& coreIds() const noexcept { return _cores; }

private:
    CpuAffinity() = default;

    bool _any{true};
    std::vector<unsigned int> _cores;
};

} // namespace tools::os::thread
