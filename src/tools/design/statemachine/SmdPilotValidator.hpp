/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file SmdPilotValidator.hpp
 * @brief Semantic validation — pilot @c *_mh.cpp implements handlers deduced from @c .smd.
 */
#pragma once

#include "tools/design/statemachine/SmdParser.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace tools::design::statemachine::smd
{

struct RequiredPilotMethod
{
    std::string name;
    bool isGuard{false}; /**< @c true → @c bool @c name(), else @c void @c name() */
};

class SmdPilotValidator
{
public:
    SmdPilotValidator() = delete;

    /** @brief Handlers expected in the pilot @c *_mh.cpp (entry, exit, guards, actions). */
    [[nodiscard]] static std::vector<RequiredPilotMethod> requiredMethods(const Document& doc);

    /** @brief Throws if @p handlersSource lacks any required method definition. */
    static void validateSource(const Document& doc, std::string_view handlersSource);

    /** @brief Loads @p handlersPath and calls @c validateSource. */
    static void validateFile(const Document& doc, const std::string& handlersPath);

    /**
     * @brief Validates @c {smdParent}/{machine}_mh.cpp when the file exists.
     * @param smdPath Path to the @c .smd file (used to locate the handlers source).
     */
    static void validateIfPresent(const Document& doc, const std::string& smdPath);
};

} // namespace tools::design::statemachine::smd
