include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(PROJECT_VERSION)
    set(KARMA_EXTRAS_VERSION "${PROJECT_VERSION}")
else()
    set(KARMA_EXTRAS_VERSION "0.0.0")
endif()

set(KARMA_EXTRAS_SOURCES
    ${PROJECT_SOURCE_DIR}/src/karma-extras/karma_extras.cpp
)
if(KARMA_UI_BACKEND STREQUAL "imgui")
    if(KARMA_RENDER_BACKEND STREQUAL "bgfx")
        list(APPEND KARMA_EXTRAS_SOURCES
            ${PROJECT_SOURCE_DIR}/src/karma-extras/ui/platform/imgui/renderer_bgfx.cpp
            ${PROJECT_SOURCE_DIR}/src/karma-extras/ui/imgui/thumbnail_cache.cpp
            ${PROJECT_SOURCE_DIR}/src/karma-extras/ui/imgui/texture_registry.cpp
        )
    elseif(KARMA_RENDER_BACKEND STREQUAL "diligent")
        list(APPEND KARMA_EXTRAS_SOURCES
            ${PROJECT_SOURCE_DIR}/src/karma-extras/ui/platform/imgui/renderer_diligent.cpp
            ${PROJECT_SOURCE_DIR}/src/karma-extras/ui/imgui/thumbnail_cache.cpp
            ${PROJECT_SOURCE_DIR}/src/karma-extras/ui/imgui/texture_registry.cpp
        )
    endif()
elseif(KARMA_UI_BACKEND STREQUAL "rmlui")
    if(KARMA_RENDER_BACKEND STREQUAL "bgfx")
        list(APPEND KARMA_EXTRAS_SOURCES
            ${PROJECT_SOURCE_DIR}/src/karma-extras/ui/platform/rmlui/renderer_bgfx.cpp
        )
    elseif(KARMA_RENDER_BACKEND STREQUAL "diligent")
        list(APPEND KARMA_EXTRAS_SOURCES
            ${PROJECT_SOURCE_DIR}/src/karma-extras/ui/platform/rmlui/renderer_diligent.cpp
        )
    endif()
endif()

add_library(karma_extras STATIC ${KARMA_EXTRAS_SOURCES})
add_library(karma_extras::karma_extras ALIAS karma_extras)

target_link_libraries(karma_extras PUBLIC karma)
if(KARMA_RENDER_BACKEND STREQUAL "bgfx")
    target_link_libraries(karma_extras PUBLIC bgfx::bgfx)
elseif(KARMA_RENDER_BACKEND STREQUAL "diligent")
    target_link_libraries(karma_extras PUBLIC $<BUILD_INTERFACE:Diligent-GraphicsEngineVk-static>)
    if(TARGET Diligent-Common)
        target_link_libraries(karma_extras PUBLIC $<BUILD_INTERFACE:Diligent-Common>)
    endif()
    if(TARGET Diligent-GraphicsEngine)
        target_link_libraries(karma_extras PUBLIC $<BUILD_INTERFACE:Diligent-GraphicsEngine>)
    endif()
    if(TARGET Diligent-GraphicsEngineNextGenBase)
        target_link_libraries(karma_extras PUBLIC $<BUILD_INTERFACE:Diligent-GraphicsEngineNextGenBase>)
    endif()
    if(TARGET Diligent-GraphicsAccessories)
        target_link_libraries(karma_extras PUBLIC $<BUILD_INTERFACE:Diligent-GraphicsAccessories>)
    endif()
    if(TARGET Diligent-ShaderTools)
        target_link_libraries(karma_extras PUBLIC $<BUILD_INTERFACE:Diligent-ShaderTools>)
    endif()
    if(TARGET Diligent-PlatformInterface)
        target_link_libraries(karma_extras PUBLIC $<BUILD_INTERFACE:Diligent-PlatformInterface>)
    endif()
    if(TARGET Diligent-TargetPlatform)
        target_link_libraries(karma_extras PUBLIC $<BUILD_INTERFACE:Diligent-TargetPlatform>)
    endif()
    if(DEFINED DiligentEngine_SOURCE_DIR)
        target_include_directories(karma_extras PUBLIC $<BUILD_INTERFACE:${DiligentEngine_SOURCE_DIR}>)
        target_include_directories(karma_extras PUBLIC $<BUILD_INTERFACE:${DiligentEngine_SOURCE_DIR}/DiligentCore>)
        target_include_directories(karma_extras PUBLIC $<BUILD_INTERFACE:${DiligentEngine_SOURCE_DIR}/DiligentTools>)
    endif()
    if(UNIX AND NOT APPLE)
        target_compile_definitions(karma_extras PRIVATE PLATFORM_LINUX=1)
    endif()
endif()
if(KARMA_UI_BACKEND STREQUAL "imgui")
    target_link_libraries(karma_extras PUBLIC
        imgui::imgui
        Freetype::Freetype
        harfbuzz::harfbuzz
        PkgConfig::LIBRSVG
    )
    if(UNIX AND NOT APPLE)
        target_link_libraries(karma_extras PUBLIC Fontconfig::Fontconfig)
    endif()
endif()
if(KARMA_UI_BACKEND STREQUAL "rmlui")
    target_link_libraries(karma_extras PUBLIC RmlUi::RmlUi md4c::md4c)
endif()

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
