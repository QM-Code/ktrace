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
    ${PROJECT_SOURCE_DIR}/src/engine/common/content/archive.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/content/catalog.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/content/primitives.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/simulation_clock.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/stb_image_impl.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/world_archive.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/world_content.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/app/backend_resolution.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/app/bootstrap_scaffold.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/app/server_bootstrap_runner.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/app/server_runner.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/cli/cli_parse_scaffold.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/cli/client_runtime_options.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/cli/server_app_options.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/cli/server_runtime_options.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/app/engine_server_app.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/client_reconnect_policy.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/community_heartbeat.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/heartbeat_client.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/server_join_runtime.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/server_preauth.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/server_session_hooks.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/server_session_runtime.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/client_transport.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/server_transport.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/transport_config_mapping.cpp
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
    ${PROJECT_SOURCE_DIR}/src/engine/app/backend_resolution_client.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/app/client_bootstrap_runner.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/app/client_runner.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/cli/cli_client_backend_options.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/cli/client_app_options.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/app/engine_app.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/input/input_system.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/backends/software/backend.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/backends/imgui/backend.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/backends/imgui/input.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/backends/imgui/rasterizer.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/backends/imgui/stub.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/backends/rmlui/backend.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/backends/rmlui/adapter.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/backends/rmlui/input.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/backends/rmlui/markup.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/backends/rmlui/cpu_renderer.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/backends/rmlui/system_interface.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/backends/rmlui/stub.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/ui/system.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/geometry/mesh_loader.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/scene/roaming_camera.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/scene/scene_bootstrap.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/platform/window_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_sdl3.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/renderer/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/renderer/device.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/renderer/render_system.cpp
)

if(KARMA_RENDER_BACKEND_BGFX_ENABLED)
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/renderer/backends/bgfx/core.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/renderer/backends/bgfx/assets.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/renderer/backends/bgfx/textures.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/renderer/backends/bgfx/pipeline.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/renderer/backends/bgfx/contracts.cpp
    )
endif()

if(KARMA_RENDER_BACKEND_DILIGENT_ENABLED)
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/renderer/backends/diligent/core.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/renderer/backends/diligent/assets.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/renderer/backends/diligent/textures.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/renderer/backends/diligent/pipeline.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/renderer/backends/diligent/contracts.cpp
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
    OpenSSL::Crypto
)
if(TARGET enet_static)
    target_link_libraries(karma_engine_core PUBLIC enet_static)
elseif(TARGET enet)
    target_link_libraries(karma_engine_core PUBLIC enet)
endif()
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
    find_package(unofficial-omniverse-physx-sdk CONFIG REQUIRED)
    set(KARMA_PHYSX_LIBS
        unofficial::omniverse-physx-sdk::sdk
        unofficial::omniverse-physx-sdk::PhysXCharacterKinematic
        unofficial::omniverse-physx-sdk::PhysXExtensions
        unofficial::omniverse-physx-sdk::PhysXCooking
        unofficial::omniverse-physx-sdk::PhysXCommon
        unofficial::omniverse-physx-sdk::PhysXFoundation
        unofficial::omniverse-physx-sdk::PhysXPvdSDK
        unofficial::omniverse-physx-sdk::PhysXVehicle
    )
    if(UNIX AND NOT APPLE)
        target_link_libraries(karma_engine_core PUBLIC "-Wl,--start-group" ${KARMA_PHYSX_LIBS} "-Wl,--end-group")
    else()
        target_link_libraries(karma_engine_core PUBLIC ${KARMA_PHYSX_LIBS})
    endif()
    target_compile_definitions(karma_engine_core PRIVATE KARMA_HAS_PHYSICS_PHYSX)
endif()

if(KARMA_AUDIO_BACKEND_SDL3AUDIO_ENABLED)
    target_link_libraries(karma_engine_core PUBLIC SDL3::SDL3)
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

add_executable(renderer_shadow_sandbox
    ${PROJECT_SOURCE_DIR}/src/engine/renderer/tests/renderer_shadow_sandbox.cpp
)
target_link_libraries(renderer_shadow_sandbox PRIVATE karma_engine_client)
target_include_directories(renderer_shadow_sandbox PRIVATE ${PROJECT_SOURCE_DIR}/src/engine)
set_target_properties(renderer_shadow_sandbox PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/src/engine
)

if(BUILD_TESTING)
    add_library(network_test_support STATIC
        ${PROJECT_SOURCE_DIR}/src/engine/network/tests/loopback_endpoint_alloc.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/network/tests/loopback_enet_fixture.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/network/tests/structured_log_event_sink.cpp
    )
    target_include_directories(network_test_support PRIVATE ${PROJECT_SOURCE_DIR}/src/engine)
    target_link_libraries(network_test_support PRIVATE spdlog::spdlog)
    if(TARGET enet_static)
        target_link_libraries(network_test_support PRIVATE enet_static)
    elseif(TARGET enet)
        target_link_libraries(network_test_support PRIVATE enet)
    endif()

    add_executable(client_transport_contract_test
        ${PROJECT_SOURCE_DIR}/src/engine/network/tests/client_transport_contract_test.cpp
    )
    target_link_libraries(client_transport_contract_test PRIVATE
        karma_engine_core
        network_test_support
    )
    target_include_directories(client_transport_contract_test PRIVATE ${PROJECT_SOURCE_DIR}/src/engine)

    add_executable(server_transport_contract_test
        ${PROJECT_SOURCE_DIR}/src/engine/network/tests/server_transport_contract_test.cpp
    )
    target_link_libraries(server_transport_contract_test PRIVATE
        karma_engine_core
        network_test_support
    )
    target_include_directories(server_transport_contract_test PRIVATE ${PROJECT_SOURCE_DIR}/src/engine)

    add_executable(directional_shadow_contract_test
        ${PROJECT_SOURCE_DIR}/src/engine/renderer/tests/directional_shadow_contract_test.cpp
    )
    target_link_libraries(directional_shadow_contract_test PRIVATE karma_engine_core)
    target_include_directories(directional_shadow_contract_test PRIVATE ${PROJECT_SOURCE_DIR}/src/engine)

    add_executable(physics_backend_parity_test
        ${PROJECT_SOURCE_DIR}/src/engine/physics/tests/physics_backend_parity_test.cpp
    )
    target_link_libraries(physics_backend_parity_test PRIVATE karma_engine_core)
    target_include_directories(physics_backend_parity_test PRIVATE ${PROJECT_SOURCE_DIR}/src/engine)

    add_executable(audio_backend_smoke_test
        ${PROJECT_SOURCE_DIR}/src/engine/audio/tests/audio_backend_smoke_test.cpp
    )
    target_link_libraries(audio_backend_smoke_test PRIVATE karma_engine_core)
    target_include_directories(audio_backend_smoke_test PRIVATE ${PROJECT_SOURCE_DIR}/src/engine)

    if(KARMA_PHYSICS_BACKEND_JOLT_ENABLED)
        add_test(NAME physics_backend_parity_jolt
                 COMMAND physics_backend_parity_test --backend jolt)
    endif()

    if(KARMA_PHYSICS_BACKEND_PHYSX_ENABLED)
        add_test(NAME physics_backend_parity_physx
                 COMMAND physics_backend_parity_test --backend physx)
    endif()

    if(KARMA_AUDIO_BACKEND_SDL3AUDIO_ENABLED)
        add_test(NAME audio_backend_smoke_sdl3audio
                 COMMAND audio_backend_smoke_test --backend sdl3audio)
        set_tests_properties(audio_backend_smoke_sdl3audio PROPERTIES
                             ENVIRONMENT "SDL_AUDIODRIVER=dummy")
    endif()

    if(KARMA_AUDIO_BACKEND_MINIAUDIO_ENABLED)
        add_test(NAME audio_backend_smoke_miniaudio
                 COMMAND audio_backend_smoke_test --backend miniaudio)
        set_tests_properties(audio_backend_smoke_miniaudio PROPERTIES
                             ENVIRONMENT "KARMA_MINIAUDIO_FORCE_NULL=1;SDL_AUDIODRIVER=dummy")
    endif()

    add_test(NAME client_transport_contract_test
             COMMAND client_transport_contract_test)
    add_test(NAME server_transport_contract_test
             COMMAND server_transport_contract_test)

    add_test(NAME renderer_directional_shadow_contract
             COMMAND directional_shadow_contract_test)
endif()
