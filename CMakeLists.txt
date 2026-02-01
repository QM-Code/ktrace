cmake_minimum_required(VERSION 3.20)

set(_VCPKG_TOOLCHAIN "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
if (EXISTS "${_VCPKG_TOOLCHAIN}")
    set(CMAKE_TOOLCHAIN_FILE "${_VCPKG_TOOLCHAIN}" CACHE FILEPATH "vcpkg toolchain" FORCE)
else()
    message(STATUS "vcpkg toolchain not found at ${_VCPKG_TOOLCHAIN}; configuring without vcpkg")
endif()

project(bz3)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_language(ASM)

option(KARMA_ENGINE_ONLY "Build only the Karma engine libraries" OFF)
option(KARMA_EXTRAS "Build the Karma extras layer" ON)
option(BZ3_GAME_ONLY "Build only the BZ3 game (requires installed Karma)" OFF)
if(KARMA_ENGINE_ONLY AND BZ3_GAME_ONLY)
    message(FATAL_ERROR "KARMA_ENGINE_ONLY and BZ3_GAME_ONLY are mutually exclusive.")
endif()
if(KARMA_ENGINE_ONLY)
    set(KARMA_EXTRAS OFF)
endif()

set(KARMA_UI_BACKEND "imgui" CACHE STRING "UI backend (imgui or rmlui)")
set_property(CACHE KARMA_UI_BACKEND PROPERTY STRINGS imgui rmlui)
if(KARMA_UI_BACKEND STREQUAL "imgui")
    add_compile_definitions(KARMA_UI_BACKEND_IMGUI)
elseif(KARMA_UI_BACKEND STREQUAL "rmlui")
    add_compile_definitions(KARMA_UI_BACKEND_RMLUI)
else()
    message(FATAL_ERROR "Unknown KARMA_UI_BACKEND value: ${KARMA_UI_BACKEND}")
endif()

set(KARMA_WINDOW_BACKEND "sdl3" CACHE STRING "Window backend (sdl3 or sdl2)")
set_property(CACHE KARMA_WINDOW_BACKEND PROPERTY STRINGS sdl3 sdl2)
if(KARMA_WINDOW_BACKEND STREQUAL "sdl3")
    add_compile_definitions(KARMA_WINDOW_BACKEND_SDL3)
    set(WINDOW_TARGET SDL3::SDL3)
elseif(KARMA_WINDOW_BACKEND STREQUAL "sdl2")
    add_compile_definitions(KARMA_WINDOW_BACKEND_SDL2)
    set(WINDOW_TARGET "")
else()
    message(FATAL_ERROR "Unknown KARMA_WINDOW_BACKEND value: ${KARMA_WINDOW_BACKEND}")
endif()

set(KARMA_PHYSICS_BACKEND "jolt" CACHE STRING "Physics backend (jolt or physx)")
set_property(CACHE KARMA_PHYSICS_BACKEND PROPERTY STRINGS jolt physx)
if(KARMA_PHYSICS_BACKEND STREQUAL "jolt")
    add_compile_definitions(KARMA_PHYSICS_BACKEND_JOLT)
elseif(KARMA_PHYSICS_BACKEND STREQUAL "physx")
    add_compile_definitions(KARMA_PHYSICS_BACKEND_PHYSX)
else()
    message(FATAL_ERROR "Unknown KARMA_PHYSICS_BACKEND value: ${KARMA_PHYSICS_BACKEND}")
endif()

set(KARMA_AUDIO_BACKEND "sdlaudio" CACHE STRING "Audio backend (miniaudio or sdlaudio)")
set_property(CACHE KARMA_AUDIO_BACKEND PROPERTY STRINGS miniaudio sdlaudio)
if(KARMA_AUDIO_BACKEND STREQUAL "miniaudio")
    add_compile_definitions(KARMA_AUDIO_BACKEND_MINIAUDIO)
elseif(KARMA_AUDIO_BACKEND STREQUAL "sdlaudio")
    add_compile_definitions(KARMA_AUDIO_BACKEND_SDL)
else()
    message(FATAL_ERROR "Unknown KARMA_AUDIO_BACKEND value: ${KARMA_AUDIO_BACKEND}")
endif()

set(KARMA_NETWORK_BACKEND "enet" CACHE STRING "Network backend (enet)")
set_property(CACHE KARMA_NETWORK_BACKEND PROPERTY STRINGS enet)
if(KARMA_NETWORK_BACKEND STREQUAL "enet")
    add_compile_definitions(KARMA_NETWORK_BACKEND_ENET)
else()
    message(FATAL_ERROR "Unknown KARMA_NETWORK_BACKEND value: ${KARMA_NETWORK_BACKEND}")
endif()

set(KARMA_WORLD_BACKEND "fs" CACHE STRING "World backend (fs)")
set_property(CACHE KARMA_WORLD_BACKEND PROPERTY STRINGS fs)
if(KARMA_WORLD_BACKEND STREQUAL "fs")
    add_compile_definitions(KARMA_WORLD_BACKEND_FS)
else()
    message(FATAL_ERROR "Unknown KARMA_WORLD_BACKEND value: ${KARMA_WORLD_BACKEND}")
endif()

set(KARMA_RENDER_BACKEND "bgfx" CACHE STRING "Render backend (diligent|bgfx)")
set_property(CACHE KARMA_RENDER_BACKEND PROPERTY STRINGS diligent bgfx)
if(KARMA_RENDER_BACKEND STREQUAL "diligent")
    add_compile_definitions(KARMA_RENDER_BACKEND_DILIGENT)
elseif(KARMA_RENDER_BACKEND STREQUAL "bgfx")
    add_compile_definitions(KARMA_RENDER_BACKEND_BGFX)
else()
    message(FATAL_ERROR "Unknown KARMA_RENDER_BACKEND value: ${KARMA_RENDER_BACKEND}")
endif()

set(CMAKE_BUILD_TYPE Release)
set(BUILD_SHARED_LIBS OFF)

include(FetchContent)

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

if(NOT KARMA_ENGINE_ONLY)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.11.1
    )
    FetchContent_MakeAvailable(pybind11)
endif()

if(KARMA_RENDER_BACKEND STREQUAL "diligent")
    set(DILIGENT_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(DILIGENT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(DILIGENT_BUILD_FX OFF CACHE BOOL "" FORCE)
    set(DILIGENT_NO_ARCHIVER ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_DIRECT3D11 ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_DIRECT3D12 ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_OPENGL ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_GLES ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_METAL ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_WEBGPU ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_GLSLANG OFF CACHE BOOL "" FORCE)
    set(DILIGENT_NO_HLSL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        DiligentEngine
        GIT_REPOSITORY https://github.com/DiligentGraphics/DiligentEngine.git
        GIT_TAG v2.5.6
        GIT_SUBMODULES "DiligentCore;DiligentTools;DiligentFX;DiligentSamples"
        GIT_SUBMODULES_RECURSE ON
        PATCH_COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/cmake/diligent_wayland_apply.py
    )
    FetchContent_MakeAvailable(DiligentEngine)

    if(TARGET Diligent-GraphicsEngineVk-shared)
        target_link_libraries(Diligent-GraphicsEngineVk-shared
        PRIVATE
            Diligent-GraphicsEngine
            Diligent-GraphicsEngineNextGenBase
            Diligent-Common
            Diligent-GraphicsAccessories
            Diligent-ShaderTools
            Diligent-PlatformInterface
            Diligent-TargetPlatform
        )
    endif()
endif()

FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/zpl-c/enet.git
    GIT_TAG master
)
set(ENET_TEST OFF CACHE BOOL "" FORCE)
set(ENET_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(enet)

# No particle system dependency.


# Find external packages
find_package(glm REQUIRED)
if(KARMA_PHYSICS_BACKEND STREQUAL "jolt")
    find_package(Jolt REQUIRED)
elseif(KARMA_PHYSICS_BACKEND STREQUAL "physx")
    find_package(unofficial-omniverse-physx-sdk CONFIG REQUIRED)
endif()
find_package(assimp REQUIRED)
if(NOT KARMA_ENGINE_ONLY)
    find_package(cxxopts REQUIRED)
endif()
if(KARMA_UI_BACKEND STREQUAL "imgui")
    find_package(imgui REQUIRED)
elseif(KARMA_UI_BACKEND STREQUAL "rmlui")
    find_package(RmlUi CONFIG REQUIRED)
    find_package(md4c REQUIRED)
endif()
if(KARMA_WINDOW_BACKEND STREQUAL "sdl3" OR KARMA_AUDIO_BACKEND STREQUAL "sdlaudio")
    find_package(SDL3 CONFIG REQUIRED)
endif()
find_package(spdlog REQUIRED)
find_package(miniz REQUIRED)
find_package(nlohmann_json REQUIRED)
if(NOT KARMA_ENGINE_ONLY)
    find_package(Protobuf REQUIRED)
endif()
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
if(KARMA_RENDER_BACKEND STREQUAL "bgfx")
    find_package(bgfx CONFIG REQUIRED)
    set(BGFX_SHADER_DIR "${PROJECT_SOURCE_DIR}/data/bgfx/shaders")
    set(BGFX_SHADER_BIN_DIR "${PROJECT_SOURCE_DIR}/data/bgfx/shaders/bin")
    set(BGFX_SHADER_BIN_DIR_VK "${BGFX_SHADER_BIN_DIR}/vk")
    file(MAKE_DIRECTORY "${BGFX_SHADER_BIN_DIR_VK}")

    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_HOST_TRIPLET)
        set(_BGFX_SHADERC_HINT "${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET}/tools/bgfx")
    endif()
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(_BGFX_SHADER_INCLUDE_HINT "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
    endif()
    if(NOT _BGFX_SHADER_INCLUDE_HINT)
        set(_BGFX_SHADER_INCLUDE_HINT "${BGFX_SHADER_DIR}")
    endif()

    find_program(BGFX_SHADERC_EXECUTABLE NAMES shaderc HINTS ${_BGFX_SHADERC_HINT})
    if(NOT BGFX_SHADERC_EXECUTABLE)
        message(WARNING "bgfx shaderc not found; bgfx shaders will not be compiled.")
    endif()

    set(BGFX_SHADER_PROFILE_VK "spirv")
    set(BGFX_SHADER_RAW "")

    function(bz3_bgfx_compile_shader out_file type profile input varying)
        if(NOT BGFX_SHADERC_EXECUTABLE)
            return()
        endif()
        set(_shaderc_args
            -f "${input}"
            -o "${out_file}"
            --type "${type}"
            --platform linux
            --profile "${profile}"
            --varyingdef "${varying}"
            -i "${BGFX_SHADER_DIR}"
            -i "${_BGFX_SHADER_INCLUDE_HINT}"
            -i "${_BGFX_SHADER_INCLUDE_HINT}/bgfx"
        )
        if(BGFX_SHADER_RAW)
            list(APPEND _shaderc_args --raw)
        endif()
        add_custom_command(
            OUTPUT "${out_file}"
            COMMAND "${BGFX_SHADERC_EXECUTABLE}" ${_shaderc_args}
            DEPENDS "${input}" "${varying}"
            COMMENT "Compiling bgfx shader ${input}"
            VERBATIM
        )
    endfunction()

    set(BGFX_VS_BIN_VK "${BGFX_SHADER_BIN_DIR_VK}/vs_triangle.bin")
    set(BGFX_FS_BIN_VK "${BGFX_SHADER_BIN_DIR_VK}/fs_triangle.bin")
    bz3_bgfx_compile_shader("${BGFX_VS_BIN_VK}" vertex "${BGFX_SHADER_PROFILE_VK}" "${BGFX_SHADER_DIR}/vs_triangle.sc" "${BGFX_SHADER_DIR}/varying.def.sc")
    bz3_bgfx_compile_shader("${BGFX_FS_BIN_VK}" fragment "${BGFX_SHADER_PROFILE_VK}" "${BGFX_SHADER_DIR}/fs_triangle.sc" "${BGFX_SHADER_DIR}/varying.def.sc")

    set(BGFX_IMGUI_SHADER_DIR "${BGFX_SHADER_DIR}/imgui")
    set(BGFX_IMGUI_BIN_DIR_VK "${BGFX_SHADER_BIN_DIR_VK}/imgui")
    file(MAKE_DIRECTORY "${BGFX_IMGUI_BIN_DIR_VK}")
    set(BGFX_IMGUI_VS_BIN_VK "${BGFX_IMGUI_BIN_DIR_VK}/vs_imgui.bin")
    set(BGFX_IMGUI_FS_BIN_VK "${BGFX_IMGUI_BIN_DIR_VK}/fs_imgui.bin")
    bz3_bgfx_compile_shader("${BGFX_IMGUI_VS_BIN_VK}" vertex "${BGFX_SHADER_PROFILE_VK}" "${BGFX_IMGUI_SHADER_DIR}/vs_imgui.sc" "${BGFX_IMGUI_SHADER_DIR}/varying.def.sc")
    bz3_bgfx_compile_shader("${BGFX_IMGUI_FS_BIN_VK}" fragment "${BGFX_SHADER_PROFILE_VK}" "${BGFX_IMGUI_SHADER_DIR}/fs_imgui.sc" "${BGFX_IMGUI_SHADER_DIR}/varying.def.sc")

    set(BGFX_MESH_SHADER_DIR "${BGFX_SHADER_DIR}/mesh")
    set(BGFX_MESH_BIN_DIR_VK "${BGFX_SHADER_BIN_DIR_VK}/mesh")
    file(MAKE_DIRECTORY "${BGFX_MESH_BIN_DIR_VK}")
    set(BGFX_MESH_VS_BIN_VK "${BGFX_MESH_BIN_DIR_VK}/vs_mesh.bin")
    set(BGFX_MESH_FS_BIN_VK "${BGFX_MESH_BIN_DIR_VK}/fs_mesh.bin")
    bz3_bgfx_compile_shader("${BGFX_MESH_VS_BIN_VK}" vertex "${BGFX_SHADER_PROFILE_VK}" "${BGFX_MESH_SHADER_DIR}/vs_mesh.sc" "${BGFX_MESH_SHADER_DIR}/varying.def.sc")
    bz3_bgfx_compile_shader("${BGFX_MESH_FS_BIN_VK}" fragment "${BGFX_SHADER_PROFILE_VK}" "${BGFX_MESH_SHADER_DIR}/fs_mesh.sc" "${BGFX_MESH_SHADER_DIR}/varying.def.sc")

    set(BGFX_SKYBOX_SHADER_DIR "${BGFX_SHADER_DIR}/skybox")
    set(BGFX_SKYBOX_BIN_DIR_VK "${BGFX_SHADER_BIN_DIR_VK}/skybox")
    file(MAKE_DIRECTORY "${BGFX_SKYBOX_BIN_DIR_VK}")
    set(BGFX_SKYBOX_VS_BIN_VK "${BGFX_SKYBOX_BIN_DIR_VK}/vs_skybox.bin")
    set(BGFX_SKYBOX_FS_BIN_VK "${BGFX_SKYBOX_BIN_DIR_VK}/fs_skybox.bin")
    bz3_bgfx_compile_shader("${BGFX_SKYBOX_VS_BIN_VK}" vertex "${BGFX_SHADER_PROFILE_VK}" "${BGFX_SKYBOX_SHADER_DIR}/vs_skybox.sc" "${BGFX_SKYBOX_SHADER_DIR}/varying.def.sc")
    bz3_bgfx_compile_shader("${BGFX_SKYBOX_FS_BIN_VK}" fragment "${BGFX_SHADER_PROFILE_VK}" "${BGFX_SKYBOX_SHADER_DIR}/fs_skybox.sc" "${BGFX_SKYBOX_SHADER_DIR}/varying.def.sc")

    set(BGFX_RMLUI_SHADER_DIR "${BGFX_SHADER_DIR}/rmlui")
    set(BGFX_RMLUI_BIN_DIR_VK "${BGFX_SHADER_BIN_DIR_VK}/rmlui")
    file(MAKE_DIRECTORY "${BGFX_RMLUI_BIN_DIR_VK}")
    set(BGFX_RMLUI_VS_BIN_VK "${BGFX_RMLUI_BIN_DIR_VK}/vs_rmlui.bin")
    set(BGFX_RMLUI_FS_TEX_BIN_VK "${BGFX_RMLUI_BIN_DIR_VK}/fs_rmlui_texture.bin")
    set(BGFX_RMLUI_FS_COLOR_BIN_VK "${BGFX_RMLUI_BIN_DIR_VK}/fs_rmlui_color.bin")
    bz3_bgfx_compile_shader("${BGFX_RMLUI_VS_BIN_VK}" vertex "${BGFX_SHADER_PROFILE_VK}" "${BGFX_RMLUI_SHADER_DIR}/vs_rmlui.sc" "${BGFX_RMLUI_SHADER_DIR}/varying.def.sc")
    bz3_bgfx_compile_shader("${BGFX_RMLUI_FS_TEX_BIN_VK}" fragment "${BGFX_SHADER_PROFILE_VK}" "${BGFX_RMLUI_SHADER_DIR}/fs_rmlui_texture.sc" "${BGFX_RMLUI_SHADER_DIR}/varying.def.sc")
    bz3_bgfx_compile_shader("${BGFX_RMLUI_FS_COLOR_BIN_VK}" fragment "${BGFX_SHADER_PROFILE_VK}" "${BGFX_RMLUI_SHADER_DIR}/fs_rmlui_color.sc" "${BGFX_RMLUI_SHADER_DIR}/varying.def.sc")

    if(BGFX_SHADERC_EXECUTABLE)
        add_custom_target(bz3_bgfx_shaders ALL DEPENDS
            "${BGFX_VS_BIN_VK}" "${BGFX_FS_BIN_VK}"
            "${BGFX_IMGUI_VS_BIN_VK}" "${BGFX_IMGUI_FS_BIN_VK}"
            "${BGFX_MESH_VS_BIN_VK}" "${BGFX_MESH_FS_BIN_VK}"
            "${BGFX_SKYBOX_VS_BIN_VK}" "${BGFX_SKYBOX_FS_BIN_VK}"
            "${BGFX_RMLUI_VS_BIN_VK}" "${BGFX_RMLUI_FS_TEX_BIN_VK}" "${BGFX_RMLUI_FS_COLOR_BIN_VK}"
        )
    else()
        add_custom_target(bz3_bgfx_shaders ALL)
    endif()
endif()

# Compile protobuf files
set(PROTO_FILES ${PROJECT_SOURCE_DIR}/src/game/protos/messages.proto)
set(PROTO_OUTPUT_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_OUTPUT_DIR})

# Generate protobuf sources
if(NOT KARMA_ENGINE_ONLY)
    protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

    # Create a library for proto files
    add_library(proto_lib STATIC ${PROTO_SRCS} ${PROTO_HDRS})
    target_link_libraries(proto_lib PUBLIC protobuf::libprotobuf)
    target_include_directories(proto_lib PUBLIC ${CMAKE_BINARY_DIR})
endif()

if(KARMA_UI_BACKEND STREQUAL "imgui")
    find_package(Freetype REQUIRED)
    find_package(harfbuzz REQUIRED)
endif()
if(UNIX AND NOT APPLE)
    find_package(Fontconfig REQUIRED)
endif()
find_package(PkgConfig REQUIRED)
if(KARMA_UI_BACKEND STREQUAL "imgui")
    pkg_check_modules(LIBRSVG REQUIRED IMPORTED_TARGET librsvg-2.0)
endif()

if(BZ3_GAME_ONLY)
    find_package(karma REQUIRED)
    if(NOT TARGET karma)
        add_library(karma ALIAS karma::karma)
    endif()
    if(NOT TARGET karma_server)
        add_library(karma_server ALIAS karma::karma_server)
    endif()
    if(KARMA_EXTRAS)
        find_package(karma_extras REQUIRED)
        if(NOT TARGET karma_extras)
            add_library(karma_extras ALIAS karma_extras::karma_extras)
        endif()
    endif()
else()
    add_subdirectory(src/engine)
    if(KARMA_EXTRAS)
        add_subdirectory(src/karma-extras)
    endif()
endif()

if(NOT KARMA_ENGINE_ONLY)
    add_subdirectory(src/game)
endif()

if(NOT KARMA_ENGINE_ONLY)
    install(TARGETS bz3 BUNDLE DESTINATION .)
    install(TARGETS bz3-server
        RUNTIME DESTINATION bin
    )

    # Install data folder
    install(DIRECTORY ${PROJECT_SOURCE_DIR}/data/
        DESTINATION share/${PROJECT_NAME}/data
    )
endif()

# CPack configuration
set(CPACK_PACKAGE_NAME "bz3")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VENDOR "Quinn Carmack")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BZFlag remade")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "bz3")

# Linux-specific generators
if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Quinn Carmack <quinncarmack@gmail.com>")
    if(KARMA_WINDOW_BACKEND STREQUAL "sdl3")
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgl1, libsdl3")
    else()
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgl1")
    endif()
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
endif()

include(CPack)
