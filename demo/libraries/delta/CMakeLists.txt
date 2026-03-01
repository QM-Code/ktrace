cmake_minimum_required(VERSION 3.20)

project(delta VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(KTraceSDK CONFIG REQUIRED)
include(CMakePackageConfigHelpers)

add_library(delta_sdk STATIC
    src/trace.cpp
)
add_library(delta::sdk ALIAS delta_sdk)

target_compile_definitions(delta_sdk PRIVATE KTRACE_NAMESPACE="delta")

target_include_directories(delta_sdk
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(delta_sdk PUBLIC ktrace::sdk)

set_target_properties(delta_sdk PROPERTIES
    OUTPUT_NAME delta
    EXPORT_NAME sdk
)

install(TARGETS delta_sdk
    EXPORT DeltaSDKTargets
    ARCHIVE DESTINATION lib COMPONENT DeltaSDK
    LIBRARY DESTINATION lib COMPONENT DeltaSDK
    RUNTIME DESTINATION bin COMPONENT DeltaSDK
    INCLUDES DESTINATION include
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include
    COMPONENT DeltaSDK
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT DeltaSDKTargets
    FILE DeltaSDKTargets.cmake
    NAMESPACE delta::
    DESTINATION lib/cmake/DeltaSDK
    COMPONENT DeltaSDK
)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/DeltaSDKConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/DeltaSDKConfig.cmake
    INSTALL_DESTINATION lib/cmake/DeltaSDK
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/DeltaSDKConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/DeltaSDKConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/DeltaSDKConfigVersion.cmake
    DESTINATION lib/cmake/DeltaSDK
    COMPONENT DeltaSDK
)
