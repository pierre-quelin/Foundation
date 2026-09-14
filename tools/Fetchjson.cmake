# nlohmann/json — sources fetched by Description.xml into lib/nlohmann_json

set(_JSON_ROOT "${FOUNDATION_SRC_ROOT}/lib/nlohmann_json")
if(NOT EXISTS "${_JSON_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "lib/nlohmann_json missing — run Build.bat fetch")
endif()

if(NOT TARGET nlohmann_json::nlohmann_json)
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set(JSON_Install OFF CACHE BOOL "" FORCE)
    add_subdirectory("${_JSON_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/nlohmann_json" EXCLUDE_FROM_ALL)
endif()

message(STATUS "nlohmann json is available: ${_JSON_ROOT}")
