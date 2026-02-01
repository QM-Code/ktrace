set(ENGINE_COMMON_SOURCES
    ${PROJECT_SOURCE_DIR}/src/engine/common/curl_global.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/config_helpers.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/config_store.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/config_validation.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/i18n.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/data_path_resolver.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/data_dir_override.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/file_utils.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/stb_image_impl.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/world_content.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/world_archive.cpp
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

if(PROJECT_VERSION)
    set(KARMA_VERSION "${PROJECT_VERSION}")
else()
    set(KARMA_VERSION "0.0.0")
endif()

## Separate engine sources for client and server to avoid over-linking
set(ENGINE_CLIENT_SOURCES
    ${PROJECT_SOURCE_DIR}/src/engine/app/engine_app.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/geometry/mesh_loader.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/enet_transport.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/transport_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/graphics/device.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/graphics/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/input/input.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/input/mapping/binding.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/input/mapping/map.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/input/mapping/mapper.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/input/bindings_text.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/platform/window_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/audio/audio.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/audio/backend_factory.cpp
    ${ENGINE_COMMON_SOURCES}
)
set(ENGINE_SERVER_SOURCES
    ${PROJECT_SOURCE_DIR}/src/engine/app/engine_app.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/geometry/mesh_loader.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/enet_transport.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/transport_factory.cpp
    ${ENGINE_COMMON_SOURCES}
)

if(KARMA_WINDOW_BACKEND STREQUAL "sdl3")
    list(APPEND ENGINE_CLIENT_SOURCES ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_sdl.cpp)
elseif(KARMA_WINDOW_BACKEND STREQUAL "sdl2")
    list(APPEND ENGINE_CLIENT_SOURCES ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_sdl2_stub.cpp)
endif()

if(KARMA_UI_BACKEND STREQUAL "imgui")
    if(KARMA_RENDER_BACKEND STREQUAL "bgfx")
        list(APPEND ENGINE_CLIENT_SOURCES
        )
    elseif(KARMA_RENDER_BACKEND STREQUAL "diligent")
        list(APPEND ENGINE_CLIENT_SOURCES
        )
    endif()
elseif(KARMA_UI_BACKEND STREQUAL "rmlui")
    if(KARMA_RENDER_BACKEND STREQUAL "bgfx")
        list(APPEND ENGINE_CLIENT_SOURCES
        )
    elseif(KARMA_RENDER_BACKEND STREQUAL "diligent")
        list(APPEND ENGINE_CLIENT_SOURCES
        )
    endif()
endif()

set(PHYSICS_SOURCES
    ${PROJECT_SOURCE_DIR}/src/engine/physics/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/physics_world.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/rigid_body.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/static_body.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/player_controller.cpp
)
if(KARMA_PHYSICS_BACKEND STREQUAL "jolt")
    list(APPEND PHYSICS_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/jolt/physics_world_jolt.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/jolt/rigid_body_jolt.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/jolt/static_body_jolt.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/jolt/player_controller_jolt.cpp
    )
elseif(KARMA_PHYSICS_BACKEND STREQUAL "physx")
    list(APPEND PHYSICS_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/physx/physics_world_physx.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/physx/rigid_body_physx.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/physx/static_body_physx.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/physx/player_controller_physx.cpp
    )
endif()

list(APPEND ENGINE_CLIENT_SOURCES ${PHYSICS_SOURCES})
list(APPEND ENGINE_SERVER_SOURCES ${PHYSICS_SOURCES})

if(KARMA_AUDIO_BACKEND STREQUAL "miniaudio")
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/audio/backends/miniaudio/backend.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/audio/backends/miniaudio/clip.cpp
    )
elseif(KARMA_AUDIO_BACKEND STREQUAL "sdlaudio")
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/audio/backends/sdl/backend.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/audio/backends/sdl/clip.cpp
    )
endif()

if(KARMA_RENDER_BACKEND STREQUAL "diligent")
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/graphics/backends/diligent/backend.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/graphics/backends/diligent/ui_bridge.cpp
    )
elseif(KARMA_RENDER_BACKEND STREQUAL "bgfx")
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/graphics/backends/bgfx/backend.cpp
    )
endif()

# Create engine libraries
add_library(karma STATIC ${ENGINE_CLIENT_SOURCES})
add_library(karma_server STATIC ${ENGINE_SERVER_SOURCES})
target_compile_definitions(karma_server PRIVATE KARMA_SERVER)

target_include_directories(karma PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/engine>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(karma PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${enet_SOURCE_DIR}/include
)
target_include_directories(karma_server PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/engine>
    $<INSTALL_INTERFACE:include>
)
target_include_directories(karma_server PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${enet_SOURCE_DIR}/include
)

target_link_libraries(karma PUBLIC
    ${WINDOW_TARGET}
    glm::glm
    enet_static
    assimp::assimp
    spdlog::spdlog
    miniz::miniz
    nlohmann_json::nlohmann_json
    CURL::libcurl
    OpenSSL::Crypto
)
target_link_libraries(karma_server PRIVATE
    glm::glm
    enet_static
    assimp::assimp
    spdlog::spdlog
    miniz::miniz
    nlohmann_json::nlohmann_json
    CURL::libcurl
)

if(KARMA_RENDER_BACKEND STREQUAL "diligent")
    target_link_libraries(karma PUBLIC $<BUILD_INTERFACE:Diligent-GraphicsEngineVk-static>)
    if(TARGET Diligent-Common)
        target_link_libraries(karma PUBLIC $<BUILD_INTERFACE:Diligent-Common>)
    endif()
    if(TARGET Diligent-GraphicsEngine)
        target_link_libraries(karma PUBLIC $<BUILD_INTERFACE:Diligent-GraphicsEngine>)
    endif()
    if(TARGET Diligent-GraphicsEngineNextGenBase)
        target_link_libraries(karma PUBLIC $<BUILD_INTERFACE:Diligent-GraphicsEngineNextGenBase>)
    endif()
    if(TARGET Diligent-GraphicsAccessories)
        target_link_libraries(karma PUBLIC $<BUILD_INTERFACE:Diligent-GraphicsAccessories>)
    endif()
    if(TARGET Diligent-ShaderTools)
        target_link_libraries(karma PUBLIC $<BUILD_INTERFACE:Diligent-ShaderTools>)
    endif()
    if(TARGET Diligent-PlatformInterface)
        target_link_libraries(karma PUBLIC $<BUILD_INTERFACE:Diligent-PlatformInterface>)
    endif()
    if(TARGET Diligent-TargetPlatform)
        target_link_libraries(karma PUBLIC $<BUILD_INTERFACE:Diligent-TargetPlatform>)
    endif()
    if(DEFINED DiligentEngine_SOURCE_DIR)
        target_include_directories(karma PRIVATE ${DiligentEngine_SOURCE_DIR})
        target_include_directories(karma PRIVATE ${DiligentEngine_SOURCE_DIR}/DiligentCore)
        target_include_directories(karma PRIVATE ${DiligentEngine_SOURCE_DIR}/DiligentTools)
    endif()
    if(UNIX AND NOT APPLE)
        target_compile_definitions(karma PRIVATE PLATFORM_LINUX=1)
    endif()
elseif(KARMA_RENDER_BACKEND STREQUAL "bgfx")
    target_link_libraries(karma PUBLIC bgfx::bgfx)
endif()
if(KARMA_PHYSICS_BACKEND STREQUAL "jolt")
    target_link_libraries(karma PUBLIC Jolt::Jolt)
    target_link_libraries(karma_server PUBLIC Jolt::Jolt)
elseif(KARMA_PHYSICS_BACKEND STREQUAL "physx")
    target_link_libraries(karma PUBLIC unofficial::omniverse-physx-sdk::sdk)
    target_link_libraries(karma PUBLIC
        unofficial::omniverse-physx-sdk::PhysXCharacterKinematic
        unofficial::omniverse-physx-sdk::PhysXExtensions
    )
    target_link_libraries(karma_server PUBLIC unofficial::omniverse-physx-sdk::sdk)
    target_link_libraries(karma_server PUBLIC
        unofficial::omniverse-physx-sdk::PhysXCharacterKinematic
        unofficial::omniverse-physx-sdk::PhysXExtensions
    )
endif()
if(KARMA_AUDIO_BACKEND STREQUAL "sdlaudio")
    target_link_libraries(karma PUBLIC SDL3::SDL3)
endif()

# Install Karma headers and libraries (standalone engine boundary).
install(TARGETS karma karma_server
    EXPORT karmaTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/karma
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(EXPORT karmaTargets
    NAMESPACE karma::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/karma
)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/karma-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/karma-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/karma
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/karma-config-version.cmake
    VERSION ${KARMA_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/karma-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/karma-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/karma
)
