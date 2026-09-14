/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/config/Config.hpp"

#include "tools/design/config/Node.hpp"
#include "tools/design/config/Reference.hpp"
#include "tools/design/config/Registry.hpp"
#include "tools/design/config/ResolvePlatform.hpp"
#include "util/chrono/Delay.hpp"

#include <boost/test/unit_test.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

using namespace tools::design::config;
using namespace util::chrono;

namespace
{

constexpr const char* SampleConfigJson = R"({
  "EsploraBoard": {
    "InstanceOf": "driver::board::EsploraBoard",
    "LogLevel": "DEBUG",
    "TestInt": 42,
    "TestDelay": "5s",
    "MCP2221": "MCP2221",
    "PCA9539": "PCA9539",
    "PCA9633s": [
      { "Item": "PCA9633_0" }
    ],
    "Inputs": [
      { "Item": "pres24v" }
    ],
    "Objects": {
      "MCP2221": {
        "InstanceOf": "driver::chip::MCP2221"
      },
      "PCA9539": {
        "InstanceOf": "driver::chip::PCA9539",
        "Address": 116
      },
      "PCA9633_0": {
        "InstanceOf": "driver::chip::PCA9633",
        "I2CMaster": "EsploraBoard.MCP2221",
        "Address": 64
      },
      "pres24v": {
        "InstanceOf": "io::in::InByPCA9539",
        "PCA9539": "EsploraBoard.PCA9539"
      }
    }
  }
})";

} // namespace

BOOST_AUTO_TEST_CASE(config_parse_navigate_instance_of)
{
    const auto center = createLocalFromString(SampleConfigJson);
    const auto node   = center->root().at("EsploraBoard/Objects/pres24v/InstanceOf");
    BOOST_CHECK_EQUAL(node.value<std::string>(), "io::in::InByPCA9539");
}

BOOST_AUTO_TEST_CASE(config_value_delay)
{
    const auto center = createLocalFromString(SampleConfigJson);
    const auto delay  = center->root().at("EsploraBoard/TestDelay").value<Delay>();
    BOOST_CHECK_EQUAL(delay.toNanoseconds().count(),
                      std::chrono::nanoseconds(std::chrono::seconds(5)).count());
}

BOOST_AUTO_TEST_CASE(config_value_or_missing_key_returns_default)
{
    const auto center = createLocalFromString(SampleConfigJson);
    const auto board  = center->root().at("EsploraBoard");
    BOOST_CHECK_EQUAL(board.value_or("MissingInt", 7), 7);
    BOOST_CHECK_EQUAL(board.value_or("TestInt", 0), 42);
    const Delay fallback{std::chrono::milliseconds(200)};
    const Delay delay = board.value_or("MissingDelay", fallback);
    BOOST_CHECK_EQUAL(delay.toNanoseconds().count(), fallback.toNanoseconds().count());
    BOOST_CHECK_EQUAL(board.value_or("TestDelay", fallback).toNanoseconds().count(),
                      std::chrono::nanoseconds(std::chrono::seconds(5)).count());
}

BOOST_AUTO_TEST_CASE(config_set_value_runtime)
{
    auto center      = createLocalFromString(SampleConfigJson);
    auto node        = center->root().at("EsploraBoard/TestInt");
    const int before = node.value<int>();
    BOOST_CHECK_EQUAL(before, 42);
    node.set_value(99);
    BOOST_CHECK_EQUAL(node.value<int>(), 99);
}

BOOST_AUTO_TEST_CASE(config_save_round_trip)
{
    const auto center = createLocalFromString(SampleConfigJson);
    auto node         = center->root().at("EsploraBoard/TestInt");
    node.set_value(123);

    const auto tempPath =
        std::filesystem::temp_directory_path() / "foundation_config_test.json";
    center->save(tempPath);

    const auto reloaded = createLocal(tempPath);
    BOOST_CHECK_EQUAL(reloaded->root().at("EsploraBoard/TestInt").value<int>(), 123);

    std::error_code ec;
    std::filesystem::remove(tempPath, ec);
}

BOOST_AUTO_TEST_CASE(config_missing_key_throws)
{
    const auto center = createLocalFromString(SampleConfigJson);
    BOOST_CHECK_THROW((void)center->root().at("EsploraBoard/Missing"), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(config_install_current_reset)
{
    reset();
    BOOST_CHECK_THROW((void)current(), std::logic_error);

    const auto center = createLocalFromString(SampleConfigJson);
    install(center);
    (void)current();
    reset();
    BOOST_CHECK_THROW((void)current(), std::logic_error);
}

BOOST_AUTO_TEST_CASE(config_invalid_json_throws)
{
    BOOST_CHECK_THROW((void)createLocalFromString("{"), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(config_is_reference)
{
    BOOST_CHECK(isReference("EsploraBoard.MCP2221"));
    BOOST_CHECK(isReference("EsploraBoard.PCA9633_0"));
    BOOST_CHECK(!isReference("io::in::InByPCA9539"));
    BOOST_CHECK(!isReference("5s"));
    BOOST_CHECK(!isReference("DEBUG"));
}

BOOST_AUTO_TEST_CASE(config_resolve_reference)
{
    const auto center = createLocalFromString(SampleConfigJson);
    const auto root   = center->root();

    const auto mcp = resolveReference(root, "EsploraBoard.MCP2221");
    BOOST_CHECK_EQUAL(mcp.path(), "EsploraBoard/Objects/MCP2221");
    BOOST_CHECK_EQUAL(mcp["InstanceOf"].value<std::string>(), "driver::chip::MCP2221");

    const auto pca9633 = root.at("EsploraBoard/Objects/PCA9633_0");
    const auto master  = follow(root, pca9633, "I2CMaster");
    BOOST_CHECK_EQUAL(master.path(), "EsploraBoard/Objects/MCP2221");

    const auto pres24v = root.at("EsploraBoard/Objects/pres24v");
    const auto pca9539 = follow(root, pres24v, "PCA9539");
    BOOST_CHECK_EQUAL(pca9539.path(), "EsploraBoard/Objects/PCA9539");
    BOOST_CHECK_EQUAL(pca9539["Address"].value<int>(), 116);

    const auto shortRefMcp = follow(root, root["EsploraBoard"], "MCP2221");
    BOOST_CHECK_EQUAL(shortRefMcp.path(), "EsploraBoard/Objects/MCP2221");

    const auto namedMcp = resolveNamed(root, root["EsploraBoard"], "MCP2221");
    BOOST_CHECK_EQUAL(namedMcp.path(), "EsploraBoard/Objects/MCP2221");

    const auto namedPca9633 = resolveNamed(root, root["EsploraBoard"], "PCA9633_0");
    BOOST_CHECK_EQUAL(namedPca9633.path(), "EsploraBoard/Objects/PCA9633_0");

    const auto dottedPca9539 = resolveReference(root, "EsploraBoard.PCA9539");
    BOOST_CHECK_EQUAL(dottedPca9539.path(), "EsploraBoard/Objects/PCA9539");

    const auto dottedPca9633 = resolveReference(root, "EsploraBoard.PCA9633_0");
    BOOST_CHECK_EQUAL(dottedPca9633.path(), "EsploraBoard/Objects/PCA9633_0");
}

BOOST_AUTO_TEST_CASE(config_instance_path_strips_objects)
{
    BOOST_CHECK_EQUAL(instancePath("IOBoard/Objects/MCP2221"), "IOBoard.MCP2221");
    BOOST_CHECK_EQUAL(instancePath("A/Objects/B/Objects/C"), "A.B.C");
    BOOST_CHECK_EQUAL(instancePath("IOBoard"), "IOBoard");
    BOOST_CHECK_EQUAL(instancePath(""), "");
    BOOST_CHECK_EQUAL(instancePath("IOBoard/Objects/Objects"), "IOBoard.Objects");
    BOOST_CHECK_EQUAL(instancePath("Board/Objects/Objects/Objects/Child"), "Board.Objects.Child");
    BOOST_CHECK_EQUAL(instanceName("IOBoard/Objects/MCP2221"), "MCP2221");
    BOOST_CHECK_EQUAL(instanceName("IOBoard/Objects/Objects"), "Objects");
    BOOST_CHECK_EQUAL(instanceName("MCP2221"), "MCP2221");

    const auto center = createLocalFromString(SampleConfigJson);
    const auto mcp    = resolveReference(center->root(), "EsploraBoard.MCP2221");
    BOOST_CHECK_EQUAL(mcp.path(), "EsploraBoard/Objects/MCP2221");
    BOOST_CHECK_EQUAL(instancePath(mcp.path()), "EsploraBoard.MCP2221");
    BOOST_CHECK_EQUAL(instanceName(mcp.path()), "MCP2221");
}

BOOST_AUTO_TEST_CASE(config_resolve_reference_invalid_throws)
{
    const auto center = createLocalFromString(SampleConfigJson);
    BOOST_CHECK_THROW((void)resolveReference(center->root(), "io::in::InByPCA9539"), std::runtime_error);
    BOOST_CHECK_THROW((void)resolveReference(center->root(), "EsploraBoard.Missing"), std::runtime_error);
}

#ifdef FOUNDATION_SOURCE_DIR
BOOST_AUTO_TEST_CASE(config_load_main_json)
{
    const std::filesystem::path path =
        std::filesystem::path(FOUNDATION_SOURCE_DIR) / "tests" / "fixtures" / "main.json";
    if (!std::filesystem::exists(path))
    {
        BOOST_TEST_MESSAGE("skip config_load_main_json: " << path.string());
        return;
    }

    const auto center = createLocal(path);
    BOOST_CHECK_EQUAL(
        center->root().at("IOBoard/Objects/btnUp/InstanceOf").value<std::string>(),
        "io::in::InByPCA9539");
    BOOST_CHECK_EQUAL(
        center->root().at("IOBoard/Objects/btnUp/PCA9539").value<std::string>(),
        "IOBoard.PCA9539");
    BOOST_CHECK(!center->root().at("IOBoard/Objects").contains("MCP2221"));
    BOOST_CHECK(!center->root().at("IOBoard/Objects").contains("PCA9539"));
    BOOST_CHECK(!center->root().at("IOBoard/Objects").contains("PCA9633_0"));
}
#endif

BOOST_AUTO_TEST_CASE(resolve_platform_noop_without_key)
{
    const auto center = createLocalFromString(R"({ "Serport": { "InstanceOf": "tools::os::serport::Serport" } })");
    const auto node   = center->root()["Serport"];
    BOOST_CHECK(!resolvePlatformJson(node, "Local").has_value());
}

BOOST_AUTO_TEST_CASE(resolve_platform_merges_object_branch)
{
    constexpr const char* json = R"({
      "Serport": {
        "DeviceName": "COM3",
        "Platform": {
          "Local": {
            "InstanceOf": "tools::os::serport::Serport",
            "Bridged": "tools::os::serport::SerportBridge"
          },
          "Remote": {
            "InstanceOf": "tools::os::serport::SerportGhost"
          }
        }
      }
    })";
    const auto center          = createLocalFromString(json);
    const auto node            = center->root()["Serport"];

    const auto local = resolvePlatformJson(node, "Local");
    BOOST_REQUIRE(local.has_value());
    BOOST_CHECK_EQUAL((*local)["InstanceOf"], "tools::os::serport::Serport");
    BOOST_CHECK_EQUAL((*local)["Bridged"], "tools::os::serport::SerportBridge");
    BOOST_CHECK_EQUAL((*local)["DeviceName"], "COM3");
    BOOST_CHECK(!local->contains("Platform"));

    const auto remote = resolvePlatformJson(node, "Remote");
    BOOST_REQUIRE(remote.has_value());
    BOOST_CHECK_EQUAL((*remote)["InstanceOf"], "tools::os::serport::SerportGhost");
    BOOST_CHECK_EQUAL((*remote)["DeviceName"], "COM3");
    BOOST_CHECK(!remote->contains("Bridged"));
}

BOOST_AUTO_TEST_CASE(resolve_platform_replaces_with_array_branch)
{
    constexpr const char* json = R"({
      "GlobalObjects": {
        "Platform": {
          "Local": [ { "Item": "IOBoard" } ],
          "Remote": [ { "Item": "Serport" } ]
        }
      }
    })";
    const auto center          = createLocalFromString(json);
    const auto node            = center->root()["GlobalObjects"];
    const auto local           = resolvePlatformJson(node, "Local");
    BOOST_REQUIRE(local.has_value());
    BOOST_CHECK(local->is_array());
    BOOST_CHECK_EQUAL(local->size(), 1u);
    BOOST_CHECK_EQUAL((*local)[0]["Item"], "IOBoard");
}

BOOST_AUTO_TEST_CASE(resolve_platform_missing_branch_throws)
{
    constexpr const char* json = R"({
      "X": { "Platform": { "Local": { "InstanceOf": "A" } } }
    })";
    const auto center          = createLocalFromString(json);
    BOOST_CHECK_THROW((void)resolvePlatformJson(center->root()["X"], "Remote"), std::runtime_error);
}
