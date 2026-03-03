cmake_minimum_required(VERSION 3.20)

project(ktrace_demo_executable LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(KTraceSDK CONFIG REQUIRED)
find_package(AlphaSDK CONFIG REQUIRED)
find_package(BetaSDK CONFIG REQUIRED)
find_package(GammaSDK CONFIG REQUIRED)

add_executable(ktrace_demo_executable
    src/main.cpp
)

target_compile_definitions(ktrace_demo_executable PRIVATE KTRACE_NAMESPACE="executable")

target_link_libraries(ktrace_demo_executable PRIVATE
    ktrace::sdk
    alpha::sdk
    beta::sdk
    gamma::sdk
)

set_target_properties(ktrace_demo_executable PROPERTIES
    OUTPUT_NAME test
)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME demo_cli_unknown_trace_option_shows_help
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/trace_cli_cases.sh
                $<TARGET_FILE:ktrace_demo_executable>
                unknown_option
    )

    add_test(
        NAME demo_cli_blank_trace_shows_help
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/trace_cli_cases.sh
                $<TARGET_FILE:ktrace_demo_executable>
                blank_trace
    )

    add_test(
        NAME demo_cli_bad_selector_shows_examples
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tests/trace_cli_cases.sh
                $<TARGET_FILE:ktrace_demo_executable>
                bad_selector
    )
endif()
