/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file StateMachinePlantUmlExporter.hpp
 * @brief PlantUML state diagram export from @c foundation-smd-4 documents.
 */
#pragma once

#include "tools/design/statemachine/SmdParser.hpp"

#include <string>

namespace tools::design::statemachine::smd
{

enum class PlantUmlDiagramMode
{
    Logical, /**< Composite border transitions on the composite (viewer default). */
    Expanded /**< Full @c doc.transitions after composite border expansion (MSM debug). */
};

struct PlantUmlEmitOptions
{
    PlantUmlDiagramMode mode{PlantUmlDiagramMode::Logical};
};

class StateMachinePlantUmlExporter
{
public:
    StateMachinePlantUmlExporter() = delete;

    /** @brief Emit a PlantUML state diagram for @p doc. */
    [[nodiscard]] static std::string emit(const Document& doc,
                                          const PlantUmlEmitOptions& options = {});
};

} // namespace tools::design::statemachine::smd
