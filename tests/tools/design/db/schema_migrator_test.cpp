/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/config/Config.hpp"
#include "tools/design/db/DbException.hpp"
#include "tools/design/db/SchemaMigrator.hpp"
#include "tools/design/db/SqliteDatabase.hpp"
#include "tools/design/factory/ApplicationServices.hpp"

#include <boost/test/unit_test.hpp>

#include <filesystem>

using namespace tools::design;
using namespace tools::design::config;
using namespace tools::design::db;
using namespace tools::design::factory;

namespace
{

[[nodiscard]] std::filesystem::path tempDbPath(const char* name)
{
    return std::filesystem::temp_directory_path() / name;
}

[[nodiscard]] std::filesystem::path migrationsDir()
{
    return std::filesystem::path(FOUNDATION_SOURCE_DIR) / "tests" / "tools" / "design" / "db" / "migrations";
}

[[nodiscard]] ApplicationServices makeApp(const std::string& json)
{
    ApplicationServices app;
    app.config = createLocalFromString(json);
    return app;
}

[[nodiscard]] std::unique_ptr<SqliteDatabase> openTempDb(ApplicationServices& app,
                                                         const std::filesystem::path& path)
{
    const std::string json = std::string(R"({
      "Database": {
        "Path": ")") + path.generic_string() +
                             R"("
      }
    })";
    app                    = makeApp(json);
    auto db                = std::make_unique<SqliteDatabase>(app, app.config->root()["Database"]);
    db->open();
    return db;
}

[[nodiscard]] bool tableHasColumn(IDatabase& db, const char* table, const char* column)
{
    auto stmt = db.prepare("PRAGMA table_info(" + std::string(table) + ")");
    auto rs   = stmt->query();
    while (rs->next())
    {
        if (rs->row().getText("name") == column)
        {
            return true;
        }
    }
    return false;
}

} // namespace

BOOST_AUTO_TEST_CASE(schema_migrator_fresh_to_v2)
{
    const auto path = tempDbPath("foundation_migrator_fresh.db");
    std::filesystem::remove(path);

    ApplicationServices app;
    auto db = openTempDb(app, path);
    SchemaMigrator migrator(*db, migrationsDir());

    BOOST_CHECK_EQUAL(migrator.currentVersion(), 0);
    migrator.migrateTo(2);
    BOOST_CHECK_EQUAL(migrator.currentVersion(), 2);

    BOOST_CHECK(tableHasColumn(*db, "app_item", "name"));
    BOOST_CHECK(tableHasColumn(*db, "app_item", "created_at"));
    BOOST_CHECK(tableHasColumn(*db, "app_item", "updated_at"));

    db->close();
    std::filesystem::remove(path);
}

BOOST_AUTO_TEST_CASE(schema_migrator_v1_then_v2)
{
    const auto path = tempDbPath("foundation_migrator_step.db");
    std::filesystem::remove(path);

    ApplicationServices app;
    auto db = openTempDb(app, path);
    SchemaMigrator migrator(*db, migrationsDir());

    migrator.migrateTo(1);
    BOOST_CHECK_EQUAL(migrator.currentVersion(), 1);
    BOOST_CHECK(tableHasColumn(*db, "app_item", "name"));
    BOOST_CHECK(!tableHasColumn(*db, "app_item", "created_at"));

    migrator.migrateTo(2);
    BOOST_CHECK_EQUAL(migrator.currentVersion(), 2);
    BOOST_CHECK(tableHasColumn(*db, "app_item", "created_at"));
    BOOST_CHECK(tableHasColumn(*db, "app_item", "updated_at"));

    db->close();
    std::filesystem::remove(path);
}

BOOST_AUTO_TEST_CASE(schema_migrator_already_at_target_noop)
{
    const auto path = tempDbPath("foundation_migrator_noop.db");
    std::filesystem::remove(path);

    ApplicationServices app;
    auto db = openTempDb(app, path);
    SchemaMigrator migrator(*db, migrationsDir());

    migrator.migrateTo(2);
    migrator.migrateTo(2);
    BOOST_CHECK_EQUAL(migrator.currentVersion(), 2);

    db->close();
    std::filesystem::remove(path);
}

BOOST_AUTO_TEST_CASE(schema_migrator_db_newer_than_target_throws)
{
    const auto path = tempDbPath("foundation_migrator_newer.db");
    std::filesystem::remove(path);

    ApplicationServices app;
    auto db = openTempDb(app, path);
    SchemaMigrator migrator(*db, migrationsDir());

    migrator.migrateTo(2);
    db->execute(
        "INSERT INTO schema_version (version, applied_at, name) "
        "VALUES (3, datetime('now'), 'fake_future')");
    BOOST_CHECK_EQUAL(migrator.currentVersion(), 3);
    BOOST_CHECK_THROW(migrator.migrateTo(2), DbException);

    db->close();
    std::filesystem::remove(path);
}
