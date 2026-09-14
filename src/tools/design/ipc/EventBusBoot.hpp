/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file EventBusBoot.hpp
 * @brief Optional boot helper: factory-create EventBus, start, optional synchronize.
 *
 * Call from application boot (Phase 7.5). Remains optional — @c ApplicationServices::eventBus
 * stays null until this helper (or equivalent) runs.
 */
#pragma once

#include "tools/design/config/Node.hpp"
#include "tools/design/config/ResolvePlatform.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Obtain.hpp"
#include "tools/design/ipc/IEventBus.hpp"
#include "util/chrono/Delay.hpp"

#include <string>
#include <utility>
#include <vector>

namespace tools::design::ipc
{

/**
 * @brief Create EventBus from @p node, @c start(), optional @c synchronize, set @c app.eventBus.
 *
 * Applies optional @c Platform merge. Reads optional @c ExpectedPlatforms (string array) and
 * @c SynchronizeTimeout (Delay, default 30s).
 */
inline void wireEventBus(ApplicationServices& app, config::Node node)
{
    config::PlatformResolvedNode resolved(node, app.platformName);
    const config::Node effective = resolved.get();

    auto bus = factory::createShared<IEventBus>(app, effective);
    bus->start();

    if (effective.contains("ExpectedPlatforms") && effective["ExpectedPlatforms"].is_array())
    {
        std::vector<std::string> expected;
        const auto& arr = effective["ExpectedPlatforms"];
        expected.reserve(arr.size());
        for (std::size_t i = 0; i < arr.size(); ++i)
        {
            expected.push_back(arr[i].value<std::string>());
        }
        const auto timeout =
            effective.value_or("SynchronizeTimeout", util::chrono::Delay::parse("30s"));
        bus->synchronize(expected, timeout);
    }

    app.eventBus = std::move(bus);
}

} // namespace tools::design::ipc
