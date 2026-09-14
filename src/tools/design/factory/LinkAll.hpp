/**
 * Copyright (c) 2021-2026 - Pierre Quélin <pierre.quelin.1972@gmail.com>
 *
 * All rights reserved
 *
 * For the full license text, see:
 * https://opensource.org/license/lgpl-3-0
 *
 * @file LinkAll.hpp
 * @brief Application-side link anchors (static libfoundation.a).
 *
 * Declare touch_*() for each factory type used by this application, then call
 * foundationFactoryLinkAll() once at startup before factory::createFromNode.
 *
 * @code
 * // build/generated/FactoryLink.cpp (CMake — not in libfoundation, not in src/)
 * #include "tools/design/factory/LinkAll.hpp"
 *
 * namespace tools::design::factory::link {
 * void touch_tools_os_serport_Serport();
 * void touch_driver_board_EsploraBoard();
 * }
 *
 * void foundationFactoryLinkAll()
 * {
 *     using namespace tools::design::factory::link;
 *     touch_tools_os_serport_Serport();
 *     touch_driver_board_EsploraBoard();
 * }
 * @endcode
 */
#pragma once

namespace tools::design::factory
{

/** Implemented in the application build (generated FactoryLink.cpp). */
void foundationFactoryLinkAll();

} // namespace tools::design::factory
