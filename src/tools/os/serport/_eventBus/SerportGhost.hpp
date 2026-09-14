/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SerportGhost.hpp
 * @brief ISerport over EventBus / ITransport (no local hardware).
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
#include "util/json/Json.hpp"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace tools::os::serport
{

class SerportGhost : public tools::design::factory::IObject,
                     public ISerport,
                     public tools::design::factory::ILaunchable
{
public:
    SerportGhost(tools::design::ApplicationServices& app, tools::design::config::Node node);

    ~SerportGhost() override;

    SerportGhost(const SerportGhost&)            = delete;
    SerportGhost& operator=(const SerportGhost&) = delete;

    void launch() override;

    void start();
    void stop();

    [[nodiscard]] int read(char* buffer, unsigned int size) override;
    [[nodiscard]] int read(char* buffer, unsigned int size, util::chrono::Delay delay) override;
    [[nodiscard]] int write(const char* buffer, unsigned int size) override;
    [[nodiscard]] int getNRead() override;
    [[nodiscard]] int getNWrite() override;
    int flush() override;
    int wflush() override;
    int rflush() override;
    int cancel() override;
    int setParams(BitRate bRate, DataBit nData, Parity parity, StopBit nStop) override;
    int setFlowCtrl(FlowCtrl flowCtrl) override;
    [[nodiscard]] bool isReady() const override;
    int reset() override;

private:
    struct PendingCmd
    {
        std::mutex mutex;
        std::condition_variable cv;
        bool done = false;
        bool ok   = false;
        std::string body;
    };

    [[nodiscard]] bool busOnline() const;
    [[nodiscard]] std::string nextCorr();
    [[nodiscard]] bool waitCmd(const std::string& corr,
                               util::chrono::Delay timeout,
                               std::string* bodyOut);
    int sendCmd(const util::json::Json& body, util::chrono::Delay timeout);
    void onRx(const tools::design::ipc::Envelope& env);
    void onRep(const tools::design::ipc::Envelope& env);
    void onStatus(const tools::design::ipc::Envelope& env);

    tools::design::ipc::IEventBus& _bus;
    tools::design::ipc::ITransport& _transport;
    SerportTopics _topics;

    mutable std::mutex _mutex;
    bool _started = false;
    bool _ready   = false;
    std::deque<char> _rx;

    tools::design::ipc::SubscriptionId _rxSub     = 0;
    tools::design::ipc::SubscriptionId _repSub    = 0;
    tools::design::ipc::SubscriptionId _statusSub = 0;

    std::atomic<std::uint64_t> _corr{1};
    std::unordered_map<std::string, std::shared_ptr<PendingCmd>> _pending;
};

} // namespace tools::os::serport
