/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SqliteDatabase.hpp
 * @brief SQLite implementation of IDatabase.
 */
#pragma once

#include "tools/design/config/Node.hpp"
#include "tools/design/db/IDatabase.hpp"
#include "tools/design/factory/ApplicationServices.hpp"
#include "tools/design/factory/IObject.hpp"

#include <string>

struct sqlite3;

namespace tools::design::db
{

class SqliteDatabase : public tools::design::factory::IObject, public IDatabase
{
public:
    SqliteDatabase(tools::design::ApplicationServices& app, tools::design::config::Node node);
    ~SqliteDatabase() override;

    SqliteDatabase(const SqliteDatabase&)            = delete;
    SqliteDatabase& operator=(const SqliteDatabase&) = delete;

    void open(ISecretProvider* secrets = nullptr) override;
    void close() override;
    [[nodiscard]] bool isOpen() const override;

    [[nodiscard]] std::unique_ptr<IStatement> prepare(std::string_view sql) override;
    void execute(std::string_view sql) override;
    [[nodiscard]] std::unique_ptr<ITransaction> begin() override;

    [[nodiscard]] std::string engineName() const override;

    [[nodiscard]] sqlite3* handle() const noexcept { return _db; }

private:
    void throwIfError(int rc, const char* context) const;

    std::string _path;
    sqlite3* _db = nullptr;
};

} // namespace tools::design::db
