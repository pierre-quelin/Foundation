/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 */

#include "tools/design/statemachine/SmdPilotValidator.hpp"

#include "tools/design/statemachine/SmdNaming.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace tools::design::statemachine::smd
{

namespace
{

[[nodiscard]] bool hasMethodDefinition(const std::string_view source, const std::string_view className, const std::string_view methodName, const bool isGuard)
{
    const std::string signature =
        (isGuard ? "bool " : "void ") + std::string{className} + "::" + std::string{methodName};
    return source.find(signature) != std::string_view::npos;
}

void appendUnique(std::vector<RequiredPilotMethod>& out, std::unordered_set<std::string>& seen, const std::string& name, const bool isGuard)
{
    const std::string key = (isGuard ? "b:" : "v:") + name;
    if (!seen.insert(key).second)
    {
        return;
    }
    out.push_back(RequiredPilotMethod{name, isGuard});
}

} // namespace

std::vector<RequiredPilotMethod> SmdPilotValidator::requiredMethods(const Document& doc)
{
    std::vector<RequiredPilotMethod> methods;
    std::unordered_set<std::string> seen;

    for (const auto& state : doc.states)
    {
        for (const auto& handler : state.onEntry)
        {
            appendUnique(methods, seen, handler.handler, false);
        }
        for (const auto& handler : state.onExit)
        {
            appendUnique(methods, seen, handler.handler, false);
        }
    }

    for (const auto& composite : doc.composites)
    {
        for (const auto& handler : composite.onEntry)
        {
            appendUnique(methods, seen, handler.handler, false);
        }
        for (const auto& handler : composite.onExit)
        {
            appendUnique(methods, seen, handler.handler, false);
        }
    }

    for (const auto& tr : doc.transitions)
    {
        if (!tr.guard.empty())
        {
            appendUnique(methods, seen, tr.guard, true);
        }
        if (!tr.actionHandler.empty())
        {
            appendUnique(methods, seen, tr.actionHandler, false);
        }
    }

    for (const auto& timer : doc.timeRequests)
    {
        appendUnique(methods, seen, timeRequestCallback(timer.name), false);
    }

    return methods;
}

void SmdPilotValidator::validateSource(const Document& doc, const std::string_view handlersSource)
{
    for (const auto& method : requiredMethods(doc))
    {
        if (!hasMethodDefinition(handlersSource, doc.machine, method.name, method.isGuard))
        {
            throw std::runtime_error("smd: pilot missing " +
                                     std::string{method.isGuard ? "guard " : "handler "} +
                                     "'" + method.name + "' in " + handlersFile(doc.machine));
        }
    }
}

void SmdPilotValidator::validateFile(const Document& doc, const std::string& handlersPath)
{
    std::ifstream in(handlersPath);
    if (!in)
    {
        throw std::runtime_error("smd: cannot open pilot handlers '" + handlersPath + "'");
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    validateSource(doc, buffer.str());
}

void SmdPilotValidator::validateIfPresent(const Document& doc, const std::string& smdPath)
{
    const std::filesystem::path path{smdPath};
    const std::filesystem::path handlersPath = path.parent_path() / handlersFile(doc.machine);
    if (!std::filesystem::exists(handlersPath))
    {
        return;
    }
    validateFile(doc, handlersPath.string());
}

} // namespace tools::design::statemachine::smd
