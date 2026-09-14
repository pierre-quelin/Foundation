cmake_minimum_required (VERSION 3.25) # Debian Trixie 3.31

# Framework library — application shell lives in sibling Sample.
set(FOUNDATION_SRC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

set(_FOUNDATION_IS_ROOT FALSE)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(_FOUNDATION_IS_ROOT TRUE)
endif()

if(NOT _FOUNDATION_IS_ROOT)
    # Nested under Sample (or another consumer): skip utests here
    set(BUILD_TESTING OFF CACHE BOOL "Build unit tests" FORCE)
endif()

project(foundation VERSION 1.0.0)

include(${FOUNDATION_SRC_ROOT}/tools/FoundationDist.cmake)

# Générateurs mono-config (Ninja, Make, MinGW Makefiles) : sans CMAKE_BUILD_TYPE, le cache peut rester en Release (-O3 -NDEBUG).
if(NOT CMAKE_CONFIGURATION_TYPES)
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Debug, Release, RelWithDebInfo, MinSizeRel" FORCE)
    endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_VERBOSE_MAKEFILE OFF)
# clang-tidy / IDE : écrit build/compile_commands.json (Makefiles, Ninja, MinGW — no-op sous VS).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

################################################################################
# Packages
################################################################################
INCLUDE(tools/Packages.cmake)
INCLUDE(tools/FoundationPackages.cmake)

include(tools/Fetchjson.cmake)
add_subdirectory(src/tools/statemachine_gen)

# Shared protocol / util / logger
add_package(protocol/i2c)
add_package(protocol/usb)
add_package(util/json)
add_package(util/logger)
add_package(util/logger/_Controller)
add_package(tools/design/signal)
add_package(tools/os/socket)
add_foundation_packages()

################################################################################
# Third-party + libfoundation
################################################################################
include(tools/Fetchspdlog.cmake)
include(tools/Fetchboost.cmake)
include(tools/Fetchsqlite.cmake)
include(tools/Fetchpaho.cmake)

# mcp2221 required by driver/chip/_MCP2221 (always part of foundation)
if(NOT TARGET libmcp2221a_hid)
    if(NOT EXISTS "${FOUNDATION_SRC_ROOT}/lib/mcp2221a_hid/CMakeLists.txt")
        message(FATAL_ERROR "lib/mcp2221a_hid missing — run Build.bat fetch")
    endif()
    # Headers stay internal; Foundation dist publishes foundation API only.
    set(MCP2221A_HID_INSTALL_PUBLIC_HEADERS OFF CACHE INTERNAL "")
    add_subdirectory("${FOUNDATION_SRC_ROOT}/lib/mcp2221a_hid"
                     "${CMAKE_CURRENT_BINARY_DIR}/mcp2221a_hid")
endif()

# esplora_hid required by driver/board/_EsploraBoard HID pump
if(NOT TARGET libesplora_hid)
    if(NOT EXISTS "${FOUNDATION_SRC_ROOT}/lib/esplora_hid/CMakeLists.txt")
        message(FATAL_ERROR "lib/esplora_hid missing — run Build.bat fetch")
    endif()
    add_subdirectory("${FOUNDATION_SRC_ROOT}/lib/esplora_hid"
                     "${CMAKE_CURRENT_BINARY_DIR}/esplora_hid")
endif()

include(tools/FoundationLib.cmake)
foundation_add_static_library()

if ($ENV{BUILD_TARGET} MATCHES "linux-x86.*")
    set(CMAKE_C_FLAGS   "-O0 -g3 -Wall -Wextra")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 6)
        set(CMAKE_CXX_FLAGS "-O0 -g3 -Wall -Wextra -Wnon-virtual-dtor -Wcast-align -Wunused -Wno-overloaded-virtual -Wlogical-op -Wdouble-promotion -Wshadow -Wpedantic -Wmisleading-indentation -Wduplicated-cond -Wnull-dereference ")
    else()
        set(CMAKE_CXX_FLAGS "-O0 -g3 -Wall -Wextra -Wnon-virtual-dtor -Wcast-align -Wunused -Wno-overloaded-virtual -Wlogical-op -Wdouble-promotion")
    endif()
elseif ($ENV{BUILD_TARGET} STREQUAL "mingw64")
    set(SEARCH_DIRS "${TOOLCHAIN_BIN}")
    find_library(_LIBUSB_IMPLIB NAMES usb-1.0 libusb-1.0 NO_CMAKE_FIND_ROOT_PATH)
    if(_LIBUSB_IMPLIB)
        get_filename_component(_libusb_lib_dir "${_LIBUSB_IMPLIB}" DIRECTORY)
        list(APPEND SEARCH_DIRS "${_libusb_lib_dir}")
    endif()
elseif ($ENV{BUILD_TARGET} MATCHES "msvc[0-9]+")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
    add_definitions(-D_CRTDBG_MAP_ALLOC -D_BIND_TO_CURRENT_CRT_VERSION)
    if($ENV{BUILD_TARGET} STREQUAL "msvc9" OR $ENV{BUILD_TARGET} MATCHES "msvc[0-9]+_xp")
        add_definitions(-DWINVER=0x0501)
    endif()
endif()

set(_foundation_targets foundation)

target_link_libraries(foundation PUBLIC
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    Boost::signals2
    FoundationBoostMsm
    libmcp2221a_hid
    libesplora_hid
    sqlite3
    ${_FOUNDATION_PAHO_TARGET}
)

if ($ENV{BUILD_TARGET} MATCHES "linux-x86.*")
   target_link_libraries(foundation PUBLIC c rt pthread)
elseif ($ENV{BUILD_TARGET} MATCHES "msvc[0-9]+" OR
    $ENV{BUILD_TARGET} STREQUAL "mingw64")
   target_link_libraries(foundation PUBLIC ws2_32)
endif()

install(TARGETS foundation
        ARCHIVE DESTINATION ${FOUNDATION_DIST_ROOT}/lib)

include(${FOUNDATION_SRC_ROOT}/tools/FoundationInstall.cmake)
foundation_install_public_headers()

# mcp2221 may still install headers when built from an older lib tag; drop them from Foundation dist.
install(CODE "
if(EXISTS \"${FOUNDATION_DIST_ROOT}/include/mcp2221a\")
  file(REMOVE_RECURSE \"${FOUNDATION_DIST_ROOT}/include/mcp2221a\")
endif()
")

if ($ENV{BUILD_TARGET} MATCHES "msvc[0-9]+")
    if(NOT (MSVC_VERSION LESS 1910) AND NOT ($ENV{BUILD_TARGET} MATCHES "msvc[0-9]+_xp"))
        foreach(_t ${_foundation_targets})
            target_compile_options(${_t} PRIVATE
                $<$<CXX_COMPILER_ID:MSVC>:
                    /std:c++17
                    /permissive-
                    /Zc:__cplusplus
                    /EHsc
                    /W4
                >
            )
        endforeach()
    endif()
endif()

################################################################################
# Unit tests
################################################################################
option(BUILD_TESTING "Build unit tests" ON)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()
