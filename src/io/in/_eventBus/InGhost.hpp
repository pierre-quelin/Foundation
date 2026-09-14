/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file InGhost.hpp
 * @brief In over EventBus / ITransport (no local hardware).
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
#include "util/json/Json.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace io::in
{

class InGhost : public tools::design::factory::IObject,
                public IIn,
                public tools::design::factory::ILaunchable
{
public:
    InGhost(tools::design::ApplicationServices& app, tools::design::config::Node node);

    ~InGhost() override;

    InGhost(const InGhost&)            = delete;
    InGhost& operator=(const InGhost&) = delete;

    void launch() override;

    void start();
    void stop();

    int init() override;
    int get(unsigned int& value) const override;

private:
    struct PendingCmd
    {
        std::mutex mutex;
        std::condition_variable cv;
        bool done = false;
        bool ok   = false;
    };

    [[nodiscard]] bool busOnline() const;
    [[nodiscard]] std::string nextCorr();
    int sendCmd(const util::json::Json& body);
    void onValue(const tools::design::ipc::Envelope& env);
    void onRep(const tools::design::ipc::Envelope& env);
    void onStatus(const tools::design::ipc::Envelope& env);

    tools::design::ipc::IEventBus& _bus;
    tools::design::ipc::ITransport& _transport;
    InTopics _topics;

    mutable std::mutex _mutex;
    bool _started       = false;
    bool _ready         = false;
    bool _hasValue      = false;
    unsigned int _value = 0;

    tools::design::ipc::SubscriptionId _valueSub  = 0;
    tools::design::ipc::SubscriptionId _repSub    = 0;
    tools::design::ipc::SubscriptionId _statusSub = 0;

    std::atomic<std::uint64_t> _corr{1};
    std::unordered_map<std::string, std::shared_ptr<PendingCmd>> _pending;
};

} // namespace io::in
