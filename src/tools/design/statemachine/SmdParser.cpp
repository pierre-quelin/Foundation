/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/statemachine/SmdParser.hpp"

#include "tools/design/statemachine/SmdNaming.hpp"
#include "util/json/Json.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace tools::design::statemachine::smd
{

namespace
{

void parseEntryField(const util::json::Json& field, bool& hasEntry, std::string& binding, bool& entryCheck);
void parseEntryExitFlag(const util::json::Json& field, bool& hasExit, std::string& binding);
void parseStringArray(const util::json::Json& node, std::vector<std::string>& out);
[[nodiscard]] bool parseActionRow(const util::json::Json& row, std::string& binding);

struct ActionSpec
{
    bool hasHandler{false};
    bool armsCheck{false};
    std::string binding{"async"};
};

[[nodiscard]] ActionSpec parseActionSpec(const util::json::Json& row);

[[nodiscard]] std::string requireString(const util::json::Json& node, const char* key)
{
    if (!node.contains(key) || !node[key].is_string())
    {
        throw std::runtime_error(std::string{"smd: missing or invalid '"} + key + "'");
    }
    return node[key].get<std::string>();
}

void assignGuard(Transition& transition, const std::string& rawGuard, const std::string& fromQualified)
{
    if (rawGuard.empty())
    {
        throw std::runtime_error("smd: empty 'guard' in state '" + fromQualified + "'");
    }
    if (rawGuard.front() == '!')
    {
        transition.guard        = rawGuard.substr(1U);
        transition.guardNegated = true;
        if (transition.guard.empty())
        {
            throw std::runtime_error("smd: empty guard name after '!' in state '" + fromQualified + "'");
        }
        return;
    }
    transition.guard        = rawGuard;
    transition.guardNegated = false;
}

[[nodiscard]] bool containsState(const Document& doc, const std::string& name)
{
    for (const auto& state : doc.states)
    {
        if (state.name == name)
        {
            return true;
        }
    }
    return false;
}

[[nodiscard]] std::string buildQualified(const std::string& parentQualified,
                                         const std::string& localName)
{
    if (parentQualified.empty())
    {
        return localName;
    }
    return parentQualified + "_" + localName;
}

[[nodiscard]] std::string localNameFromQualified(const std::string& qualified)
{
    const auto pos = qualified.rfind('_');
    if (pos == std::string::npos)
    {
        return qualified;
    }
    return qualified.substr(pos + 1U);
}

[[nodiscard]] std::string parentQualifiedFrom(const std::string& qualified)
{
    const auto pos = qualified.rfind('_');
    if (pos == std::string::npos)
    {
        return {};
    }
    return qualified.substr(0U, pos);
}

struct ParseContext
{
    std::unordered_map<std::string, std::string> compositeInitialLeaf;
    std::unordered_map<std::string, bool> compositeByQualified;
    std::unordered_map<std::string, std::string> localToQualified;
};

void registerLocalName(ParseContext& ctx, const std::string& localName, const std::string& qualified)
{
    const auto [it, inserted] = ctx.localToQualified.emplace(localName, qualified);
    if (!inserted && it->second != qualified)
    {
        throw std::runtime_error("smd: ambiguous state name '" + localName + "'");
    }
}

[[nodiscard]] std::string resolveCompositeTarget(const std::string& qualified,
                                                 const ParseContext& ctx)
{
    const auto it = ctx.compositeInitialLeaf.find(qualified);
    if (it != ctx.compositeInitialLeaf.end())
    {
        return it->second;
    }
    return qualified;
}

void loadStateFields(const util::json::Json& stateNode, State& state)
{
    if (stateNode.contains("comment") && stateNode["comment"].is_string())
    {
        state.comment = stateNode["comment"].get<std::string>();
    }
    if (stateNode.contains("entry"))
    {
        parseEntryField(stateNode["entry"], state.hasEntry, state.entryBinding, state.entryCheck);
    }
    if (stateNode.contains("exit"))
    {
        parseEntryExitFlag(stateNode["exit"], state.hasExit, state.exitBinding);
    }
    if (stateNode.contains("arm"))
    {
        parseStringArray(stateNode["arm"], state.arm);
    }
    if (stateNode.contains("cancel"))
    {
        parseStringArray(stateNode["cancel"], state.cancel);
    }
}

void parseStateEventsResolved(const std::string& fromQualified, const std::string& fromLocal, const util::json::Json& eventsNode, Document& doc, ParseContext& ctx);

void parseStateNode(const util::json::Json& stateNode, const std::string& parentQualified, int depth, Document& doc, ParseContext& ctx, bool& regionInitialSet)
{
    const std::string localName = requireString(stateNode, "name");
    const std::string qualified = buildQualified(parentQualified, localName);

    State state;
    state.localName       = localName;
    state.name            = qualified;
    state.parentQualified = parentQualified;
    state.depth           = depth;
    if (stateNode.contains("depth"))
    {
        state.depth = stateNode["depth"].get<int>();
    }

    registerLocalName(ctx, localName, qualified);

    if (stateNode.contains("initial") && stateNode["initial"].is_boolean() && stateNode["initial"].get<bool>())
    {
        state.initial = true;
        if (!parentQualified.empty())
        {
            ctx.compositeInitialLeaf[parentQualified] = qualified;
        }
        else if (regionInitialSet)
        {
            throw std::runtime_error("smd: multiple initial states in region");
        }
        else
        {
            regionInitialSet = true;
        }
    }

    loadStateFields(stateNode, state);

    const bool hasChildren =
        stateNode.contains("states") && stateNode["states"].is_array() && !stateNode["states"].empty();

    if (hasChildren)
    {
        state.isComposite = true;
        doc.composites.push_back(state);
        ctx.compositeByQualified[qualified] = true;

        bool childInitialSet = false;
        std::string initialChildQualified;
        for (const auto& childNode : stateNode["states"])
        {
            const std::string childLocal = requireString(childNode, "name");
            const bool childIsInitial =
                childNode.contains("initial") && childNode["initial"].is_boolean() && childNode["initial"].get<bool>();
            if (childIsInitial)
            {
                if (childInitialSet)
                {
                    throw std::runtime_error("smd: multiple initial substates in '" + qualified + "'");
                }
                childInitialSet       = true;
                initialChildQualified = buildQualified(qualified, childLocal);
            }
        }
        if (!childInitialSet)
        {
            throw std::runtime_error("smd: composite '" + qualified + "' requires one initial substate");
        }

        for (const auto& childNode : stateNode["states"])
        {
            parseStateNode(childNode, qualified, depth + 1, doc, ctx, regionInitialSet);
        }

        ctx.compositeInitialLeaf[qualified] =
            resolveCompositeTarget(initialChildQualified, ctx);
        doc.compositeInitialLeaf[qualified] = ctx.compositeInitialLeaf[qualified];

        if (state.initial && parentQualified.empty())
        {
            doc.initial = ctx.compositeInitialLeaf[qualified];
        }

        if (stateNode.contains("events") || stateNode.contains("transitions"))
        {
            const util::json::Json& eventsNode =
                stateNode.contains("events") ? stateNode["events"] : stateNode["transitions"];
            parseStateEventsResolved(qualified, localName, eventsNode, doc, ctx);
        }
        return;
    }

    if (state.initial && parentQualified.empty())
    {
        doc.initial = qualified;
    }

    doc.states.push_back(std::move(state));

    if (stateNode.contains("events") || stateNode.contains("transitions"))
    {
        const util::json::Json& eventsNode =
            stateNode.contains("events") ? stateNode["events"] : stateNode["transitions"];
        parseStateEventsResolved(qualified, localName, eventsNode, doc, ctx);
    }
}

void parseStateEventsResolved(const std::string& fromQualified, const std::string& fromLocal, const util::json::Json& eventsNode, Document& doc, ParseContext& ctx)
{
    struct PendingDoc
    {
        Document* doc;
        ParseContext* ctx;
        std::string fromQualified;
        std::string fromLocal;
    };

    PendingDoc pending{&doc, &ctx, fromQualified, fromLocal};

    // Reuse parseStateEvents logic via a wrapper that resolves targets
    std::string pendingTrigger;
    bool pendingTriggerHasAction{false};
    std::string pendingTriggerBinding{"async"};

    auto pushTriggerIfPending = [&]()
    {
        if (pendingTrigger.empty())
        {
            return;
        }
        Transition trigger;
        trigger.kind          = TransitionKind::TriggerCheck;
        trigger.from          = pending.fromQualified;
        trigger.fromLocal     = pending.fromLocal;
        trigger.on            = pendingTrigger;
        trigger.hasAction     = pendingTriggerHasAction;
        trigger.actionBinding = pendingTriggerBinding;
        pending.doc->transitions.push_back(std::move(trigger));
        pendingTrigger.clear();
        pendingTriggerHasAction = false;
        pendingTriggerBinding   = "async";
    };

    auto pushOnCheck = [&](const util::json::Json& row)
    {
        const std::string armedBy = pendingTrigger;
        pushTriggerIfPending();

        Transition transition;
        transition.kind         = TransitionKind::OnCheck;
        transition.from         = pending.fromQualified;
        transition.fromLocal    = pending.fromLocal;
        transition.on           = std::string{CheckEvent};
        transition.triggerEvent = armedBy;
        assignGuard(transition, row["guard"].get<std::string>(), fromQualified);
        transition.to = row["to"].get<std::string>();
        if (row.contains("else") && row["else"].is_string())
        {
            transition.elseTo = row["else"].get<std::string>();
        }
        transition.hasAction = parseActionRow(row, transition.actionBinding);
        pending.doc->transitions.push_back(std::move(transition));
    };

    for (const auto& row : eventsNode)
    {
        const bool hasEvent = row.contains("event") && row["event"].is_string();
        const bool hasGuard = row.contains("guard") && row["guard"].is_string();
        const bool hasTo    = row.contains("to") && row["to"].is_string();
        const bool hasElse  = row.contains("else") && row["else"].is_string();
        const bool hasCheck =
            row.contains("check") && row["check"].is_boolean() && row["check"].get<bool>();

        if (hasElse && !hasGuard)
        {
            throw std::runtime_error("smd: 'else' requires 'guard' in state '" + fromQualified + "'");
        }

        if (hasEvent && hasGuard && hasCheck)
        {
            throw std::runtime_error(
                "smd: a row cannot combine 'event' and 'check' with 'guard' in state '" + fromQualified + "' — use event+guard (guard on event) or check+guard (guard on check)");
        }

        std::string actionBinding{"async"};
        const ActionSpec actionSpec = parseActionSpec(row);
        const bool hasAction        = actionSpec.hasHandler;

        if (hasEvent && hasTo && actionSpec.armsCheck)
        {
            throw std::runtime_error("smd: 'action' with '^check' cannot be combined with 'to' in state '" + fromQualified + "'");
        }

        if (hasEvent && hasCheck && !hasGuard && !hasTo)
        {
            pendingTrigger          = row["event"].get<std::string>();
            pendingTriggerHasAction = hasAction;
            pendingTriggerBinding   = actionSpec.binding;
            continue;
        }

        if (hasEvent && !hasGuard && !hasTo)
        {
            const std::string event = row["event"].get<std::string>();
            if (actionSpec.armsCheck)
            {
                pendingTrigger          = event;
                pendingTriggerHasAction = actionSpec.hasHandler;
                pendingTriggerBinding   = actionSpec.binding;
                continue;
            }
            if (actionSpec.hasHandler)
            {
                Transition reaction;
                reaction.kind          = TransitionKind::Internal;
                reaction.from          = fromQualified;
                reaction.fromLocal     = fromLocal;
                reaction.on            = event;
                reaction.to            = fromQualified;
                reaction.hasAction     = true;
                reaction.actionBinding = actionSpec.binding;
                pending.doc->transitions.push_back(std::move(reaction));
                continue;
            }
            pendingTrigger          = event;
            pendingTriggerHasAction = false;
            pendingTriggerBinding   = "async";
            continue;
        }

        if (hasCheck && hasGuard && hasTo)
        {
            pushOnCheck(row);
            continue;
        }

        if (!hasEvent && hasGuard && hasTo)
        {
            pushOnCheck(row);
            continue;
        }

        if (hasEvent && hasTo && !hasGuard)
        {
            pushTriggerIfPending();
            Transition transition;
            transition.kind          = TransitionKind::Direct;
            transition.from          = fromQualified;
            transition.fromLocal     = fromLocal;
            transition.on            = row["event"].get<std::string>();
            transition.to            = row["to"].get<std::string>();
            transition.hasAction     = hasAction;
            transition.actionBinding = actionSpec.binding;
            pending.doc->transitions.push_back(std::move(transition));
            continue;
        }

        if (hasEvent && hasGuard && hasTo)
        {
            pushTriggerIfPending();
            Transition transition;
            transition.kind      = TransitionKind::Direct;
            transition.from      = fromQualified;
            transition.fromLocal = fromLocal;
            transition.on        = row["event"].get<std::string>();
            transition.to        = row["to"].get<std::string>();
            assignGuard(transition, row["guard"].get<std::string>(), fromQualified);
            transition.hasAction     = hasAction;
            transition.actionBinding = actionSpec.binding;
            if (hasElse)
            {
                transition.elseTo = row["else"].get<std::string>();
            }
            pending.doc->transitions.push_back(std::move(transition));
            continue;
        }

        throw std::runtime_error("smd: invalid event row in state '" + fromQualified + "'");
    }

    if (!pendingTrigger.empty())
    {
        throw std::runtime_error("smd: event '" + pendingTrigger + "' in state '" + fromQualified + "' must be followed by a guard row (check phase)");
    }
}

[[nodiscard]] std::string resolveTargetInDocument(const std::string& target,
                                                  const std::string& fromQualified,
                                                  const Document& doc)
{
    if (target.find('.') != std::string::npos)
    {
        std::string normalized = target;
        for (char& ch : normalized)
        {
            if (ch == '.')
            {
                ch = '_';
            }
        }
        return normalized;
    }

    const std::string parent  = parentQualifiedFrom(fromQualified);
    const std::string sibling = buildQualified(parent, target);
    if (containsState(doc, sibling) || doc.compositeInitialLeaf.count(sibling) != 0U)
    {
        const auto it = doc.compositeInitialLeaf.find(sibling);
        if (it != doc.compositeInitialLeaf.end())
        {
            return it->second;
        }
        return sibling;
    }

    if (containsState(doc, target) || doc.compositeInitialLeaf.count(target) != 0U)
    {
        const auto it = doc.compositeInitialLeaf.find(target);
        if (it != doc.compositeInitialLeaf.end())
        {
            return it->second;
        }
        return target;
    }

    std::string match;
    for (const auto& leaf : doc.states)
    {
        if (leaf.localName == target)
        {
            if (!match.empty())
            {
                throw std::runtime_error("smd: ambiguous transition target '" + target + "'");
            }
            match = leaf.name;
        }
    }
    if (!match.empty())
    {
        return match;
    }

    throw std::runtime_error("smd: unknown transition target '" + target + "' from '" + fromQualified + "'");
}

void resolveTransitionTargets(Document& doc)
{
    for (auto& tr : doc.transitions)
    {
        if (tr.kind == TransitionKind::TriggerCheck || tr.kind == TransitionKind::Internal)
        {
            continue;
        }
        tr.to = resolveTargetInDocument(tr.to, tr.from, doc);
        if (!tr.elseTo.empty())
        {
            tr.elseTo = resolveTargetInDocument(tr.elseTo, tr.from, doc);
        }
    }
}

void expandCompositeBorderTransitions(Document& doc)
{
    auto isCompositeQualified = [&](const std::string& qualified)
    {
        return std::any_of(doc.composites.begin(), doc.composites.end(), [&](const State& c)
                           { return c.name == qualified; });
    };

    auto leavesUnder = [&](const std::string& compositeQualified)
    {
        std::vector<std::string> leaves;
        for (const auto& leaf : doc.states)
        {
            if (leaf.parentQualified == compositeQualified)
            {
                leaves.push_back(leaf.name);
            }
        }
        return leaves;
    };

    std::vector<Transition> expanded;
    expanded.reserve(doc.transitions.size() * 2U);
    for (const auto& tr : doc.transitions)
    {
        if (!isCompositeQualified(tr.from))
        {
            expanded.push_back(tr);
            continue;
        }
        const auto leaves = leavesUnder(tr.from);
        if (leaves.empty())
        {
            throw std::runtime_error("smd: composite '" + tr.from + "' has no leaf states");
        }
        for (const auto& leaf : leaves)
        {
            Transition copy = tr;
            copy.from       = leaf;
            copy.fromLocal  = localNameFromQualified(leaf);
            expanded.push_back(std::move(copy));
        }
    }
    doc.transitions = std::move(expanded);
}

void buildCompositeChains(Document& doc)
{
    doc.compositeChain.clear();
    for (const auto& leaf : doc.states)
    {
        std::vector<std::string> chain;
        std::string parent = leaf.parentQualified;
        while (!parent.empty())
        {
            chain.push_back(parent);
            parent = parentQualifiedFrom(parent);
        }
        std::reverse(chain.begin(), chain.end());
        doc.compositeChain[leaf.name] = std::move(chain);
    }
}

[[nodiscard]] bool containsEvent(const Document& doc, const std::string& name)
{
    for (const auto& event : doc.events)
    {
        if (event == name)
        {
            return true;
        }
    }
    return false;
}

void parseStringArray(const util::json::Json& field, std::vector<std::string>& out)
{
    if (!field.is_array())
    {
        throw std::runtime_error("smd: expected string array");
    }
    for (const auto& item : field)
    {
        if (!item.is_string())
        {
            throw std::runtime_error("smd: expected string array");
        }
        out.push_back(item.get<std::string>());
    }
}

void parseTimeRequests(const util::json::Json& root, Document& doc)
{
    if (!root.contains("timeRequests"))
    {
        return;
    }
    if (!root["timeRequests"].is_array())
    {
        throw std::runtime_error("smd: invalid 'timeRequests'");
    }
    std::unordered_set<std::string> seen;
    for (const auto& node : root["timeRequests"])
    {
        if (!node.is_object())
        {
            throw std::runtime_error("smd: timeRequests[] expects objects");
        }
        TimeRequestSpec spec;
        spec.name = requireString(node, "name");
        if (!seen.insert(spec.name).second)
        {
            throw std::runtime_error("smd: duplicate timeRequest '" + spec.name + "'");
        }
        if (node.contains("fires") && node["fires"].is_string())
        {
            spec.fires = node["fires"].get<std::string>();
        }
        else
        {
            spec.fires = spec.name;
        }
        if (node.contains("config") && node["config"].is_string())
        {
            spec.configKey = node["config"].get<std::string>();
        }
        else
        {
            spec.configKey = timeRequestConfigKey(spec.name);
        }
        doc.timeRequests.push_back(std::move(spec));
    }
}

[[nodiscard]] bool containsTimeRequest(const Document& doc, const std::string& name)
{
    for (const auto& spec : doc.timeRequests)
    {
        if (spec.name == name)
        {
            return true;
        }
    }
    return false;
}

void validateTimeRequests(const Document& doc)
{
    for (const auto& spec : doc.timeRequests)
    {
        if (!containsEvent(doc, spec.fires))
        {
            throw std::runtime_error("smd: timeRequest '" + spec.name + "' fires unknown event '" + spec.fires + "'");
        }
    }

    for (const auto& state : doc.states)
    {
        for (const auto& timer : state.arm)
        {
            if (!containsTimeRequest(doc, timer))
            {
                throw std::runtime_error("smd: state '" + state.name + "' arms unknown timeRequest '" + timer + "'");
            }
        }
        for (const auto& timer : state.cancel)
        {
            if (!containsTimeRequest(doc, timer))
            {
                throw std::runtime_error("smd: state '" + state.name + "' cancels unknown timeRequest '" + timer + "'");
            }
        }
    }
    for (const auto& composite : doc.composites)
    {
        for (const auto& timer : composite.arm)
        {
            if (!containsTimeRequest(doc, timer))
            {
                throw std::runtime_error("smd: composite '" + composite.name + "' arms unknown timeRequest '" + timer + "'");
            }
        }
        for (const auto& timer : composite.cancel)
        {
            if (!containsTimeRequest(doc, timer))
            {
                throw std::runtime_error("smd: composite '" + composite.name + "' cancels unknown timeRequest '" + timer + "'");
            }
        }
    }
}

void parseEntryExitFlag(const util::json::Json& field, bool& present, std::string& binding)
{
    if (field.is_boolean())
    {
        present = field.get<bool>();
        return;
    }
    if (field.is_string())
    {
        const std::string value = field.get<std::string>();
        if (value == "handler")
        {
            present = true;
            return;
        }
        present = true;
        binding = value;
        return;
    }
    if (field.is_object() && field.contains("binding") && field["binding"].is_string())
    {
        present = true;
        binding = field["binding"].get<std::string>();
    }
}

void applyEntryKeyword(std::string_view keyword, bool& hasEntry, bool& entryCheck)
{
    if (keyword == "handler")
    {
        hasEntry = true;
        return;
    }
    if (keyword == "check")
    {
        entryCheck = true;
        return;
    }
    throw std::runtime_error("smd: entry array supports only 'handler' and 'check'");
}

void parseEntryField(const util::json::Json& field, bool& hasEntry, std::string& binding, bool& entryCheck)
{
    entryCheck = false;
    if (field.is_boolean())
    {
        hasEntry = field.get<bool>();
        return;
    }
    if (field.is_string())
    {
        const std::string value = field.get<std::string>();
        if (value == "handler")
        {
            hasEntry = true;
            return;
        }
        if (value == "check")
        {
            entryCheck = true;
            return;
        }
        hasEntry = true;
        binding  = value;
        return;
    }
    if (field.is_array())
    {
        for (const auto& item : field)
        {
            if (!item.is_string())
            {
                throw std::runtime_error("smd: entry array expects strings (handler, check)");
            }
            applyEntryKeyword(item.get<std::string>(), hasEntry, entryCheck);
        }
        return;
    }
    if (!field.is_object())
    {
        return;
    }
    if (field.contains("handler") && field["handler"].is_boolean() && field["handler"].get<bool>())
    {
        hasEntry = true;
    }
    if (field.contains("binding") && field["binding"].is_string())
    {
        hasEntry = true;
        binding  = field["binding"].get<std::string>();
    }
    if (field.contains("check") && field["check"].is_boolean())
    {
        entryCheck = field["check"].get<bool>();
    }
}

[[nodiscard]] ActionSpec parseActionSpec(const util::json::Json& row)
{
    ActionSpec spec;
    if (!row.contains("action"))
    {
        return spec;
    }

    if (row["action"].is_boolean())
    {
        if (row["action"].get<bool>())
        {
            spec.hasHandler = true;
        }
    }
    else if (row["action"].is_string())
    {
        const std::string value = row["action"].get<std::string>();
        if (value == "handler")
        {
            spec.hasHandler = true;
        }
        else if (value == "^check")
        {
            spec.armsCheck = true;
        }
        else if (value.size() > 6U && value.compare(value.size() - 6U, 6U, "^check") == 0)
        {
            const std::string prefix = value.substr(0U, value.size() - 6U);
            if (prefix == "handler")
            {
                spec.hasHandler = true;
                spec.armsCheck  = true;
            }
            else
            {
                throw std::runtime_error("smd: unknown action keyword '" + value + "' (expected 'handler', '^check', or 'handler^check')");
            }
        }
        else
        {
            throw std::runtime_error("smd: unknown action keyword '" + value + "' (expected 'handler', '^check', or 'handler^check')");
        }
    }
    else
    {
        throw std::runtime_error("smd: 'action' expects boolean or string");
    }

    if (row.contains("binding") && row["binding"].is_string())
    {
        spec.binding = row["binding"].get<std::string>();
    }
    return spec;
}

[[nodiscard]] bool parseActionRow(const util::json::Json& row, std::string& binding)
{
    const ActionSpec spec = parseActionSpec(row);
    binding               = spec.binding;
    return spec.hasHandler;
}

void parseRegions(const util::json::Json& root, Document& doc)
{
    if (!root.contains("regions") || !root["regions"].is_array())
    {
        throw std::runtime_error("smd: missing or invalid 'regions'");
    }

    ParseContext ctx;
    for (const auto& region : root["regions"])
    {
        if (!region.contains("states") || !region["states"].is_array())
        {
            continue;
        }
        const int regionDepth = region.contains("depth") ? region["depth"].get<int>() : 0;
        bool regionInitialSet = false;
        for (const auto& stateNode : region["states"])
        {
            parseStateNode(stateNode, "", regionDepth + 1, doc, ctx, regionInitialSet);
        }
    }
}

[[nodiscard]] Document parseRoot(const util::json::Json& root)
{
    Document doc;
    doc.schema = requireString(root, "$schema");
    if (doc.schema != Schema)
    {
        throw std::runtime_error("smd: unsupported $schema '" + doc.schema + "' (expected " + std::string{Schema} + ")");
    }

    doc.machine = requireString(root, "machine");

    if (!root.contains("genopt") || !root["genopt"].is_object())
    {
        throw std::runtime_error("smd: missing or invalid 'genopt'");
    }
    doc.ns = requireString(root["genopt"], "namespace");
    if (root["genopt"].contains("generator") && root["genopt"]["generator"].is_string())
    {
        doc.generator = root["genopt"]["generator"].get<std::string>();
    }
    if (root["genopt"].contains("stateSignal") && root["genopt"]["stateSignal"].is_string())
    {
        doc.stateSignal = root["genopt"]["stateSignal"].get<std::string>();
    }
    if (root["genopt"].contains("trace") && root["genopt"]["trace"].is_boolean())
    {
        doc.smTrace = root["genopt"]["trace"].get<bool>();
    }

    parseTimeRequests(root, doc);
    parseRegions(root, doc);
    return doc;
}

void validateBindings(const Document& doc)
{
    const auto checkState = [&](const State& state)
    {
        for (const auto& handler : state.onEntry)
        {
            if (!handler.binding.empty() && handler.binding != "bound" && handler.binding != "async")
            {
                throw std::runtime_error("smd: invalid entry binding '" + handler.binding + "' in state '" + state.name + "'");
            }
        }
        for (const auto& handler : state.onExit)
        {
            if (!handler.binding.empty() && handler.binding != "bound" && handler.binding != "async")
            {
                throw std::runtime_error("smd: invalid exit binding '" + handler.binding + "' in state '" + state.name + "'");
            }
        }
        if (!state.entryBinding.empty() && state.entryBinding != "bound" && state.entryBinding != "async")
        {
            throw std::runtime_error("smd: invalid entry binding '" + state.entryBinding + "' in state '" + state.name + "'");
        }
        if (!state.exitBinding.empty() && state.exitBinding != "bound" && state.exitBinding != "async")
        {
            throw std::runtime_error("smd: invalid exit binding '" + state.exitBinding + "' in state '" + state.name + "'");
        }
    };

    for (const auto& state : doc.states)
    {
        checkState(state);
    }
    for (const auto& composite : doc.composites)
    {
        checkState(composite);
    }
}

} // namespace

void SmdParser::finalize(Document& doc)
{
    resolveTransitionTargets(doc);
    expandCompositeBorderTransitions(doc);
    buildCompositeChains(doc);

    for (auto& leaf : doc.states)
    {
        if (leaf.localName.empty())
        {
            leaf.localName = localNameFromQualified(leaf.name);
        }
    }

    std::unordered_set<std::string> eventSet;
    for (const auto& tr : doc.transitions)
    {
        eventSet.insert(tr.on);
        if (tr.kind == TransitionKind::TriggerCheck || tr.kind == TransitionKind::OnCheck)
        {
            eventSet.insert(std::string{CheckEvent});
        }
    }
    for (const auto& timer : doc.timeRequests)
    {
        eventSet.insert(timer.fires);
    }
    doc.events.assign(eventSet.begin(), eventSet.end());
    std::sort(doc.events.begin(), doc.events.end());

    for (auto& tr : doc.transitions)
    {
        if (tr.fromLocal.empty())
        {
            tr.fromLocal = localNameFromQualified(tr.from);
        }
        if (!tr.hasAction)
        {
            tr.actionHandler.clear();
            continue;
        }
        if (tr.kind == TransitionKind::OnCheck)
        {
            tr.actionHandler = transitionHandlerName(tr.fromLocal, CheckEvent);
        }
        else
        {
            tr.actionHandler = transitionHandlerName(tr.fromLocal, tr.on);
        }
    }

    for (auto& state : doc.states)
    {
        if (state.hasEntry)
        {
            HandlerRef ref;
            ref.handler = entryHandlerName(state.localName);
            ref.binding = state.entryBinding;
            state.onEntry.push_back(std::move(ref));
        }
        if (state.hasExit)
        {
            HandlerRef ref;
            ref.handler = exitHandlerName(state.localName);
            ref.binding = state.exitBinding;
            state.onExit.push_back(std::move(ref));
        }
    }

    for (auto& composite : doc.composites)
    {
        if (composite.hasEntry)
        {
            HandlerRef ref;
            ref.handler = entryHandlerName(composite.localName);
            ref.binding = composite.entryBinding;
            composite.onEntry.push_back(std::move(ref));
        }
        if (composite.hasExit)
        {
            HandlerRef ref;
            ref.handler = exitHandlerName(composite.localName);
            ref.binding = composite.exitBinding;
            composite.onExit.push_back(std::move(ref));
        }
    }
}

Document SmdParser::parse(const std::string_view text)
{
    const util::json::Json root = util::json::Json::parse(text);
    Document doc                = parseRoot(root);
    finalize(doc);
    return doc;
}

Document SmdParser::loadFile(const std::string& path)
{
    std::ifstream in(path);
    if (!in)
    {
        throw std::runtime_error("smd: cannot open '" + path + "'");
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return parse(buffer.str());
}

void SmdParser::validate(const Document& doc)
{
    if (doc.schema != Schema)
    {
        throw std::runtime_error("smd: unsupported $schema '" + doc.schema + "'");
    }

    if (doc.machine.empty() || doc.ns.empty())
    {
        throw std::runtime_error("smd: machine and namespace required");
    }

    if (!containsState(doc, doc.initial))
    {
        throw std::runtime_error("smd: initial state '" + doc.initial + "' not in states");
    }

    std::unordered_set<std::string> eventSet(doc.events.begin(), doc.events.end());
    if (eventSet.size() != doc.events.size())
    {
        throw std::runtime_error("smd: duplicate event name");
    }

    std::unordered_set<std::string> stateSet;
    for (const auto& state : doc.states)
    {
        if (!stateSet.insert(state.name).second)
        {
            throw std::runtime_error("smd: duplicate state '" + state.name + "'");
        }
    }

    for (const auto& tr : doc.transitions)
    {
        if (stateSet.find(tr.from) == stateSet.end())
        {
            throw std::runtime_error("smd: transition from unknown state '" + tr.from + "'");
        }
        if (tr.kind == TransitionKind::TriggerCheck)
        {
            if (!containsEvent(doc, tr.on))
            {
                throw std::runtime_error("smd: transition event '" + tr.on + "' not declared");
            }
            continue;
        }
        if (stateSet.find(tr.to) == stateSet.end())
        {
            throw std::runtime_error("smd: transition to unknown state '" + tr.to + "'");
        }
        if (!tr.elseTo.empty())
        {
            if (tr.guard.empty())
            {
                throw std::runtime_error("smd: 'else' requires 'guard' on transition from '" + tr.from + "'");
            }
            if (tr.elseTo == tr.to)
            {
                throw std::runtime_error("smd: 'else' target must differ from 'to' on transition from '" + tr.from + "'");
            }
            if (stateSet.find(tr.elseTo) == stateSet.end())
            {
                throw std::runtime_error("smd: transition else to unknown state '" + tr.elseTo + "'");
            }
        }
        if (!containsEvent(doc, tr.on))
        {
            throw std::runtime_error("smd: transition event '" + tr.on + "' not declared");
        }
        if (tr.kind == TransitionKind::OnCheck && tr.guard.empty())
        {
            throw std::runtime_error("smd: check transition requires 'guard' in state '" + tr.from + "'");
        }
    }

    validateBindings(doc);
    validateTimeRequests(doc);
}

} // namespace tools::design::statemachine::smd
