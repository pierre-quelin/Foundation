/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file StateMachinePlantUmlExporter.cpp
 */

#include "tools/design/statemachine/StateMachinePlantUmlExporter.hpp"

#include <algorithm>
#include <cstddef>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace tools::design::statemachine::smd
{
namespace
{

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

[[nodiscard]] const State* findLeaf(const Document& doc, const std::string& qualified)
{
    for (const auto& state : doc.states)
    {
        if (state.name == qualified)
        {
            return &state;
        }
    }
    return nullptr;
}

[[nodiscard]] const State* findComposite(const Document& doc, const std::string& qualified)
{
    for (const auto& composite : doc.composites)
    {
        if (composite.name == qualified)
        {
            return &composite;
        }
    }
    return nullptr;
}

[[nodiscard]] std::vector<std::string> leavesUnder(const Document& doc,
                                                   const std::string& compositeQualified)
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
}

[[nodiscard]] bool isCompositeQualified(const Document& doc, const std::string& qualified)
{
    return findComposite(doc, qualified) != nullptr;
}

[[nodiscard]] bool transitionsMatch(const Transition& a, const Transition& b)
{
    return a.kind == b.kind && a.on == b.on && a.guard == b.guard && a.guardNegated == b.guardNegated && a.to == b.to && a.elseTo == b.elseTo && a.triggerEvent == b.triggerEvent && a.hasAction == b.hasAction && a.actionHandler == b.actionHandler;
}

[[nodiscard]] bool isDuplicateOnAllLeaves(const Document& doc, const std::string& compositeQualified, const Transition& tr)
{
    const auto leaves = leavesUnder(doc, compositeQualified);
    if (leaves.size() <= 1U)
    {
        return false;
    }
    for (const auto& leaf : leaves)
    {
        bool found = false;
        for (const auto& candidate : doc.transitions)
        {
            if (candidate.from == leaf && transitionsMatch(candidate, tr))
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::vector<Transition> transitionsForMode(const Document& doc,
                                                         PlantUmlDiagramMode mode)
{
    if (mode == PlantUmlDiagramMode::Expanded)
    {
        return doc.transitions;
    }

    std::vector<Transition> out;
    out.reserve(doc.transitions.size());
    std::unordered_set<std::string> emittedBorder;

    for (const auto& tr : doc.transitions)
    {
        const State* fromLeaf = findLeaf(doc, tr.from);
        if (fromLeaf == nullptr || fromLeaf->parentQualified.empty())
        {
            out.push_back(tr);
            continue;
        }

        const std::string& parent = fromLeaf->parentQualified;
        if (!isDuplicateOnAllLeaves(doc, parent, tr))
        {
            out.push_back(tr);
            continue;
        }

        std::ostringstream key;
        key << parent << '|' << static_cast<int>(tr.kind) << '|' << tr.on << '|' << tr.guard << '|'
            << tr.guardNegated << '|'
            << tr.to << '|' << tr.elseTo << '|' << tr.triggerEvent << '|' << tr.hasAction << '|'
            << tr.actionHandler;
        if (!emittedBorder.insert(key.str()).second)
        {
            continue;
        }

        Transition collapsed   = tr;
        collapsed.from         = parent;
        const State* composite = findComposite(doc, parent);
        collapsed.fromLocal =
            composite != nullptr && !composite->localName.empty() ? composite->localName
                                                                  : localNameFromQualified(parent);
        out.push_back(std::move(collapsed));
    }

    return out;
}

[[nodiscard]] bool isDescendantOfLeaf(const std::string& leafQualified,
                                      const std::string& compositeQualified)
{
    std::string parent = parentQualifiedFrom(leafQualified);
    while (!parent.empty())
    {
        if (parent == compositeQualified)
        {
            return true;
        }
        parent = parentQualifiedFrom(parent);
    }
    return leafQualified == compositeQualified;
}

[[nodiscard]] std::string leafParentQualified(const Document& doc, const std::string& qualified)
{
    const State* leaf = findLeaf(doc, qualified);
    if (leaf != nullptr)
    {
        return leaf->parentQualified;
    }
    if (isCompositeQualified(doc, qualified))
    {
        const State* composite = findComposite(doc, qualified);
        return composite != nullptr ? composite->parentQualified : std::string{};
    }
    return {};
}

[[nodiscard]] std::string displayName(const Document& doc, const std::string& qualified)
{
    if (const State* leaf = findLeaf(doc, qualified))
    {
        return leaf->localName.empty() ? localNameFromQualified(qualified) : leaf->localName;
    }
    if (const State* composite = findComposite(doc, qualified))
    {
        return composite->localName.empty() ? localNameFromQualified(qualified) : composite->localName;
    }
    return localNameFromQualified(qualified);
}

[[nodiscard]] std::string logicalTargetName(const Document& doc, const Transition& tr, PlantUmlDiagramMode mode)
{
    if (mode == PlantUmlDiagramMode::Expanded)
    {
        return displayName(doc, tr.to);
    }

    const State* toLeaf = findLeaf(doc, tr.to);
    if (toLeaf != nullptr && !toLeaf->parentQualified.empty())
    {
        const std::string& composite = toLeaf->parentQualified;
        const bool fromInside =
            isDescendantOfLeaf(tr.from, composite) || tr.from == composite;
        if (!fromInside)
        {
            return displayName(doc, composite);
        }
    }
    return displayName(doc, tr.to);
}

[[nodiscard]] std::string logicalSourceName(const Document& doc, const Transition& tr)
{
    if (isCompositeQualified(doc, tr.from))
    {
        return displayName(doc, tr.from);
    }
    return displayName(doc, tr.from);
}

[[nodiscard]] std::string indentLine(const std::string& text, int level)
{
    return std::string(static_cast<std::size_t>(level) * 2U, ' ') + text;
}

[[nodiscard]] std::string guardPostcondition(const std::string& guard, const std::string& qualifier, const bool negated)
{
    if (guard.empty())
    {
        return {};
    }
    return "/ [" + qualifier + (negated ? "!" : "") + guard + "()]";
}

[[nodiscard]] std::string transitionLabel(const Transition& tr)
{
    std::ostringstream label;

    switch (tr.kind)
    {
        case TransitionKind::OnCheck:
            if (!tr.triggerEvent.empty())
            {
                label << tr.triggerEvent;
                if (!tr.guard.empty())
                {
                    label << ' ' << guardPostcondition(tr.guard, {}, tr.guardNegated);
                }
            }
            else if (!tr.guard.empty())
            {
                label << guardPostcondition(tr.guard, {}, tr.guardNegated);
            }
            if (tr.hasAction && !tr.actionHandler.empty())
            {
                label << " / " << tr.actionHandler;
            }
            break;
        case TransitionKind::Direct:
            label << tr.on;
            if (!tr.guard.empty())
            {
                label << ' ' << guardPostcondition(tr.guard, {}, tr.guardNegated);
            }
            if (tr.hasAction && !tr.actionHandler.empty())
            {
                label << " / " << tr.actionHandler;
            }
            break;
        default:
            break;
    }

    return label.str();
}

[[nodiscard]] std::string elseTransitionLabel(const Transition& tr)
{
    std::ostringstream label;

    if (tr.kind == TransitionKind::OnCheck)
    {
        if (!tr.triggerEvent.empty())
        {
            label << tr.triggerEvent << ' ';
        }
        label << guardPostcondition(tr.guard, "else ", tr.guardNegated);
    }
    else
    {
        label << tr.on << ' ' << guardPostcondition(tr.guard, "else ", tr.guardNegated);
    }

    if (tr.hasAction && !tr.actionHandler.empty())
    {
        label << " / " << tr.actionHandler;
    }

    return label.str();
}

void appendStateActivities(std::ostringstream& out, const Document& doc, const State& state, const std::string& display)
{
    if (state.entryCheck)
    {
        out << display << " : entry / ^check\n";
    }
    for (const auto& handler : state.onEntry)
    {
        out << display << " : entry / " << handler.handler << '\n';
    }
    for (const auto& handler : state.onExit)
    {
        out << display << " : exit / " << handler.handler << '\n';
    }

    if (!state.arm.empty() || !state.cancel.empty())
    {
        out << "note right of " << display << '\n';
        for (const auto& timer : state.arm)
        {
            out << "  arm " << timer << '\n';
        }
        if (!state.cancel.empty())
        {
            out << "  cancel on exit\n";
        }
        out << "end note\n";
    }

    (void)doc;
}

void emitLeafState(std::ostringstream& out, const State& leaf)
{
    out << "state " << leaf.localName << '\n';
}

void emitInitialPointer(std::ostringstream& out, const std::string& target, int indent)
{
    out << indentLine("[*] --> " + target, indent) << '\n';
}

[[nodiscard]] bool isInternalTransition(const Document& doc, const Transition& tr);

void emitTransitionArrow(std::ostringstream& out, const Document& doc, const Transition& tr, PlantUmlDiagramMode mode, int indent);

void emitTransitionsInsideComposite(std::ostringstream& out, const Document& doc, const std::string& compositeQualified, const std::vector<Transition>& transitions, PlantUmlDiagramMode mode);

void emitCompositeBlock(std::ostringstream& out, const Document& doc, const State& composite, const std::vector<Transition>& transitions, PlantUmlDiagramMode mode, int indent)
{
    out << indentLine("state " + composite.localName + " {", indent) << '\n';

    const auto leaves = leavesUnder(doc, composite.name);
    for (const auto& leafName : leaves)
    {
        const State* leaf = findLeaf(doc, leafName);
        if (leaf == nullptr)
        {
            continue;
        }
        out << indentLine("state " + leaf->localName, indent + 1) << '\n';
    }

    const auto initialIt = doc.compositeInitialLeaf.find(composite.name);
    if (initialIt != doc.compositeInitialLeaf.end())
    {
        const State* initialLeaf = findLeaf(doc, initialIt->second);
        if (initialLeaf != nullptr)
        {
            emitInitialPointer(out, initialLeaf->localName, indent + 1);
        }
    }

    for (const auto& leafName : leaves)
    {
        const State* leaf = findLeaf(doc, leafName);
        if (leaf == nullptr)
        {
            continue;
        }
        const std::string prefix = indentLine("", indent + 1);
        std::ostringstream activities;
        appendStateActivities(activities, doc, *leaf, leaf->localName);
        const std::string activityText = activities.str();
        if (!activityText.empty())
        {
            std::istringstream lines(activityText);
            std::string line;
            while (std::getline(lines, line))
            {
                if (!line.empty())
                {
                    out << prefix << line << '\n';
                }
            }
        }
    }

    emitTransitionsInsideComposite(out, doc, composite.name, transitions, mode);

    out << indentLine("}", indent) << '\n';
}

[[nodiscard]] bool isInternalTransition(const Document& doc, const Transition& tr)
{
    if (tr.kind == TransitionKind::Internal || tr.kind == TransitionKind::TriggerCheck)
    {
        const std::string parent = leafParentQualified(doc, tr.from);
        return !parent.empty();
    }

    const std::string fromParent = leafParentQualified(doc, tr.from);
    const std::string toParent   = leafParentQualified(doc, tr.to);
    return !fromParent.empty() && fromParent == toParent;
}

void emitInternalReaction(std::ostringstream& out, const Transition& tr)
{
    out << tr.fromLocal << " : " << tr.on;
    if (tr.hasAction && !tr.actionHandler.empty())
    {
        out << " / " << tr.actionHandler;
    }
    out << '\n';
}

void emitTriggerCheckReaction(std::ostringstream& out, const Transition& tr)
{
    out << tr.fromLocal << " : " << tr.on << " / ";
    if (tr.hasAction && !tr.actionHandler.empty())
    {
        out << tr.actionHandler << "^check";
    }
    else
    {
        out << "^check";
    }
    out << '\n';
}

void emitTransitionArrow(std::ostringstream& out, const Document& doc, const Transition& tr, PlantUmlDiagramMode mode, int indent)
{
    const std::string prefix = indent > 0 ? indentLine("", indent) : std::string{};

    if (tr.kind == TransitionKind::Internal)
    {
        out << prefix;
        emitInternalReaction(out, tr);
        return;
    }

    if (tr.kind == TransitionKind::TriggerCheck)
    {
        out << prefix;
        emitTriggerCheckReaction(out, tr);
        return;
    }

    const std::string fromName = logicalSourceName(doc, tr);
    const std::string toName   = logicalTargetName(doc, tr, mode);

    out << prefix << fromName << " --> " << toName << " : " << transitionLabel(tr) << '\n';

    if (!tr.elseTo.empty())
    {
        Transition elseTr            = tr;
        elseTr.to                    = tr.elseTo;
        const std::string elseTarget = logicalTargetName(doc, elseTr, mode);
        out << prefix << fromName << " --> " << elseTarget << " : " << elseTransitionLabel(tr)
            << '\n';
    }
}

void emitTransitionsInsideComposite(std::ostringstream& out, const Document& doc, const std::string& compositeQualified, const std::vector<Transition>& transitions, PlantUmlDiagramMode mode)
{
    const int indent = 2;
    for (const auto& tr : transitions)
    {
        if (!isInternalTransition(doc, tr))
        {
            continue;
        }
        const std::string parent = leafParentQualified(doc, tr.from);
        if (parent != compositeQualified)
        {
            continue;
        }
        emitTransitionArrow(out, doc, tr, mode, indent);
    }
}

} // namespace

std::string StateMachinePlantUmlExporter::emit(const Document& doc, const PlantUmlEmitOptions& options)
{
    std::ostringstream out;
    out << "@startuml " << doc.machine << '\n';
    out << "title " << doc.machine << " (" << doc.schema << ")\n\n";

    const std::vector<Transition> transitions = transitionsForMode(doc, options.mode);

    if (!doc.initial.empty())
    {
        emitInitialPointer(out, displayName(doc, doc.initial), 0);
    }

    for (const auto& leaf : doc.states)
    {
        if (!leaf.parentQualified.empty())
        {
            continue;
        }
        emitLeafState(out, leaf);
        appendStateActivities(out, doc, leaf, leaf.localName);
    }

    for (const auto& composite : doc.composites)
    {
        if (!composite.parentQualified.empty())
        {
            continue;
        }
        emitCompositeBlock(out, doc, composite, transitions, options.mode, 0);
        appendStateActivities(out, doc, composite, composite.localName);
        out << '\n';
    }

    for (const auto& tr : transitions)
    {
        if (isInternalTransition(doc, tr))
        {
            const std::string parent = leafParentQualified(doc, tr.from);
            if (!parent.empty())
            {
                continue;
            }
        }
        emitTransitionArrow(out, doc, tr, options.mode, 0);
    }

    out << "@enduml\n";
    return out.str();
}

} // namespace tools::design::statemachine::smd
