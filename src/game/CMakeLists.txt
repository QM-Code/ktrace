add_executable(bz3
    ${PROJECT_SOURCE_DIR}/src/game/client/bootstrap.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/cli_options.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/main.cpp
    ${PROJECT_SOURCE_DIR}/src/game/game.cpp
)

set_target_properties(bz3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_include_directories(bz3 PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/engine
    ${PROJECT_SOURCE_DIR}/src/game
)

target_link_libraries(bz3 PRIVATE karma_engine)
