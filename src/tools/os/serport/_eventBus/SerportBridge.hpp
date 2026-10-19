/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SerportBridge.hpp
 * @brief Hosts a real ISerport and mirrors it on the bus.
 */
#pragma once

#include "tools/design/config/Node.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/ILaunchable.hpp"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/ipc/IEventBus.hpp"
#include "tools/design/ipc/ITransport.hpp"
#include "tools/os/serport/ISerport.h"
#include "tools/os/serport/SerportTopics.hpp"
#include "tools/os/thread/Thread.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace tools::os::serport
{

class SerportBridge : public tools::design::factory::IObject,
                      public tools::design::factory::ILaunchable
{
public:
    SerportBridge(tools::design::ApplicationServices& app, tools::design::config::Node node);
    SerportBridge(tools::design::ApplicationServices& app,
                  tools::design::config::Node node,
                  std::shared_ptr<ISerport> instance);

    ~SerportBridge() override;

    SerportBridge(const SerportBridge&)            = delete;
    SerportBridge& operator=(const SerportBridge&) = delete;

    void launch() override;

    void start();
    void stop();

private:
    void onTx(const tools::design::ipc::Envelope& env);
    void onCmd(const tools::design::ipc::Envelope& env);
    void publishStatus(bool ready);
    void rxLoop();

    tools::design::ipc::IEventBus& _bus;
    tools::design::ipc::ITransport& _transport;
    SerportTopics _topics;
    std::shared_ptr<ISerport> _ownedInstance;
    ISerport& _instance;

    mutable std::mutex _mutex;
    bool _started = false;

    tools::design::ipc::SubscriptionId _txSub  = 0;
    tools::design::ipc::SubscriptionId _cmdSub = 0;

    std::atomic<bool> _rxRunning{false};
    std::unique_ptr<tools::os::thread::Thread> _rxThread;
};

} // namespace tools::os::serport
