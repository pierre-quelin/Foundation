/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file StateMachineCppGenerator.hpp
 * @brief C++ code generator (Boost.MSM) from @c foundation-smd-4 documents.
 */
#pragma once

#include "tools/design/statemachine/SmdParser.hpp"
#include "tools/design/statemachine/StateMachineLimits.hpp"

#include <cstddef>
#include <map>
#include <string>

namespace tools::design::statemachine::smd
{

/** @brief Emitted source files keyed by basename (e.g. @c MyDevice_sm.h). */
using GeneratedSources = std::map<std::string, std::string>;

struct GeneratorOptions
{
    /** @brief Directory for @c writeFiles (trailing separator optional). */
    std::string outputDir;

    /** @brief Emit @c *_mh.cpp skeleton when absent. */
    bool writeMhSkeleton{true};

    /** @brief Do not overwrite an existing @c *_mh.cpp. */
    bool skipExistingMh{true};

    /** @brief Include path prefix for @c #include (default: namespace with @c :: → @c /). */
    std::string includePrefix;
};

class StateMachineCppGenerator
{
public:
    StateMachineCppGenerator() = delete;

    [[nodiscard]] static GeneratedSources emit(const Document& doc,
                                               const GeneratorOptions& options = {});

    /** @brief Write emitted files to @c options.outputDir. */
    static void writeFiles(const Document& doc, const GeneratorOptions& options);

    /** @brief MSM @c transition_table row count after guard/check expansion. */
    [[nodiscard]] static std::size_t countTransitionRows(const Document& doc);
};

} // namespace tools::design::statemachine::smd
