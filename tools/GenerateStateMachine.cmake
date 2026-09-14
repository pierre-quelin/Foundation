#
# Build-time codegen from Foundation .smd files (statemachine-cpp).
#

include_guard(GLOBAL)

#[=======================================================================[.rst:
foundation_add_smd_statemachine
--------------------------------

Internal implementation for package macro @c add_smd (see Packages.cmake).
.smd files live only under @c src/<PKG_NAME>/; packages call @c add_smd(MyDevice.smd).

Generate Boost.MSM sources from a @c .smd file at build time (after @c set_inc_path).

.. code-block:: cmake

   set_inc_path(sample/statemachine)
   add_smd(MyDevice.smd)

Arguments:

``smd_path``
  Absolute path to the @c foundation-smd-4 file (required).
#]=======================================================================]

function(foundation_add_smd_statemachine smd_path)
    if(NOT smd_path)
        message(FATAL_ERROR "foundation_add_smd_statemachine: SMD path required")
    endif()
    if(NOT TARGET statemachine_gen)
        message(FATAL_ERROR "foundation_add_smd_statemachine: statemachine_gen target missing")
    endif()
    if(NOT PKG_INCPATH)
        message(FATAL_ERROR "foundation_add_smd_statemachine: call set_inc_path first")
    endif()

    get_filename_component(_machine "${smd_path}" NAME_WE)
    set(_gen_dir "${CMAKE_BINARY_DIR}/generated/statemachine/${_machine}")
    set(_sm_h "${_gen_dir}/${_machine}_sm.h")
    set(_sm_cpp "${_gen_dir}/${_machine}_sm.cpp")
    set(_sm_fwd "${_gen_dir}/${_machine}_sm_fwd.h")
    set(_sm_decl "${_gen_dir}/${_machine}_sm_decl.h")

    add_custom_command(
        OUTPUT "${_sm_h}" "${_sm_cpp}" "${_sm_fwd}" "${_sm_decl}"
        COMMAND "$<TARGET_FILE:statemachine_gen>"
                --smd "${smd_path}"
                --out "${_gen_dir}"
        DEPENDS "${smd_path}" statemachine_gen
        COMMENT "Generating state machine ${_machine} from ${smd_path}"
        VERBATIM
    )

    # Keep the .smd in the package filter (not compiled).
    set_source_files_properties("${smd_path}" PROPERTIES HEADER_FILE_ONLY TRUE)
    set_source_files_properties(
        "${_sm_h}" "${_sm_fwd}" "${_sm_decl}"
        PROPERTIES HEADER_FILE_ONLY TRUE
    )

    # Generated files: package\generated (not default "Source Files").
    if(PKG_NAME)
        string(REPLACE "/" "\\" _gen_group "${PKG_NAME}/generated")
    else()
        set(_gen_group "generated/statemachine/${_machine}")
        string(REPLACE "/" "\\" _gen_group "${_gen_group}")
    endif()
    source_group("${_gen_group}" FILES "${_sm_h}" "${_sm_cpp}" "${_sm_fwd}" "${_sm_decl}")

    list(APPEND ALL_SRCS "${smd_path}" "${_sm_cpp}" "${_sm_h}" "${_sm_fwd}" "${_sm_decl}")
    set(ALL_SRCS "${ALL_SRCS}" PARENT_SCOPE)

    # Hand-written / design only — generated files have their own source_group above.
    list(APPEND PKG_SRCS "${smd_path}")
    foreach(_hdr IN ITEMS "${_sm_h}" "${_sm_fwd}" "${_sm_decl}")
        get_filename_component(_hdr_name "${_hdr}" NAME)
        file(WRITE "${CMAKE_BINARY_DIR}/include/${PKG_INCPATH}/${_hdr_name}"
             "#include \"${_hdr}\"\n")
    endforeach()
    if(DEFINED FOUNDATION_DIST_ROOT)
        install(
            FILES "${_sm_h}" "${_sm_fwd}" "${_sm_decl}"
            DESTINATION "${FOUNDATION_DIST_ROOT}/include/${PKG_INCPATH}"
        )
    endif()
    set(PKG_SRCS "${PKG_SRCS}" PARENT_SCOPE)

    message(STATUS "foundation_add_smd_statemachine: ${_machine} -> ${_gen_dir}")
endfunction()
