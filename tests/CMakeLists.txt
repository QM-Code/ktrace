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
