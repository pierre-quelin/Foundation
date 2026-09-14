/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file OutByPCA9539.h
 * @brief A PCA9539 I/O Output. Can aggregate several outputs.
 */
#pragma once

#include "driver/chip/PCA9539.h"
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

class OutByPCA9539 : public tools::design::factory::IObject, public IOut
{
public:
    ~OutByPCA9539() override;
    OutByPCA9539(tools::design::ApplicationServices& app,
                 tools::design::config::Node node);
    OutByPCA9539(const OutByPCA9539&)            = delete;
    OutByPCA9539& operator=(const OutByPCA9539&) = delete;
    OutByPCA9539(OutByPCA9539&&)                 = delete;
    OutByPCA9539& operator=(OutByPCA9539&&)      = delete;

    int init() override;

    int set(const unsigned int value) override;
    int get(unsigned int& value) const override;

private:
    OutByPCA9539(const std::shared_ptr<driver::chip::PCA9539> device,
                 const std::vector<driver::chip::PCA9539::Pin>& pins);

    std::shared_ptr<driver::chip::PCA9539> _device;
    std::vector<driver::chip::PCA9539::Pin> _pins;

    unsigned int _value;
    mutable std::shared_mutex _mutex;
};

} // namespace io::out
