if(NOT DEFINED KTOOLS_CMAKE_MINIMUM_VERSION OR KTOOLS_CMAKE_MINIMUM_VERSION STREQUAL "")
    set(KTOOLS_CMAKE_MINIMUM_VERSION "3.20")
endif()
cmake_minimum_required(VERSION ${KTOOLS_CMAKE_MINIMUM_VERSION})

project(ktrace_demo_bootstrap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT TARGET ktrace::sdk)
    find_package(KTraceSDK CONFIG REQUIRED)
endif()

add_executable(ktrace_demo_bootstrap
    src/main.cpp
)

target_compile_definitions(ktrace_demo_bootstrap PRIVATE KTRACE_NAMESPACE="bootstrap")

target_link_libraries(ktrace_demo_bootstrap PRIVATE
    ktrace::sdk
)

set_target_properties(ktrace_demo_bootstrap PROPERTIES
    OUTPUT_NAME bootstrap
)

include(CTest)
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/cmake/tests/CMakeLists.txt")
    add_subdirectory(cmake/tests)
endif()
