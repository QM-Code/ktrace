cmake_minimum_required(VERSION 3.20)

project(beta VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(KTraceSDK CONFIG REQUIRED)
include(CMakePackageConfigHelpers)

add_library(beta_sdk STATIC
    src/trace.cpp
)
add_library(beta::sdk ALIAS beta_sdk)

target_compile_definitions(beta_sdk PRIVATE KTRACE_NAMESPACE="beta")

target_include_directories(beta_sdk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(beta_sdk PUBLIC ktrace::sdk)

set_target_properties(beta_sdk PROPERTIES
    OUTPUT_NAME beta
    EXPORT_NAME sdk
)

install(TARGETS beta_sdk
    EXPORT BetaSDKTargets
    ARCHIVE DESTINATION lib COMPONENT BetaSDK
    LIBRARY DESTINATION lib COMPONENT BetaSDK
    RUNTIME DESTINATION bin COMPONENT BetaSDK
    INCLUDES DESTINATION include
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
    COMPONENT BetaSDK
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT BetaSDKTargets
    FILE BetaSDKTargets.cmake
    NAMESPACE beta::
    DESTINATION lib/cmake/BetaSDK
    COMPONENT BetaSDK
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/BetaSDKConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/BetaSDKConfig.cmake
    INSTALL_DESTINATION lib/cmake/BetaSDK
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/BetaSDKConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/BetaSDKConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/BetaSDKConfigVersion.cmake
    DESTINATION lib/cmake/BetaSDK
    COMPONENT BetaSDK
)
