add_executable(statemachine_gen
    main.cpp
    ${FOUNDATION_SRC_ROOT}/src/tools/design/statemachine/SmdParser.cpp
    ${FOUNDATION_SRC_ROOT}/src/tools/design/statemachine/SmdPilotValidator.cpp
    ${FOUNDATION_SRC_ROOT}/src/tools/design/statemachine/StateMachineCppGenerator.cpp
    ${FOUNDATION_SRC_ROOT}/src/tools/design/statemachine/StateMachinePlantUmlExporter.cpp
)

target_link_libraries(statemachine_gen PRIVATE nlohmann_json::nlohmann_json)

target_include_directories(statemachine_gen PRIVATE
    ${FOUNDATION_SRC_ROOT}/src
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/include
)

if(WIN32)
    target_compile_definitions(statemachine_gen PRIVATE NOMINMAX NOGDI)
endif()

install(TARGETS statemachine_gen
        RUNTIME DESTINATION ${FOUNDATION_DIST_ROOT}/tools)
