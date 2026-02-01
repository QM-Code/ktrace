include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(PROJECT_VERSION)
    set(KARMA_EXTRAS_VERSION "${PROJECT_VERSION}")
else()
    set(KARMA_EXTRAS_VERSION "0.0.0")
endif()

add_library(karma_extras STATIC
    ${PROJECT_SOURCE_DIR}/src/karma-extras/karma_extras.cpp
)
add_library(karma_extras::karma_extras ALIAS karma_extras)

target_link_libraries(karma_extras PUBLIC karma)

target_include_directories(karma_extras
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src/karma-extras
)

install(TARGETS karma_extras
    EXPORT karma_extrasTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/karma_extras
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT karma_extrasTargets
    NAMESPACE karma_extras::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/karma_extras
)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/karma-extras-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/karma-extras-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/karma_extras
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/karma-extras-config-version.cmake
    VERSION ${KARMA_EXTRAS_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/karma-extras-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/karma-extras-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/karma_extras
)
