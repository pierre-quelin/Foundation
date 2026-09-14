# Eclipse Paho MQTT C++ — sources fetched by Description.xml into lib/paho.mqtt.cpp
# (git tag v1.6.0 + submodule externals/paho.mqtt.c @ 1.3.16)

set(_PAHO_CPP_ROOT "${FOUNDATION_SRC_ROOT}/lib/paho.mqtt.cpp")
if(NOT EXISTS "${_PAHO_CPP_ROOT}/CMakeLists.txt")
    message(FATAL_ERROR "lib/paho.mqtt.cpp missing — run Build.bat fetch")
endif()

set(_PAHO_C_ROOT "${_PAHO_CPP_ROOT}/externals/paho.mqtt.c")
if(NOT EXISTS "${_PAHO_C_ROOT}/CMakeLists.txt")
    # Description.xml clones the C++ tree; submodule must be initialised separately.
    find_package(Git QUIET)
    if(Git_FOUND AND EXISTS "${_PAHO_CPP_ROOT}/.git")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
            WORKING_DIRECTORY "${_PAHO_CPP_ROOT}"
            RESULT_VARIABLE _paho_sub_rc
        )
    endif()
    if(NOT EXISTS "${_PAHO_C_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR
            "lib/paho.mqtt.cpp/externals/paho.mqtt.c missing — "
            "run Build.bat fetch then: git -C lib/paho.mqtt.cpp submodule update --init --recursive")
    endif()
endif()

if(NOT TARGET PahoMqttCpp::paho-mqttpp3 AND NOT TARGET paho-mqttpp3-static)
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        set(PAHO_WITH_SSL ON CACHE BOOL "Paho SSL/TLS" FORCE)
        message(STATUS "Paho MQTT: SSL enabled (OpenSSL found)")
    else()
        set(PAHO_WITH_SSL OFF CACHE BOOL "Paho SSL/TLS" FORCE)
        message(STATUS "Paho MQTT: SSL disabled (OpenSSL not found — cleartext tcp:// only)")
    endif()

    set(PAHO_WITH_MQTT_C ON CACHE BOOL "" FORCE)
    set(PAHO_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(PAHO_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(PAHO_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(PAHO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(PAHO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(PAHO_BUILD_DOCUMENTATION OFF CACHE BOOL "" FORCE)
    set(PAHO_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(PAHO_HIGH_PERFORMANCE ON CACHE BOOL "" FORCE)

    add_subdirectory("${_PAHO_CPP_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/paho.mqtt.cpp" EXCLUDE_FROM_ALL)
endif()

if(TARGET PahoMqttCpp::paho-mqttpp3)
    set(_FOUNDATION_PAHO_TARGET PahoMqttCpp::paho-mqttpp3)
elseif(TARGET paho-mqttpp3-static)
    set(_FOUNDATION_PAHO_TARGET paho-mqttpp3-static)
else()
    message(FATAL_ERROR "Paho MQTT C++ target not found after add_subdirectory")
endif()

message(STATUS "Paho MQTT C++ is available: ${_PAHO_CPP_ROOT} (link ${_FOUNDATION_PAHO_TARGET})")
