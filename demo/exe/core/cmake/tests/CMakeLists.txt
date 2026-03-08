set(KTRACE_TRACE_CLI_CASES_SCRIPT
    ${CMAKE_CURRENT_LIST_DIR}/trace_cli_cases.sh
)

add_test(
    NAME demo_cli_unknown_trace_option_shows_help
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_core>
            unknown_option
)

add_test(
    NAME demo_cli_blank_trace_shows_help
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_core>
            blank_trace
)

add_test(
    NAME demo_cli_timestamps_option
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_core>
            timestamps_option
)

add_test(
    NAME demo_cli_imported_selector_enables_alpha
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_core>
            imported_selector
)
