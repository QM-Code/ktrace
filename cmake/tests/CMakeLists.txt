add_executable(ktrace_changed_thread_safety_test
    ktrace_changed_thread_safety_test.cpp
)

target_compile_definitions(ktrace_changed_thread_safety_test PRIVATE KTRACE_NAMESPACE="tests")

target_link_libraries(ktrace_changed_thread_safety_test PRIVATE
    ktrace::sdk
)

add_test(
    NAME ktrace_changed_thread_safety
    COMMAND ktrace_changed_thread_safety_test
)
set_tests_properties(ktrace_changed_thread_safety PROPERTIES TIMEOUT 30)

add_executable(ktrace_channel_semantics_test
    ktrace_channel_semantics_test.cpp
)

target_compile_definitions(ktrace_channel_semantics_test PRIVATE KTRACE_NAMESPACE="tests")

target_link_libraries(ktrace_channel_semantics_test PRIVATE
    ktrace::sdk
)

add_test(
    NAME ktrace_channel_semantics
    COMMAND ktrace_channel_semantics_test
)
set_tests_properties(ktrace_channel_semantics PROPERTIES TIMEOUT 30)

add_executable(ktrace_log_api_test
    ktrace_log_api_test.cpp
)

target_compile_definitions(ktrace_log_api_test PRIVATE KTRACE_NAMESPACE="tests")

target_link_libraries(ktrace_log_api_test PRIVATE
    ktrace::sdk
)

add_test(
    NAME ktrace_log_api
    COMMAND ktrace_log_api_test
)
set_tests_properties(ktrace_log_api PROPERTIES TIMEOUT 30)
