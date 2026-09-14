/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file MyDevice_mh.cpp
 * @brief Device-thread message handlers (EventScheduler).
 */

#include "io/out/IOut.h"
#include "sample/statemachine/MyDevice.h"
#include "sample/statemachine/MyDevice_sm.h"

namespace sample::statemachine
{

bool MyDevice::isReady()
{
    return _isReady;
}

bool MyDevice::canTransfer()
{
    return _canTransfer;
}

bool MyDevice::isBlocked()
{
    return _isBlocked;
}

void MyDevice::readyEntry() {}

void MyDevice::idleEntry()
{
    (void)_outTransfer->set(0U);
}

void MyDevice::idleOnHeartbeat() {}

void MyDevice::noTrayEntry()
{
    (void)_outTransfer->set(0U);
}

void MyDevice::trayEntry()
{
    (void)_outTransfer->set(0U);
}

void MyDevice::waitingTrayEntry()
{
    (void)_outTransfer->set(1U);
}

void MyDevice::waitingTrayExit()
{
    (void)_outTransfer->set(0U);
}

void MyDevice::waitingTrayOnBlocked() {}

void MyDevice::trayOnTrayAbsent() {}

void MyDevice::jamEntry() {}

void MyDevice::trayInputOnPresent()
{
    postSmEvent(TrayPresentEvt{});
}

void MyDevice::trayInputOnAbsent()
{
    postSmEvent(TrayAbsentEvt{});
}

void MyDevice::onTransferTimeout()
{
    postSmEvent(TransferTimeoutEvt{});
}

void MyDevice::onTrayInputChanged(const unsigned int value)
{
    if (value != 0U)
    {
        trayInputOnPresent();
    }
    else
    {
        trayInputOnAbsent();
    }
}

} // namespace sample::statemachine
