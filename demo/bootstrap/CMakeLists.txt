if(NOT DEFINED KTOOLS_CMAKE_MINIMUM_VERSION OR KTOOLS_CMAKE_MINIMUM_VERSION STREQUAL "")
    set(KTOOLS_CMAKE_MINIMUM_VERSION "3.20")
endif()
cmake_minimum_required(VERSION ${KTOOLS_CMAKE_MINIMUM_VERSION})

project(ktrace_demo_bootstrap LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KTOOLS_DEMO_BUILD_STATIC "Build demo static executable target" ON)
option(KTOOLS_DEMO_BUILD_SHARED "Build demo shared executable target" ON)

if(NOT KTOOLS_DEMO_BUILD_STATIC AND NOT KTOOLS_DEMO_BUILD_SHARED)
    message(FATAL_ERROR "Demo bootstrap requires at least one of KTOOLS_DEMO_BUILD_STATIC or KTOOLS_DEMO_BUILD_SHARED to be ON.")
endif()

function(ktools_apply_runtime_rpath target_name)
    if(NOT TARGET "${target_name}")
        return()
    endif()
    if(NOT DEFINED KTOOLS_RUNTIME_RPATH_DIRS OR KTOOLS_RUNTIME_RPATH_DIRS STREQUAL "")
        return()
    endif()
    set_target_properties("${target_name}" PROPERTIES
        BUILD_RPATH "${KTOOLS_RUNTIME_RPATH_DIRS}"
    )
endfunction()

if(NOT TARGET ktrace::sdk_shared)
    find_package(KtraceSDK CONFIG REQUIRED)
endif()

set(_ktrace_bootstrap_shared_dep ktrace::sdk_shared)
if(NOT TARGET ktrace::sdk_shared)
    set(_ktrace_bootstrap_shared_dep ktrace::sdk)
endif()

set(_ktrace_bootstrap_static_dep ktrace::sdk_static)
if(NOT TARGET ktrace::sdk_static)
    set(_ktrace_bootstrap_static_dep ktrace::sdk)
endif()

if(KTOOLS_DEMO_BUILD_SHARED)
    add_executable(ktrace_demo_bootstrap
        src/main.cpp
    )

    target_link_libraries(ktrace_demo_bootstrap PRIVATE
        ${_ktrace_bootstrap_shared_dep}
    )

    set_target_properties(ktrace_demo_bootstrap PROPERTIES
        OUTPUT_NAME bootstrap
    )
    ktools_apply_runtime_rpath(ktrace_demo_bootstrap)
endif()

if(KTOOLS_DEMO_BUILD_STATIC)
    add_executable(ktrace_demo_bootstrap_static
        src/main.cpp
    )

    target_link_libraries(ktrace_demo_bootstrap_static PRIVATE
        ${_ktrace_bootstrap_static_dep}
    )

    set_target_properties(ktrace_demo_bootstrap_static PROPERTIES
        OUTPUT_NAME bootstrap_static
    )
endif()

include(CTest)
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/cmake/tests/CMakeLists.txt")
    add_subdirectory(cmake/tests)
endif()
