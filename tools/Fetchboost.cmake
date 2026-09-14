# Boost 1.86.0 — official zip fetched by Description.xml into lib/boost
# Header-only consume: INTERFACE targets so callers keep Boost::signals2 / Boost.Test names.

set(_BOOST_BASE "${FOUNDATION_SRC_ROOT}/lib/boost")
set(BOOST_ROOT "")
if(EXISTS "${_BOOST_BASE}/boost/signals2.hpp")
    set(BOOST_ROOT "${_BOOST_BASE}")
elseif(EXISTS "${_BOOST_BASE}/boost_1_86_0/boost/signals2.hpp")
    set(BOOST_ROOT "${_BOOST_BASE}/boost_1_86_0")
else()
    file(GLOB _boost_unpack "${_BOOST_BASE}/boost_*")
    foreach(_cand IN LISTS _boost_unpack)
        if(EXISTS "${_cand}/boost/signals2.hpp")
            set(BOOST_ROOT "${_cand}")
            break()
        endif()
    endforeach()
endif()

if(NOT BOOST_ROOT)
    message(FATAL_ERROR "lib/boost missing or incomplete — run Build.bat fetch")
endif()

if(NOT TARGET Boost_headers)
    add_library(Boost_headers INTERFACE)
    add_library(Boost::headers ALIAS Boost_headers)
    target_include_directories(Boost_headers INTERFACE "${BOOST_ROOT}")
    # Header-only archive: do not look for Boost .lib autolink on MSVC.
    target_compile_definitions(Boost_headers INTERFACE BOOST_ALL_NO_LIB)
endif()

if(NOT TARGET Boost::signals2)
    add_library(Boost::signals2 ALIAS Boost_headers)
endif()

if(NOT TARGET Boost::included_unit_test_framework)
    add_library(Boost::included_unit_test_framework ALIAS Boost_headers)
endif()

if(NOT TARGET FoundationBoostMsm)
    add_library(FoundationBoostMsm INTERFACE)
    target_compile_definitions(FoundationBoostMsm INTERFACE
        BOOST_MPL_CFG_NO_PREPROCESSED_HEADERS
        BOOST_MPL_LIMIT_VECTOR_SIZE=30
    )
    if(MSVC)
        # Boost.MSM: compile-time branches in state_machine.hpp (third-party).
        target_compile_options(FoundationBoostMsm INTERFACE /wd4127)
    endif()
    target_link_libraries(FoundationBoostMsm INTERFACE Boost_headers)
endif()

message(STATUS "boost is available: ${BOOST_ROOT}")
