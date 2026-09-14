/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file OutByPCA9633.h
 * @brief A PCA9633 4-bit LED driver as Output. Can aggregate several outputs.
 */
#pragma once

#include "driver/chip/PCA9633.h"
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

class OutByPCA9633 : public tools::design::factory::IObject, public IOut
{
public:
    ~OutByPCA9633() override;
    OutByPCA9633(tools::design::ApplicationServices& app,
                 tools::design::config::Node node);
    OutByPCA9633(const OutByPCA9633&)            = delete;
    OutByPCA9633& operator=(const OutByPCA9633&) = delete;
    OutByPCA9633(OutByPCA9633&&)                 = delete;
    OutByPCA9633& operator=(OutByPCA9633&&)      = delete;

    int init() override;

    int set(unsigned int value) override;
    int get(unsigned int& value) const override;

private:
    OutByPCA9633(const std::shared_ptr<driver::chip::PCA9633> device,
                 const std::vector<driver::chip::PCA9633::Led>& pins);

    std::shared_ptr<driver::chip::PCA9633> _device;
    std::vector<driver::chip::PCA9633::Led> _leds;

    unsigned int _value;
    mutable std::shared_mutex _mutex;
};

} // namespace io::out
