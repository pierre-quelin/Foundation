/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file DatabaseBoot.hpp
 * @brief Optional boot helper: factory-create, open, migrate, assign ApplicationServices::db.
 */
#pragma once

#include "tools/design/config/Node.hpp"
#include "tools/design/db/IDatabase.hpp"
#include "tools/design/db/SchemaMigrator.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/Obtain.hpp"

#include <filesystem>
#include <utility>

namespace tools::design::db
{

/**
 * @brief Create database from @p node, open it, optionally migrate, set @c app.db.
 *
 * Reads optional @c MigrationsDir and @c TargetSchemaVersion from @p node.
 * Does not run unless the application calls this helper (Database remains optional).
 */
inline void wireDatabase(ApplicationServices& app, config::Node node)
{
    auto database = factory::createShared<IDatabase>(app, node);
    database->open();

    if (node.contains("MigrationsDir") && node.contains("TargetSchemaVersion"))
    {
        const auto migrationsDir =
            std::filesystem::path(node["MigrationsDir"].value<std::string>());
        const int target = node["TargetSchemaVersion"].value<int>();
        SchemaMigrator migrator(*database, migrationsDir);
        migrator.migrateTo(target);
    }

    app.db = std::move(database);
}

} // namespace tools::design::db
