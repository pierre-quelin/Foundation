/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/config/LocalConfigCenter.hpp"

#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>

namespace tools::design::config
{

LocalConfigCenter::LocalConfigCenter(util::json::Json root) : _root(std::move(root))
{
    if (!_root.is_object() && !_root.is_array())
    {
        throw std::runtime_error("LocalConfigCenter: root JSON must be an object or array");
    }
}

Node LocalConfigCenter::root()
{
    return Node(&_root, "", &_mutex);
}

Node LocalConfigCenter::root() const
{
    return Node(const_cast<util::json::Json*>(&_root), "", const_cast<tools::os::sync::SemM*>(&_mutex));
}

std::string LocalConfigCenter::toJsonString() const
{
    const std::lock_guard<tools::os::sync::SemM> lock(_mutex);
    return _root.dump(3);
}

void LocalConfigCenter::save(const std::filesystem::path& path) const
{
    const std::lock_guard<tools::os::sync::SemM> lock(_mutex);
    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        throw std::runtime_error("LocalConfigCenter::save: cannot open " + path.string());
    }
    out << _root.dump(3);
}

namespace
{

[[nodiscard]] util::json::Json parseJson(std::string_view text, const char* context)
{
    try
    {
        return util::json::Json::parse(text);
    }
    catch (const util::json::Json::parse_error& ex)
    {
        std::ostringstream msg;
        msg << context << ": JSON parse error at byte " << ex.byte << ": " << ex.what();
        throw std::runtime_error(msg.str());
    }
}

[[nodiscard]] std::string readFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        throw std::runtime_error("config: cannot open " + path.string());
    }
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace

ConfigCenterPtr createLocal(const std::filesystem::path& path)
{
    return std::make_shared<LocalConfigCenter>(parseJson(readFile(path), path.string().c_str()));
}

ConfigCenterPtr createLocalFromString(std::string_view json)
{
    return std::make_shared<LocalConfigCenter>(parseJson(json, "config::createLocalFromString"));
}

} // namespace tools::design::config
