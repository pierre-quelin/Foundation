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
#include "tools/design/statemachine/SmdPilotValidator.hpp"

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <filesystem>
#include <stdexcept>
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

} // namespace

BOOST_AUTO_TEST_CASE(smd_naming_conventions)
{
    BOOST_CHECK_EQUAL(entryHandlerName("WaitingTray"), "waitingTrayEntry");
    BOOST_CHECK_EQUAL(exitHandlerName("WaitingTray"), "waitingTrayExit");
    BOOST_CHECK_EQUAL(transitionHandlerName("Idle", "transfer"), "idleOnTransfer");
    BOOST_CHECK_EQUAL(msmEventName("transfer"), "TransferEvt");
    BOOST_CHECK_EQUAL(smHeaderFile("MyDevice"), "MyDevice_sm.h");
}

BOOST_AUTO_TEST_CASE(smd_parser_load_my_device_example)
{
    const auto doc = SmdParser::loadFile(myDeviceSpecPath().string());
    BOOST_CHECK_EQUAL(doc.schema, std::string{Schema});
    BOOST_CHECK_EQUAL(doc.machine, "MyDevice");
    BOOST_CHECK_EQUAL(doc.ns, "sample::statemachine");
    BOOST_CHECK_EQUAL(doc.generator, "statemachine-cpp");
    BOOST_CHECK_EQUAL(doc.initial, "NotReady");
    BOOST_REQUIRE_EQUAL(doc.states.size(), 6u);
    BOOST_REQUIRE_EQUAL(doc.composites.size(), 1u);
    BOOST_CHECK_EQUAL(doc.composites.front().name, "Ready");
    BOOST_REQUIRE_EQUAL(doc.transitions.size(), 20u);
    BOOST_REQUIRE_EQUAL(doc.events.size(), 8u);
    BOOST_CHECK((std::find(doc.events.begin(), doc.events.end(), "transfer") != doc.events.end()));
    BOOST_CHECK((std::find(doc.events.begin(), doc.events.end(), "check") != doc.events.end()));
    BOOST_CHECK(
        (std::find(doc.events.begin(), doc.events.end(), "transferTimeout") != doc.events.end()));
    BOOST_CHECK(
        (std::find(doc.events.begin(), doc.events.end(), "trayPresent") != doc.events.end()));
    BOOST_CHECK(
        (std::find(doc.events.begin(), doc.events.end(), "heartbeat") != doc.events.end()));
    SmdParser::validate(doc);
}

BOOST_AUTO_TEST_CASE(smd_parser_my_device_reference_event_shapes)
{
    const auto doc = SmdParser::loadFile(myDeviceSpecPath().string());

    bool entryCheckRow    = false;
    bool internalReaction = false;
    bool triggerCheck     = false;
    bool checkGuards      = false;
    bool actionAndGuard   = false;
    bool guardOnEvent     = false;
    bool actionTransition = false;

    for (const auto& state : doc.states)
    {
        if (state.name == "NotReady")
        {
            BOOST_CHECK(state.entryCheck);
        }
    }

    for (const auto& tr : doc.transitions)
    {
        if (tr.from == "NotReady" && tr.kind == TransitionKind::OnCheck && tr.on == "check")
        {
            entryCheckRow = true;
        }
        if (tr.from == "Ready_Idle" && tr.kind == TransitionKind::Internal && tr.on == "heartbeat")
        {
            internalReaction = true;
            BOOST_CHECK_EQUAL(tr.hasAction, true);
        }
        if (tr.from == "Ready_Idle" && tr.kind == TransitionKind::TriggerCheck && tr.on == "transfer")
        {
            triggerCheck = true;
        }
        if (tr.from == "Ready_Idle" && tr.kind == TransitionKind::OnCheck)
        {
            checkGuards = true;
        }
        if (tr.from == "Ready_WaitingTray" && tr.on == "blocked" && tr.hasAction && !tr.guard.empty())
        {
            actionAndGuard = true;
        }
        if (tr.from == "Ready_Tray" && tr.on == "transfer" && !tr.guard.empty() && !tr.hasAction)
        {
            guardOnEvent = true;
        }
        if (tr.from == "Ready_Tray" && tr.on == "trayAbsent" && tr.hasAction)
        {
            actionTransition = true;
        }
    }

    BOOST_CHECK(entryCheckRow);
    BOOST_CHECK(internalReaction);
    BOOST_CHECK(triggerCheck);
    BOOST_CHECK(checkGuards);
    BOOST_CHECK(actionAndGuard);
    BOOST_CHECK(guardOnEvent);
    BOOST_CHECK(actionTransition);
}

BOOST_AUTO_TEST_CASE(smd_parser_waiting_tray_entry_exit_deduced)
{
    const auto doc = SmdParser::loadFile(myDeviceSpecPath().string());
    bool foundIdle = false;
    bool found     = false;
    for (const auto& state : doc.states)
    {
        if (state.name == "Ready_Idle")
        {
            BOOST_REQUIRE(state.hasEntry);
            BOOST_REQUIRE_EQUAL(state.onEntry.size(), 1u);
            BOOST_CHECK_EQUAL(state.onEntry.front().handler, "idleEntry");
            foundIdle = true;
        }
        if (state.name == "Ready_WaitingTray")
        {
            BOOST_REQUIRE(state.hasEntry);
            BOOST_REQUIRE(state.hasExit);
            BOOST_REQUIRE_EQUAL(state.onEntry.size(), 1u);
            BOOST_CHECK_EQUAL(state.onEntry.front().handler, "waitingTrayEntry");
            BOOST_CHECK_EQUAL(state.onEntry.front().binding, "async");
            BOOST_REQUIRE_EQUAL(state.onExit.size(), 1u);
            BOOST_CHECK_EQUAL(state.onExit.front().handler, "waitingTrayExit");
            found = true;
        }
    }
    BOOST_CHECK(foundIdle);
    BOOST_CHECK(found);
}

BOOST_AUTO_TEST_CASE(smd_parser_transition_handlers_deduced)
{
    const auto doc    = SmdParser::loadFile(myDeviceSpecPath().string());
    bool foundTrigger = false;
    bool foundCheck   = false;
    for (const auto& tr : doc.transitions)
    {
        if (tr.from == "Ready_Idle" && tr.kind == TransitionKind::TriggerCheck && tr.on == "transfer")
        {
            BOOST_CHECK_EQUAL(tr.hasAction, false);
            BOOST_CHECK(tr.actionHandler.empty());
            foundTrigger = true;
        }
        if (tr.from == "Ready_Idle" && tr.kind == TransitionKind::OnCheck && tr.on == "check" && tr.guard == "canTransfer")
        {
            BOOST_CHECK_EQUAL(tr.to, "Ready_WaitingTray");
            BOOST_CHECK_EQUAL(tr.elseTo, "Ready_NoTray");
            BOOST_CHECK_EQUAL(tr.hasAction, false);
            BOOST_CHECK(tr.actionHandler.empty());
            foundCheck = true;
        }
        if (tr.from == "Ready_Idle" && tr.kind == TransitionKind::OnCheck && tr.on == "check" && tr.guard == "isReady" && tr.guardNegated)
        {
            BOOST_CHECK_EQUAL(tr.to, "NotReady");
            BOOST_CHECK(tr.elseTo.empty());
        }
    }
    BOOST_CHECK(foundTrigger);
    BOOST_CHECK(foundCheck);
}

BOOST_AUTO_TEST_CASE(smd_parser_events_internal_reaction_and_transition)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{
          "name": "Initializing",
          "initial": true,
          "events": [
            { "event": "tryConnect", "action": "handler" },
            { "event": "connected", "to": "Connected" }
          ]
        }, { "name": "Connected" }]
      }]
    })";

    const auto doc = SmdParser::parse(json);
    BOOST_REQUIRE_EQUAL(doc.transitions.size(), 2u);
    BOOST_CHECK(doc.transitions[0].kind == TransitionKind::Internal);
    BOOST_CHECK_EQUAL(doc.transitions[0].on, "tryConnect");
    BOOST_CHECK_EQUAL(doc.transitions[0].to, "Initializing");
    BOOST_CHECK_EQUAL(doc.transitions[0].hasAction, true);
    BOOST_CHECK_EQUAL(doc.transitions[0].actionHandler, "initializingOnTryConnect");
    BOOST_CHECK(doc.transitions[1].kind == TransitionKind::Direct);
    BOOST_CHECK_EQUAL(doc.transitions[1].to, "Connected");
    BOOST_CHECK_EQUAL(doc.transitions[1].hasAction, false);
    BOOST_CHECK(doc.transitions[1].actionHandler.empty());
}

BOOST_AUTO_TEST_CASE(smd_parser_accepts_transitions_key)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{
          "name": "Idle",
          "initial": true,
          "transitions": [{ "event": "go", "to": "Done" }]
        }, { "name": "Done" }]
      }]
    })";

    const auto doc = SmdParser::parse(json);
    BOOST_REQUIRE_EQUAL(doc.transitions.size(), 1u);
    BOOST_CHECK_EQUAL(doc.transitions.front().on, "go");
}

BOOST_AUTO_TEST_CASE(smd_parser_parses_negated_guard)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "Gate",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [
          {
            "name": "Ready",
            "initial": true,
            "entry": "check",
            "transitions": [
              { "guard": "!isReady", "to": "NotReady" }
            ]
          },
          { "name": "NotReady" }
        ]
      }]
    })";

    const auto doc = SmdParser::parse(json);
    BOOST_REQUIRE_EQUAL(doc.transitions.size(), 1u);
    BOOST_CHECK_EQUAL(doc.transitions.front().guard, "isReady");
    BOOST_CHECK(doc.transitions.front().guardNegated);
    BOOST_CHECK_EQUAL(doc.transitions.front().to, "NotReady");
}

BOOST_AUTO_TEST_CASE(smd_parser_rejects_unknown_transition_state)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "genopt": { "namespace": "sample::statemachine" },
      "machine": "X",
      "regions": [{
        "name": "Root",
        "states": [{
          "name": "A",
          "initial": true,
          "transitions": [ { "event": "go", "to": "Missing" } ]
        }]
      }]
    })";

    BOOST_REQUIRE_THROW((void)SmdParser::parse(json), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(smd_parser_rejects_schema)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-2",
      "genopt": { "namespace": "x" },
      "machine": "X",
      "regions": []
    })";

    BOOST_CHECK_THROW((void)SmdParser::parse(json), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(smd_parser_guard_only_after_event_trigger)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{
          "name": "Idle",
          "initial": true,
          "transitions": [
            { "event": "go" },
            { "guard": "ok", "to": "Done" }
          ]
        }, { "name": "Done" }]
      }]
    })";

    const auto doc = SmdParser::parse(json);
    bool trigger   = false;
    bool onCheck   = false;
    for (const auto& tr : doc.transitions)
    {
        if (tr.kind == TransitionKind::TriggerCheck)
        {
            trigger = true;
        }
        if (tr.kind == TransitionKind::OnCheck && tr.guard == "ok")
        {
            onCheck = true;
        }
    }
    BOOST_CHECK(trigger);
    BOOST_CHECK(onCheck);
}

BOOST_AUTO_TEST_CASE(smd_parser_guard_only_row_is_check_guard)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{
          "name": "NotReady",
          "initial": true,
          "entry": "check",
          "events": [
            { "guard": "isReady", "to": "Ready" }
          ]
        }, { "name": "Ready" }]
      }]
    })";

    const auto doc = SmdParser::parse(json);
    BOOST_REQUIRE_EQUAL(doc.transitions.size(), 1u);
    BOOST_CHECK(doc.transitions.front().kind == TransitionKind::OnCheck);
    BOOST_CHECK_EQUAL(doc.transitions.front().guard, "isReady");
    BOOST_CHECK(doc.transitions.front().triggerEvent.empty());
}

BOOST_AUTO_TEST_CASE(smd_parser_action_check_notation)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{
          "name": "Idle",
          "initial": true,
          "events": [
            { "event": "transfer", "action": "^check" },
            { "guard": "canTransfer", "to": "Done" },
            { "event": "reset", "action": "handler^check" },
            { "guard": "ok", "to": "Idle" }
          ]
        }, { "name": "Done" }]
      }]
    })";

    const auto doc         = SmdParser::parse(json);
    bool transferCheck     = false;
    bool resetHandlerCheck = false;
    for (const auto& tr : doc.transitions)
    {
        if (tr.kind == TransitionKind::TriggerCheck && tr.on == "transfer")
        {
            transferCheck = true;
            BOOST_CHECK(!tr.hasAction);
        }
        if (tr.kind == TransitionKind::TriggerCheck && tr.on == "reset")
        {
            resetHandlerCheck = true;
            BOOST_CHECK(tr.hasAction);
            BOOST_CHECK_EQUAL(tr.actionHandler, transitionHandlerName("Idle", "reset"));
        }
    }
    BOOST_CHECK(transferCheck);
    BOOST_CHECK(resetHandlerCheck);
}

BOOST_AUTO_TEST_CASE(smd_parser_rejects_check_action_with_to)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{
          "name": "Idle",
          "initial": true,
          "events": [
            { "event": "go", "action": "^check", "to": "Done" }
          ]
        }, { "name": "Done" }]
      }]
    })";

    BOOST_CHECK_THROW((void)SmdParser::parse(json), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(smd_parser_rejects_event_and_check_guard_on_same_row)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{
          "name": "Idle",
          "initial": true,
          "transitions": [
            { "event": "go", "check": true, "guard": "ok", "to": "Done" }
          ]
        }, { "name": "Done" }]
      }]
    })";

    BOOST_CHECK_THROW((void)SmdParser::parse(json), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(smd_parser_entry_check_flag)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "ReadyGate",
      "genopt": {
        "namespace": "sample::statemachine",
        "generator": "statemachine-cpp"
      },
      "regions": [{
        "states": [
          {
            "name": "NotReady",
            "initial": true,
            "entry": { "check": true },
            "transitions": [
              { "check": true, "guard": "isReady", "to": "Ready" }
            ]
          },
          { "name": "Ready", "entry": true }
        ]
      }]
    })";

    const auto doc     = SmdParser::parse(json);
    bool foundNotReady = false;
    for (const auto& state : doc.states)
    {
        if (state.name == "NotReady")
        {
            BOOST_CHECK(state.entryCheck);
            BOOST_CHECK(!state.hasEntry);
            foundNotReady = true;
        }
    }
    BOOST_REQUIRE(foundNotReady);
    SmdParser::validate(doc);
}

BOOST_AUTO_TEST_CASE(smd_parser_entry_binding_and_check)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "ReadyGate",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [
          {
            "name": "NotReady",
            "initial": true,
            "entry": { "binding": "bound", "check": true },
            "transitions": [
              { "check": true, "guard": "isReady", "to": "Ready" }
            ]
          },
          { "name": "Ready" }
        ]
      }]
    })";

    const auto doc = SmdParser::parse(json);
    bool found     = false;
    State notReadyState;
    for (const auto& state : doc.states)
    {
        if (state.name == "NotReady")
        {
            notReadyState = state;
            found         = true;
            break;
        }
    }
    BOOST_REQUIRE(found);
    BOOST_CHECK(notReadyState.entryCheck);
    BOOST_CHECK(notReadyState.hasEntry);
    BOOST_CHECK_EQUAL(notReadyState.entryBinding, "bound");
}

BOOST_AUTO_TEST_CASE(smd_parser_entry_short_form_handler)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{ "name": "Idle", "initial": true, "entry": "handler" }]
      }]
    })";

    const auto doc = SmdParser::parse(json);
    BOOST_REQUIRE_EQUAL(doc.states.size(), 1u);
    BOOST_CHECK(doc.states.front().hasEntry);
    BOOST_CHECK(!doc.states.front().entryCheck);
}

BOOST_AUTO_TEST_CASE(smd_parser_entry_short_form_check)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{ "name": "NotReady", "initial": true, "entry": "check" }]
      }]
    })";

    const auto doc = SmdParser::parse(json);
    BOOST_REQUIRE_EQUAL(doc.states.size(), 1u);
    BOOST_CHECK(!doc.states.front().hasEntry);
    BOOST_CHECK(doc.states.front().entryCheck);
}

BOOST_AUTO_TEST_CASE(smd_parser_entry_short_form_handler_and_check)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{
          "name": "NotReady",
          "initial": true,
          "entry": ["handler", "check"]
        }]
      }]
    })";

    const auto doc = SmdParser::parse(json);
    BOOST_REQUIRE_EQUAL(doc.states.size(), 1u);
    BOOST_CHECK(doc.states.front().hasEntry);
    BOOST_CHECK(doc.states.front().entryCheck);
}

BOOST_AUTO_TEST_CASE(smd_parser_entry_binding_string)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{ "name": "Idle", "initial": true, "entry": "bound" }]
      }]
    })";

    const auto doc = SmdParser::parse(json);
    BOOST_REQUIRE_EQUAL(doc.states.size(), 1u);
    BOOST_CHECK(doc.states.front().hasEntry);
    BOOST_CHECK_EQUAL(doc.states.front().entryBinding, "bound");
}

BOOST_AUTO_TEST_CASE(smd_parser_exit_short_form_handler)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{ "name": "WaitingTray", "initial": true, "exit": "handler" }]
      }]
    })";

    const auto doc = SmdParser::parse(json);
    BOOST_REQUIRE_EQUAL(doc.states.size(), 1u);
    BOOST_CHECK(doc.states.front().hasExit);
    BOOST_CHECK_EQUAL(doc.states.front().exitBinding, "async");
}

BOOST_AUTO_TEST_CASE(smd_parser_entry_array_rejects_unknown_token)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{ "name": "Idle", "initial": true, "entry": ["handler", "bound"] }]
      }]
    })";

    BOOST_CHECK_THROW((void)SmdParser::parse(json), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(smd_pilot_validator_my_device_handlers_present)
{
    const auto doc = SmdParser::loadFile(myDeviceSpecPath().string());
    SmdParser::validate(doc);

    const auto mhPath = myDeviceSpecPath().parent_path() / handlersFile(doc.machine);
    BOOST_REQUIRE(std::filesystem::exists(mhPath));
    BOOST_CHECK_NO_THROW(SmdPilotValidator::validateFile(doc, mhPath.string()));
}

BOOST_AUTO_TEST_CASE(smd_pilot_validator_rejects_missing_handler)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{
          "name": "Idle",
          "initial": true,
          "entry": "handler",
          "transitions": [{ "event": "go", "to": "Done", "action": "handler" }]
        }, {
          "name": "Done"
        }]
      }]
    })";

    const auto doc = SmdParser::parse(json);
    SmdParser::validate(doc);

    constexpr const char* mh = R"(
namespace sample::statemachine {
void X::idleEntry() {}
}
)";
    BOOST_CHECK_THROW(SmdPilotValidator::validateSource(doc, mh), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(smd_parser_time_requests_and_arm_cancel)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "timeRequests": [
        { "name": "transferTimeout", "config": "TransferTimeout" }
      ],
      "regions": [{
        "states": [{
          "name": "WaitingTray",
          "initial": true,
          "entry": "handler",
          "exit": "handler",
          "arm": ["transferTimeout"],
          "cancel": ["transferTimeout"],
          "transitions": [
            { "event": "transferTimeout", "to": "Done" }
          ]
        }, { "name": "Done" }]
      }]
    })";

    const auto doc = SmdParser::parse(json);
    BOOST_REQUIRE_EQUAL(doc.timeRequests.size(), 1u);
    BOOST_CHECK_EQUAL(doc.timeRequests.front().name, "transferTimeout");
    BOOST_CHECK_EQUAL(doc.timeRequests.front().fires, "transferTimeout");
    BOOST_CHECK_EQUAL(doc.timeRequests.front().configKey, "TransferTimeout");
    BOOST_REQUIRE_EQUAL(doc.states.size(), 2u);
    bool foundWaitingTray = false;
    for (const auto& state : doc.states)
    {
        if (state.name == "WaitingTray")
        {
            BOOST_REQUIRE_EQUAL(state.arm.size(), 1u);
            BOOST_REQUIRE_EQUAL(state.cancel.size(), 1u);
            foundWaitingTray = true;
        }
    }
    BOOST_CHECK(foundWaitingTray);
    SmdParser::validate(doc);
}

BOOST_AUTO_TEST_CASE(smd_parser_rejects_unknown_arm_timer)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{
          "name": "Idle",
          "initial": true,
          "arm": ["missing"],
          "transitions": [{ "event": "go", "to": "Done" }]
        }, { "name": "Done" }]
      }]
    })";

    const auto doc = SmdParser::parse(json);
    BOOST_CHECK_THROW(SmdParser::validate(doc), std::runtime_error);
}
