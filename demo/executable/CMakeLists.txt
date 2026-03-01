cmake_minimum_required(VERSION 3.20)

project(ktrace_demo_executable LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(KTraceSDK CONFIG REQUIRED)
find_package(AlphaSDK CONFIG REQUIRED)
find_package(BetaSDK CONFIG REQUIRED)
find_package(DeltaSDK CONFIG REQUIRED)

add_executable(ktrace_demo_executable
    src/main.cpp
)

target_compile_definitions(ktrace_demo_executable PRIVATE KTRACE_NAMESPACE="executable")

target_link_libraries(ktrace_demo_executable PRIVATE
    ktrace::sdk
    alpha::sdk
    beta::sdk
    delta::sdk
)

set_target_properties(ktrace_demo_executable PROPERTIES
    OUTPUT_NAME test
)
