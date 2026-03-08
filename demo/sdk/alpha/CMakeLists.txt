if(NOT DEFINED KTOOLS_CMAKE_MINIMUM_VERSION OR KTOOLS_CMAKE_MINIMUM_VERSION STREQUAL "")
    set(KTOOLS_CMAKE_MINIMUM_VERSION "3.20")
endif()
cmake_minimum_required(VERSION ${KTOOLS_CMAKE_MINIMUM_VERSION})

project(alpha VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KTOOLS_DEMO_BUILD_STATIC "Build demo static library target" ON)
option(KTOOLS_DEMO_BUILD_SHARED "Build demo shared library target" ON)

if(NOT KTOOLS_DEMO_BUILD_STATIC AND NOT KTOOLS_DEMO_BUILD_SHARED)
    message(FATAL_ERROR "Demo library requires at least one of KTOOLS_DEMO_BUILD_STATIC or KTOOLS_DEMO_BUILD_SHARED to be ON.")
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
        INSTALL_RPATH "${KTOOLS_RUNTIME_RPATH_DIRS}"
    )
endfunction()

if(NOT TARGET ktrace::sdk_shared)
    find_package(KTraceSDK CONFIG REQUIRED)
endif()
include(CMakePackageConfigHelpers)

set(_alpha_sdk_static_dep ktrace::sdk_static)
if(NOT TARGET ktrace::sdk_static)
    set(_alpha_sdk_static_dep ktrace::sdk)
endif()

set(_alpha_sdk_shared_dep ktrace::sdk_shared)
if(NOT TARGET ktrace::sdk_shared)
    set(_alpha_sdk_shared_dep ktrace::sdk)
endif()

set(_alpha_install_targets)

if(KTOOLS_DEMO_BUILD_STATIC)
    add_library(alpha_sdk_static STATIC
        src/trace.cpp
    )
    add_library(alpha::sdk_static ALIAS alpha_sdk_static)

    target_compile_definitions(alpha_sdk_static PRIVATE KTRACE_NAMESPACE="alpha")

    target_include_directories(alpha_sdk_static
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(alpha_sdk_static PUBLIC ${_alpha_sdk_static_dep})

    set_target_properties(alpha_sdk_static PROPERTIES
        OUTPUT_NAME alpha
        EXPORT_NAME sdk_static
    )
    list(APPEND _alpha_install_targets alpha_sdk_static)
endif()

if(KTOOLS_DEMO_BUILD_SHARED)
    add_library(alpha_sdk_shared SHARED
        src/trace.cpp
    )
    add_library(alpha::sdk_shared ALIAS alpha_sdk_shared)

    target_compile_definitions(alpha_sdk_shared PRIVATE KTRACE_NAMESPACE="alpha")

    target_include_directories(alpha_sdk_shared
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    target_link_libraries(alpha_sdk_shared PUBLIC ${_alpha_sdk_shared_dep})

    set_target_properties(alpha_sdk_shared PROPERTIES
        OUTPUT_NAME alpha
        EXPORT_NAME sdk_shared
    )
    ktools_apply_runtime_rpath(alpha_sdk_shared)
    list(APPEND _alpha_install_targets alpha_sdk_shared)
endif()

if(TARGET alpha_sdk_shared)
    add_library(alpha::sdk ALIAS alpha_sdk_shared)
elseif(TARGET alpha_sdk_static)
    add_library(alpha::sdk ALIAS alpha_sdk_static)
endif()

install(TARGETS ${_alpha_install_targets}
    EXPORT AlphaSDKTargets
    ARCHIVE DESTINATION lib COMPONENT AlphaSDK
    LIBRARY DESTINATION lib COMPONENT AlphaSDK
    RUNTIME DESTINATION bin COMPONENT AlphaSDK
    INCLUDES DESTINATION include
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
    COMPONENT AlphaSDK
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT AlphaSDKTargets
    FILE AlphaSDKTargets.cmake
    NAMESPACE alpha::
    DESTINATION lib/cmake/AlphaSDK
    COMPONENT AlphaSDK
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/AlphaSDKConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/AlphaSDKConfig.cmake
    INSTALL_DESTINATION lib/cmake/AlphaSDK
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/AlphaSDKConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/AlphaSDKConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/AlphaSDKConfigVersion.cmake
    DESTINATION lib/cmake/AlphaSDK
    COMPONENT AlphaSDK
)

include(CTest)
if(BUILD_TESTING AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/cmake/tests/CMakeLists.txt")
    add_subdirectory(cmake/tests)
endif()
