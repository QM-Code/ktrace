set(ENGINE_SOURCES
    ${PROJECT_SOURCE_DIR}/src/engine/common/config_helpers.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/config_store.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/config_validation.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/curl_global.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/data_dir_override.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/data_path_resolver.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/file_utils.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/i18n.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/logging.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/stb_image_impl.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/world_archive.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/world_content.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/app/engine_app.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/input/input_system.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/physics_system.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/ui_system.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/geometry/mesh_loader.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/scene/roaming_camera.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/platform/window_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_sdl3.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_sdl2_stub.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_glfw_stub.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/renderer/device.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/renderer/render_system.cpp
)

if(KARMA_RENDER_BACKEND STREQUAL "bgfx")
    list(APPEND ENGINE_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/renderer/backends/bgfx/backend_bgfx.cpp
    )
elseif(KARMA_RENDER_BACKEND STREQUAL "diligent")
    list(APPEND ENGINE_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/renderer/backends/diligent/backend_diligent.cpp
    )
endif()

add_library(karma_engine STATIC ${ENGINE_SOURCES})

target_include_directories(karma_engine PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/engine
)

target_link_libraries(karma_engine PUBLIC
    glm::glm
    assimp::assimp
    spdlog::spdlog
    miniz::miniz
    nlohmann_json::nlohmann_json
    CURL::libcurl
    stb::stb
)

if(X11_FOUND)
    if(DEFINED X11_INCLUDE_DIR)
        target_include_directories(karma_engine PUBLIC ${X11_INCLUDE_DIR})
    endif()
    if(DEFINED X11_LIBRARIES)
        target_link_libraries(karma_engine PUBLIC ${X11_LIBRARIES})
    endif()
    if(DEFINED X11_XCB_LIB)
        target_link_libraries(karma_engine PUBLIC ${X11_XCB_LIB})
        target_compile_definitions(karma_engine PUBLIC KARMA_HAS_X11_XCB)
    elseif(DEFINED X11_X11_xcb_LIB)
        target_link_libraries(karma_engine PUBLIC ${X11_X11_xcb_LIB})
        target_compile_definitions(karma_engine PUBLIC KARMA_HAS_X11_XCB)
    elseif(TARGET X11::XCB)
        target_link_libraries(karma_engine PUBLIC X11::XCB)
        target_compile_definitions(karma_engine PUBLIC KARMA_HAS_X11_XCB)
    endif()
endif()

if(KARMA_WINDOW_BACKEND STREQUAL "sdl3")
    target_link_libraries(karma_engine PUBLIC SDL3::SDL3)
endif()

if(KARMA_RENDER_BACKEND STREQUAL "bgfx")
    target_link_libraries(karma_engine PUBLIC bgfx::bgfx)
    add_dependencies(karma_engine bz3_bgfx_shaders)
    if(TARGET bx::bx)
        target_link_libraries(karma_engine PUBLIC bx::bx)
    endif()
    if(TARGET bimg::bimg)
        target_link_libraries(karma_engine PUBLIC bimg::bimg)
    endif()
    if(DEFINED BGFX_FETCHCONTENT_SOURCE_DIR)
        target_include_directories(karma_engine PRIVATE
            ${BGFX_FETCHCONTENT_SOURCE_DIR}/bx/include
            ${BGFX_FETCHCONTENT_SOURCE_DIR}/bimg/include
        )
    endif()
endif()

if(KARMA_RENDER_BACKEND STREQUAL "diligent")
    target_link_libraries(karma_engine PUBLIC Diligent-GraphicsEngineVk-static)
    if(TARGET Diligent-Common)
        target_link_libraries(karma_engine PUBLIC Diligent-Common)
    endif()
    if(TARGET Diligent-GraphicsEngine)
        target_link_libraries(karma_engine PUBLIC Diligent-GraphicsEngine)
    endif()
    if(TARGET Diligent-GraphicsEngineNextGenBase)
        target_link_libraries(karma_engine PUBLIC Diligent-GraphicsEngineNextGenBase)
    endif()
    if(TARGET Diligent-GraphicsAccessories)
        target_link_libraries(karma_engine PUBLIC Diligent-GraphicsAccessories)
    endif()
    if(TARGET Diligent-ShaderTools)
        target_link_libraries(karma_engine PUBLIC Diligent-ShaderTools)
    endif()
    if(TARGET Diligent-PlatformInterface)
        target_link_libraries(karma_engine PUBLIC Diligent-PlatformInterface)
    endif()
    if(TARGET Diligent-TargetPlatform)
        target_link_libraries(karma_engine PUBLIC Diligent-TargetPlatform)
    endif()
    if(DEFINED DiligentEngine_SOURCE_DIR)
        target_include_directories(karma_engine PRIVATE ${DiligentEngine_SOURCE_DIR})
        target_include_directories(karma_engine PRIVATE ${DiligentEngine_SOURCE_DIR}/DiligentCore)
        target_include_directories(karma_engine PRIVATE ${DiligentEngine_SOURCE_DIR}/DiligentTools)
    endif()
endif()
