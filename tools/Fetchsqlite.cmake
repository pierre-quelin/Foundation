# SQLite amalgamation — sources fetched by Description.xml into lib/sqlite
# (zip unpacks to sqlite-amalgamation-NNNNNNN/; flatten sqlite3.c/h to lib/sqlite/)

set(_SQLITE_ROOT "${FOUNDATION_SRC_ROOT}/lib/sqlite")

if(NOT EXISTS "${_SQLITE_ROOT}/sqlite3.c" OR NOT EXISTS "${_SQLITE_ROOT}/sqlite3.h")
    file(GLOB _sqlite_unpack "${_SQLITE_ROOT}/sqlite-amalgamation-*")
    list(LENGTH _sqlite_unpack _sqlite_n)
    if(_sqlite_n EQUAL 1 AND EXISTS "${_sqlite_unpack}/sqlite3.c")
        file(COPY "${_sqlite_unpack}/sqlite3.c" "${_sqlite_unpack}/sqlite3.h"
             DESTINATION "${_SQLITE_ROOT}")
        message(STATUS "sqlite: flattened amalgamation from ${_sqlite_unpack}")
    endif()
endif()

if(NOT EXISTS "${_SQLITE_ROOT}/sqlite3.c" OR NOT EXISTS "${_SQLITE_ROOT}/sqlite3.h")
    message(FATAL_ERROR "lib/sqlite missing sqlite3.c/h — run Build.bat fetch (sqlite amalgamation)")
endif()

if(NOT TARGET sqlite3)
    add_library(sqlite3 STATIC "${_SQLITE_ROOT}/sqlite3.c")
    target_include_directories(sqlite3 PUBLIC "${_SQLITE_ROOT}")
    target_compile_definitions(sqlite3 PUBLIC SQLITE_THREADSAFE=1)
    if(MSVC)
        target_compile_definitions(sqlite3 PRIVATE _CRT_SECURE_NO_WARNINGS)
        # Amalgamation is noisy under /W4-style flags from the parent.
        target_compile_options(sqlite3 PRIVATE /W0)
    else()
        target_compile_options(sqlite3 PRIVATE -w)
    endif()
endif()

message(STATUS "sqlite3 amalgamation is available: ${_SQLITE_ROOT}")
