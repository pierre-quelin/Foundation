/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/statemachine/SmdNaming.hpp"
#include "tools/design/statemachine/SmdParser.hpp"
#include "tools/design/statemachine/StateMachinePlantUmlExporter.hpp"

#include <boost/test/unit_test.hpp>

#include <filesystem>
#include <string>

using namespace tools::design::statemachine::smd;

namespace
{

[[nodiscard]] std::filesystem::path myDeviceSpecPath()
{
#ifdef FOUNDATION_SOURCE_DIR
    return std::filesystem::path(FOUNDATION_SOURCE_DIR) / "src" / "sample" / "statemachine" / "MyDevice.smd";
#else
    return std::filesystem::path("src/sample/statemachine/MyDevice.smd");
#endif
}

[[nodiscard]] bool contains(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle) != std::string::npos;
}

[[nodiscard]] std::size_t countOccurrences(const std::string& haystack, const std::string& needle)
{
    std::size_t count = 0U;
    std::size_t pos   = 0U;
    while ((pos = haystack.find(needle, pos)) != std::string::npos)
    {
        ++count;
        pos += needle.size();
    }
    return count;
}

} // namespace

BOOST_AUTO_TEST_CASE(smd_plantuml_my_device_logical_structure)
{
    const auto doc = SmdParser::loadFile(myDeviceSpecPath().string());

    PlantUmlEmitOptions options;
    options.mode           = PlantUmlDiagramMode::Logical;
    const std::string puml = StateMachinePlantUmlExporter::emit(doc, options);

    BOOST_CHECK(contains(puml, "@startuml MyDevice"));
    BOOST_CHECK(contains(puml, "title MyDevice (foundation-smd-4)"));
    BOOST_CHECK(contains(puml, "state Ready {"));
    BOOST_CHECK(contains(puml, "state Idle"));
    BOOST_CHECK(contains(puml, "[*] --> Idle"));
    BOOST_CHECK(!contains(puml, "<<initial>>"));
    BOOST_CHECK(contains(puml, "NotReady : entry / ^check"));
    BOOST_CHECK(contains(puml, "Idle : entry / " + entryHandlerName("Idle")));
    BOOST_CHECK(contains(puml, "Idle : heartbeat / " + transitionHandlerName("Idle", "heartbeat")));
    BOOST_CHECK(contains(puml, "/ [isReady()]"));
    BOOST_CHECK(contains(puml, "Ready --> NotReady : / [!isReady()]"));
    BOOST_CHECK(!contains(puml, "<<check>>"));
    BOOST_CHECK(contains(puml, "@enduml"));
}

BOOST_AUTO_TEST_CASE(smd_plantuml_my_device_logical_border_dedup)
{
    const auto doc = SmdParser::loadFile(myDeviceSpecPath().string());

    PlantUmlEmitOptions options;
    options.mode           = PlantUmlDiagramMode::Logical;
    const std::string puml = StateMachinePlantUmlExporter::emit(doc, options);

    BOOST_CHECK_EQUAL(countOccurrences(puml, "Ready --> NotReady : / [!isReady()]"), 1U);
    BOOST_CHECK(!contains(puml, "Idle --> NotReady : / [!isReady()]"));
    BOOST_CHECK(!contains(puml, "WaitingTray --> NotReady : / [!isReady()]"));
}

BOOST_AUTO_TEST_CASE(smd_plantuml_my_device_expanded_border)
{
    const auto doc = SmdParser::loadFile(myDeviceSpecPath().string());

    PlantUmlEmitOptions options;
    options.mode           = PlantUmlDiagramMode::Expanded;
    const std::string puml = StateMachinePlantUmlExporter::emit(doc, options);

    BOOST_CHECK_EQUAL(countOccurrences(puml, "/ [!isReady()]"), 5U);
}

BOOST_AUTO_TEST_CASE(smd_plantuml_my_device_check_and_handlers)
{
    const auto doc = SmdParser::loadFile(myDeviceSpecPath().string());

    PlantUmlEmitOptions options;
    options.mode           = PlantUmlDiagramMode::Logical;
    const std::string puml = StateMachinePlantUmlExporter::emit(doc, options);

    BOOST_CHECK(contains(puml, "Idle --> WaitingTray : transfer / [canTransfer()]"));
    BOOST_CHECK(contains(puml, "Idle --> NoTray : transfer / [else canTransfer()]"));
    BOOST_CHECK(contains(puml, "Idle : transfer / ^check"));
    BOOST_CHECK(contains(puml, "Jam : reset / ^check"));
    BOOST_CHECK(contains(puml, "Jam --> Idle : reset / [canTransfer()]"));
    BOOST_CHECK(contains(puml, "Jam --> NoTray : reset / [else canTransfer()]"));
    BOOST_CHECK(contains(puml, "Tray --> WaitingTray : transfer / [canTransfer()]"));
    BOOST_CHECK(!contains(puml, "<<trigger check>>"));
    BOOST_CHECK(!contains(puml, "triggerCheck_"));
    BOOST_CHECK(contains(puml, "note right of WaitingTray"));
    BOOST_CHECK(contains(puml, "arm transferTimeout"));
}
