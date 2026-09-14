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
#include "tools/design/statemachine/StateMachineCppGenerator.hpp"
#include "tools/design/statemachine/StateMachineLimits.hpp"

#include <boost/test/unit_test.hpp>

#include <string>

using namespace tools::design::statemachine::smd;

BOOST_AUTO_TEST_CASE(statemachine_cpp_generator_emits_my_device_sm_header)
{
    const auto doc = SmdParser::loadFile(
        std::string{FOUNDATION_SOURCE_DIR} + "/src/sample/statemachine/MyDevice.smd");
    SmdParser::validate(doc);

    const auto files = StateMachineCppGenerator::emit(doc);
    BOOST_REQUIRE(files.count(smHeaderFile("MyDevice")) != 0);
    BOOST_REQUIRE(files.count(smSourceFile("MyDevice")) != 0);
    BOOST_REQUIRE(files.count(smDeclFile("MyDevice")) != 0);

    const auto& smh = files.at(smHeaderFile("MyDevice"));
    BOOST_CHECK(smh.find("struct TransferEvt") != std::string::npos);
    BOOST_CHECK(smh.find("struct CheckEvt") != std::string::npos);
    BOOST_CHECK(smh.find("SmAiRow_Ready_Idle_TransferEvt") != std::string::npos);
    BOOST_CHECK(smh.find("SmGRow_Ready_Idle_CheckEvt_Ready_WaitingTray") != std::string::npos);
    BOOST_CHECK(smh.find("SmGRow_Ready_Idle_CheckEvt_Ready_NoTray_else") != std::string::npos);
    BOOST_CHECK(smh.find("callEntry_Ready_WaitingTray") != std::string::npos);
    BOOST_CHECK(smh.find("callExit_Ready_WaitingTray") != std::string::npos);
    BOOST_CHECK(smh.find("_row<Ready_WaitingTray, TrayPresentEvt, Ready_Tray>") != std::string::npos);
    BOOST_CHECK(smh.find("callEntry_Ready_Tray") != std::string::npos);
    BOOST_CHECK(smh.find("SmAiRow_Ready_Idle_HeartbeatEvt") != std::string::npos);
    BOOST_CHECK(smh.find("SmRow_Ready_WaitingTray_BlockedEvt_Ready_Jam") != std::string::npos);
    BOOST_CHECK(smh.find("SmGRow_Ready_Tray_TransferEvt_Ready_WaitingTray") != std::string::npos);
    BOOST_CHECK(smh.find("SmARow_Ready_Tray_TrayAbsentEvt_Ready_NoTray") != std::string::npos);
    BOOST_CHECK(smh.find("SmGRow_NotReady_CheckEvt_Ready_Idle") != std::string::npos);
    BOOST_CHECK(smh.find("SmGRow_Ready_Idle_CheckEvt_NotReady") != std::string::npos);
    BOOST_CHECK(smh.find("static_cast<MyDeviceSm_&>(fsm).guard_canTransfer_Check(evt)") != std::string::npos);
    BOOST_CHECK(smh.find("TransitionRowCount = 22u") != std::string::npos);
    BOOST_CHECK(smh.find("static_assert(TransitionRowCount <= " + std::to_string(MsmMaxTransitionRows)) != std::string::npos);

    const auto& smcpp = files.at(smSourceFile("MyDevice"));
    BOOST_CHECK(smcpp.find("void MyDeviceSm_::callEntry_Ready_WaitingTray") != std::string::npos);
    BOOST_CHECK(smcpp.find("self.owner->enterReady_WaitingTray") != std::string::npos);
    BOOST_CHECK(smcpp.find("void MyDevice::idleEntry()") == std::string::npos);
    BOOST_CHECK(smcpp.find("idleEntry();") != std::string::npos);
    BOOST_CHECK(smcpp.find("waitingTrayExit();") != std::string::npos);
    BOOST_CHECK(smcpp.find("void MyDeviceSm_::triggerCheck_Transfer(TransferEvt const&)") != std::string::npos);
    BOOST_CHECK(smcpp.find("syncComposites({\"Ready\"})") != std::string::npos);
    BOOST_CHECK(smcpp.find("readyEntry();") != std::string::npos);

    const auto& decl = files.at(smDeclFile("MyDevice"));
    BOOST_CHECK(decl.find("friend struct MyDeviceSm_;") != std::string::npos);
    BOOST_CHECK(decl.find("void idleEntry();") != std::string::npos);
    BOOST_CHECK(decl.find("syncComposites") != std::string::npos);
    BOOST_CHECK(decl.find("void idleOnTransfer();") == std::string::npos);
    BOOST_CHECK(decl.find("std::unique_ptr<MyDeviceSm>") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(statemachine_cpp_generator_emits_trace_when_genopt_trace)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "TracePilot",
      "genopt": {
        "namespace": "sample::statemachine",
        "generator": "statemachine-cpp",
        "trace": true
      },
      "regions": [{
        "states": [
          {
            "name": "Idle",
            "initial": true,
            "entry": "check",
            "transitions": [
              { "event": "go", "action": "^check" },
              { "guard": "isAllowed", "to": "Done" }
            ]
          },
          { "name": "Done" }
        ]
      }]
    })";

    const auto doc = SmdParser::parse(json);
    BOOST_CHECK(doc.smTrace);
    const auto files  = StateMachineCppGenerator::emit(doc);
    const auto& smcpp = files.at(smSourceFile("TracePilot"));
    const auto& decl  = files.at(smDeclFile("TracePilot"));

    BOOST_CHECK(smcpp.find("smStateName") != std::string::npos);
    BOOST_CHECK(smcpp.find("[TracePilot] state ->") != std::string::npos);
    BOOST_CHECK(smcpp.find("[TracePilot] <- go") != std::string::npos);
    BOOST_CHECK(smcpp.find("[TracePilot] ^check from Idle (go)") != std::string::npos);
    BOOST_CHECK(smcpp.find("[TracePilot] guard isAllowed (check)") != std::string::npos);
    BOOST_CHECK(smcpp.find("[TracePilot] -> check (entry Idle)") != std::string::npos);
    BOOST_CHECK(decl.find("void postSmEvent(GoEvt const&)") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(statemachine_cpp_generator_rejects_unknown_backend)
{
    Document doc;
    doc.machine   = "X";
    doc.ns        = "sample::statemachine";
    doc.generator = "other-backend";
    doc.initial   = "Idle";
    doc.events    = {"go"};
    State idleState;
    idleState.name    = "Idle";
    idleState.initial = true;
    doc.states.push_back(idleState);

    BOOST_CHECK_THROW((void)StateMachineCppGenerator::emit(doc), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(statemachine_cpp_generator_emits_guarded_transition)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "Gate",
      "genopt": {
        "namespace": "sample::statemachine",
        "generator": "statemachine-cpp"
      },
      "regions": [{
        "states": [
          {
            "name": "Idle",
            "initial": true,
            "transitions": [
              { "event": "go", "to": "Done", "guard": "isAllowed", "action": "handler" }
            ]
          },
          { "name": "Done" }
        ]
      }]
    })";

    const auto doc   = SmdParser::parse(json);
    const auto files = StateMachineCppGenerator::emit(doc);
    const auto& smh  = files.at(smHeaderFile("Gate"));

    BOOST_CHECK(smh.find("SmRow_Idle_GoEvt_Done") != std::string::npos);
    BOOST_CHECK(smh.find("static_cast<GateSm_&>(fsm).guard_isAllowed_Go(evt)") != std::string::npos);
    BOOST_CHECK(smh.find("bool guard_isAllowed_Go(GoEvt const&)") != std::string::npos);
    BOOST_CHECK(files.at(smSourceFile("Gate")).find("owner->isAllowed();") != std::string::npos);
    BOOST_CHECK(files.at(smSourceFile("Gate")).find("return ok;") != std::string::npos);
    BOOST_CHECK(files.at(smSourceFile("Gate")).find("owner->idleOnGo();") != std::string::npos);
    BOOST_CHECK(files.at(smDeclFile("Gate")).find("bool isAllowed();") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(statemachine_cpp_generator_emits_guard_else_transition)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "Gate",
      "genopt": {
        "namespace": "sample::statemachine",
        "generator": "statemachine-cpp"
      },
      "regions": [{
        "states": [
          {
            "name": "Idle",
            "initial": true,
            "transitions": [
              { "event": "go", "guard": "isAllowed", "to": "Done", "else": "Rejected", "action": "handler" }
            ]
          },
          { "name": "Done" },
          { "name": "Rejected" }
        ]
      }]
    })";

    const auto doc   = SmdParser::parse(json);
    const auto files = StateMachineCppGenerator::emit(doc);
    const auto& smh  = files.at(smHeaderFile("Gate"));
    const auto& smcp = files.at(smSourceFile("Gate"));

    BOOST_CHECK(smh.find("SmRow_Idle_GoEvt_Done") != std::string::npos);
    BOOST_CHECK(smh.find("SmRow_Idle_GoEvt_Rejected_else") != std::string::npos);
    BOOST_CHECK(smcp.find("guard_else_isAllowed_Go") != std::string::npos);
    BOOST_CHECK(smcp.find("ok = !ok;") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(statemachine_cpp_generator_emits_check_guarded_transition)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "Gate",
      "genopt": {
        "namespace": "sample::statemachine",
        "generator": "statemachine-cpp"
      },
      "regions": [{
        "states": [
          {
            "name": "Idle",
            "initial": true,
            "transitions": [
              { "event": "go" },
              { "guard": "isAllowed", "to": "Done", "else": "Rejected" }
            ]
          },
          { "name": "Done" },
          { "name": "Rejected" }
        ]
      }]
    })";

    const auto doc   = SmdParser::parse(json);
    const auto files = StateMachineCppGenerator::emit(doc);
    const auto& smh  = files.at(smHeaderFile("Gate"));

    BOOST_CHECK(smh.find("SmAiRow_Idle_GoEvt_Idle") != std::string::npos);
    BOOST_CHECK(smh.find("SmGRow_Idle_CheckEvt_Done") != std::string::npos);
    BOOST_CHECK(smh.find("SmGRow_Idle_CheckEvt_Rejected_else") != std::string::npos);
    BOOST_CHECK(smh.find("action_idleOnCheck") == std::string::npos);
    BOOST_CHECK(files.at(smSourceFile("Gate")).find("static_cast<GateSm*>(this)->process_event(CheckEvt{});") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(smd_parser_rejects_else_without_guard)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "X",
      "genopt": { "namespace": "sample::statemachine" },
      "regions": [{
        "states": [{
          "name": "A",
          "initial": true,
          "transitions": [ { "event": "go", "to": "B", "else": "C" } ]
        }, { "name": "B" }, { "name": "C" }]
      }]
    })";

    BOOST_CHECK_THROW((void)SmdParser::parse(json), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(statemachine_cpp_generator_emits_entry_check)
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

    const auto doc    = SmdParser::parse(json);
    const auto files  = StateMachineCppGenerator::emit(doc);
    const auto& smcpp = files.at(smSourceFile("ReadyGate"));

    BOOST_CHECK(smcpp.find("void ReadyGate::enterNotReady()") != std::string::npos);
    BOOST_CHECK(smcpp.find("_stateMachine->process_event(CheckEvt{});") != std::string::npos);
    BOOST_CHECK(smcpp.find("notReadyEntry();") == std::string::npos);
    BOOST_CHECK(files.at(smHeaderFile("ReadyGate"))
                    .find("SmGRow_NotReady_CheckEvt_Ready") != std::string::npos);
    BOOST_CHECK(files.at(smHeaderFile("ReadyGate")).find("action_notReadyOnCheck") == std::string::npos);
}

BOOST_AUTO_TEST_CASE(statemachine_cpp_generator_emits_entry_handler_then_check)
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
            "entry": { "binding": "bound", "check": true },
            "transitions": [
              { "check": true, "guard": "isReady", "to": "Ready" }
            ]
          },
          { "name": "Ready" }
        ]
      }]
    })";

    const auto doc    = SmdParser::parse(json);
    const auto files  = StateMachineCppGenerator::emit(doc);
    const auto& smcpp = files.at(smSourceFile("ReadyGate"));

    const auto enterPos = smcpp.find("void ReadyGate::enterNotReady()");
    BOOST_REQUIRE(enterPos != std::string::npos);
    const auto entryPos = smcpp.find("notReadyEntry();", enterPos);
    const auto checkPos = smcpp.find("process_event(CheckEvt{})", enterPos);
    BOOST_REQUIRE(entryPos != std::string::npos);
    BOOST_REQUIRE(checkPos != std::string::npos);
    BOOST_CHECK(entryPos < checkPos);
}

BOOST_AUTO_TEST_CASE(statemachine_cpp_generator_emits_timer_arm_cancel)
{
    constexpr const char* json = R"({
      "$schema": "foundation-smd-4",
      "machine": "TimerPilot",
      "genopt": {
        "namespace": "sample::statemachine",
        "generator": "statemachine-cpp",
        "stateSignal": "state"
      },
      "timeRequests": [
        { "name": "transferTimeout", "fires": "transferTimeout" }
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
    SmdParser::validate(doc);
    const auto files  = StateMachineCppGenerator::emit(doc);
    const auto& smcpp = files.at(smSourceFile("TimerPilot"));
    const auto& decl  = files.at(smDeclFile("TimerPilot"));

    BOOST_CHECK(smcpp.find("armTimeout(_transferTimeout)") != std::string::npos);
    BOOST_CHECK(smcpp.find("cancelTimeout(_transferTimeout)") != std::string::npos);
    const auto enterPos = smcpp.find("void TimerPilot::enterWaitingTray()");
    const auto armPos   = smcpp.find("armTimeout(_transferTimeout)", enterPos);
    const auto entryPos = smcpp.find("waitingTrayEntry();", enterPos);
    BOOST_REQUIRE(enterPos != std::string::npos);
    BOOST_REQUIRE(entryPos != std::string::npos);
    BOOST_REQUIRE(armPos != std::string::npos);
    BOOST_CHECK(entryPos < armPos);
    BOOST_CHECK(decl.find("TimeRequest _transferTimeout") != std::string::npos);
    BOOST_CHECK(decl.find("void onTransferTimeout()") != std::string::npos);
    BOOST_CHECK(smcpp.find("state(id)") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(statemachine_cpp_generator_counts_my_device_transition_rows)
{
    const auto doc = SmdParser::loadFile(
        std::string{FOUNDATION_SOURCE_DIR} + "/src/sample/statemachine/MyDevice.smd");
    BOOST_CHECK_EQUAL(StateMachineCppGenerator::countTransitionRows(doc), 22u);
    BOOST_CHECK(StateMachineCppGenerator::countTransitionRows(doc) <= MsmMaxTransitionRows);
}

BOOST_AUTO_TEST_CASE(statemachine_cpp_generator_rejects_transition_row_limit_exceeded)
{
    Document doc;
    doc.machine   = "TooBig";
    doc.ns        = "sample::statemachine";
    doc.generator = "statemachine-cpp";
    doc.initial   = "S0";
    doc.events    = {"check"};

    for (int i = 0; i < 32; ++i)
    {
        State state;
        state.name = "S" + std::to_string(i);
        if (i == 0)
        {
            state.initial = true;
        }
        doc.states.push_back(state);

        if (i + 1 < 32)
        {
            Transition tr;
            tr.kind  = TransitionKind::OnCheck;
            tr.from  = "S" + std::to_string(i);
            tr.on    = "check";
            tr.to    = "S" + std::to_string(i + 1);
            tr.guard = "always";
            doc.transitions.push_back(std::move(tr));
        }
    }

    BOOST_REQUIRE_EQUAL(StateMachineCppGenerator::countTransitionRows(doc), 31u);
    BOOST_CHECK_THROW((void)StateMachineCppGenerator::emit(doc), std::runtime_error);
}
