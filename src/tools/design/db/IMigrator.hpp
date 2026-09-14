/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file IMigrator.hpp
 * @brief Schema version migration interface.
 */
#pragma once

namespace tools::design::db
{

/**
 * @brief Applies ordered upgrade scripts up to a target schema version.
 *
 * Upgrade only (no automatic downgrade). Table @c schema_version tracks progress.
 */
class IMigrator
{
public:
    virtual ~IMigrator() = default;

    [[nodiscard]] virtual int currentVersion() = 0;

    /**
     * @brief Apply migrations until @p targetVersion (inclusive).
     * No-op when already at or past target (past target → error in implementations).
     */
    virtual void migrateTo(int targetVersion) = 0;
};

} // namespace tools::design::db
