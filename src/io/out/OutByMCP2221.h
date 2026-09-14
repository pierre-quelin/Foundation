/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file OutByMCP2221.h
 * @brief A MCP2221 I/O Output. Can aggregate several outputs.
 */
#pragma once

#include "driver/chip/MCP2221.h"
#include "io/out/IOut.h"
#include "tools/design/factory/IObject.hpp"

#include <memory>
#include <shared_mutex>
#include <vector>

namespace tools::design
{
struct ApplicationServices;
}

namespace tools::design::config
{
class Node;
}

namespace io::out
{

class OutByMCP2221 : public tools::design::factory::IObject, public IOut
{
public:
    ~OutByMCP2221() override;
    OutByMCP2221(tools::design::ApplicationServices& app,
                 tools::design::config::Node node);
    OutByMCP2221(const OutByMCP2221&)            = delete;
    OutByMCP2221& operator=(const OutByMCP2221&) = delete;
    OutByMCP2221(OutByMCP2221&&)                 = delete;
    OutByMCP2221& operator=(OutByMCP2221&&)      = delete;

    int init() override;

    int set(const unsigned int value) override;
    int get(unsigned int& value) const override;

private:
    OutByMCP2221(const std::shared_ptr<driver::chip::MCP2221> device,
                 const std::vector<driver::chip::MCP2221::GPPin>& pins);

    std::shared_ptr<driver::chip::MCP2221> _device;
    std::vector<driver::chip::MCP2221::GPPin> _pins;

    unsigned int _value;
    mutable std::shared_mutex _mutex;
};

} // namespace io::out
