if(NOT DEFINED KTOOLS_CMAKE_MINIMUM_VERSION OR KTOOLS_CMAKE_MINIMUM_VERSION STREQUAL "")
    set(KTOOLS_CMAKE_MINIMUM_VERSION "3.20")
endif()
cmake_minimum_required(VERSION ${KTOOLS_CMAKE_MINIMUM_VERSION})

project(alpha VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT TARGET ktrace::sdk)
    find_package(KTraceSDK CONFIG REQUIRED)
endif()
include(CMakePackageConfigHelpers)

add_library(alpha_sdk STATIC
    src/trace.cpp
)
add_library(alpha::sdk ALIAS alpha_sdk)

target_compile_definitions(alpha_sdk PRIVATE KTRACE_NAMESPACE="alpha")

target_include_directories(alpha_sdk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(alpha_sdk PUBLIC ktrace::sdk)

set_target_properties(alpha_sdk PROPERTIES
    OUTPUT_NAME alpha
    EXPORT_NAME sdk
)

install(TARGETS alpha_sdk
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
