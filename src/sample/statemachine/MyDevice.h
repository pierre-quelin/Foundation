/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file MyDevice.h
 * @brief Reference pilot — flat + nested state machine patterns (see MyDevice.smd).
 */
#pragma once

#include "io/in/IIn.h"
#include "io/out/IOut.h"
#include "sample/statemachine/MyDevice_sm_fwd.h"
#include "tools/design/config/Node.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/objkit/ObjKit.hpp"
#include "tools/design/signal/Signal.hpp"
#include "tools/design/time/TimeRequest.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace sample::statemachine
{

/**
 * @brief Reference device — user API: @c transfer() / @c reset().
 *
 * @c MyDevice.smd is the Foundation reference (all @c events shapes, composites, timers).
 */
class MyDevice : public tools::design::objkit::ObjKit
{
public:
    struct State
    {
        enum Id : std::uint32_t
        {
            NotReady = 0,
            Ready_Idle,
            Ready_WaitingTray,
            Ready_Tray,
            Ready_NoTray,
            Ready_Jam,
            MAX
        };
    };

    struct Msg
    {
        enum Id : std::uint32_t
        {
            heartbeat = 0,
            transfer,
            reset,
            transferTimeout,
            trayPresent,
            trayAbsent,
            blocked,
            MAX
        };
    };

    using StateMachine = MyDeviceSm;

    tools::design::signal::Signal<void(State::Id)> state;

    MyDevice(tools::design::ApplicationServices& app, tools::design::config::Node node);

    ~MyDevice();

    void transfer();
    void reset();

    [[nodiscard]] State::Id stateId() const
    {
        return static_cast<State::Id>(_stateId.load());
    }

private:
    void trayInputOnPresent();
    void trayInputOnAbsent();
    void onTrayInputChanged(unsigned int value);

    std::shared_ptr<io::in::IIn> _inOccupied;
    std::shared_ptr<io::out::IOut> _outTransfer;
    tools::design::signal::ScopedConnection _inOccupiedCnx;

    std::atomic<State::Id> _stateId{State::Id::NotReady};
    bool _canTransfer{true};
    bool _isReady{true};
    bool _isBlocked{false};

#include "sample/statemachine/MyDevice_sm_decl.h"
};

} // namespace sample::statemachine
