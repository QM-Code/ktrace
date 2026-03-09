set(KTRACE_TRACE_CLI_CASES_SCRIPT
    ${CMAKE_CURRENT_LIST_DIR}/trace_cli_cases.sh
)

add_test(
    NAME demo_cli_unknown_trace_option_shows_help
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_omega>
            unknown_option
)

add_test(
    NAME demo_cli_blank_trace_shows_help
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_omega>
            blank_trace
)

add_test(
    NAME demo_cli_bad_selector_shows_examples
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_omega>
            bad_selector
)

add_test(
    NAME demo_cli_exact_selector_warning
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_omega>
            exact_selector_warning
)

add_test(
    NAME demo_cli_wildcard_selector_warning
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_omega>
            wildcard_selector_warning
)

add_test(
    NAME demo_cli_timestamps_option
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_omega>
            timestamps_option
)

add_test(
    NAME demo_cli_files_option
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_omega>
            files_option
)

add_test(
    NAME demo_cli_functions_option
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_omega>
            functions_option
)

add_test(
    NAME demo_cli_functions_with_timestamps_option
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_omega>
            functions_with_timestamps
)

add_test(
    NAME demo_cli_removed_lines_option_is_unknown
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_omega>
            removed_lines_option
)

add_test(
    NAME demo_cli_wildcard_all_includes_depth3
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_omega>
            wildcard_all_depth3
)

add_test(
    NAME demo_cli_brace_selector_enables_expected_channels
    COMMAND ${KTRACE_TRACE_CLI_CASES_SCRIPT}
            $<TARGET_FILE:ktrace_demo_omega>
            brace_selector
)
