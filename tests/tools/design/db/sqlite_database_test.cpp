/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/config/Config.hpp"
#include "tools/design/db/ScopedTransaction.hpp"
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

[[nodiscard]] ApplicationServices makeApp(const std::string& json)
{
    ApplicationServices app;
    app.config = createLocalFromString(json);
    return app;
}

} // namespace

BOOST_AUTO_TEST_CASE(sqlite_crud_int_double_text)
{
    const auto path = tempDbPath("foundation_sqlite_crud.db");
    std::filesystem::remove(path);

    const std::string json = std::string(R"({
      "Database": {
        "InstanceOf": "tools::design::db::SqliteDatabase",
        "Path": ")") + path.generic_string() +
                             R"("
      }
    })";

    auto app = makeApp(json);
    SqliteDatabase db(app, app.config->root()["Database"]);
    BOOST_CHECK(!db.isOpen());
    db.open();
    BOOST_CHECK(db.isOpen());
    BOOST_CHECK_EQUAL(db.engineName(), "sqlite");

    db.execute("CREATE TABLE item (id INTEGER PRIMARY KEY, name TEXT, weight REAL)");

    {
        auto insert = db.prepare("INSERT INTO item (id, name, weight) VALUES (?, ?, ?)");
        insert->bindInt64(1, 42);
        insert->bindText(2, "widget");
        insert->bindDouble(3, 1.5);
        insert->execute();
    }

    {
        auto select = db.prepare("SELECT id, name, weight FROM item WHERE id = ?");
        select->bindInt64(1, 42);
        auto rs = select->query();
        BOOST_REQUIRE(rs->next());
        const IRow& row = rs->row();
        BOOST_CHECK_EQUAL(row.getInt64(0), 42);
        BOOST_CHECK_EQUAL(row.getText("name"), "widget");
        BOOST_CHECK_CLOSE(row.getDouble(2), 1.5, 0.0001);
        BOOST_CHECK(!rs->next());
    }

    db.close();
    BOOST_CHECK(!db.isOpen());
    std::filesystem::remove(path);
}

BOOST_AUTO_TEST_CASE(sqlite_transaction_commit_and_rollback)
{
    const auto path = tempDbPath("foundation_sqlite_tx.db");
    std::filesystem::remove(path);

    const std::string json = std::string(R"({
      "Database": {
        "Path": ")") + path.generic_string() +
                             R"("
      }
    })";

    auto app = makeApp(json);
    SqliteDatabase db(app, app.config->root()["Database"]);
    db.open();
    db.execute("CREATE TABLE t (v INTEGER)");

    {
        ScopedTransaction tx(db.begin());
        auto stmt = db.prepare("INSERT INTO t (v) VALUES (?)");
        stmt->bindInt64(1, 1);
        stmt->execute();
        tx.commit();
    }

    {
        ScopedTransaction tx(db.begin());
        auto stmt = db.prepare("INSERT INTO t (v) VALUES (?)");
        stmt->bindInt64(1, 2);
        stmt->execute();
    }

    {
        auto count = db.prepare("SELECT COUNT(*) AS c FROM t");
        auto rs    = count->query();
        BOOST_REQUIRE(rs->next());
        BOOST_CHECK_EQUAL(rs->row().getInt64("c"), 1);
    }

    db.close();
    std::filesystem::remove(path);
}

BOOST_AUTO_TEST_CASE(sqlite_memory_database)
{
    constexpr const char* json = R"({
      "Database": { "Path": ":memory:" }
    })";
    auto app                   = makeApp(json);
    SqliteDatabase db(app, app.config->root()["Database"]);
    db.open();
    db.execute("CREATE TABLE x (a INTEGER)");
    db.execute("INSERT INTO x (a) VALUES (7)");
    {
        auto stmt = db.prepare("SELECT a FROM x");
        auto rs   = stmt->query();
        BOOST_REQUIRE(rs->next());
        BOOST_CHECK_EQUAL(rs->row().getInt64(0), 7);
    }
    db.close();
}
