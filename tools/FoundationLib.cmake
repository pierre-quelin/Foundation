#
# Static libfoundation target and application link (Phase 2.9).
#

option(FOUNDATION_WHOLE_ARCHIVE
       "Link the entire libfoundation archive (all factory registrars)"
       OFF)

function(foundation_add_static_library)
    if(NOT ALL_SRCS)
        message(FATAL_ERROR "foundation_add_static_library: ALL_SRCS is empty — add packages first")
    endif()

    add_library(foundation STATIC ${ALL_SRCS})

    target_include_directories(foundation PUBLIC
        $<BUILD_INTERFACE:${CMAKE_BINARY_DIR};${CMAKE_BINARY_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

    if(WIN32)
        target_compile_definitions(foundation PUBLIC NOMINMAX NOGDI)
    endif()
endfunction()

function(foundation_link_application app_target)
    if(FOUNDATION_WHOLE_ARCHIVE)
        if(MSVC)
            target_link_libraries(${app_target} PRIVATE
                "$<LINK_LIBRARY:WHOLE_ARCHIVE,foundation>")
        else()
            target_link_libraries(${app_target} PRIVATE
                -Wl,--whole-archive foundation -Wl,--no-whole-archive)
        endif()
    else()
        target_link_libraries(${app_target} PRIVATE foundation)
    endif()
endfunction()
