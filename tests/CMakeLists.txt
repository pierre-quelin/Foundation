# Unit tests — layout mirrors src/ (one *_test.cpp per package under test).

set(_FOUNDATION_ALL_SRCS_SAVED ${ALL_SRCS})
set(ALL_SRCS "")

include(${CMAKE_SOURCE_DIR}/tools/FoundationPackages.cmake)
# Core only: board packages need libmcp2221a_hid (linked by libfoundation, not utests).
# LoggerBoot.cpp needs logger controller + socket (same as the app).
add_package(util/logger)
add_package(util/logger/_Controller)
add_package(tools/os/socket)
add_foundation_core_packages()
add_package(actsens/solenoid)
# In/Out interfaces only — skip PCA/MCP adapters (chip deps) in utests.
set(PKG_NAME io/in)
set(INC_FILE IIn.h)
configure_file(${CMAKE_BINARY_DIR}/trampoline.h ${CMAKE_BINARY_DIR}/include/io/in/IIn.h)
add_package(io/in/_eventBus)
set(PKG_NAME io/out)
set(INC_FILE IOut.h)
configure_file(${CMAKE_BINARY_DIR}/trampoline.h ${CMAKE_BINARY_DIR}/include/io/out/IOut.h)
add_package(io/out/_eventBus)
# PWM interface header only — avoid PCA9633/PCA9635 adapters (chip deps) in utests
set(PKG_NAME io/pwm)
set(INC_FILE IPWM.h)
configure_file(${CMAKE_BINARY_DIR}/trampoline.h ${CMAKE_BINARY_DIR}/include/io/pwm/IPWM.h)
# EsploraBoard stub (no chip deps) — interface headers only for board/led/adc
# EsploraBoard HID pump needs libesplora_hid (same as foundation; not via libfoundation).
set(PKG_NAME io/board)
set(INC_FILE IBoard.h)
configure_file(${CMAKE_BINARY_DIR}/trampoline.h ${CMAKE_BINARY_DIR}/include/io/board/IBoard.h)
set(PKG_NAME io/adc)
set(INC_FILE IADC.h)
configure_file(${CMAKE_BINARY_DIR}/trampoline.h ${CMAKE_BINARY_DIR}/include/io/adc/IADC.h)
set(PKG_NAME io/led)
set(INC_FILE ILed.h)
configure_file(${CMAKE_BINARY_DIR}/trampoline.h ${CMAKE_BINARY_DIR}/include/io/led/ILed.h)
set(PKG_NAME io/led)
set(INC_FILE IRGBWLed.h)
configure_file(${CMAKE_BINARY_DIR}/trampoline.h ${CMAKE_BINARY_DIR}/include/io/led/IRGBWLed.h)
add_package(driver/board/_EsploraBoard)
set(UTEST_PACKAGE_SRCS ${ALL_SRCS})
set(ALL_SRCS ${_FOUNDATION_ALL_SRCS_SAVED})

set(FOUNDATION_UTEST_SRCS
    main.cpp
    util/chrono/chrono_test.cpp
    util/chrono/local_test.cpp
    tools/os/sync/sync_test.cpp
    tools/os/thread/thread_test.cpp
    tools/os/timer/timer_test.cpp
    tools/os/startup/main_ini_test.cpp
    tools/os/serport/serport_eventbus_test.cpp
    io/in/in_eventbus_test.cpp
    io/out/out_eventbus_test.cpp
    driver/board/esplora_board_test.cpp
    tools/design/config/config_test.cpp
    tools/design/factory/factory_test.cpp
    tools/design/db/db_api_test.cpp
    tools/design/db/sqlite_database_test.cpp
    tools/design/db/schema_migrator_test.cpp
    tools/design/db/database_factory_test.cpp
    tools/design/ipc/topic_scheme_test.cpp
    tools/design/ipc/transport_loopback_test.cpp
    tools/design/ipc/event_bus_test.cpp
    tools/design/ipc/transport_mqtt_test.cpp
    tools/design/ipc/eventbus_factory_test.cpp
    tools/design/scheduler/scheduler_test.cpp
    tools/design/scheduler/scheduler_service_test.cpp
    tools/design/time/time_test.cpp
    tools/design/statemachine/frontend_integration_test.cpp
    tools/design/statemachine/smd_parser_test.cpp
    tools/design/statemachine/smd_generator_test.cpp
    tools/design/statemachine/smd_plantuml_test.cpp
    tools/design/objkit/objkit_test.cpp
    actsens/solenoid/solenoid_test.cpp
)

add_executable(foundation_utests
    ${FOUNDATION_UTEST_SRCS}
    ${UTEST_PACKAGE_SRCS}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "" FILES ${FOUNDATION_UTEST_SRCS})

target_link_libraries(foundation_utests PRIVATE
    Boost::included_unit_test_framework
    Boost::signals2
    FoundationBoostMsm
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    sqlite3
    libesplora_hid
    ${_FOUNDATION_PAHO_TARGET}
)

target_compile_definitions(foundation_utests PRIVATE
    "FOUNDATION_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\""
)

target_include_directories(foundation_utests PRIVATE
    ${CMAKE_BINARY_DIR}/include
)

if(WIN32)
    target_compile_definitions(foundation_utests PRIVATE NOMINMAX NOGDI)
endif()

# main.cpp pulls in Boost.Test's amalgamated debug.ipp (GetProcAddress casts).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_source_files_properties(main.cpp PROPERTIES COMPILE_OPTIONS "-Wno-cast-function-type")
endif()

if ($ENV{BUILD_TARGET} MATCHES "linux-x86.*")
    target_link_libraries(foundation_utests PRIVATE pthread)
elseif(WIN32)
    target_link_libraries(foundation_utests PRIVATE ws2_32)
    if ($ENV{BUILD_TARGET} MATCHES "msvc[0-9]+")
        target_compile_options(foundation_utests PRIVATE
            $<$<CXX_COMPILER_ID:MSVC>:/std:c++17 /permissive- /Zc:__cplusplus /EHsc /W4 /Z7>
        )
    endif()
endif()

add_test(NAME foundation_utests COMMAND foundation_utests)

# Runtime libusb: MSVC → DLL installée dans dist/ via Fetchlibusb (pas de copie locale).
# Linux / mingw : lib système (pkg-config / toolchain PATH), rien à préfixer ici.
if(DEFINED ENV{BUILD_TARGET} AND "$ENV{BUILD_TARGET}" MATCHES "msvc[0-9]+")
    if(DEFINED FOUNDATION_DIST_ROOT)
        set_tests_properties(foundation_utests PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${FOUNDATION_DIST_ROOT}")
    endif()
endif()
