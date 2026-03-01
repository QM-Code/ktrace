cmake_minimum_required(VERSION 3.20)

project(ktrace_demo_initialize LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(KTraceSDK CONFIG REQUIRED)

add_executable(ktrace_demo_initialize
    src/main.cpp
)
target_compile_definitions(ktrace_demo_initialize PRIVATE KTRACE_NAMESPACE="demo")
target_link_libraries(ktrace_demo_initialize PRIVATE ktrace::sdk)
set_target_properties(ktrace_demo_initialize PROPERTIES
    OUTPUT_NAME test
)
