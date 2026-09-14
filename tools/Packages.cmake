#
# Define macros to add packages and files
#
# FOUNDATION_SRC_ROOT: repo root of Foundation (set by Foundation CMakeLists).
# Defaults to CMAKE_SOURCE_DIR for standalone / Sample-local Packages.cmake copies.
#

if(NOT DEFINED FOUNDATION_SRC_ROOT)
   set(FOUNDATION_SRC_ROOT "${CMAKE_SOURCE_DIR}")
endif()

include(${FOUNDATION_SRC_ROOT}/tools/GenerateStateMachine.cmake)

INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR})
INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR}/include)

SET(ALL_SRCS)
SET(PKG_SRCS)

#
# Set the path include files shall be exported to
#
MACRO(set_inc_path _ns)
   SET(PKG_INCPATH ${_ns})
ENDMACRO()


#
# Add a source file
#
MACRO(add_src _file)
   LIST(APPEND ALL_SRCS ${FOUNDATION_SRC_ROOT}/src/${PKG_NAME}/${_file})
   LIST(APPEND PKG_SRCS ${FOUNDATION_SRC_ROOT}/src/${PKG_NAME}/${_file})
ENDMACRO()


#
# Add an include file to be exported to inc_path
#
MACRO(add_inc _file)
   SET(INC_FILE ${_file})
   CONFIGURE_FILE(${CMAKE_BINARY_DIR}/trampoline.h ${CMAKE_BINARY_DIR}/include/${PKG_INCPATH}/${_file})
   set_property(GLOBAL APPEND PROPERTY FOUNDATION_PUBLIC_HEADER_RELPATHS "${PKG_INCPATH}/${_file}")
   set_property(GLOBAL APPEND PROPERTY FOUNDATION_PUBLIC_HEADER_SOURCES "${FOUNDATION_SRC_ROOT}/src/${PKG_NAME}/${_file}")
   LIST(APPEND PKG_SRCS ${FOUNDATION_SRC_ROOT}/src/${PKG_NAME}/${_file})
   LIST(APPEND ALL_SRCS ${FOUNDATION_SRC_ROOT}/src/${PKG_NAME}/${_file})
ENDMACRO()

#
# Add a .smd state machine (package-relative; requires set_inc_path first)
#
MACRO(add_smd _file)
   foundation_add_smd_statemachine(${FOUNDATION_SRC_ROOT}/src/${PKG_NAME}/${_file})
ENDMACRO()

#
# add_packages(name)
#
# Add the package to the build
#
MACRO(add_package _name)
   SET(PKG_NAME ${_name})
   SET(PKG_SRCS)
   SET(PKG_INCPATH .)
   INCLUDE(${FOUNDATION_SRC_ROOT}/src/${PKG_NAME}/CMakeLists.txt)
   STRING(REPLACE "/" "\\\\" PKG_NAME_VS ${PKG_NAME})
   SOURCE_GROUP(${PKG_NAME_VS}\\ FILES ${PKG_SRCS})
ENDMACRO()

#
# Create the template "trampoline.h" file
#
FILE(WRITE ${CMAKE_BINARY_DIR}/trampoline.h "\#include \"${FOUNDATION_SRC_ROOT}/src/\${PKG_NAME}/\${INC_FILE}\"")

