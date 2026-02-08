set(ENGINE_CORE_SOURCES
    ${PROJECT_SOURCE_DIR}/src/engine/common/config_helpers.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/config_store.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/config_validation.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/curl_global.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/data_dir_override.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/data_path_resolver.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/file_utils.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/i18n.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/logging.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/simulation_clock.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/stb_image_impl.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/world_archive.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/world_content.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/app/engine_server_app.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/audio/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/audio/audio_system.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/audio/backends/backend_sdl3audio_stub.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/audio/backends/backend_miniaudio_stub.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/physics_system.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/backend_jolt_stub.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/backend_physx_stub.cpp
)

set(ENGINE_CLIENT_SOURCES
    ${PROJECT_SOURCE_DIR}/src/engine/app/engine_app.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/input/input_system.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/ui_backend.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/ui_backend_imgui.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/ui_backend_rmlui.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/ui_rmlui_adapter_rmlui.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/ui_rmlui_adapter_stub.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/ui_system.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/geometry/mesh_loader.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/scene/roaming_camera.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/scene/scene_bootstrap.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/platform/window_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_sdl3.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_sdl2_stub.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_glfw_stub.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/renderer/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/renderer/device.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/renderer/render_system.cpp
)

if(KARMA_RENDER_BACKEND_BGFX_ENABLED)
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/renderer/backends/bgfx/backend_bgfx.cpp
    )
endif()

if(KARMA_RENDER_BACKEND_DILIGENT_ENABLED)
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/renderer/backends/diligent/backend_diligent.cpp
    )
endif()

add_library(karma_engine_core STATIC ${ENGINE_CORE_SOURCES})
add_library(karma_engine_client STATIC ${ENGINE_CLIENT_SOURCES})
add_library(karma_engine ALIAS karma_engine_client)
add_library(karma::engine_core ALIAS karma_engine_core)
add_library(karma::engine_client ALIAS karma_engine_client)

set_target_properties(karma_engine_core PROPERTIES EXPORT_NAME engine_core)
set_target_properties(karma_engine_client PROPERTIES EXPORT_NAME engine_client)

target_include_directories(karma_engine_core PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(karma_engine_core PRIVATE
    ${PROJECT_SOURCE_DIR}/src/engine
)

target_link_libraries(karma_engine_core PUBLIC
    glm::glm
    assimp::assimp
    spdlog::spdlog
    miniz::miniz
    nlohmann_json::nlohmann_json
    CURL::libcurl
)
if(TARGET stb::stb)
    get_target_property(_KARMA_STB_INCLUDE_DIRS stb::stb INTERFACE_INCLUDE_DIRECTORIES)
    if(_KARMA_STB_INCLUDE_DIRS)
        target_include_directories(karma_engine_core PRIVATE ${_KARMA_STB_INCLUDE_DIRS})
    endif()
endif()

target_include_directories(karma_engine_client PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(karma_engine_client PRIVATE
    ${PROJECT_SOURCE_DIR}/src/engine
)

target_link_libraries(karma_engine_client PUBLIC
    karma_engine_core
)

if(KARMA_RENDER_BACKEND_BGFX_ENABLED)
    target_compile_definitions(karma_engine_client PRIVATE KARMA_HAS_RENDER_BGFX)
endif()

if(KARMA_RENDER_BACKEND_DILIGENT_ENABLED)
    target_compile_definitions(karma_engine_client PRIVATE KARMA_HAS_RENDER_DILIGENT)
endif()

if(KARMA_PHYSICS_BACKEND_JOLT_ENABLED)
    find_package(Jolt CONFIG REQUIRED)
    target_link_libraries(karma_engine_core PUBLIC Jolt::Jolt)
    target_compile_definitions(karma_engine_core PRIVATE KARMA_HAS_PHYSICS_JOLT)
endif()

if(KARMA_PHYSICS_BACKEND_PHYSX_ENABLED)
    target_compile_definitions(karma_engine_core PRIVATE KARMA_HAS_PHYSICS_PHYSX)
endif()

if(KARMA_AUDIO_BACKEND_SDL3AUDIO_ENABLED)
    target_compile_definitions(karma_engine_core PRIVATE KARMA_HAS_AUDIO_SDL3AUDIO)
endif()

if(KARMA_AUDIO_BACKEND_MINIAUDIO_ENABLED)
    target_compile_definitions(karma_engine_core PRIVATE KARMA_HAS_AUDIO_MINIAUDIO)
endif()

find_package(imgui CONFIG QUIET)
set(KARMA_ENGINE_HAS_IMGUI OFF)
if(TARGET imgui::imgui)
    target_link_libraries(karma_engine_client PUBLIC imgui::imgui)
    target_compile_definitions(karma_engine_client PRIVATE KARMA_HAS_IMGUI)
    set(KARMA_ENGINE_HAS_IMGUI ON)
endif()
set(KARMA_ENGINE_HAS_IMGUI ${KARMA_ENGINE_HAS_IMGUI} PARENT_SCOPE)

set(KARMA_ENGINE_HAS_RMLUI OFF)
if(KARMA_ENABLE_RMLUI_BACKEND)
    find_package(RmlUi CONFIG REQUIRED)
    target_link_libraries(karma_engine_client PUBLIC RmlUi::RmlUi)
    target_compile_definitions(karma_engine_client PRIVATE KARMA_HAS_RMLUI)
    set(KARMA_ENGINE_HAS_RMLUI ON)
endif()
set(KARMA_ENGINE_HAS_RMLUI ${KARMA_ENGINE_HAS_RMLUI} PARENT_SCOPE)

if(X11_FOUND)
    if(DEFINED X11_INCLUDE_DIR)
        target_include_directories(karma_engine_client PRIVATE ${X11_INCLUDE_DIR})
    endif()
    if(DEFINED X11_LIBRARIES)
        target_link_libraries(karma_engine_client PUBLIC ${X11_LIBRARIES})
    endif()
    if(DEFINED X11_XCB_LIB)
        target_link_libraries(karma_engine_client PUBLIC ${X11_XCB_LIB})
        target_compile_definitions(karma_engine_client PRIVATE KARMA_HAS_X11_XCB)
    elseif(DEFINED X11_X11_xcb_LIB)
        target_link_libraries(karma_engine_client PUBLIC ${X11_X11_xcb_LIB})
        target_compile_definitions(karma_engine_client PRIVATE KARMA_HAS_X11_XCB)
    elseif(TARGET X11::XCB)
        target_link_libraries(karma_engine_client PUBLIC X11::XCB)
        target_compile_definitions(karma_engine_client PRIVATE KARMA_HAS_X11_XCB)
    endif()
endif()

if(KARMA_WINDOW_BACKEND STREQUAL "sdl3")
    target_link_libraries(karma_engine_client PUBLIC SDL3::SDL3)
endif()

if(KARMA_RENDER_BACKEND_BGFX_ENABLED)
    target_link_libraries(karma_engine_client PUBLIC bgfx::bgfx)
    add_dependencies(karma_engine_client bz3_bgfx_shaders)
    if(TARGET bx::bx)
        target_link_libraries(karma_engine_client PUBLIC bx::bx)
    endif()
    if(TARGET bimg::bimg)
        target_link_libraries(karma_engine_client PUBLIC bimg::bimg)
    endif()
    if(DEFINED BGFX_FETCHCONTENT_SOURCE_DIR)
        target_include_directories(karma_engine_client PRIVATE
            ${BGFX_FETCHCONTENT_SOURCE_DIR}/bx/include
            ${BGFX_FETCHCONTENT_SOURCE_DIR}/bimg/include
        )
    endif()
endif()

if(KARMA_RENDER_BACKEND_DILIGENT_ENABLED)
    target_link_libraries(karma_engine_client PUBLIC Diligent-GraphicsEngineVk-static)
    if(TARGET Diligent-Common)
        target_link_libraries(karma_engine_client PUBLIC Diligent-Common)
    endif()
    if(TARGET Diligent-GraphicsEngine)
        target_link_libraries(karma_engine_client PUBLIC Diligent-GraphicsEngine)
    endif()
    if(TARGET Diligent-GraphicsEngineNextGenBase)
        target_link_libraries(karma_engine_client PUBLIC Diligent-GraphicsEngineNextGenBase)
    endif()
    if(TARGET Diligent-GraphicsAccessories)
        target_link_libraries(karma_engine_client PUBLIC Diligent-GraphicsAccessories)
    endif()
    if(TARGET Diligent-ShaderTools)
        target_link_libraries(karma_engine_client PUBLIC Diligent-ShaderTools)
    endif()
    if(TARGET Diligent-PlatformInterface)
        target_link_libraries(karma_engine_client PUBLIC Diligent-PlatformInterface)
    endif()
    if(TARGET Diligent-TargetPlatform)
        target_link_libraries(karma_engine_client PUBLIC Diligent-TargetPlatform)
    endif()
    if(DEFINED DiligentEngine_SOURCE_DIR)
        target_include_directories(karma_engine_client PRIVATE ${DiligentEngine_SOURCE_DIR})
        target_include_directories(karma_engine_client PRIVATE ${DiligentEngine_SOURCE_DIR}/DiligentCore)
        target_include_directories(karma_engine_client PRIVATE ${DiligentEngine_SOURCE_DIR}/DiligentTools)
    endif()
endif()
