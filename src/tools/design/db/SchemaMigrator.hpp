/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SchemaMigrator.hpp
 * @brief Ordered schema upgrades via SQL scripts and schema_version table.
 */
#pragma once

#include "tools/design/db/IDatabase.hpp"
#include "tools/design/db/IMigrator.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace tools::design::db
{

/**
 * @brief Discovers @c NNN_name.sql scripts under a directory and applies them in order.
 *
 * Creates @c schema_version when missing. Upgrade only (no downgrade).
 */
class SchemaMigrator : public IMigrator
{
public:
    SchemaMigrator(IDatabase& db, std::filesystem::path migrationsDir);

    [[nodiscard]] int currentVersion() override;
    void migrateTo(int targetVersion) override;

private:
    struct MigrationScript
    {
        int version = 0;
        std::string name;
        std::filesystem::path path;
    };

    void ensureSchemaVersionTable();
    [[nodiscard]] std::vector<MigrationScript> listScripts() const;
    [[nodiscard]] static std::string readFile(const std::filesystem::path& path);
    void applyScript(const MigrationScript& script);

    IDatabase& _db;
    std::filesystem::path _migrationsDir;
};

} // namespace tools::design::db
