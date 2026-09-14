/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file OutBridge.hpp
 * @brief Hosts a real Out and applies remote set commands.
 */
#pragma once

#include "io/out/IOut.h"
#include "io/out/OutTopics.hpp"
#include "tools/design/config/Node.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/ILaunchable.hpp"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/ipc/IEventBus.hpp"
#include "tools/design/ipc/ITransport.hpp"

#include <memory>
#include <mutex>
#include <string>

namespace io::out
{

class OutBridge : public tools::design::factory::IObject,
                  public tools::design::factory::ILaunchable
{
public:
    OutBridge(tools::design::ApplicationServices& app, tools::design::config::Node node);
    OutBridge(tools::design::ApplicationServices& app,
              tools::design::config::Node node,
              std::shared_ptr<IOut> instance);

    ~OutBridge() override;

    OutBridge(const OutBridge&)            = delete;
    OutBridge& operator=(const OutBridge&) = delete;

    void launch() override;

    void start();
    void stop();

private:
    void onSet(const tools::design::ipc::Envelope& env);
    void onCmd(const tools::design::ipc::Envelope& env);
    void publishValue(unsigned int value);
    void publishStatus(bool ready);

    tools::design::ipc::IEventBus& _bus;
    tools::design::ipc::ITransport& _transport;
    OutTopics _topics;
    std::shared_ptr<IOut> _ownedInstance;
    IOut& _instance;

    mutable std::mutex _mutex;
    bool _started = false;

    tools::design::ipc::SubscriptionId _setSub = 0;
    tools::design::ipc::SubscriptionId _cmdSub = 0;
};

} // namespace io::out
