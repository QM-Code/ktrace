include(CMakePackageConfigHelpers)

set(KTOOLS_INSTALL_CMAKEDIR "lib/cmake/KtraceSDK")

set(_ktrace_install_targets)
if(TARGET ktrace_sdk_static)
    list(APPEND _ktrace_install_targets ktrace_sdk_static)
endif()
if(TARGET ktrace_sdk_shared)
    list(APPEND _ktrace_install_targets ktrace_sdk_shared)
endif()

install(TARGETS ${_ktrace_install_targets}
    EXPORT KtraceSDKTargets
    ARCHIVE DESTINATION lib COMPONENT KtraceSDK
    LIBRARY DESTINATION lib COMPONENT KtraceSDK
    RUNTIME DESTINATION bin COMPONENT KtraceSDK
    INCLUDES DESTINATION include
)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/
    DESTINATION include
    COMPONENT KtraceSDK
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT KtraceSDKTargets
    FILE KtraceSDKTargets.cmake
    NAMESPACE ktrace::
    DESTINATION ${KTOOLS_INSTALL_CMAKEDIR}
    COMPONENT KtraceSDK
)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/KtraceSDKConfig.cmake.in
    ${PROJECT_BINARY_DIR}/KtraceSDKConfig.cmake
    INSTALL_DESTINATION ${KTOOLS_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/KtraceSDKConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${PROJECT_BINARY_DIR}/KtraceSDKConfig.cmake
    ${PROJECT_BINARY_DIR}/KtraceSDKConfigVersion.cmake
    DESTINATION ${KTOOLS_INSTALL_CMAKEDIR}
    COMPONENT KtraceSDK
)
