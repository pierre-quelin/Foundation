/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file OutByPCA9635.h
 * @brief A PCA9635 16-bit LED driver as Output. Can aggregate several outputs.
 */
#pragma once

#include "driver/chip/PCA9635.h"
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

class OutByPCA9635 : public tools::design::factory::IObject, public IOut
{
public:
    ~OutByPCA9635() override;
    OutByPCA9635(tools::design::ApplicationServices& app,
                 tools::design::config::Node node);
    OutByPCA9635(const OutByPCA9635&)            = delete;
    OutByPCA9635& operator=(const OutByPCA9635&) = delete;
    OutByPCA9635(OutByPCA9635&&)                 = delete;
    OutByPCA9635& operator=(OutByPCA9635&&)      = delete;

    int init() override;

    int set(unsigned int value) override;
    int get(unsigned int& value) const override;

private:
    OutByPCA9635(const std::shared_ptr<driver::chip::PCA9635> device,
                 const std::vector<driver::chip::PCA9635::Led>& pins);

    std::shared_ptr<driver::chip::PCA9635> _device;
    std::vector<driver::chip::PCA9635::Led> _leds;

    unsigned int _value;
    mutable std::shared_mutex _mutex;
};

} // namespace io::out
