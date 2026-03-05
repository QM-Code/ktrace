if(NOT DEFINED KTOOLS_CMAKE_MINIMUM_VERSION OR KTOOLS_CMAKE_MINIMUM_VERSION STREQUAL "")
    set(KTOOLS_CMAKE_MINIMUM_VERSION "3.20")
endif()
cmake_minimum_required(VERSION ${KTOOLS_CMAKE_MINIMUM_VERSION})

project(ktrace_demo_executable LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KTOOLS_DEMO_BUILD_STATIC "Build demo static executable target" ON)
option(KTOOLS_DEMO_BUILD_SHARED "Build demo shared executable target" ON)

if(NOT KTOOLS_DEMO_BUILD_STATIC AND NOT KTOOLS_DEMO_BUILD_SHARED)
    message(FATAL_ERROR "Demo executable requires at least one of KTOOLS_DEMO_BUILD_STATIC or KTOOLS_DEMO_BUILD_SHARED to be ON.")
endif()

if(NOT TARGET ktrace::sdk_shared)
    find_package(KTraceSDK CONFIG REQUIRED)
endif()
if(NOT TARGET alpha::sdk_shared)
    find_package(AlphaSDK CONFIG REQUIRED)
endif()
if(NOT TARGET beta::sdk_shared)
    find_package(BetaSDK CONFIG REQUIRED)
endif()
if(NOT TARGET gamma::sdk_shared)
    find_package(GammaSDK CONFIG REQUIRED)
endif()

set(_ktrace_demo_shared_sdk_dep ktrace::sdk_shared)
if(NOT TARGET ktrace::sdk_shared)
    set(_ktrace_demo_shared_sdk_dep ktrace::sdk)
endif()

set(_ktrace_demo_static_sdk_dep ktrace::sdk_static)
if(NOT TARGET ktrace::sdk_static)
    set(_ktrace_demo_static_sdk_dep ktrace::sdk)
endif()

set(_ktrace_demo_shared_alpha_dep alpha::sdk_shared)
if(NOT TARGET alpha::sdk_shared)
    set(_ktrace_demo_shared_alpha_dep alpha::sdk)
endif()

set(_ktrace_demo_static_alpha_dep alpha::sdk_static)
if(NOT TARGET alpha::sdk_static)
    set(_ktrace_demo_static_alpha_dep alpha::sdk)
endif()

set(_ktrace_demo_shared_beta_dep beta::sdk_shared)
if(NOT TARGET beta::sdk_shared)
    set(_ktrace_demo_shared_beta_dep beta::sdk)
endif()

set(_ktrace_demo_static_beta_dep beta::sdk_static)
if(NOT TARGET beta::sdk_static)
    set(_ktrace_demo_static_beta_dep beta::sdk)
endif()

set(_ktrace_demo_shared_gamma_dep gamma::sdk_shared)
if(NOT TARGET gamma::sdk_shared)
    set(_ktrace_demo_shared_gamma_dep gamma::sdk)
endif()

set(_ktrace_demo_static_gamma_dep gamma::sdk_static)
if(NOT TARGET gamma::sdk_static)
    set(_ktrace_demo_static_gamma_dep gamma::sdk)
endif()

if(KTOOLS_DEMO_BUILD_SHARED)
    add_executable(ktrace_demo_executable
        src/main.cpp
    )

    target_compile_definitions(ktrace_demo_executable PRIVATE KTRACE_NAMESPACE="executable")

    target_link_libraries(ktrace_demo_executable PRIVATE
        ${_ktrace_demo_shared_sdk_dep}
        ${_ktrace_demo_shared_alpha_dep}
        ${_ktrace_demo_shared_beta_dep}
        ${_ktrace_demo_shared_gamma_dep}
    )

    set_target_properties(ktrace_demo_executable PROPERTIES
        OUTPUT_NAME test
    )
endif()

if(KTOOLS_DEMO_BUILD_STATIC)
    add_executable(ktrace_demo_executable_static
        src/main.cpp
    )

    target_compile_definitions(ktrace_demo_executable_static PRIVATE KTRACE_NAMESPACE="executable")

    target_link_libraries(ktrace_demo_executable_static PRIVATE
        ${_ktrace_demo_static_sdk_dep}
        ${_ktrace_demo_static_alpha_dep}
        ${_ktrace_demo_static_beta_dep}
        ${_ktrace_demo_static_gamma_dep}
    )

    set_target_properties(ktrace_demo_executable_static PROPERTIES
        OUTPUT_NAME test_static
    )
endif()

include(CTest)
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/cmake/tests/CMakeLists.txt")
    add_subdirectory(cmake/tests)
endif()
