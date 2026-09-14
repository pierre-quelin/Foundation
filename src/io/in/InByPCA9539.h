/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file InByPCA9539.h
 * @brief A PCA9539 I/O Input. Can aggregate several inputs.
 */
#pragma once

#include "driver/chip/PCA9539.h"
#include "io/in/IIn.h"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/signal/Signal.hpp"

#include <memory>
#include <utility>
#include <vector>

namespace tools::design
{
struct ApplicationServices;
}

namespace tools::design::config
{
class Node;
}

namespace io::in
{

class InByPCA9539 : public tools::design::factory::IObject, public IIn
{
public:
    ~InByPCA9539() override = default;
    InByPCA9539(tools::design::ApplicationServices& app,
                tools::design::config::Node node);
    InByPCA9539(const InByPCA9539&)            = delete;
    InByPCA9539& operator=(const InByPCA9539&) = delete;
    InByPCA9539(InByPCA9539&&)                 = delete;
    InByPCA9539& operator=(InByPCA9539&&)      = delete;

    int init() override;

    int get(unsigned int& value) const override;

    int refreshValues(uint16_t reg);

private:
    InByPCA9539(const std::shared_ptr<driver::chip::PCA9539> device,
                const std::vector<std::pair<driver::chip::PCA9539::Pin, driver::chip::PCA9539::Polarity>>& pins);

    /**
     * @brief Computes the input value from the register value.
     *
     * @param reg The register value
     * @return The input value
     */
    [[nodiscard]] unsigned int computeIn(uint16_t reg) const;

    std::shared_ptr<driver::chip::PCA9539> _device;
    std::vector<std::pair<driver::chip::PCA9539::Pin, driver::chip::PCA9539::Polarity>> _pins;

    unsigned int _value;
    // boost::signals2::scoped_connection _cnx; // RAII
    tools::design::signal::ScopedConnection _cnx;
};

} // namespace io::in
