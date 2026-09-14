/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SchemaMigrator.cpp
 * @brief SchemaMigrator implementation.
 */
#include "tools/design/db/SchemaMigrator.hpp"

#include "tools/design/db/DbException.hpp"
#include "tools/design/db/ScopedTransaction.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace tools::design::db
{
namespace
{

[[nodiscard]] int parseVersionPrefix(const std::string& stem)
{
    if (stem.empty() || !std::isdigit(static_cast<unsigned char>(stem[0])))
    {
        throw DbException("SchemaMigrator: migration filename must start with a version number: " + stem);
    }
    std::size_t i = 0;
    while (i < stem.size() && std::isdigit(static_cast<unsigned char>(stem[i])))
    {
        ++i;
    }
    try
    {
        return std::stoi(stem.substr(0, i));
    }
    catch (const std::exception&)
    {
        throw DbException("SchemaMigrator: invalid version prefix in: " + stem);
    }
}

} // namespace

SchemaMigrator::SchemaMigrator(IDatabase& db, std::filesystem::path migrationsDir) : _db(db), _migrationsDir(std::move(migrationsDir))
{
}

void SchemaMigrator::ensureSchemaVersionTable()
{
    _db.execute(
        "CREATE TABLE IF NOT EXISTS schema_version ("
        "  version INTEGER PRIMARY KEY,"
        "  applied_at TEXT NOT NULL,"
        "  name TEXT NOT NULL"
        ")");
}

int SchemaMigrator::currentVersion()
{
    if (!_db.isOpen())
    {
        throw DbException("SchemaMigrator::currentVersion: database is not open");
    }
    ensureSchemaVersionTable();

    auto stmt = _db.prepare("SELECT COALESCE(MAX(version), 0) AS v FROM schema_version");
    auto rs   = stmt->query();
    if (!rs->next())
    {
        return 0;
    }
    return static_cast<int>(rs->row().getInt64("v"));
}

std::vector<SchemaMigrator::MigrationScript> SchemaMigrator::listScripts() const
{
    if (!std::filesystem::is_directory(_migrationsDir))
    {
        throw DbException("SchemaMigrator: migrations directory not found: " + _migrationsDir.string());
    }

    std::vector<MigrationScript> scripts;
    for (const auto& entry : std::filesystem::directory_iterator(_migrationsDir))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        const auto& path = entry.path();
        if (path.extension() != ".sql")
        {
            continue;
        }
        const std::string stem = path.stem().string();
        MigrationScript script;
        script.version = parseVersionPrefix(stem);
        script.name    = stem;
        script.path    = path;
        scripts.push_back(std::move(script));
    }

    std::sort(scripts.begin(), scripts.end(), [](const MigrationScript& a, const MigrationScript& b)
              { return a.version < b.version; });

    for (std::size_t i = 1; i < scripts.size(); ++i)
    {
        if (scripts[i].version == scripts[i - 1].version)
        {
            throw DbException("SchemaMigrator: duplicate migration version " + std::to_string(scripts[i].version));
        }
    }
    return scripts;
}

std::string SchemaMigrator::readFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in)
    {
        throw DbException("SchemaMigrator: cannot read migration file: " + path.string());
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void SchemaMigrator::applyScript(const MigrationScript& script)
{
    const std::string sql = readFile(script.path);
    ScopedTransaction tx(_db.begin());
    if (!sql.empty())
    {
        _db.execute(sql);
    }
    auto insert = _db.prepare(
        "INSERT INTO schema_version (version, applied_at, name) VALUES (?, datetime('now'), ?)");
    insert->bindInt64(1, script.version);
    insert->bindText(2, script.name);
    insert->execute();
    tx.commit();
}

void SchemaMigrator::migrateTo(int targetVersion)
{
    if (!_db.isOpen())
    {
        throw DbException("SchemaMigrator::migrateTo: database is not open");
    }
    if (targetVersion < 0)
    {
        throw DbException("SchemaMigrator::migrateTo: target version must be >= 0");
    }

    const int current = currentVersion();
    if (current > targetVersion)
    {
        throw DbException("SchemaMigrator: database schema version (" + std::to_string(current) + ") is newer than target (" + std::to_string(targetVersion) + ")");
    }
    if (current == targetVersion)
    {
        return;
    }

    const auto scripts = listScripts();
    for (const auto& script : scripts)
    {
        if (script.version <= current || script.version > targetVersion)
        {
            continue;
        }
        applyScript(script);
    }

    const int after = currentVersion();
    if (after < targetVersion)
    {
        throw DbException("SchemaMigrator: missing migrations to reach target " + std::to_string(targetVersion) + " (current=" + std::to_string(after) + ")");
    }
}

} // namespace tools::design::db
