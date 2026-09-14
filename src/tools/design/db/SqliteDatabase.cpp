/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SqliteDatabase.cpp
 * @brief SQLite IDatabase / statement / result / transaction.
 */
#include "tools/design/db/SqliteDatabase.hpp"

#include "tools/design/db/DbException.hpp"
#include "tools/design/db/IResultSet.hpp"
#include "tools/design/db/IStatement.hpp"
#include "tools/design/db/ITransaction.hpp"
#include "tools/design/factory/Register.hpp"

#include <sqlite3.h>

#include <cstring>
#include <utility>
#include <vector>

namespace tools::design::db
{
namespace
{

[[noreturn]] void throwSqlite(sqlite3* db, int rc, const char* context)
{
    const char* msg = (db != nullptr) ? sqlite3_errmsg(db) : sqlite3_errstr(rc);
    throw DbException(std::string(context) + ": " + (msg != nullptr ? msg : "unknown error"));
}

class SqliteRow : public IRow
{
public:
    explicit SqliteRow(sqlite3_stmt* stmt) : _stmt(stmt)
    {
    }

    void bind(sqlite3_stmt* stmt) { _stmt = stmt; }

    [[nodiscard]] std::size_t columnCount() const override
    {
        return static_cast<std::size_t>(sqlite3_column_count(_stmt));
    }

    [[nodiscard]] int resolveIndex(std::string_view name) const
    {
        const int n = sqlite3_column_count(_stmt);
        for (int i = 0; i < n; ++i)
        {
            const char* col = sqlite3_column_name(_stmt, i);
            if (col != nullptr && name == col)
            {
                return i;
            }
        }
        throw DbException("SqliteRow: unknown column '" + std::string(name) + "'");
    }

    [[nodiscard]] bool isNull(std::size_t index) const override
    {
        return sqlite3_column_type(_stmt, static_cast<int>(index)) == SQLITE_NULL;
    }

    [[nodiscard]] bool isNull(std::string_view name) const override
    {
        return isNull(static_cast<std::size_t>(resolveIndex(name)));
    }

    [[nodiscard]] std::int64_t getInt64(std::size_t index) const override
    {
        return sqlite3_column_int64(_stmt, static_cast<int>(index));
    }

    [[nodiscard]] std::int64_t getInt64(std::string_view name) const override
    {
        return getInt64(static_cast<std::size_t>(resolveIndex(name)));
    }

    [[nodiscard]] double getDouble(std::size_t index) const override
    {
        return sqlite3_column_double(_stmt, static_cast<int>(index));
    }

    [[nodiscard]] double getDouble(std::string_view name) const override
    {
        return getDouble(static_cast<std::size_t>(resolveIndex(name)));
    }

    [[nodiscard]] std::string getText(std::size_t index) const override
    {
        const auto* text =
            reinterpret_cast<const char*>(sqlite3_column_text(_stmt, static_cast<int>(index)));
        if (text == nullptr)
        {
            return {};
        }
        return std::string(text);
    }

    [[nodiscard]] std::string getText(std::string_view name) const override
    {
        return getText(static_cast<std::size_t>(resolveIndex(name)));
    }

private:
    sqlite3_stmt* _stmt = nullptr;
};

class SqliteResultSet : public IResultSet
{
public:
    explicit SqliteResultSet(sqlite3_stmt* stmt) : _stmt(stmt), _row(stmt)
    {
    }

    bool next() override
    {
        const int rc = sqlite3_step(_stmt);
        if (rc == SQLITE_ROW)
        {
            return true;
        }
        if (rc == SQLITE_DONE)
        {
            return false;
        }
        throwSqlite(sqlite3_db_handle(_stmt), rc, "SqliteResultSet::next");
    }

    [[nodiscard]] const IRow& row() const override { return _row; }

private:
    sqlite3_stmt* _stmt = nullptr;
    SqliteRow _row;
};

class SqliteStatement : public IStatement
{
public:
    SqliteStatement(sqlite3* db, sqlite3_stmt* stmt) : _db(db), _stmt(stmt)
    {
    }

    ~SqliteStatement() override
    {
        if (_stmt != nullptr)
        {
            sqlite3_finalize(_stmt);
            _stmt = nullptr;
        }
    }

    void reset() override
    {
        const int rc = sqlite3_reset(_stmt);
        if (rc != SQLITE_OK)
        {
            throwSqlite(_db, rc, "SqliteStatement::reset");
        }
    }

    void clearBindings() override
    {
        const int rc = sqlite3_clear_bindings(_stmt);
        if (rc != SQLITE_OK)
        {
            throwSqlite(_db, rc, "SqliteStatement::clearBindings");
        }
    }

    void bindNull(int index) override
    {
        const int rc = sqlite3_bind_null(_stmt, index);
        if (rc != SQLITE_OK)
        {
            throwSqlite(_db, rc, "SqliteStatement::bindNull");
        }
    }

    void bindInt64(int index, std::int64_t value) override
    {
        const int rc = sqlite3_bind_int64(_stmt, index, value);
        if (rc != SQLITE_OK)
        {
            throwSqlite(_db, rc, "SqliteStatement::bindInt64");
        }
    }

    void bindDouble(int index, double value) override
    {
        const int rc = sqlite3_bind_double(_stmt, index, value);
        if (rc != SQLITE_OK)
        {
            throwSqlite(_db, rc, "SqliteStatement::bindDouble");
        }
    }

    void bindText(int index, std::string_view value) override
    {
        const int rc = sqlite3_bind_text(_stmt, index, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
        if (rc != SQLITE_OK)
        {
            throwSqlite(_db, rc, "SqliteStatement::bindText");
        }
    }

    void execute() override
    {
        const int rc = sqlite3_step(_stmt);
        if (rc != SQLITE_DONE && rc != SQLITE_ROW)
        {
            throwSqlite(_db, rc, "SqliteStatement::execute");
        }
        reset();
    }

    [[nodiscard]] std::unique_ptr<IResultSet> query() override
    {
        return std::make_unique<SqliteResultSet>(_stmt);
    }

private:
    sqlite3* _db        = nullptr;
    sqlite3_stmt* _stmt = nullptr;
};

class SqliteTransaction : public ITransaction
{
public:
    explicit SqliteTransaction(SqliteDatabase& db) : _db(db)
    {
        _db.execute("BEGIN");
        _open = true;
    }

    ~SqliteTransaction() override
    {
        if (_open)
        {
            try
            {
                rollback();
            }
            catch (...)
            {
            }
        }
    }

    void commit() override
    {
        if (!_open)
        {
            return;
        }
        _db.execute("COMMIT");
        _open = false;
    }

    void rollback() override
    {
        if (!_open)
        {
            return;
        }
        _db.execute("ROLLBACK");
        _open = false;
    }

    [[nodiscard]] bool isOpen() const override { return _open; }

private:
    SqliteDatabase& _db;
    bool _open = false;
};

} // namespace

SqliteDatabase::SqliteDatabase(ApplicationServices& app, config::Node node)
{
    (void)app;
    _path = node.value_or("Path", std::string{":memory:"});
}

SqliteDatabase::~SqliteDatabase()
{
    try
    {
        close();
    }
    catch (...)
    {
    }
}

void SqliteDatabase::throwIfError(int rc, const char* context) const
{
    if (rc != SQLITE_OK)
    {
        throwSqlite(_db, rc, context);
    }
}

void SqliteDatabase::open(ISecretProvider* secrets)
{
    (void)secrets;
    if (_db != nullptr)
    {
        return;
    }

    sqlite3* db = nullptr;
    const int rc =
        sqlite3_open_v2(_path.c_str(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (rc != SQLITE_OK)
    {
        const char* msg = (db != nullptr) ? sqlite3_errmsg(db) : sqlite3_errstr(rc);
        if (db != nullptr)
        {
            sqlite3_close(db);
        }
        throw DbException(std::string("SqliteDatabase::open: ") +
                          (msg != nullptr ? msg : "unknown error"));
    }
    _db = db;
}

void SqliteDatabase::close()
{
    if (_db == nullptr)
    {
        return;
    }
    const int rc = sqlite3_close_v2(_db);
    _db          = nullptr;
    if (rc != SQLITE_OK)
    {
        throw DbException(std::string("SqliteDatabase::close: ") + sqlite3_errstr(rc));
    }
}

bool SqliteDatabase::isOpen() const
{
    return _db != nullptr;
}

std::unique_ptr<IStatement> SqliteDatabase::prepare(std::string_view sql)
{
    if (_db == nullptr)
    {
        throw DbException("SqliteDatabase::prepare: database is not open");
    }
    sqlite3_stmt* stmt = nullptr;
    const int rc =
        sqlite3_prepare_v2(_db, sql.data(), static_cast<int>(sql.size()), &stmt, nullptr);
    throwIfError(rc, "SqliteDatabase::prepare");
    return std::make_unique<SqliteStatement>(_db, stmt);
}

void SqliteDatabase::execute(std::string_view sql)
{
    if (_db == nullptr)
    {
        throw DbException("SqliteDatabase::execute: database is not open");
    }
    char* err = nullptr;
    const int rc =
        sqlite3_exec(_db, std::string(sql).c_str(), nullptr, nullptr, &err);
    if (rc != SQLITE_OK)
    {
        std::string message = "SqliteDatabase::execute: ";
        message += (err != nullptr) ? err : sqlite3_errmsg(_db);
        sqlite3_free(err);
        throw DbException(message);
    }
}

std::unique_ptr<ITransaction> SqliteDatabase::begin()
{
    if (_db == nullptr)
    {
        throw DbException("SqliteDatabase::begin: database is not open");
    }
    return std::make_unique<SqliteTransaction>(*this);
}

std::string SqliteDatabase::engineName() const
{
    return "sqlite";
}

} // namespace tools::design::db

FOUNDATION_FACTORY_REGISTER(tools::design::db::SqliteDatabase,
                            "tools::design::db::SqliteDatabase",
                            tools_design_db_SqliteDatabase)
