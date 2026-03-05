if(NOT DEFINED KTOOLS_CMAKE_MINIMUM_VERSION OR KTOOLS_CMAKE_MINIMUM_VERSION STREQUAL "")
    set(KTOOLS_CMAKE_MINIMUM_VERSION "3.20")
endif()
cmake_minimum_required(VERSION ${KTOOLS_CMAKE_MINIMUM_VERSION})

project(ktrace_demo_executable LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT TARGET ktrace::sdk)
    find_package(KTraceSDK CONFIG REQUIRED)
endif()
if(NOT TARGET alpha::sdk)
    find_package(AlphaSDK CONFIG REQUIRED)
endif()
if(NOT TARGET beta::sdk)
    find_package(BetaSDK CONFIG REQUIRED)
endif()
if(NOT TARGET gamma::sdk)
    find_package(GammaSDK CONFIG REQUIRED)
endif()

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
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/cmake/tests/CMakeLists.txt")
    add_subdirectory(cmake/tests)
endif()
