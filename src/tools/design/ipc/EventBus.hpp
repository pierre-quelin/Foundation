/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file EventBus.hpp
 * @brief EventBus implementation over ITransport + EventScheduler.
 */
#pragma once

#include "tools/design/config/Node.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/IObject.hpp"
#include "tools/design/ipc/IEventBus.hpp"
#include "tools/design/ipc/ITransport.hpp"
#include "tools/design/ipc/TopicScheme.hpp"
#include "tools/design/scheduler/EventScheduler.hpp"

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tools::design::ipc
{

class EventBus : public tools::design::factory::IObject, public IEventBus
{
public:
    EventBus(ITransport& transport,
             TopicScheme topics,
             std::string platformName,
             std::shared_ptr<scheduler::EventScheduler> scheduler);

    /** @brief Factory constructor — builds transport from @c Transport or Broker/Tls fields. */
    EventBus(tools::design::ApplicationServices& app, tools::design::config::Node node);

    ~EventBus() override;

    EventBus(const EventBus&)            = delete;
    EventBus& operator=(const EventBus&) = delete;

    void start() override;
    void stop() override;

    [[nodiscard]] LinkState linkState() const override;
    [[nodiscard]] bool peerOnline(std::string_view platform) const override;

    void synchronize(const std::vector<std::string>& expectedPlatforms,
                     util::chrono::Delay timeout) override;

    void publish(std::string_view objectPath,
                 std::string_view event,
                 std::string_view payload,
                 bool retained = false) override;

    [[nodiscard]] EventBusSubscriptionId subscribe(std::string_view objectPath,
                                                   std::string_view event,
                                                   EventHandler handler) override;
    void unsubscribe(EventBusSubscriptionId id) override;

    void onRequest(std::string_view objectPath,
                   std::string_view event,
                   RequestHandler handler) override;

    [[nodiscard]] std::string request(std::string_view objectPath,
                                      std::string_view event,
                                      std::string_view payload,
                                      util::chrono::Delay timeout) override;

    [[nodiscard]] const std::string& platformName() const noexcept { return _platform; }

    [[nodiscard]] ITransport& transport() override { return _transport; }
    [[nodiscard]] const TopicScheme& topicScheme() const override { return _topics; }

private:
    struct BusSubscription
    {
        std::string objectPath;
        std::string event;
        EventHandler handler;
    };

    struct PendingRequest
    {
        std::mutex mutex;
        std::condition_variable cv;
        bool done = false;
        std::string reply;
    };

    void setLinkState(LinkState state);
    void setPeerOnline(const std::string& platform, bool online);
    void onTransportEnvelope(const Envelope& envelope);
    void dispatchEvent(const EventId& id, const std::string& payload);
    void handleRequestMessage(const EventId& id, const std::string& corr, const std::string& data);
    void handleReplyMessage(const std::string& corr, const std::string& data);
    [[nodiscard]] std::string makeCorrId();
    [[nodiscard]] static std::string handlerKey(std::string_view objectPath, std::string_view event);

    std::shared_ptr<ITransport> _ownedTransport;
    ITransport& _transport;
    TopicScheme _topics;
    std::string _platform;
    std::shared_ptr<scheduler::EventScheduler> _scheduler;

    mutable std::mutex _mutex;
    LinkState _linkState        = LinkState::Offline;
    SubscriptionId _evtSubId    = 0;
    SubscriptionId _statusSubId = 0;

    EventBusSubscriptionId _nextBusSubId = 1;
    std::unordered_map<EventBusSubscriptionId, BusSubscription> _busSubs;
    std::unordered_map<std::string, RequestHandler> _requestHandlers; // key object|event
    std::unordered_map<std::string, bool> _peers;
    std::unordered_map<std::string, std::shared_ptr<PendingRequest>> _pending;

    std::atomic<std::uint64_t> _corrCounter{1};
};

} // namespace tools::design::ipc
