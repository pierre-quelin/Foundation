/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file MyDevice.cpp
 * @brief User-thread API — transfer / reset post events; I/O wired at construction.
 */

#include "sample/statemachine/MyDevice.h"

#include "sample/statemachine/MyDevice_sm.h"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/time/ITimeManager.hpp"
#include "util/chrono/Delay.hpp"

#include <chrono>
#include <exception>

namespace sample::statemachine
{

MyDevice::MyDevice(tools::design::ApplicationServices& app, tools::design::config::Node node) : ObjKit(app, std::move(node)), _transferTimeout(config().value_or("TransferTimeout",
                                                                                                                                                                 util::chrono::Delay{std::chrono::milliseconds(50)}),
                                                                                                                                               false,
                                                                                                                                               "MyDeviceTransferTimeout")
{
    using namespace tools::design::factory;

    needStateMachine(_stateMachine);
    static_cast<MyDeviceSm_&>(*_stateMachine).owner = this;
    _transferTimeout.setTarget(scheduler(), &MyDevice::onTransferTimeout, *this);

    _inOccupied  = obtain<io::in::IIn>(services(), config(), "InputOccupied",
                                       /*createIfMissing=*/true);
    _outTransfer = obtain<io::out::IOut>(services(), config(), "OutputTransfer",
                                         /*createIfMissing=*/true);

    _canTransfer = config().value_or("CanTransfer", true);
    _isReady     = config().value_or("IsReady", true);
    _isBlocked   = config().value_or("IsBlocked", false);

    _inOccupiedCnx = _inOccupied->valueChanged.connect([this](const unsigned int value)
                                                       { frontEnd().schedule([this, value]()
                                                                             { onTrayInputChanged(value); }); });

    startStateMachine(*_stateMachine);
}

MyDevice::~MyDevice()
{
    _inOccupiedCnx.disconnect();
    try
    {
        if (_transferTimeout.isRunning())
        {
            services().timeManagerService().cancel(_transferTimeout);
        }
    }
    catch (const std::exception&)
    {
    }
    (void)drainScheduler();
}

void MyDevice::transfer()
{
    postSmEvent(TransferEvt{});
}

void MyDevice::reset()
{
    postSmEvent(ResetEvt{});
}

} // namespace sample::statemachine
