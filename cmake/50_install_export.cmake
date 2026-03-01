include(CMakePackageConfigHelpers)

set(KTRACE_INSTALL_CMAKEDIR "lib/cmake/KTraceSDK")

install(TARGETS ktrace_sdk
    EXPORT KTraceSDKTargets
    ARCHIVE DESTINATION lib COMPONENT KTraceSDK
    LIBRARY DESTINATION lib COMPONENT KTraceSDK
    RUNTIME DESTINATION bin COMPONENT KTraceSDK
    INCLUDES DESTINATION include
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
    DESTINATION include
    COMPONENT KTraceSDK
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT KTraceSDKTargets
    FILE KTraceSDKTargets.cmake
    NAMESPACE ktrace::
    DESTINATION ${KTRACE_INSTALL_CMAKEDIR}
    COMPONENT KTraceSDK
)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/KTraceSDKConfig.cmake.in
    ${PROJECT_BINARY_DIR}/KTraceSDKConfig.cmake
    INSTALL_DESTINATION ${KTRACE_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/KTraceSDKConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${PROJECT_BINARY_DIR}/KTraceSDKConfig.cmake
    ${PROJECT_BINARY_DIR}/KTraceSDKConfigVersion.cmake
    DESTINATION ${KTRACE_INSTALL_CMAKEDIR}
    COMPONENT KTraceSDK
)
