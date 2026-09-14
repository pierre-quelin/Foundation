/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file main.cpp
 * @brief CLI for the @c statemachine-cpp generator (@c foundation-smd-4).
 */

#include "tools/design/statemachine/SmdParser.hpp"
#include "tools/design/statemachine/SmdPilotValidator.hpp"
#include "tools/design/statemachine/StateMachineCppGenerator.hpp"
#include "tools/design/statemachine/StateMachineLimits.hpp"
#include "tools/design/statemachine/StateMachinePlantUmlExporter.hpp"

#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>

namespace
{

void usage(const char* argv0)
{
    std::cerr << "Usage: " << argv0
              << " --smd <path.smd> [--out <directory>] [--emit-plantuml <file|->]"
                 " [--diagram-mode logical|expanded] [--report-rows] [--force-mh]"
                 " [--include-prefix <path>]\n";
}

[[nodiscard]] bool writeTextFile(const std::string& path, const std::string& content)
{
    std::ofstream out(path, std::ios::binary);
    if (!out)
    {
        return false;
    }
    out << content;
    return static_cast<bool>(out);
}

} // namespace

int main(const int argc, char* argv[])
{
    std::string smdPath;
    std::string outDir;
    std::string includePrefix;
    std::string emitPlantUmlPath;
    std::string diagramMode{"logical"};
    bool forceMh    = false;
    bool reportRows = false;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--smd" && i + 1 < argc)
        {
            smdPath = argv[++i];
        }
        else if (arg == "--out" && i + 1 < argc)
        {
            outDir = argv[++i];
        }
        else if (arg == "--include-prefix" && i + 1 < argc)
        {
            includePrefix = argv[++i];
        }
        else if (arg == "--emit-plantuml" && i + 1 < argc)
        {
            emitPlantUmlPath = argv[++i];
        }
        else if (arg == "--diagram-mode" && i + 1 < argc)
        {
            diagramMode = argv[++i];
        }
        else if (arg == "--force-mh")
        {
            forceMh = true;
        }
        else if (arg == "--report-rows")
        {
            reportRows = true;
        }
        else if (arg == "--help" || arg == "-h")
        {
            usage(argv[0]);
            return 0;
        }
        else
        {
            std::cerr << "Unknown argument: " << arg << '\n';
            usage(argv[0]);
            return 1;
        }
    }

    if (smdPath.empty())
    {
        usage(argv[0]);
        return 1;
    }

    if (!reportRows && outDir.empty() && emitPlantUmlPath.empty())
    {
        usage(argv[0]);
        return 1;
    }

    try
    {
        auto doc = tools::design::statemachine::smd::SmdParser::loadFile(smdPath);
        tools::design::statemachine::smd::SmdParser::validate(doc);
        tools::design::statemachine::smd::SmdPilotValidator::validateIfPresent(doc, smdPath);

        const std::size_t rowCount =
            tools::design::statemachine::smd::StateMachineCppGenerator::countTransitionRows(doc);

        if (reportRows)
        {
            std::cout << doc.machine << ": transition_rows=" << rowCount
                      << " limit=" << tools::design::statemachine::smd::MsmMaxTransitionRows
                      << '\n';
            if (outDir.empty() && emitPlantUmlPath.empty())
            {
                return rowCount > tools::design::statemachine::smd::MsmMaxTransitionRows ? 2 : 0;
            }
        }

        if (!emitPlantUmlPath.empty())
        {
            tools::design::statemachine::smd::PlantUmlEmitOptions pumlOptions;
            if (diagramMode == "logical")
            {
                pumlOptions.mode = tools::design::statemachine::smd::PlantUmlDiagramMode::Logical;
            }
            else if (diagramMode == "expanded")
            {
                pumlOptions.mode = tools::design::statemachine::smd::PlantUmlDiagramMode::Expanded;
            }
            else
            {
                throw std::runtime_error("statemachine_gen: unknown --diagram-mode '" + diagramMode + "' (expected logical or expanded)");
            }

            const std::string puml =
                tools::design::statemachine::smd::StateMachinePlantUmlExporter::emit(doc,
                                                                                     pumlOptions);
            if (emitPlantUmlPath == "-")
            {
                std::cout << puml;
            }
            else if (!writeTextFile(emitPlantUmlPath, puml))
            {
                throw std::runtime_error("statemachine_gen: cannot write '" + emitPlantUmlPath + "'");
            }
            else
            {
                std::cout << "Wrote PlantUML diagram to " << emitPlantUmlPath << '\n';
            }
        }

        if (!outDir.empty())
        {
            tools::design::statemachine::smd::GeneratorOptions options;
            options.outputDir      = outDir;
            options.skipExistingMh = !forceMh;
            options.includePrefix  = includePrefix;

            tools::design::statemachine::smd::StateMachineCppGenerator::writeFiles(doc, options);
            std::cout << "Generated state machine files in " << outDir << '\n';
        }

        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
