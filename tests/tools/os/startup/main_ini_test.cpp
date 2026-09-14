/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/os/startup/MainIni.hpp"

#include <boost/test/unit_test.hpp>

#include <stdexcept>
#include <string>

using namespace tools::os::startup;

BOOST_AUTO_TEST_CASE(main_ini_parse_sample)
{
    constexpr const char* Sample = R"([CFG]
platformName=Sample
logger=console, tcp:8023
cfg=./cfg/main.json
)";

    const MainIni ini = parseMainIniFromString(Sample);
    BOOST_TEST(ini.platformName == "Sample");
    BOOST_REQUIRE_EQUAL(ini.loggerTokens.size(), 2u);
    BOOST_TEST(ini.loggerTokens[0] == "console");
    BOOST_TEST(ini.loggerTokens[1] == "tcp:8023");
    BOOST_TEST(ini.cfgPath == "./cfg/main.json");
}

BOOST_AUTO_TEST_CASE(main_ini_parse_ignores_comments_and_other_sections)
{
    constexpr const char* Sample = R"(
; comment
# also a comment
[OTHER]
platformName=IGNORE
[CFG]
platformName = Sample
logger=console
cfg=cfg/main.json
)";

    const MainIni ini = parseMainIniFromString(Sample);
    BOOST_TEST(ini.platformName == "Sample");
    BOOST_REQUIRE_EQUAL(ini.loggerTokens.size(), 1u);
    BOOST_TEST(ini.loggerTokens[0] == "console");
}

BOOST_AUTO_TEST_CASE(main_ini_parse_missing_platform_throws)
{
    constexpr const char* Sample = R"([CFG]
logger=console
cfg=./cfg/main.json
)";
    BOOST_CHECK_THROW((void)parseMainIniFromString(Sample), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(main_ini_parse_empty_logger_throws)
{
    constexpr const char* Sample = R"([CFG]
platformName=Sample
logger=
cfg=./cfg/main.json
)";
    BOOST_CHECK_THROW((void)parseMainIniFromString(Sample), std::runtime_error);
}
