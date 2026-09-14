/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/config/Config.hpp"
#include "tools/design/db/DatabaseBoot.hpp"
#include "tools/design/db/SchemaMigrator.hpp"
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

} // namespace

BOOST_AUTO_TEST_CASE(database_service_optional_null_by_default)
{
    ApplicationServices app;
    BOOST_CHECK(app.db == nullptr);
}

BOOST_AUTO_TEST_CASE(database_factory_wire_open_migrate_query)
{
    const auto path = tempDbPath("foundation_db_factory.db");
    std::filesystem::remove(path);

    const std::string json =
        std::string(R"({
      "Database": {
        "InstanceOf": "tools::design::db::SqliteDatabase",
        "Path": ")") +
        path.generic_string() + R"(",
        "MigrationsDir": ")" +
        migrationsDir().generic_string() + R"(",
        "TargetSchemaVersion": 2
      }
    })";

    auto app = makeApp(json);
    BOOST_CHECK(app.db == nullptr);

    wireDatabase(app, app.config->root()["Database"]);

    BOOST_REQUIRE(app.db != nullptr);
    BOOST_CHECK(app.db->isOpen());
    BOOST_CHECK_EQUAL(app.db->engineName(), "sqlite");

    SchemaMigrator migrator(*app.db, migrationsDir());
    BOOST_CHECK_EQUAL(migrator.currentVersion(), 2);

    app.db->execute("INSERT INTO app_item (id, name) VALUES (1, 'wired')");
    {
        auto stmt = app.db->prepare("SELECT name FROM app_item WHERE id = 1");
        auto rs   = stmt->query();
        BOOST_REQUIRE(rs->next());
        BOOST_CHECK_EQUAL(rs->row().getText(0), "wired");
    }

    app.db->close();
    app.db.reset();
    std::filesystem::remove(path);
}
