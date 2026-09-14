/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SmdParser.hpp
 * @brief Parser and validator for Foundation state machine design files (@c .smd).
 */
#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace tools::design::statemachine::smd
{

inline constexpr std::string_view Schema = "foundation-smd-4"; /**< First functional @c .smd schema */

/** @brief Pseudo-event @c check() — guard evaluation phase. */
inline constexpr std::string_view CheckEvent = "check";

enum class TransitionKind
{
    Direct,       /**< @c event → @c to (state change) */
    Internal,     /**< @c event without @c to — internal reaction (stay in state) */
    TriggerCheck, /**< @c event arms a @c check pass */
    OnCheck       /**< guard only — evaluated on @c check */
};

struct TimeRequestSpec
{
    std::string name;
    std::string fires;     /**< MSM event posted on expiration — defaults to @c name */
    std::string configKey; /**< optional pilot config key for @c Delay (PascalCase) */
};

struct HandlerRef
{
    std::string handler;
    std::string binding; /**< @c bound (%b) or @c async (%a) — default @c async */
};

struct Transition
{
    TransitionKind kind{TransitionKind::Direct};
    std::string from;      /**< qualified leaf state — e.g. @c Ready_Idle */
    std::string fromLocal; /**< local name for handlers — e.g. @c Idle */
    std::string on;
    std::string triggerEvent;
    std::string to;
    std::string guard;
    bool guardNegated{false}; /**< @c true when @c guard was written @c !name in @c .smd */
    std::string elseTo;
    bool hasAction{false};
    std::string actionHandler;
    std::string actionBinding{"async"};
};

struct State
{
    std::string name;            /**< unique id (qualified) — e.g. @c Ready_Idle */
    std::string localName;       /**< local UML name — e.g. @c Idle */
    std::string parentQualified; /**< parent composite, empty at root */
    int depth{0};
    bool initial{false};
    bool isComposite{false};
    bool hasEntry{false};
    bool hasExit{false};
    bool entryCheck{false}; /**< @c entry / ^check — posts @c CheckEvt after entry */
    std::string entryBinding{"async"};
    std::string exitBinding{"async"};
    std::vector<HandlerRef> onEntry;
    std::vector<HandlerRef> onExit;
    std::vector<std::string> arm;    /**< @c timeRequests to arm on entry (after handler) */
    std::vector<std::string> cancel; /**< @c timeRequests to cancel on exit (after handler) */
    std::string comment;
};

/** @brief Normalized machine description from a @c .smd file. */
struct Document
{
    std::string schema;
    std::string machine;
    std::string ns;
    std::string generator;
    bool smTrace{false};              /**< @c genopt.trace — trace globale machine (pas par état) */
    std::string stateSignal{"state"}; /**< pilot @c Signal in @c setStateId — default @c state */
    std::string initial;
    std::vector<std::string> events; /**< deduced from transition events */
    std::vector<TimeRequestSpec> timeRequests;
    std::vector<State> states;     /**< leaf states (flattened) */
    std::vector<State> composites; /**< composite states (border entry/exit) */
    std::vector<Transition> transitions;
    /** @brief Ancestor composite chain per leaf — e.g. @c Ready_Idle → @c ["Ready"]. */
    std::unordered_map<std::string, std::vector<std::string>> compositeChain;
    /** @brief Initial leaf per composite — e.g. @c Ready → @c Ready_Idle. */
    std::unordered_map<std::string, std::string> compositeInitialLeaf;
};

class SmdParser
{
public:
    SmdParser() = delete;

    [[nodiscard]] static Document parse(std::string_view text);
    [[nodiscard]] static Document loadFile(const std::string& path);
    static void validate(const Document& doc);

    /** @brief Fill deduced events, entry/exit and transition handler names. */
    static void finalize(Document& doc);
};

} // namespace tools::design::statemachine::smd
