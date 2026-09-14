/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file LocalConfigCenter.hpp
 * @brief In-memory JSON configuration (Phase 1 implementation).
 */
#pragma once

#include "tools/design/config/IConfigCenter.hpp"
#include "tools/design/config/Node.hpp"
#include "tools/os/sync/SemM.hpp"
#include "util/json/Json.hpp"

namespace tools::design::config
{

class LocalConfigCenter final : public IConfigCenter
{
public:
    explicit LocalConfigCenter(util::json::Json root);

    [[nodiscard]] Node root() override;
    [[nodiscard]] Node root() const override;

    [[nodiscard]] std::string toJsonString() const override;
    void save(const std::filesystem::path& path) const override;

private:
    mutable tools::os::sync::SemM _mutex;
    util::json::Json _root;
};

} // namespace tools::design::config
