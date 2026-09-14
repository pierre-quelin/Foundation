/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/statemachine/StateMachineCppGenerator.hpp"

#include "tools/design/statemachine/SmdNaming.hpp"
#include "tools/design/statemachine/SmdParser.hpp"
#include "tools/design/statemachine/StateMachineLimits.hpp"

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <vector>

namespace tools::design::statemachine::smd
{

namespace
{

[[nodiscard]] std::string defaultIncludePrefix(const std::string& ns)
{
    std::string prefix = ns;
    for (char& ch : prefix)
    {
        if (ch == ':')
        {
            ch = '/';
        }
    }
    return prefix;
}

[[nodiscard]] std::string generatedBanner(const Document& doc, const std::string& suffix)
{
    return "/** This file was generated automatically from " + doc.machine + ".smd" + suffix + " — do not edit manually. */\n";
}

[[nodiscard]] std::string smStructName(const Document& doc)
{
    return doc.machine + "Sm_";
}

[[nodiscard]] std::string smTypeName(const Document& doc)
{
    return doc.machine + "Sm";
}

[[nodiscard]] std::string enterPlumbingName(const std::string& state)
{
    return "enter" + state;
}

[[nodiscard]] std::string leavePlumbingName(const std::string& state)
{
    return "leave" + state;
}

[[nodiscard]] std::string stateIdEnum(const std::string& state)
{
    return "State::Id::" + state;
}

[[nodiscard]] std::string pilotStateIdEnum(const Document& doc, const std::string& state)
{
    return doc.machine + "::State::Id::" + state;
}

[[nodiscard]] std::string smTraceTag(const Document& doc)
{
    return "[" + doc.machine + "] ";
}

void emitSyncCompositesCall(std::ostringstream& out, const Document& doc, const std::string& leafQualified)
{
    const auto it = doc.compositeChain.find(leafQualified);
    if (it == doc.compositeChain.end() || it->second.empty())
    {
        out << "    syncComposites({});\n";
        return;
    }
    out << "    syncComposites({";
    for (std::size_t i = 0; i < it->second.size(); ++i)
    {
        out << "\"" << it->second[i] << "\"";
        if (i + 1U < it->second.size())
        {
            out << ", ";
        }
    }
    out << "});\n";
}

[[nodiscard]] std::string emitSyncCompositesMethod(const Document& doc)
{
    if (doc.composites.empty())
    {
        return {};
    }

    std::ostringstream out;
    out << "void " << doc.machine << "::syncComposites(std::initializer_list<const char*> desired)\n";
    out << "{\n";
    out << "    const std::vector<std::string> next(desired.begin(), desired.end());\n";
    out << "    std::size_t common = 0;\n";
    out << "    while (common < _smActiveComposites.size() && common < next.size()\n";
    out << "           && _smActiveComposites[common] == next[common])\n";
    out << "    {\n";
    out << "        ++common;\n";
    out << "    }\n";

    bool hasCompositeExit = false;
    for (const auto& composite : doc.composites)
    {
        if (composite.hasExit && !composite.onExit.empty())
        {
            hasCompositeExit = true;
            break;
        }
    }

    if (hasCompositeExit)
    {
        out << "    for (std::size_t i = _smActiveComposites.size(); i > common; --i)\n";
        out << "    {\n";
        out << "        const std::string& composite = _smActiveComposites[i - 1U];\n";
        for (const auto& composite : doc.composites)
        {
            if (composite.hasExit && !composite.onExit.empty())
            {
                out << "        if (composite == \"" << composite.name << "\")\n";
                out << "        {\n";
                out << "            " << composite.onExit.front().handler << "();\n";
                out << "        }\n";
            }
        }
        out << "    }\n";
    }

    out << "    for (std::size_t i = common; i < next.size(); ++i)\n";
    out << "    {\n";
    out << "        const std::string& composite = next[i];\n";
    for (const auto& composite : doc.composites)
    {
        if (composite.hasEntry && !composite.onEntry.empty())
        {
            out << "        if (composite == \"" << composite.name << "\")\n";
            out << "        {\n";
            out << "            " << composite.onEntry.front().handler << "();\n";
            out << "        }\n";
        }
    }
    out << "    }\n";
    out << "    _smActiveComposites = next;\n";
    out << "}\n\n";
    return out.str();
}

[[nodiscard]] std::string mplVectorType(std::size_t count)
{
    (void)count;
    return "boost::mpl::vector";
}

void appendBindingCall(std::ostringstream& out, const std::string& indent, const std::string& binding, const std::string& call)
{
    if (binding == "bound")
    {
        out << indent << call << ";\n";
        return;
    }
    out << indent << "frontEnd().schedule([this]() { " << call << "; });\n";
}

void appendArmCalls(std::ostringstream& out, const State& state)
{
    for (const auto& timer : state.arm)
    {
        out << "    frontEnd().armTimeout(" << timeRequestMember(timer) << ");\n";
    }
}

void appendCancelCalls(std::ostringstream& out, const State& state)
{
    for (const auto& timer : state.cancel)
    {
        out << "    frontEnd().cancelTimeout(" << timeRequestMember(timer) << ");\n";
    }
}

void appendEntryBody(std::ostringstream& out, const Document& doc, const State& state)
{
    const bool hasHandler = state.hasEntry && !state.onEntry.empty();
    const std::string binding =
        hasHandler ? state.onEntry.front().binding : state.entryBinding;

    if (state.entryCheck)
    {
        if (binding == "bound")
        {
            if (hasHandler)
            {
                out << "    " << state.onEntry.front().handler << "();\n";
            }
            if (doc.smTrace)
            {
                out << "    frontEnd().logger().log(util::logger::LogService::LogLevel::DEBUG,\"" << smTraceTag(doc) << "-> check (entry "
                    << state.name << ")\");\n";
            }
            out << "    _stateMachine->process_event(" << msmEventName(CheckEvent) << "{});\n";
            appendArmCalls(out, state);
            return;
        }
        out << "    frontEnd().schedule([this]() {\n";
        if (hasHandler)
        {
            out << "        " << state.onEntry.front().handler << "();\n";
        }
        if (doc.smTrace)
        {
            out << "        frontEnd().logger().log(util::logger::LogService::LogLevel::DEBUG,\"" << smTraceTag(doc) << "-> check (entry "
                << state.name << ")\");\n";
        }
        out << "        _stateMachine->process_event(" << msmEventName(CheckEvent) << "{});\n";
        out << "    });\n";
        appendArmCalls(out, state);
        return;
    }

    if (hasHandler)
    {
        appendBindingCall(out, "    ", binding, state.onEntry.front().handler + "()");
    }
    appendArmCalls(out, state);
}

enum class GuardBranch
{
    WhenTrue,
    WhenFalse
};

struct GuardAdapterKey
{
    std::string guard;
    std::string event;
    GuardBranch branch;
    bool guardNegated{false};

    bool operator==(const GuardAdapterKey& other) const
    {
        return guard == other.guard && event == other.event && branch == other.branch && guardNegated == other.guardNegated;
    }
};

struct GuardAdapterKeyHash
{
    std::size_t operator()(const GuardAdapterKey& key) const
    {
        return std::hash<std::string>{}(key.guard + '\0' + key.event + (key.branch == GuardBranch::WhenTrue ? 'T' : 'F') + (key.guardNegated ? 'N' : 'P'));
    }
};

struct TriggerCheckKey
{
    std::string from;
    std::string event;

    bool operator==(const TriggerCheckKey& other) const
    {
        return from == other.from && event == other.event;
    }
};

struct TriggerCheckKeyHash
{
    std::size_t operator()(const TriggerCheckKey& key) const
    {
        return std::hash<std::string>{}(key.from + '\0' + key.event);
    }
};

struct ActionAdapterKey
{
    std::string actionHandler;
    std::string event;

    bool operator==(const ActionAdapterKey& other) const
    {
        return actionHandler == other.actionHandler && event == other.event;
    }
};

struct ActionAdapterKeyHash
{
    std::size_t operator()(const ActionAdapterKey& key) const
    {
        return std::hash<std::string>{}(key.actionHandler + '\0' + key.event);
    }
};

[[nodiscard]] std::vector<ActionAdapterKey> collectTransitionActions(const Document& doc)
{
    std::vector<ActionAdapterKey> actions;
    std::unordered_set<ActionAdapterKey, ActionAdapterKeyHash> seen;
    for (const auto& tr : doc.transitions)
    {
        if (tr.kind == TransitionKind::TriggerCheck || tr.actionHandler.empty())
        {
            continue;
        }
        ActionAdapterKey key{tr.actionHandler, tr.on};
        if (seen.insert(key).second)
        {
            actions.push_back(std::move(key));
        }
    }
    return actions;
}

[[nodiscard]] const Transition* findTriggerCheckTransition(const Document& doc,
                                                           const TriggerCheckKey& trigger)
{
    for (const auto& tr : doc.transitions)
    {
        if (tr.kind == TransitionKind::TriggerCheck && tr.from == trigger.from && tr.on == trigger.event)
        {
            return &tr;
        }
    }
    return nullptr;
}

[[nodiscard]] std::vector<TriggerCheckKey> collectTriggerChecks(const Document& doc)
{
    std::vector<TriggerCheckKey> checks;
    std::unordered_set<TriggerCheckKey, TriggerCheckKeyHash> seen;
    for (const auto& tr : doc.transitions)
    {
        if (tr.kind != TransitionKind::TriggerCheck)
        {
            continue;
        }
        TriggerCheckKey key{tr.from, tr.on};
        if (seen.insert(key).second)
        {
            checks.push_back(std::move(key));
        }
    }
    return checks;
}

[[nodiscard]] std::string guardAdapterMethodName(const GuardAdapterKey& key)
{
    if (key.branch == GuardBranch::WhenFalse)
    {
        return smGuardElseMethodName(key.guard, key.event, key.guardNegated);
    }
    return smGuardMethodName(key.guard, key.event, key.guardNegated);
}

[[nodiscard]] std::vector<GuardAdapterKey> collectGuardAdapters(const Document& doc)
{
    std::vector<GuardAdapterKey> adapters;
    std::unordered_set<GuardAdapterKey, GuardAdapterKeyHash> seen;
    for (const auto& tr : doc.transitions)
    {
        if (tr.guard.empty())
        {
            continue;
        }
        GuardAdapterKey whenTrue{tr.guard, tr.on, GuardBranch::WhenTrue, tr.guardNegated};
        if (seen.insert(whenTrue).second)
        {
            adapters.push_back(std::move(whenTrue));
        }
        if (!tr.elseTo.empty())
        {
            GuardAdapterKey whenFalse{tr.guard, tr.on, GuardBranch::WhenFalse, tr.guardNegated};
            if (seen.insert(whenFalse).second)
            {
                adapters.push_back(std::move(whenFalse));
            }
        }
    }
    return adapters;
}

[[nodiscard]] std::string smRowStructName(const std::string& prefix, const std::string& from, const std::string& evt, const std::string& to, const std::string& suffix = {})
{
    return prefix + "_" + from + "_" + evt + "_" + to + suffix;
}

void emitGuardRowStruct(std::ostringstream& out, const std::string& smStruct, const std::string& name, const std::string& from, const std::string& evtType, const std::string& to, const std::string& guardMethod)
{
    out << "    struct " << name << "\n    {\n";
    out << "        typedef ::boost::msm::g_row_tag row_type_tag;\n";
    out << "        typedef " << from << " Source;\n";
    out << "        typedef " << to << " Target;\n";
    out << "        typedef " << evtType << " Evt;\n";
    out << "        template<class FSM, class SourceState, class TargetState, class AllStates>\n";
    out << "        static bool guard_call(FSM& fsm, " << evtType << " const& evt,\n";
    out << "                                  SourceState&, TargetState&, AllStates&)\n";
    out << "        {\n";
    out << "            return static_cast<" << smStruct << "&>(fsm)." << guardMethod << "(evt);\n";
    out << "        }\n";
    out << "    };\n\n";
}

void emitActionRowStruct(std::ostringstream& out, const std::string& smStruct, const std::string& name, const std::string& from, const std::string& evtType, const std::string& to, const std::string& actionMethod)
{
    out << "    struct " << name << "\n    {\n";
    out << "        typedef ::boost::msm::a_row_tag row_type_tag;\n";
    out << "        typedef " << from << " Source;\n";
    out << "        typedef " << to << " Target;\n";
    out << "        typedef " << evtType << " Evt;\n";
    out << "        template<class FSM, class SourceState, class TargetState, class AllStates>\n";
    out << "        static ::boost::msm::back::HandledEnum action_call(FSM& fsm, " << evtType
        << " const& evt,\n";
    out << "                                                         SourceState&, TargetState&, AllStates&)\n";
    out << "        {\n";
    out << "            static_cast<" << smStruct << "&>(fsm)." << actionMethod << "(evt);\n";
    out << "            return ::boost::msm::back::HANDLED_TRUE;\n";
    out << "        }\n";
    out << "    };\n\n";
}

void emitActionGuardRowStruct(std::ostringstream& out, const std::string& smStruct, const std::string& name, const std::string& from, const std::string& evtType, const std::string& to, const std::string& actionMethod, const std::string& guardMethod)
{
    out << "    struct " << name << "\n    {\n";
    out << "        typedef ::boost::msm::row_tag row_type_tag;\n";
    out << "        typedef " << from << " Source;\n";
    out << "        typedef " << to << " Target;\n";
    out << "        typedef " << evtType << " Evt;\n";
    out << "        template<class FSM, class SourceState, class TargetState, class AllStates>\n";
    out << "        static ::boost::msm::back::HandledEnum action_call(FSM& fsm, " << evtType
        << " const& evt,\n";
    out << "                                                         SourceState&, TargetState&, AllStates&)\n";
    out << "        {\n";
    out << "            static_cast<" << smStruct << "&>(fsm)." << actionMethod << "(evt);\n";
    out << "            return ::boost::msm::back::HANDLED_TRUE;\n";
    out << "        }\n";
    out << "        template<class FSM, class SourceState, class TargetState, class AllStates>\n";
    out << "        static bool guard_call(FSM& fsm, " << evtType << " const& evt,\n";
    out << "                                  SourceState&, TargetState&, AllStates&)\n";
    out << "        {\n";
    out << "            return static_cast<" << smStruct << "&>(fsm)." << guardMethod << "(evt);\n";
    out << "        }\n";
    out << "    };\n\n";
}

void emitInternalActionRowStruct(std::ostringstream& out, const std::string& smStruct, const std::string& name, const std::string& state, const std::string& evtType, const std::string& actionMethod)
{
    out << "    struct " << name << "\n    {\n";
    out << "        typedef ::boost::msm::a_irow_tag row_type_tag;\n";
    out << "        typedef " << state << " Source;\n";
    out << "        typedef " << state << " Target;\n";
    out << "        typedef " << evtType << " Evt;\n";
    out << "        template<class FSM, class SourceState, class TargetState, class AllStates>\n";
    out << "        static ::boost::msm::back::HandledEnum action_call(FSM& fsm, " << evtType
        << " const& evt,\n";
    out << "                                                         SourceState&, TargetState&, AllStates&)\n";
    out << "        {\n";
    out << "            static_cast<" << smStruct << "&>(fsm)." << actionMethod << "(evt);\n";
    out << "            return ::boost::msm::back::HANDLED_TRUE;\n";
    out << "        }\n";
    out << "    };\n\n";
}

void appendTransitionRows(std::ostringstream& structOut, std::vector<std::string>& rowRefs, const Transition& tr, const std::string& smStruct)
{
    const std::string evt        = msmEventName(tr.on);
    const std::string corePrefix = tr.from + ", " + evt + ", ";

    if (tr.kind == TransitionKind::TriggerCheck)
    {
        const std::string name = smRowStructName("SmAiRow", tr.from, evt, tr.from);
        emitInternalActionRowStruct(structOut, smStruct, name, tr.from, evt, triggerCheckMethodName(tr.on));
        rowRefs.push_back(name);
        return;
    }

    if (tr.actionHandler.empty())
    {
        if (!tr.guard.empty() && !tr.elseTo.empty())
        {
            const std::string trueName =
                smRowStructName("SmGRow", tr.from, evt, tr.to);
            emitGuardRowStruct(structOut, smStruct, trueName, tr.from, evt, tr.to, smGuardMethodName(tr.guard, tr.on, tr.guardNegated));
            rowRefs.push_back(trueName);

            const std::string falseName =
                smRowStructName("SmGRow", tr.from, evt, tr.elseTo, "_else");
            emitGuardRowStruct(structOut, smStruct, falseName, tr.from, evt, tr.elseTo, smGuardElseMethodName(tr.guard, tr.on, tr.guardNegated));
            rowRefs.push_back(falseName);
            return;
        }
        if (!tr.guard.empty())
        {
            const std::string name = smRowStructName("SmGRow", tr.from, evt, tr.to);
            emitGuardRowStruct(structOut, smStruct, name, tr.from, evt, tr.to, smGuardMethodName(tr.guard, tr.on, tr.guardNegated));
            rowRefs.push_back(name);
            return;
        }
        rowRefs.push_back("_row<" + corePrefix + tr.to + ">");
        return;
    }

    const std::string actionMethod = smActionMethodName(tr.actionHandler);

    if (tr.kind == TransitionKind::Internal)
    {
        const std::string name = smRowStructName("SmAiRow", tr.from, evt, tr.from);
        emitInternalActionRowStruct(structOut, smStruct, name, tr.from, evt, actionMethod);
        rowRefs.push_back(name);
        return;
    }

    if (!tr.guard.empty() && !tr.elseTo.empty())
    {
        const std::string trueName = smRowStructName("SmRow", tr.from, evt, tr.to);
        emitActionGuardRowStruct(structOut, smStruct, trueName, tr.from, evt, tr.to, actionMethod, smGuardMethodName(tr.guard, tr.on, tr.guardNegated));
        rowRefs.push_back(trueName);

        const std::string falseName = smRowStructName("SmRow", tr.from, evt, tr.elseTo, "_else");
        emitActionGuardRowStruct(structOut, smStruct, falseName, tr.from, evt, tr.elseTo, actionMethod, smGuardElseMethodName(tr.guard, tr.on, tr.guardNegated));
        rowRefs.push_back(falseName);
        return;
    }
    if (!tr.guard.empty())
    {
        const std::string name = smRowStructName("SmRow", tr.from, evt, tr.to);
        emitActionGuardRowStruct(structOut, smStruct, name, tr.from, evt, tr.to, actionMethod, smGuardMethodName(tr.guard, tr.on, tr.guardNegated));
        rowRefs.push_back(name);
        return;
    }

    const std::string name = smRowStructName("SmARow", tr.from, evt, tr.to);
    emitActionRowStruct(structOut, smStruct, name, tr.from, evt, tr.to, actionMethod);
    rowRefs.push_back(name);
}

[[nodiscard]] std::vector<std::string> emitAllTransitionRows(const Document& doc,
                                                             const std::string& smStruct)
{
    std::ostringstream structOut;
    std::vector<std::string> rows;
    for (const auto& tr : doc.transitions)
    {
        appendTransitionRows(structOut, rows, tr, smStruct);
    }
    return rows;
}

void emitAllTransitionRowStructs(std::ostringstream& out, const Document& doc, const std::string& smStruct)
{
    std::vector<std::string> unused;
    for (const auto& tr : doc.transitions)
    {
        appendTransitionRows(out, unused, tr, smStruct);
    }
}

void validateTransitionRowCount(const Document& doc)
{
    const std::size_t count = emitAllTransitionRows(doc, smStructName(doc)).size();
    if (count > MsmMaxTransitionRows)
    {
        throw std::runtime_error("statemachine-cpp: " + doc.machine + " exceeds transition row limit (" + std::to_string(count) + " > " + std::to_string(MsmMaxTransitionRows) + ")");
    }
}

[[nodiscard]] std::string emitSmHeader(const Document& doc)
{
    const std::string smStruct = smStructName(doc);
    const std::string smType   = smTypeName(doc);

    std::ostringstream out;
    out << generatedBanner(doc, " (state machine header)");
    out << "#pragma once\n\n";
    out << "#ifndef BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS\n";
    out << "#define BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS\n";
    out << "#endif\n";
    out << "#ifndef BOOST_MPL_LIMIT_VECTOR_SIZE\n";
    out << "#define BOOST_MPL_LIMIT_VECTOR_SIZE " << MsmMaxTransitionRows << "\n";
    out << "#endif\n\n";
    out << "#include <boost/msm/back/state_machine.hpp>\n";
    out << "#include <boost/msm/front/state_machine_def.hpp>\n";
    out << "#include <boost/mpl/vector.hpp>\n\n";
    out << "namespace " << doc.ns << "\n{\n\n";
    out << "class " << doc.machine << ";\n\n";

    for (const auto& event : doc.events)
    {
        out << "struct " << msmEventName(event) << " {};\n";
    }
    out << "\n";

    out << "struct " << smStruct << " : public boost::msm::front::state_machine_def<" << smStruct
        << ">\n{\n";
    out << "    " << doc.machine << "* owner{nullptr};\n\n";

    for (const auto& state : doc.states)
    {
        out << "    struct " << state.name << " : boost::msm::front::state<>\n";
        out << "    {\n";
        out << "        template<class Event, class Fsm>\n";
        out << "        void on_entry(const Event&, Fsm& fsm)\n";
        out << "        {\n";
        out << "            " << smStruct << "::callEntry_" << state.name << "(static_cast<"
            << smStruct << "&>(fsm));\n";
        out << "        }\n";

        if (state.hasExit)
        {
            out << "\n        template<class Event, class Fsm>\n";
            out << "        void on_exit(const Event&, Fsm& fsm)\n";
            out << "        {\n";
            out << "            " << smStruct << "::callExit_" << state.name << "(static_cast<"
                << smStruct << "&>(fsm));\n";
            out << "        }\n";
        }

        out << "    };\n\n";
    }

    for (const auto& state : doc.states)
    {
        out << "    static void callEntry_" << state.name << "(" << smStruct << "& self);\n";
        if (state.hasExit)
        {
            out << "    static void callExit_" << state.name << "(" << smStruct << "& self);\n";
        }
    }
    out << "\n";

    for (const auto& adapter : collectGuardAdapters(doc))
    {
        out << "    bool " << guardAdapterMethodName(adapter) << "(" << msmEventName(adapter.event)
            << " const&);\n\n";
    }

    for (const auto& action : collectTransitionActions(doc))
    {
        out << "    void " << smActionMethodName(action.actionHandler) << "("
            << msmEventName(action.event) << " const&);\n\n";
    }

    for (const auto& trigger : collectTriggerChecks(doc))
    {
        out << "    void " << triggerCheckMethodName(trigger.event) << "("
            << msmEventName(trigger.event) << " const&);\n\n";
    }

    emitAllTransitionRowStructs(out, doc, smStruct);

    const auto transitionRows = emitAllTransitionRows(doc, smStruct);
    out << "    static constexpr std::size_t TransitionRowCount = " << transitionRows.size()
        << "u;\n\n";
    out << "    using initial_state = " << doc.initial << ";\n\n";
    out << "    using transition_table = " << mplVectorType(transitionRows.size()) << "<\n";
    for (std::size_t i = 0; i < transitionRows.size(); ++i)
    {
        out << "        " << transitionRows[i];
        if (i + 1 < transitionRows.size())
        {
            out << ",";
        }
        out << "\n";
    }
    out << "        >;\n";
    out << "    static_assert(TransitionRowCount <= " << MsmMaxTransitionRows
        << "u, \"" << doc.machine
        << ": transition table exceeds Foundation MSM limit (see StateMachineLimits.hpp)\");\n";
    out << "};\n\n";
    out << "class " << smType << " : public boost::msm::back::state_machine<" << smStruct << ">\n";
    out << "{\n";
    out << "public:\n";
    out << "    using boost::msm::back::state_machine<" << smStruct << ">::state_machine;\n";
    out << "};\n\n";
    out << "} // namespace " << doc.ns << "\n";
    return out.str();
}

[[nodiscard]] std::string emitSmSource(const Document& doc, const std::string& includePrefix)
{
    std::ostringstream out;
    out << generatedBanner(doc, " (state machine plumbing)");
    out << "\n#include \"" << includePrefix << "/" << smHeaderFile(doc.machine) << "\"\n\n";
    out << "#include \"" << includePrefix << "/" << pilotHeaderFile(doc.machine) << "\"\n\n";
    out << "namespace " << doc.ns << "\n{\n\n";

    if (doc.smTrace)
    {
        out << "namespace\n{\n";
        out << "const char* smStateName(const " << doc.machine << "::State::Id id)\n";
        out << "{\n";
        out << "    switch (id)\n";
        out << "    {\n";
        for (const auto& state : doc.states)
        {
            out << "    case " << pilotStateIdEnum(doc, state.name) << ":\n";
            out << "        return \"" << state.name << "\";\n";
        }
        out << "    default:\n";
        out << "        return \"?\";\n";
        out << "    }\n";
        out << "}\n";
        out << "} // namespace\n\n";
    }

    const std::string smStruct = smStructName(doc);

    for (const auto& state : doc.states)
    {
        out << "void " << smStruct << "::callEntry_" << state.name << "(" << smStruct << "& self)\n";
        out << "{\n";
        out << "    self.owner->" << enterPlumbingName(state.name) << "();\n";
        out << "}\n\n";

        if (state.hasExit)
        {
            out << "void " << smStruct << "::callExit_" << state.name << "(" << smStruct << "& self)\n";
            out << "{\n";
            out << "    self.owner->" << leavePlumbingName(state.name) << "();\n";
            out << "}\n\n";
        }
    }

    for (const auto& trigger : collectTriggerChecks(doc))
    {
        out << "void " << smStruct << "::" << triggerCheckMethodName(trigger.event) << "("
            << msmEventName(trigger.event) << " const&)\n";
        out << "{\n";
        if (doc.smTrace)
        {
            out << "    owner->frontEnd().logger().log(util::logger::LogService::LogLevel::DEBUG,\"" << smTraceTag(doc) << "^check from "
                << trigger.from << " (" << trigger.event << ")\");\n";
        }
        const Transition* tr = findTriggerCheckTransition(doc, trigger);
        if (tr != nullptr && tr->hasAction && !tr->actionHandler.empty())
        {
            out << "    owner->frontEnd().schedule([owner = owner]() { owner->"
                << tr->actionHandler << "(); });\n";
        }
        out << "    static_cast<" << smTypeName(doc) << "*>(this)->process_event("
            << msmEventName(CheckEvent) << "{});\n";
        out << "}\n\n";
    }

    for (const auto& action : collectTransitionActions(doc))
    {
        out << "void " << smStruct << "::" << smActionMethodName(action.actionHandler) << "("
            << msmEventName(action.event) << " const&)\n";
        out << "{\n";
        if (doc.smTrace)
        {
            out << "    owner->frontEnd().logger().log(util::logger::LogService::LogLevel::DEBUG,\"" << smTraceTag(doc) << "action "
                << action.actionHandler << " (" << action.event << ")\");\n";
        }
        out << "    owner->frontEnd().schedule([owner = owner]() { owner->"
            << action.actionHandler << "(); });\n";
        out << "}\n\n";
    }

    for (const auto& adapter : collectGuardAdapters(doc))
    {
        out << "bool " << smStruct << "::" << guardAdapterMethodName(adapter) << "("
            << msmEventName(adapter.event) << " const&)\n";
        out << "{\n";
        if (doc.smTrace)
        {
            out << "    bool ok = owner->" << adapter.guard << "();\n";
            if (adapter.guardNegated)
            {
                out << "    ok = !ok;\n";
            }
            if (adapter.branch == GuardBranch::WhenFalse)
            {
                out << "    ok = !ok;\n";
            }
            out << "    owner->frontEnd().logger().log(util::logger::LogService::LogLevel::DEBUG,\""
                << smTraceTag(doc) << "guard ";
            if (adapter.branch == GuardBranch::WhenFalse)
            {
                out << "else ";
            }
            if (adapter.guardNegated)
            {
                out << '!';
            }
            out << adapter.guard << " (" << adapter.event << ") = {}\", ok);\n";
            out << "    return ok;\n";
        }
        else
        {
            out << "    bool ok = owner->" << adapter.guard << "();\n";
            if (adapter.guardNegated)
            {
                out << "    ok = !ok;\n";
            }
            if (adapter.branch == GuardBranch::WhenFalse)
            {
                out << "    ok = !ok;\n";
            }
            out << "    return ok;\n";
        }
        out << "}\n\n";
    }

    for (const auto& event : doc.events)
    {
        if (event == CheckEvent)
        {
            continue;
        }
        const std::string evtType = msmEventName(event);
        out << "void " << doc.machine << "::postSmEvent(" << evtType << " const&)\n";
        out << "{\n";
        if (doc.smTrace)
        {
            out << "    frontEnd().logger().log(util::logger::LogService::LogLevel::DEBUG,\"" << smTraceTag(doc) << "<- " << event << "\");\n";
        }
        out << "    frontEnd().postEvent(*_stateMachine, " << evtType << "{});\n";
        out << "}\n\n";
    }

    out << "void " << doc.machine << "::setStateId(const State::Id id)\n";
    out << "{\n";
    out << "    _stateId.store(id);\n";
    if (doc.smTrace)
    {
        out << "    frontEnd().logger().log(util::logger::LogService::LogLevel::DEBUG,\"" << smTraceTag(doc) << "state -> {}\", smStateName(id));\n";
    }
    out << "    " << doc.stateSignal << "(id);\n";
    out << "}\n\n";

    const std::string syncMethod = emitSyncCompositesMethod(doc);
    if (!syncMethod.empty())
    {
        out << syncMethod;
    }

    for (const auto& state : doc.states)
    {
        out << "void " << doc.machine << "::" << enterPlumbingName(state.name) << "()\n";
        out << "{\n";
        emitSyncCompositesCall(out, doc, state.name);
        out << "    setStateId(" << stateIdEnum(state.name) << ");\n";
        appendEntryBody(out, doc, state);
        out << "}\n\n";
    }

    for (const auto& state : doc.states)
    {
        if (!state.hasExit && state.cancel.empty())
        {
            continue;
        }
        out << "void " << doc.machine << "::" << leavePlumbingName(state.name) << "()\n";
        out << "{\n";
        if (state.hasExit && !state.onExit.empty())
        {
            const auto& handler = state.onExit.front();
            appendBindingCall(out, "    ", handler.binding, handler.handler + "()");
        }
        appendCancelCalls(out, state);
        out << "}\n\n";
    }

    out << "} // namespace " << doc.ns << "\n";
    return out.str();
}

[[nodiscard]] std::string emitSmFwd(const Document& doc)
{
    std::ostringstream out;
    out << generatedBanner(doc, " (state machine forward declarations)");
    out << "#pragma once\n\n";
    out << "namespace " << doc.ns << "\n{\n\n";
    out << "class " << smTypeName(doc) << ";\n\n";
    for (const auto& event : doc.events)
    {
        out << "struct " << msmEventName(event) << ";\n";
    }
    if (!doc.events.empty())
    {
        out << "\n";
    }
    out << "} // namespace " << doc.ns << "\n";
    return out.str();
}

[[nodiscard]] std::string emitSmDecl(const Document& doc)
{
    std::ostringstream out;
    out << generatedBanner(doc, " (pilot state-machine declarations)");
    out << "#pragma once\n\n";
    out << "/** Include inside @c " << doc.machine << " (private section). */\n\n";
    out << "    friend struct " << smStructName(doc) << ";\n\n";
    out << "    void setStateId(State::Id id);\n\n";
    if (!doc.events.empty())
    {
        out << "    /** @brief Post MSM event";
        if (doc.smTrace)
        {
            out << " (genopt.trace)";
        }
        out << " (" << smSourceFile(doc.machine) << "). */\n";
        for (const auto& event : doc.events)
        {
            if (event == CheckEvent)
            {
                continue;
            }
            out << "    void postSmEvent(" << msmEventName(event) << " const&);\n";
        }
        out << "\n";
    }
    if (!doc.composites.empty())
    {
        out << "    void syncComposites(std::initializer_list<const char*> desired);\n";
        out << "    std::vector<std::string> _smActiveComposites;\n\n";
    }
    out << "    /** @brief SM plumbing (" << smSourceFile(doc.machine) << "). */\n";

    for (const auto& state : doc.states)
    {
        out << "    void " << enterPlumbingName(state.name) << "();\n";
    }
    for (const auto& state : doc.states)
    {
        if (state.hasExit || !state.cancel.empty())
        {
            out << "    void " << leavePlumbingName(state.name) << "();\n";
        }
    }

    if (!doc.timeRequests.empty())
    {
        out << "\n    /** @brief Timers (pilot wiring + " << handlersFile(doc.machine) << "). */\n";
        for (const auto& timer : doc.timeRequests)
        {
            out << "    tools::design::time::TimeRequest " << timeRequestMember(timer.name)
                << ";\n";
            out << "    void " << timeRequestCallback(timer.name) << "();\n";
        }
    }

    out << "\n    /** @brief Message handlers (device thread, " << handlersFile(doc.machine)
        << "). */\n";
    for (const auto& state : doc.states)
    {
        for (const auto& handler : state.onEntry)
        {
            out << "    void " << handler.handler << "();\n";
        }
        for (const auto& handler : state.onExit)
        {
            out << "    void " << handler.handler << "();\n";
        }
    }
    for (const auto& composite : doc.composites)
    {
        for (const auto& handler : composite.onEntry)
        {
            out << "    void " << handler.handler << "();\n";
        }
        for (const auto& handler : composite.onExit)
        {
            out << "    void " << handler.handler << "();\n";
        }
    }

    std::unordered_set<std::string> guards;
    for (const auto& tr : doc.transitions)
    {
        if (!tr.guard.empty())
        {
            guards.insert(tr.guard);
        }
    }
    if (!guards.empty())
    {
        out << "\n    /** @brief Transition guards (" << handlersFile(doc.machine) << "). */\n";
        for (const auto& guard : guards)
        {
            out << "    bool " << guard << "();\n";
        }
    }

    std::unordered_set<std::string> actionHandlers;
    for (const auto& tr : doc.transitions)
    {
        if (!tr.actionHandler.empty())
        {
            actionHandlers.insert(tr.actionHandler);
        }
    }
    if (!actionHandlers.empty())
    {
        out << "\n    /** @brief Transition actions (" << handlersFile(doc.machine) << "). */\n";
        for (const auto& action : actionHandlers)
        {
            out << "    void " << action << "();\n";
        }
    }

    out << "\n    std::unique_ptr<" << smTypeName(doc) << "> _stateMachine;\n";
    return out.str();
}

[[nodiscard]] std::string emitMhSkeleton(const Document& doc, const std::string& includePrefix)
{
    std::ostringstream out;
    out << generatedBanner(doc, " (message-handler skeleton)");
    out << "\n#include \"" << includePrefix << "/" << pilotHeaderFile(doc.machine) << "\"\n";
    if (doc.smTrace)
    {
        out << "#include \"util/logger/Logger.hpp\"\n";
    }
    out << "\nnamespace " << doc.ns << "\n{\n\n";

    bool any = false;
    std::unordered_set<std::string> guards;
    std::unordered_set<std::string> actionHandlers;
    for (const auto& tr : doc.transitions)
    {
        if (!tr.guard.empty())
        {
            guards.insert(tr.guard);
        }
        if (!tr.actionHandler.empty())
        {
            actionHandlers.insert(tr.actionHandler);
        }
    }
    for (const auto& action : actionHandlers)
    {
        any = true;
        out << "void " << doc.machine << "::" << action << "()\n";
        out << "{\n";
        out << "}\n\n";
    }
    for (const auto& guard : guards)
    {
        any = true;
        out << "bool " << doc.machine << "::" << guard << "()\n";
        out << "{\n";
        out << "    return false;\n";
        out << "}\n\n";
    }

    for (const auto& state : doc.states)
    {
        for (const auto& handler : state.onEntry)
        {
            any = true;
            out << "void " << doc.machine << "::" << handler.handler << "()\n";
            out << "{\n";
            out << "}\n\n";
        }
        for (const auto& handler : state.onExit)
        {
            any = true;
            out << "void " << doc.machine << "::" << handler.handler << "()\n";
            out << "{\n";
            out << "}\n\n";
        }
    }

    if (!any)
    {
        out << "} // namespace " << doc.ns << "\n";
        return out.str();
    }

    out << "} // namespace " << doc.ns << "\n";
    return out.str();
}

void writeTextFile(const std::filesystem::path& path, const std::string& content)
{
    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        throw std::runtime_error("statemachine-cpp: cannot write '" + path.string() + "'");
    }
    out << content;
}

} // namespace

GeneratedSources StateMachineCppGenerator::emit(const Document& doc,
                                                const GeneratorOptions& options)
{
    if (doc.generator != "statemachine-cpp" && !doc.generator.empty())
    {
        throw std::runtime_error("statemachine-cpp: unsupported generator '" + doc.generator + "'");
    }

    validateTransitionRowCount(doc);

    const std::string prefix =
        options.includePrefix.empty() ? defaultIncludePrefix(doc.ns) : options.includePrefix;

    GeneratedSources files;
    files.emplace(smHeaderFile(doc.machine), emitSmHeader(doc));
    files.emplace(smSourceFile(doc.machine), emitSmSource(doc, prefix));
    files.emplace(smFwdFile(doc.machine), emitSmFwd(doc));
    files.emplace(smDeclFile(doc.machine), emitSmDecl(doc));
    if (options.writeMhSkeleton)
    {
        files.emplace(handlersFile(doc.machine), emitMhSkeleton(doc, prefix));
    }
    return files;
}

void StateMachineCppGenerator::writeFiles(const Document& doc, const GeneratorOptions& options)
{
    if (options.outputDir.empty())
    {
        throw std::runtime_error("statemachine-cpp: outputDir required");
    }

    const std::filesystem::path outDir(options.outputDir);
    std::filesystem::create_directories(outDir);

    const GeneratedSources files = emit(doc, options);
    for (const auto& [name, content] : files)
    {
        const std::filesystem::path path = outDir / name;
        if (options.skipExistingMh && name == handlersFile(doc.machine) && std::filesystem::exists(path))
        {
            continue;
        }
        writeTextFile(path, content);
    }
}

std::size_t StateMachineCppGenerator::countTransitionRows(const Document& doc)
{
    return emitAllTransitionRows(doc, smStructName(doc)).size();
}

} // namespace tools::design::statemachine::smd
