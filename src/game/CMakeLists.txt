file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/game/client/*.cpp")
list(APPEND CLIENT_SOURCES
    ${PROJECT_SOURCE_DIR}/src/game/client/actor.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/server/community_browser_controller.cpp
)
file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/game/server/*.cpp")
list(APPEND SERVER_SOURCES
    ${PROJECT_SOURCE_DIR}/src/game/server/terminal_commands.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/server_cli_options.cpp
)

option(KARMA_BUILD_ENGINEAPP_SAMPLE "Build EngineApp sample executable" OFF)

set(GAME_CLIENT_SOURCES
    ${PROJECT_SOURCE_DIR}/src/game/engine/client_engine.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/client_network.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/server_network.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/proto_codec.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/backends/enet/client_backend.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/backends/enet/server_backend.cpp
    ${PROJECT_SOURCE_DIR}/src/game/world/config.cpp
    ${PROJECT_SOURCE_DIR}/src/game/common/data_path_spec.cpp
    ${PROJECT_SOURCE_DIR}/src/game/renderer/renderer.cpp
    ${PROJECT_SOURCE_DIR}/src/game/renderer/radar_renderer.cpp
    ${PROJECT_SOURCE_DIR}/src/game/input/bindings.cpp
    ${PROJECT_SOURCE_DIR}/src/game/input/state.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/core/system.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/core/validation.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/core/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/config/config.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/config/ui_config.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/config/input_mapping.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/config/hud_settings.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/config/render_settings.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/config/render_scale.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/controllers/console_controller.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/controllers/hud_controller.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/controllers/bindings_controller.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/controllers/settings_controller.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/console/keybindings.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/fonts/console_fonts.cpp
)
set(GAME_SERVER_SOURCES
    ${PROJECT_SOURCE_DIR}/src/game/engine/server_engine.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/client_network.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/server_network.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/proto_codec.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/backends/enet/client_backend.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/backends/enet/server_backend.cpp
    ${PROJECT_SOURCE_DIR}/src/game/world/config.cpp
    ${PROJECT_SOURCE_DIR}/src/game/common/data_path_spec.cpp
)

if(KARMA_UI_BACKEND STREQUAL "imgui")
    list(APPEND GAME_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/backend.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/console/console.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/console/thumbnail_cache.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/console/panels/panel_community.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/console/panels/panel_settings.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/console/panels/panel_bindings.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/console/panels/panel_documentation.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/console/panels/panel_start_server.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/hud/hud.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/hud/chat.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/hud/crosshair.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/hud/fps.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/hud/radar.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/hud/scoreboard.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/hud/dialog.cpp
    )
elseif(KARMA_UI_BACKEND STREQUAL "rmlui")
    list(APPEND GAME_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/backend.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/emoji_utils.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/console.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/modal_dialog.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/hud/hud.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/hud/chat.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/hud/crosshair.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/hud/dialog.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/hud/radar.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/hud/scoreboard.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/translate.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/panels/panel.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/panels/panel_community.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/panels/panel_settings.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/panels/panel_bindings.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/panels/panel_documentation.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/panels/panel_start_server.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/hud/quick_menu.cpp
    )
endif()

set(MACOSX_BUNDLE_ICON_FILE "") # Set to your .icns file if you have one
add_executable(bz3 MACOSX_BUNDLE
    ${CLIENT_SOURCES}
    ${GAME_CLIENT_SOURCES}
)

add_executable(bz3-server
    ${SERVER_SOURCES}
    ${GAME_SERVER_SOURCES}
)

if(KARMA_BUILD_ENGINEAPP_SAMPLE)
    add_executable(bz3-engineapp-sample
        ${PROJECT_SOURCE_DIR}/examples/engine_app_sample.cpp
    )
    set_target_properties(bz3-engineapp-sample PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
    target_include_directories(bz3-engineapp-sample PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/src/engine
        ${enet_SOURCE_DIR}/include
    )
    target_link_libraries(bz3-engineapp-sample PRIVATE
        karma
        ${WINDOW_TARGET}
        glm::glm
        enet_static
        assimp::assimp
        cxxopts::cxxopts
        spdlog::spdlog
        miniz::miniz
        nlohmann_json::nlohmann_json
        proto_lib
        CURL::libcurl
        OpenSSL::Crypto
    )
    if(KARMA_RENDER_BACKEND STREQUAL "diligent")
        target_link_libraries(bz3-engineapp-sample PRIVATE Diligent-GraphicsEngineVk-static)
        if(TARGET Diligent-Common)
            target_link_libraries(bz3-engineapp-sample PRIVATE Diligent-Common)
        endif()
        if(TARGET Diligent-GraphicsEngine)
            target_link_libraries(bz3-engineapp-sample PRIVATE Diligent-GraphicsEngine)
        endif()
        if(TARGET Diligent-GraphicsEngineNextGenBase)
            target_link_libraries(bz3-engineapp-sample PRIVATE Diligent-GraphicsEngineNextGenBase)
        endif()
        if(TARGET Diligent-GraphicsAccessories)
            target_link_libraries(bz3-engineapp-sample PRIVATE Diligent-GraphicsAccessories)
        endif()
        if(TARGET Diligent-ShaderTools)
            target_link_libraries(bz3-engineapp-sample PRIVATE Diligent-ShaderTools)
        endif()
        if(TARGET Diligent-PlatformInterface)
            target_link_libraries(bz3-engineapp-sample PRIVATE Diligent-PlatformInterface)
        endif()
        if(TARGET Diligent-TargetPlatform)
            target_link_libraries(bz3-engineapp-sample PRIVATE Diligent-TargetPlatform)
        endif()
        if(DEFINED DiligentEngine_SOURCE_DIR)
            target_include_directories(bz3-engineapp-sample PRIVATE ${DiligentEngine_SOURCE_DIR})
            target_include_directories(bz3-engineapp-sample PRIVATE ${DiligentEngine_SOURCE_DIR}/DiligentCore)
            target_include_directories(bz3-engineapp-sample PRIVATE ${DiligentEngine_SOURCE_DIR}/DiligentTools)
        endif()
        if(UNIX AND NOT APPLE)
            target_compile_definitions(bz3-engineapp-sample PRIVATE PLATFORM_LINUX=1)
        endif()
    elseif(KARMA_RENDER_BACKEND STREQUAL "bgfx")
        target_link_libraries(bz3-engineapp-sample PRIVATE bgfx::bgfx)
        add_dependencies(bz3-engineapp-sample bz3_bgfx_shaders)
    endif()
    if(KARMA_PHYSICS_BACKEND STREQUAL "jolt")
        target_link_libraries(bz3-engineapp-sample PRIVATE Jolt::Jolt)
    elseif(KARMA_PHYSICS_BACKEND STREQUAL "physx")
        target_link_libraries(bz3-engineapp-sample PRIVATE unofficial::omniverse-physx-sdk::sdk)
        target_link_libraries(bz3-engineapp-sample PRIVATE
            unofficial::omniverse-physx-sdk::PhysXCharacterKinematic
            unofficial::omniverse-physx-sdk::PhysXExtensions
        )
    endif()
    if(KARMA_UI_BACKEND STREQUAL "imgui")
        target_link_libraries(bz3-engineapp-sample PRIVATE
            imgui::imgui
            Freetype::Freetype
            harfbuzz::harfbuzz
            PkgConfig::LIBRSVG
        )
        target_compile_definitions(bz3-engineapp-sample PRIVATE KARMA_HAS_LIBRSVG IMGUI_USE_WCHAR32)
        if(UNIX AND NOT APPLE)
            target_link_libraries(bz3-engineapp-sample PRIVATE Fontconfig::Fontconfig)
        endif()
        if(TARGET imgui::imgui)
            target_compile_definitions(imgui::imgui INTERFACE IMGUI_USE_WCHAR32)
        endif()
    endif()

    if(KARMA_UI_BACKEND STREQUAL "rmlui")
        target_link_libraries(bz3-engineapp-sample PRIVATE RmlUi::RmlUi md4c::md4c)
    endif()

    if(KARMA_AUDIO_BACKEND STREQUAL "sdlaudio")
        target_link_libraries(bz3-engineapp-sample PRIVATE SDL3::SDL3)
    endif()

    if (UNIX AND NOT APPLE)
        target_link_libraries(bz3-engineapp-sample PRIVATE pthread dl m)
    elseif(APPLE)
        find_library(COCOA Cocoa)
        find_library(IOKIT IOKit)
        find_library(COREVIDEO CoreVideo)
        target_link_libraries(bz3-engineapp-sample PRIVATE ${COCOA} ${IOKIT} ${COREVIDEO})
    elseif(WIN32)
        target_link_libraries(bz3-engineapp-sample PRIVATE ws2_32)
    endif()

    target_compile_definitions(bz3-engineapp-sample PRIVATE
        INSTALL_DATA_DIR="${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}/data"
    )
endif()

add_custom_target(check_required_config
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/check_required_config.py
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Checking required config key list"
)
add_dependencies(bz3 check_required_config)
add_dependencies(bz3-server check_required_config)

set_target_properties(bz3 bz3-server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

target_include_directories(bz3 PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/game
)

target_include_directories(bz3-server PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/game
)

# Link libraries
target_link_libraries(bz3 PRIVATE
    karma
    ${WINDOW_TARGET}
    glm::glm
    enet_static
    assimp::assimp
    cxxopts::cxxopts
    spdlog::spdlog
    miniz::miniz
    nlohmann_json::nlohmann_json
    proto_lib
    CURL::libcurl
    OpenSSL::Crypto
)
if(KARMA_RENDER_BACKEND STREQUAL "diligent")
    target_link_libraries(bz3 PRIVATE Diligent-GraphicsEngineVk-static)
    if(TARGET Diligent-Common)
        target_link_libraries(bz3 PRIVATE Diligent-Common)
    endif()
    if(TARGET Diligent-GraphicsEngine)
        target_link_libraries(bz3 PRIVATE Diligent-GraphicsEngine)
    endif()
    if(TARGET Diligent-GraphicsEngineNextGenBase)
        target_link_libraries(bz3 PRIVATE Diligent-GraphicsEngineNextGenBase)
    endif()
    if(TARGET Diligent-GraphicsAccessories)
        target_link_libraries(bz3 PRIVATE Diligent-GraphicsAccessories)
    endif()
    if(TARGET Diligent-ShaderTools)
        target_link_libraries(bz3 PRIVATE Diligent-ShaderTools)
    endif()
    if(TARGET Diligent-PlatformInterface)
        target_link_libraries(bz3 PRIVATE Diligent-PlatformInterface)
    endif()
    if(TARGET Diligent-TargetPlatform)
        target_link_libraries(bz3 PRIVATE Diligent-TargetPlatform)
    endif()
    if(DEFINED DiligentEngine_SOURCE_DIR)
        target_include_directories(bz3 PRIVATE ${DiligentEngine_SOURCE_DIR})
        target_include_directories(bz3 PRIVATE ${DiligentEngine_SOURCE_DIR}/DiligentCore)
        target_include_directories(bz3 PRIVATE ${DiligentEngine_SOURCE_DIR}/DiligentTools)
    endif()
    if(UNIX AND NOT APPLE)
        target_compile_definitions(bz3 PRIVATE PLATFORM_LINUX=1)
    endif()
elseif(KARMA_RENDER_BACKEND STREQUAL "bgfx")
    target_link_libraries(bz3 PRIVATE bgfx::bgfx)
    add_dependencies(bz3 bz3_bgfx_shaders)
endif()
if(KARMA_PHYSICS_BACKEND STREQUAL "jolt")
    target_link_libraries(bz3 PRIVATE Jolt::Jolt)
elseif(KARMA_PHYSICS_BACKEND STREQUAL "physx")
    target_link_libraries(bz3 PRIVATE unofficial::omniverse-physx-sdk::sdk)
    # Ensure static lib link order resolves PhysX character controller deps.
    target_link_libraries(bz3 PRIVATE
        unofficial::omniverse-physx-sdk::PhysXCharacterKinematic
        unofficial::omniverse-physx-sdk::PhysXExtensions
    )
endif()
if(KARMA_UI_BACKEND STREQUAL "imgui")
    target_link_libraries(bz3 PRIVATE
        imgui::imgui
        Freetype::Freetype
        harfbuzz::harfbuzz
        PkgConfig::LIBRSVG
    )
    target_compile_definitions(bz3 PRIVATE KARMA_HAS_LIBRSVG IMGUI_USE_WCHAR32)
    if(UNIX AND NOT APPLE)
        target_link_libraries(bz3 PRIVATE Fontconfig::Fontconfig)
    endif()
    # ImGui: enable full Unicode range (required for emoji codepoints > U+FFFF)
    if(TARGET imgui::imgui)
        target_compile_definitions(imgui::imgui INTERFACE IMGUI_USE_WCHAR32)
    endif()
endif()

if(KARMA_UI_BACKEND STREQUAL "rmlui")
    target_link_libraries(bz3 PRIVATE RmlUi::RmlUi md4c::md4c)
endif()

if(KARMA_AUDIO_BACKEND STREQUAL "sdlaudio")
    target_link_libraries(bz3 PRIVATE SDL3::SDL3)
endif()

target_link_libraries(bz3-server PRIVATE
    karma_server
    glm::glm
    enet_static
    assimp::assimp
    pybind11::embed
    cxxopts::cxxopts
    spdlog::spdlog
    miniz::miniz
    nlohmann_json::nlohmann_json
    proto_lib
    CURL::libcurl
)
if(KARMA_PHYSICS_BACKEND STREQUAL "jolt")
    target_link_libraries(bz3-server PRIVATE Jolt::Jolt)
elseif(KARMA_PHYSICS_BACKEND STREQUAL "physx")
    target_link_libraries(bz3-server PRIVATE unofficial::omniverse-physx-sdk::sdk)
    # Ensure static lib link order resolves PhysX character controller deps.
    target_link_libraries(bz3-server PRIVATE
        unofficial::omniverse-physx-sdk::PhysXCharacterKinematic
        unofficial::omniverse-physx-sdk::PhysXExtensions
    )
endif()

# System libraries (platform-specific)
if (UNIX AND NOT APPLE)
    target_link_libraries(bz3 PRIVATE pthread dl m)
    target_link_libraries(bz3-server PRIVATE pthread dl m)
elseif(APPLE)
    find_library(COCOA Cocoa)
    find_library(IOKIT IOKit)
    find_library(COREVIDEO CoreVideo)
    target_link_libraries(bz3 PRIVATE ${COCOA} ${IOKIT} ${COREVIDEO})
    target_link_libraries(bz3-server PRIVATE ${COCOA})
elseif(WIN32)
    target_link_libraries(bz3 PRIVATE ws2_32)
    target_link_libraries(bz3-server PRIVATE ws2_32)
endif()

# Define install prefix for runtime data lookup
target_compile_definitions(bz3 PRIVATE
    INSTALL_DATA_DIR="${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}/data"
)
target_compile_definitions(bz3-server PRIVATE
    INSTALL_DATA_DIR="${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}/data"
)
target_compile_definitions(bz3-server PRIVATE KARMA_SERVER)
