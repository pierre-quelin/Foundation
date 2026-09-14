/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file InBridge.hpp
 * @brief Hosts a real In and mirrors values on the bus.
 */
#pragma once

#include "io/in/IIn.h"
#include "io/in/InTopics.hpp"
#include "tools/design/config/Node.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/ILaunchable.hpp"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/ipc/IEventBus.hpp"
#include "tools/design/ipc/ITransport.hpp"
#include "tools/design/signal/Signal.hpp"

#include <memory>
#include <mutex>
#include <string>

namespace io::in
{

class InBridge : public tools::design::factory::IObject,
                 public tools::design::factory::ILaunchable
{
public:
    InBridge(tools::design::ApplicationServices& app, tools::design::config::Node node);
    InBridge(tools::design::ApplicationServices& app,
             tools::design::config::Node node,
             std::shared_ptr<IIn> instance);

    ~InBridge() override;

    InBridge(const InBridge&)            = delete;
    InBridge& operator=(const InBridge&) = delete;

    void launch() override;

    void start();
    void stop();

private:
    void onCmd(const tools::design::ipc::Envelope& env);
    void publishValue(unsigned int value);
    void publishStatus(bool ready);

    tools::design::ipc::IEventBus& _bus;
    tools::design::ipc::ITransport& _transport;
    InTopics _topics;
    std::shared_ptr<IIn> _ownedInstance;
    IIn& _instance;

    mutable std::mutex _mutex;
    bool _started = false;

    tools::design::ipc::SubscriptionId _cmdSub = 0;
    tools::design::signal::ScopedConnection _valueConn;
};

} // namespace io::in
