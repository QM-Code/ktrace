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

project(bz3 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(KARMA_RENDER_BACKEND "bgfx" CACHE STRING "Render backend (bgfx|diligent)")
set_property(CACHE KARMA_RENDER_BACKEND PROPERTY STRINGS bgfx diligent)
if(KARMA_RENDER_BACKEND STREQUAL "bgfx")
    add_compile_definitions(KARMA_RENDER_BACKEND_BGFX)
elseif(KARMA_RENDER_BACKEND STREQUAL "diligent")
    add_compile_definitions(KARMA_RENDER_BACKEND_DILIGENT)
else()
    message(FATAL_ERROR "Unknown KARMA_RENDER_BACKEND: ${KARMA_RENDER_BACKEND}")
endif()

set(KARMA_WINDOW_BACKEND "sdl3" CACHE STRING "Window backend (sdl3|sdl2|glfw)")
set_property(CACHE KARMA_WINDOW_BACKEND PROPERTY STRINGS sdl3 sdl2 glfw)
if(KARMA_WINDOW_BACKEND STREQUAL "sdl3")
    add_compile_definitions(KARMA_WINDOW_BACKEND_SDL3)
elseif(KARMA_WINDOW_BACKEND STREQUAL "sdl2")
    add_compile_definitions(KARMA_WINDOW_BACKEND_SDL2)
elseif(KARMA_WINDOW_BACKEND STREQUAL "glfw")
    add_compile_definitions(KARMA_WINDOW_BACKEND_GLFW)
else()
    message(FATAL_ERROR "Unknown KARMA_WINDOW_BACKEND: ${KARMA_WINDOW_BACKEND}")
endif()

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

if(KARMA_WINDOW_BACKEND STREQUAL "sdl3")
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

if(KARMA_RENDER_BACKEND STREQUAL "bgfx")
    find_package(bgfx CONFIG QUIET)
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

    if(BGFX_SHADERC_EXECUTABLE)
        add_custom_target(bz3_bgfx_shaders ALL DEPENDS
            "${BGFX_SIMPLE_VS_BIN_VK}" "${BGFX_SIMPLE_FS_BIN_VK}")
    else()
        add_custom_target(bz3_bgfx_shaders ALL)
    endif()
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
endif()

add_subdirectory(src/engine)
add_subdirectory(src/game)
