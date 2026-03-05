set(KTRACE_TRACE_CLI_CASES_SCRIPT
    ${CMAKE_CURRENT_LIST_DIR}/trace_cli_cases.sh
)

add_test(
    NAME demo_cli_unknown_trace_option_shows_help
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_executable>
            unknown_option
)

add_test(
    NAME demo_cli_blank_trace_shows_help
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_executable>
            blank_trace
)

add_test(
    NAME demo_cli_bad_selector_shows_examples
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_executable>
            bad_selector
)

add_test(
    NAME demo_cli_wildcard_all_includes_depth3
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_executable>
            wildcard_all_depth3
)

add_test(
    NAME demo_cli_brace_selector_enables_expected_channels
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_executable>
            brace_selector
)
