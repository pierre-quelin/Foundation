/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file CpuAffinity.cpp
 */
#include "tools/os/thread/CpuAffinity.hpp"

#include <algorithm>

namespace tools::os::thread
{

CpuAffinity CpuAffinity::any()
{
    return CpuAffinity{};
}

CpuAffinity CpuAffinity::cores(std::initializer_list<unsigned int> ids)
{
    return cores(std::vector<unsigned int>(ids));
}

CpuAffinity CpuAffinity::cores(std::vector<unsigned int> ids)
{
    CpuAffinity a;
    a._any   = false;
    a._cores = std::move(ids);
    std::sort(a._cores.begin(), a._cores.end());
    a._cores.erase(std::unique(a._cores.begin(), a._cores.end()), a._cores.end());
    if (a._cores.empty())
    {
        a._any = true;
    }
    return a;
}

CpuAffinity CpuAffinity::range(unsigned int first, unsigned int lastInclusive)
{
    CpuAffinity a;
    a._any = false;
    if (lastInclusive < first)
    {
        a._any = true;
        return a;
    }
    a._cores.reserve(static_cast<std::size_t>(lastInclusive - first) + 1u);
    for (unsigned int i = first; i <= lastInclusive; ++i)
    {
        a._cores.push_back(i);
    }
    return a;
}

bool CpuAffinity::contains(unsigned int id) const
{
    if (_any)
    {
        return true;
    }
    return std::binary_search(_cores.begin(), _cores.end(), id);
}

std::uint64_t CpuAffinity::toBitMask() const
{
    if (_any)
    {
        return 0;
    }
    std::uint64_t mask = 0;
    for (const unsigned int id : _cores)
    {
        if (id < 64u)
        {
            mask |= (std::uint64_t{1} << id);
        }
    }
    return mask;
}

} // namespace tools::os::thread
