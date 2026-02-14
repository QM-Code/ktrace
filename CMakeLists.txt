cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED VCPKG_ROOT)
        set(_VCPKG_TOOLCHAIN "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
    else()
        set(_VCPKG_TOOLCHAIN "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
    endif()
    if (EXISTS "${_VCPKG_TOOLCHAIN}")
        set(CMAKE_TOOLCHAIN_FILE "${_VCPKG_TOOLCHAIN}" CACHE FILEPATH "vcpkg toolchain" FORCE)
    else()
        message(STATUS "vcpkg toolchain not found at ${_VCPKG_TOOLCHAIN}; configuring without vcpkg")
    endif()
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg.json")
    set(VCPKG_MANIFEST_MODE ON CACHE BOOL "Use vcpkg manifest mode" FORCE)
    set(VCPKG_MANIFEST_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE STRING "vcpkg manifest dir" FORCE)
    set(VCPKG_FEATURE_FLAGS "manifests" CACHE STRING "vcpkg feature flags" FORCE)
endif()

project(bz3 VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CTest)

option(KARMA_INSTALL_ENGINE_SDK "Install engine SDK headers/libraries and CMake package" ON)
set(KARMA_ENGINE_CMAKE_INSTALL_DIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/KarmaEngine"
    CACHE PATH "Install directory for KarmaEngine CMake package files")
set(KARMA_ENGINE_SDK_COMPONENT
    "KarmaEngineSDK"
    CACHE STRING "Install component name for the Karma engine SDK payload")
option(KARMA_ENABLE_RMLUI_BACKEND "Enable native RmlUi backend integration in engine client" OFF)

set(KARMA_RENDER_BACKENDS "bgfx" CACHE STRING "Render backends to compile (semicolon list: bgfx;diligent)")
set(KARMA_RENDER_BACKEND_BGFX_ENABLED OFF)
set(KARMA_RENDER_BACKEND_DILIGENT_ENABLED OFF)
foreach(_backend IN LISTS KARMA_RENDER_BACKENDS)
    string(TOLOWER "${_backend}" _backend_norm)
    if(_backend_norm STREQUAL "bgfx")
        set(KARMA_RENDER_BACKEND_BGFX_ENABLED ON)
    elseif(_backend_norm STREQUAL "diligent")
        set(KARMA_RENDER_BACKEND_DILIGENT_ENABLED ON)
    elseif(_backend_norm STREQUAL "")
        # Ignore empty list entries.
    else()
        message(FATAL_ERROR "Unknown renderer backend in KARMA_RENDER_BACKENDS: ${_backend}")
    endif()
endforeach()
if(NOT KARMA_RENDER_BACKEND_BGFX_ENABLED AND NOT KARMA_RENDER_BACKEND_DILIGENT_ENABLED)
    message(FATAL_ERROR "KARMA_RENDER_BACKENDS must enable at least one backend (bgfx and/or diligent)")
endif()

set(KARMA_PHYSICS_BACKEND "jolt" CACHE STRING "Default physics backend (auto|jolt|physx)")
set_property(CACHE KARMA_PHYSICS_BACKEND PROPERTY STRINGS auto jolt physx)
set(KARMA_PHYSICS_BACKENDS "" CACHE STRING "Physics backends to compile (semicolon list: jolt;physx)")
if(KARMA_PHYSICS_BACKENDS STREQUAL "")
    if(KARMA_PHYSICS_BACKEND STREQUAL "auto")
        set(KARMA_PHYSICS_BACKENDS "jolt;physx")
    else()
        set(KARMA_PHYSICS_BACKENDS "${KARMA_PHYSICS_BACKEND}")
    endif()
endif()
set(KARMA_PHYSICS_BACKEND_JOLT_ENABLED OFF)
set(KARMA_PHYSICS_BACKEND_PHYSX_ENABLED OFF)
foreach(_backend IN LISTS KARMA_PHYSICS_BACKENDS)
    string(TOLOWER "${_backend}" _backend_norm)
    if(_backend_norm STREQUAL "jolt")
        set(KARMA_PHYSICS_BACKEND_JOLT_ENABLED ON)
    elseif(_backend_norm STREQUAL "physx")
        set(KARMA_PHYSICS_BACKEND_PHYSX_ENABLED ON)
    elseif(_backend_norm STREQUAL "")
        # Ignore empty list entries.
    else()
        message(FATAL_ERROR "Unknown physics backend in KARMA_PHYSICS_BACKENDS: ${_backend}")
    endif()
endforeach()
if(NOT KARMA_PHYSICS_BACKEND_JOLT_ENABLED AND NOT KARMA_PHYSICS_BACKEND_PHYSX_ENABLED)
    message(FATAL_ERROR "KARMA_PHYSICS_BACKENDS must enable at least one backend (jolt and/or physx)")
endif()

set(KARMA_AUDIO_BACKEND "sdl3audio" CACHE STRING "Default audio backend (auto|sdl3audio|miniaudio)")
set_property(CACHE KARMA_AUDIO_BACKEND PROPERTY STRINGS auto sdl3audio miniaudio)
set(KARMA_AUDIO_BACKENDS "" CACHE STRING "Audio backends to compile (semicolon list: sdl3audio;miniaudio)")
if(KARMA_AUDIO_BACKENDS STREQUAL "")
    if(KARMA_AUDIO_BACKEND STREQUAL "auto")
        set(KARMA_AUDIO_BACKENDS "sdl3audio;miniaudio")
    else()
        set(KARMA_AUDIO_BACKENDS "${KARMA_AUDIO_BACKEND}")
    endif()
endif()
set(KARMA_AUDIO_BACKEND_SDL3AUDIO_ENABLED OFF)
set(KARMA_AUDIO_BACKEND_MINIAUDIO_ENABLED OFF)
foreach(_backend IN LISTS KARMA_AUDIO_BACKENDS)
    string(TOLOWER "${_backend}" _backend_norm)
    if(_backend_norm STREQUAL "sdl3audio")
        set(KARMA_AUDIO_BACKEND_SDL3AUDIO_ENABLED ON)
    elseif(_backend_norm STREQUAL "miniaudio")
        set(KARMA_AUDIO_BACKEND_MINIAUDIO_ENABLED ON)
    elseif(_backend_norm STREQUAL "")
        # Ignore empty list entries.
    else()
        message(FATAL_ERROR "Unknown audio backend in KARMA_AUDIO_BACKENDS: ${_backend}")
    endif()
endforeach()
if(NOT KARMA_AUDIO_BACKEND_SDL3AUDIO_ENABLED AND NOT KARMA_AUDIO_BACKEND_MINIAUDIO_ENABLED)
    message(FATAL_ERROR "KARMA_AUDIO_BACKENDS must enable at least one backend (sdl3audio and/or miniaudio)")
endif()

set(KARMA_WINDOW_BACKEND "sdl3" CACHE STRING "Window backend (sdl3 only)" FORCE)
set_property(CACHE KARMA_WINDOW_BACKEND PROPERTY STRINGS sdl3)
add_compile_definitions(KARMA_WINDOW_BACKEND_SDL3)

include(FetchContent)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

find_package(glm QUIET)
if(NOT glm_FOUND)
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 0.9.9.8
    )
    FetchContent_MakeAvailable(glm)
endif()

find_package(assimp QUIET)
if(NOT assimp_FOUND)
    FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v5.3.1
    )
    FetchContent_MakeAvailable(assimp)
endif()

find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.12.0
    )
    FetchContent_MakeAvailable(spdlog)
endif()

find_package(miniz QUIET)
if(NOT miniz_FOUND)
    FetchContent_Declare(
        miniz
        GIT_REPOSITORY https://github.com/richgel999/miniz.git
        GIT_TAG 2.2.0
    )
    FetchContent_GetProperties(miniz)
    if(NOT miniz_POPULATED)
        FetchContent_Populate(miniz)
        add_library(miniz STATIC ${miniz_SOURCE_DIR}/miniz.c)
        target_include_directories(miniz PUBLIC ${miniz_SOURCE_DIR})
    endif()
    set(_MINIZ_EXPORT_HEADER "${CMAKE_CURRENT_BINARY_DIR}/miniz_export.h")
    if(NOT EXISTS "${_MINIZ_EXPORT_HEADER}")
        file(WRITE "${_MINIZ_EXPORT_HEADER}" "#pragma once\n#define MINIZ_EXPORT\n")
    endif()
    target_include_directories(miniz PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
    add_library(miniz::miniz ALIAS miniz)
endif()

find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

find_package(Protobuf REQUIRED)
find_package(CURL REQUIRED)

find_package(X11 QUIET)

find_package(stb QUIET)
if(NOT stb_FOUND)
    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG master
    )
    FetchContent_GetProperties(stb)
    if(NOT stb_POPULATED)
        FetchContent_Populate(stb)
    endif()
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
    add_library(stb::stb ALIAS stb)
endif()

if(KARMA_WINDOW_BACKEND STREQUAL "sdl3" OR KARMA_AUDIO_BACKEND_SDL3AUDIO_ENABLED)
    find_package(SDL3 CONFIG REQUIRED)
endif()

FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/zpl-c/enet.git
    GIT_TAG master
)
set(ENET_TEST OFF CACHE BOOL "" FORCE)
set(ENET_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(enet)

if(KARMA_RENDER_BACKEND_BGFX_ENABLED)
    set(KARMA_USE_BGFX_PACKAGE ON)
    if(KARMA_RENDER_BACKEND_DILIGENT_ENABLED)
        # vcpkg bgfx package enables shader tool deps (including glslang), which
        # collides with Diligent's bundled glslang targets in dual-backend builds.
        # Prefer the in-tree bgfx.cmake path there.
        set(KARMA_USE_BGFX_PACKAGE OFF)
        set(bgfx_FOUND OFF)
    endif()

    if(KARMA_USE_BGFX_PACKAGE)
        find_package(bgfx CONFIG QUIET)
    endif()

    if(NOT bgfx_FOUND)
        set(BGFX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BGFX_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
        set(BGFX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(BGFX_INSTALL OFF CACHE BOOL "" FORCE)
        set(BGFX_CUSTOM_TARGETS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            bgfx
            GIT_REPOSITORY https://github.com/bkaradzic/bgfx.cmake.git
            GIT_TAG master
        )
        FetchContent_GetProperties(bgfx)
        if(NOT bgfx_POPULATED)
            FetchContent_Populate(bgfx)
            set(BGFX_FETCHCONTENT_SOURCE_DIR "${bgfx_SOURCE_DIR}" CACHE PATH "bgfx.cmake source dir" FORCE)
            add_subdirectory(${bgfx_SOURCE_DIR} ${bgfx_BINARY_DIR})
        endif()
        if(TARGET bgfx AND NOT TARGET bgfx::bgfx)
            add_library(bgfx::bgfx ALIAS bgfx)
        endif()
    endif()
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

    function(bz3_bgfx_compile_shader out_file shader_type profile input_file varying_file)
        if(NOT BGFX_SHADERC_EXECUTABLE)
            return()
        endif()
        add_custom_command(
            OUTPUT "${out_file}"
            COMMAND ${BGFX_SHADERC_EXECUTABLE}
                -f "${input_file}"
                -o "${out_file}"
                --type ${shader_type}
                --platform linux
                -p ${profile}
                --varyingdef "${varying_file}"
                -i "${BGFX_SHADER_DIR}"
                -i "${_BGFX_SHADER_INCLUDE_HINT}"
                -i "${_BGFX_SHADER_INCLUDE_HINT}/bgfx"
            DEPENDS "${input_file}" "${varying_file}"
        )
    endfunction()

    set(BGFX_SIMPLE_DIR "${BGFX_SHADER_DIR}/simple")
    set(BGFX_SIMPLE_BIN_DIR_VK "${BGFX_SHADER_BIN_DIR_VK}/simple")
    file(MAKE_DIRECTORY "${BGFX_SIMPLE_BIN_DIR_VK}")
    set(BGFX_SIMPLE_VS_BIN_VK "${BGFX_SIMPLE_BIN_DIR_VK}/vs_simple.bin")
    set(BGFX_SIMPLE_FS_BIN_VK "${BGFX_SIMPLE_BIN_DIR_VK}/fs_simple.bin")
    bz3_bgfx_compile_shader("${BGFX_SIMPLE_VS_BIN_VK}" vertex "spirv" "${BGFX_SIMPLE_DIR}/vs_simple.sc" "${BGFX_SIMPLE_DIR}/varying.def.sc")
    bz3_bgfx_compile_shader("${BGFX_SIMPLE_FS_BIN_VK}" fragment "spirv" "${BGFX_SIMPLE_DIR}/fs_simple.sc" "${BGFX_SIMPLE_DIR}/varying.def.sc")

    set(BGFX_MESH_DIR "${BGFX_SHADER_DIR}/mesh")
    set(BGFX_MESH_BIN_DIR_VK "${BGFX_SHADER_BIN_DIR_VK}/mesh")
    file(MAKE_DIRECTORY "${BGFX_MESH_BIN_DIR_VK}")
    set(BGFX_MESH_VS_BIN_VK "${BGFX_MESH_BIN_DIR_VK}/vs_mesh.bin")
    set(BGFX_MESH_FS_BIN_VK "${BGFX_MESH_BIN_DIR_VK}/fs_mesh.bin")
    bz3_bgfx_compile_shader("${BGFX_MESH_VS_BIN_VK}" vertex "spirv" "${BGFX_MESH_DIR}/vs_mesh.sc" "${BGFX_MESH_DIR}/varying.def.sc")
    bz3_bgfx_compile_shader("${BGFX_MESH_FS_BIN_VK}" fragment "spirv" "${BGFX_MESH_DIR}/fs_mesh.sc" "${BGFX_MESH_DIR}/varying.def.sc")

    set(BGFX_SHADOW_DIR "${BGFX_SHADER_DIR}/shadow")
    set(BGFX_SHADOW_BIN_DIR_VK "${BGFX_SHADER_BIN_DIR_VK}/shadow")
    file(MAKE_DIRECTORY "${BGFX_SHADOW_BIN_DIR_VK}")
    set(BGFX_SHADOW_VS_BIN_VK "${BGFX_SHADOW_BIN_DIR_VK}/vs_shadow_depth.bin")
    set(BGFX_SHADOW_FS_BIN_VK "${BGFX_SHADOW_BIN_DIR_VK}/fs_shadow_depth.bin")
    bz3_bgfx_compile_shader("${BGFX_SHADOW_VS_BIN_VK}" vertex "spirv" "${BGFX_SHADOW_DIR}/vs_shadow_depth.sc" "${BGFX_SHADOW_DIR}/varying.def.sc")
    bz3_bgfx_compile_shader("${BGFX_SHADOW_FS_BIN_VK}" fragment "spirv" "${BGFX_SHADOW_DIR}/fs_shadow_depth.sc" "${BGFX_SHADOW_DIR}/varying.def.sc")

    if(BGFX_SHADERC_EXECUTABLE)
        add_custom_target(bz3_bgfx_shaders ALL DEPENDS
            "${BGFX_SIMPLE_VS_BIN_VK}" "${BGFX_SIMPLE_FS_BIN_VK}"
            "${BGFX_MESH_VS_BIN_VK}" "${BGFX_MESH_FS_BIN_VK}"
            "${BGFX_SHADOW_VS_BIN_VK}" "${BGFX_SHADOW_FS_BIN_VK}")
    else()
        add_custom_target(bz3_bgfx_shaders ALL)
    endif()
endif()

if(KARMA_RENDER_BACKEND_DILIGENT_ENABLED)
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
    set(DILIGENT_INSTALL_CORE OFF CACHE BOOL "" FORCE)
    set(DILIGENT_INSTALL_TOOLS OFF CACHE BOOL "" FORCE)
    set(DILIGENT_INSTALL_FX OFF CACHE BOOL "" FORCE)
    set(DILIGENT_INSTALL_SAMPLES OFF CACHE BOOL "" FORCE)
    set(DILIGENT_INSTALL_PDB OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        DiligentEngine
        GIT_REPOSITORY https://github.com/DiligentGraphics/DiligentEngine.git
        GIT_TAG v2.5.6
        GIT_SUBMODULES "DiligentCore;DiligentTools;DiligentFX;DiligentSamples"
        GIT_SUBMODULES_RECURSE ON
        PATCH_COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/cmake/diligent_wayland_apply.py
    )
    FetchContent_MakeAvailable(DiligentEngine)
endif()

add_subdirectory(src/engine)
add_subdirectory(src/game)

if(KARMA_INSTALL_ENGINE_SDK)
    set(KARMA_ENGINE_PACKAGE_HAS_CLIENT ON)
    set(KARMA_ENGINE_SDK_TARGETS
        karma_engine_core
        karma_engine_client
    )
    if(KARMA_RENDER_BACKEND_DILIGENT_ENABLED)
        # Diligent is currently consumed as in-tree targets, which cannot be exported
        # in a relocatable SDK package without exporting the entire dependency graph.
        set(KARMA_ENGINE_PACKAGE_HAS_CLIENT OFF)
        set(KARMA_ENGINE_SDK_TARGETS
            karma_engine_core
        )
        message(STATUS "KarmaEngine SDK export: installing core target only for diligent backend")
    endif()

    install(TARGETS
        ${KARMA_ENGINE_SDK_TARGETS}
        EXPORT KarmaEngineTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${KARMA_ENGINE_SDK_COMPONENT}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT ${KARMA_ENGINE_SDK_COMPONENT}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT ${KARMA_ENGINE_SDK_COMPONENT}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY
        ${PROJECT_SOURCE_DIR}/include/karma
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        COMPONENT ${KARMA_ENGINE_SDK_COMPONENT}
    )

    install(EXPORT KarmaEngineTargets
        FILE KarmaEngineTargets.cmake
        NAMESPACE karma::
        DESTINATION ${KARMA_ENGINE_CMAKE_INSTALL_DIR}
        COMPONENT ${KARMA_ENGINE_SDK_COMPONENT}
    )

    set(KARMA_ENGINE_PACKAGE_WITH_IMGUI OFF)
    if(KARMA_ENGINE_HAS_IMGUI)
        set(KARMA_ENGINE_PACKAGE_WITH_IMGUI ON)
    endif()

    set(KARMA_ENGINE_PACKAGE_WITH_X11 OFF)
    if(X11_FOUND)
        set(KARMA_ENGINE_PACKAGE_WITH_X11 ON)
    endif()

    set(KARMA_ENGINE_PACKAGE_WITH_RMLUI OFF)
    if(KARMA_ENGINE_HAS_RMLUI)
        set(KARMA_ENGINE_PACKAGE_WITH_RMLUI ON)
    endif()

    set(KARMA_ENGINE_PACKAGE_WITH_RENDER_BGFX OFF)
    if(KARMA_RENDER_BACKEND_BGFX_ENABLED)
        set(KARMA_ENGINE_PACKAGE_WITH_RENDER_BGFX ON)
    endif()

    set(KARMA_ENGINE_PACKAGE_WITH_RENDER_DILIGENT OFF)
    if(KARMA_RENDER_BACKEND_DILIGENT_ENABLED)
        set(KARMA_ENGINE_PACKAGE_WITH_RENDER_DILIGENT ON)
    endif()

    configure_package_config_file(
        ${PROJECT_SOURCE_DIR}/cmake/KarmaEngineConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/KarmaEngineConfig.cmake
        INSTALL_DESTINATION ${KARMA_ENGINE_CMAKE_INSTALL_DIR}
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/KarmaEngineConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/KarmaEngineConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/KarmaEngineConfigVersion.cmake
        DESTINATION ${KARMA_ENGINE_CMAKE_INSTALL_DIR}
        COMPONENT ${KARMA_ENGINE_SDK_COMPONENT}
    )
endif()
