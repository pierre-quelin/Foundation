/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SmdNaming.hpp
 * @brief UML2 / Foundation naming conventions deduced from @c .smd regions.
 */
#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace tools::design::statemachine::smd
{

/** @brief UML state @c WaitingTray → @c waitingTray (pilot handler prefix). */
[[nodiscard]] inline std::string stateCamel(std::string_view state)
{
    if (state.empty())
    {
        return {};
    }
    std::string out{state};
    out.front() = static_cast<char>(std::tolower(static_cast<unsigned char>(out.front())));
    return out;
}

/** @brief UML event @c transfer → @c Transfer (handler / MSM suffix segment). */
[[nodiscard]] inline std::string eventPascal(std::string_view event)
{
    if (event.empty())
    {
        return {};
    }
    std::string out{event};
    out.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(out.front())));
    return out;
}

/** @brief Entry handler: @c WaitingTray → @c waitingTrayEntry. */
[[nodiscard]] inline std::string entryHandlerName(std::string_view state)
{
    return stateCamel(state) + "Entry";
}

/** @brief Exit handler: @c WaitingTray → @c waitingTrayExit. */
[[nodiscard]] inline std::string exitHandlerName(std::string_view state)
{
    return stateCamel(state) + "Exit";
}

/** @brief Transition handler: @c Idle + @c transfer → @c idleOnTransfer. */
[[nodiscard]] inline std::string transitionHandlerName(std::string_view fromState,
                                                       std::string_view event)
{
    return stateCamel(fromState) + "On" + eventPascal(event);
}

/** @brief MSM event struct: @c transfer → @c TransferEvt. */
[[nodiscard]] inline std::string msmEventName(std::string_view event)
{
    return eventPascal(event) + "Evt";
}

/** @brief MSM guard token: @c isReady or @c !isReady → @c isReady / @c not_isReady in adapter names. */
[[nodiscard]] inline std::string guardAdapterToken(std::string_view guard, const bool negated)
{
    return negated ? ("not_" + std::string{guard}) : std::string{guard};
}

/** @brief MSM guard (@c else branch): @c isBlocked + @c transfer → @c guard_else_isBlocked_Transfer. */
[[nodiscard]] inline std::string smGuardElseMethodName(std::string_view guard, std::string_view event, const bool negated = false)
{
    return "guard_else_" + guardAdapterToken(guard, negated) + "_" + eventPascal(event);
}

/** @brief MSM guard method: @c isBlocked + @c transfer → @c guard_isBlocked_Transfer. */
[[nodiscard]] inline std::string smGuardMethodName(std::string_view guard, std::string_view event, const bool negated = false)
{
    return "guard_" + guardAdapterToken(guard, negated) + "_" + eventPascal(event);
}

/** @brief Internal action: @c transfer → @c triggerCheck_Transfer (arms @c check). */
[[nodiscard]] inline std::string triggerCheckMethodName(std::string_view event)
{
    return "triggerCheck_" + eventPascal(event);
}

/** @brief MSM adapter: @c idleOnTransfer → @c action_idleOnTransfer. */
[[nodiscard]] inline std::string smActionMethodName(std::string_view actionHandler)
{
    return "action_" + std::string{actionHandler};
}

/** @brief Output file stem: @c MyDevice → @c MyDevice_sm.h (fixed convention). */
[[nodiscard]] inline std::string smHeaderFile(std::string_view machine)
{
    return std::string{machine} + "_sm.h";
}

[[nodiscard]] inline std::string smSourceFile(std::string_view machine)
{
    return std::string{machine} + "_sm.cpp";
}

[[nodiscard]] inline std::string smDeclFile(std::string_view machine)
{
    return std::string{machine} + "_sm_decl.h";
}

/** @brief Forward decls for pilot header (@c MyDevice.h) without full MSM include. */
[[nodiscard]] inline std::string smFwdFile(std::string_view machine)
{
    return std::string{machine} + "_sm_fwd.h";
}

[[nodiscard]] inline std::string handlersFile(std::string_view machine)
{
    return std::string{machine} + "_mh.cpp";
}

[[nodiscard]] inline std::string pilotHeaderFile(std::string_view machine)
{
    return std::string{machine} + ".h";
}

[[nodiscard]] inline std::string timeRequestMember(std::string_view name)
{
    return "_" + std::string{name};
}

/** @brief @c transferTimeout → @c onTransferTimeout. */
[[nodiscard]] inline std::string timeRequestCallback(std::string_view name)
{
    return "on" + eventPascal(name);
}

/** @brief Default config key for a timer: @c transferTimeout → @c TransferTimeout. */
[[nodiscard]] inline std::string timeRequestConfigKey(std::string_view name)
{
    return eventPascal(name);
}

[[nodiscard]] inline std::string pilotSourceFile(std::string_view machine)
{
    return std::string{machine} + ".cpp";
}

} // namespace tools::design::statemachine::smd
