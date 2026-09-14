/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file IDatabase.hpp
 * @brief Database access interface (engine-independent).
 *
 * Factory constructors (when registered): `(ApplicationServices&, config::Node)`.
 *
 * Config JSON (convention):
 * @code
 * "Database": {
 *   "InstanceOf": "tools::design::db::SqliteDatabase",
 *   "Path": "./data/app.db",
 *   "MigrationsDir": "./db/migrations",
 *   "TargetSchemaVersion": 2,
 *   "SslMode": "disable",
 *   "Encryption": "none"
 * }
 * @endcode
 *
 * Security levers (present, not activated by default): Password / PasswordEnv,
 * SslMode, FileMode, Encryption / KeyEnv, RefuseSymlinks.
 *
 * Column bind/get types (first approach): int64, double, text — no BLOB API;
 * opaque documents use JSON in a TEXT column.
 */
#pragma once

#include "tools/design/db/ISecretProvider.hpp"
#include "tools/design/db/IStatement.hpp"
#include "tools/design/db/ITransaction.hpp"

#include <memory>
#include <string>
#include <string_view>

namespace tools::design::db
{

/**
 * @brief Application database service (SQLite / Postgres / MySQL implementations).
 */
class IDatabase
{
public:
    virtual ~IDatabase() = default;

    /**
     * @brief Open the connection.
     * @param secrets Optional provider for PasswordEnv / KeyEnv (ignored until wired).
     */
    virtual void open(ISecretProvider* secrets = nullptr) = 0;

    virtual void close() = 0;

    [[nodiscard]] virtual bool isOpen() const = 0;

    [[nodiscard]] virtual std::unique_ptr<IStatement> prepare(std::string_view sql) = 0;

    /** @brief Execute SQL with no result set (DDL / DML without rows). */
    virtual void execute(std::string_view sql) = 0;

    [[nodiscard]] virtual std::unique_ptr<ITransaction> begin() = 0;

    /** @return Engine id: "sqlite" | "postgres" | "mysql". */
    [[nodiscard]] virtual std::string engineName() const = 0;
};

} // namespace tools::design::db
