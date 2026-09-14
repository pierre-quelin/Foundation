# spdlog — sources fetched by Description.xml into lib/spdlog

set(_SPDLOG_ROOT "${FOUNDATION_SRC_ROOT}/lib/spdlog")
if(NOT EXISTS "${_SPDLOG_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "lib/spdlog missing — run Build.bat fetch")
endif()

if(NOT TARGET spdlog::spdlog)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)
    add_subdirectory("${_SPDLOG_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/spdlog" EXCLUDE_FROM_ALL)
endif()

message(STATUS "gabime spdlog is available: ${_SPDLOG_ROOT}")
