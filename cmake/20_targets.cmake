set(KTRACE_SOURCES
    ${PROJECT_SOURCE_DIR}/src/ktrace.cpp
    ${PROJECT_SOURCE_DIR}/src/ktrace/bridge.cpp
    ${PROJECT_SOURCE_DIR}/src/ktrace/cli.cpp
    ${PROJECT_SOURCE_DIR}/src/ktrace/colors.cpp
    ${PROJECT_SOURCE_DIR}/src/ktrace/filter.cpp
    ${PROJECT_SOURCE_DIR}/src/ktrace/format.cpp
    ${PROJECT_SOURCE_DIR}/src/ktrace/registry.cpp
    ${PROJECT_SOURCE_DIR}/src/ktrace/selectors.cpp
    ${PROJECT_SOURCE_DIR}/src/ktrace/state.cpp
)

if(NOT KTRACE_BUILD_STATIC AND NOT KTRACE_BUILD_SHARED)
    message(FATAL_ERROR "ktrace requires at least one of KTRACE_BUILD_STATIC or KTRACE_BUILD_SHARED to be ON.")
endif()

function(ktools_apply_runtime_rpath target_name)
    if(NOT TARGET "${target_name}")
        return()
    endif()
    if(NOT DEFINED KTOOLS_RUNTIME_RPATH_DIRS OR KTOOLS_RUNTIME_RPATH_DIRS STREQUAL "")
        return()
    endif()
    set_target_properties("${target_name}" PROPERTIES
        BUILD_RPATH "${KTOOLS_RUNTIME_RPATH_DIRS}"
    )
endfunction()

set(_ktrace_kcli_static_dep kcli::sdk_static)
if(NOT TARGET kcli::sdk_static)
    set(_ktrace_kcli_static_dep kcli::sdk)
endif()

set(_ktrace_kcli_shared_dep kcli::sdk_shared)
if(NOT TARGET kcli::sdk_shared)
    set(_ktrace_kcli_shared_dep kcli::sdk)
endif()

if(KTRACE_BUILD_STATIC)
    add_library(ktrace_sdk_static STATIC ${KTRACE_SOURCES})
    add_library(ktrace::sdk_static ALIAS ktrace_sdk_static)

    target_include_directories(ktrace_sdk_static
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ktrace_sdk_static PUBLIC
        ${_ktrace_kcli_static_dep}
    )

    # Internal trace macros require a compile-time namespace string.
    target_compile_definitions(ktrace_sdk_static PRIVATE KTRACE_NAMESPACE="ktrace")

    set_target_properties(ktrace_sdk_static PROPERTIES
        OUTPUT_NAME ktrace
        EXPORT_NAME sdk_static
        POSITION_INDEPENDENT_CODE ON
    )
endif()

if(KTRACE_BUILD_SHARED)
    add_library(ktrace_sdk_shared SHARED ${KTRACE_SOURCES})
    add_library(ktrace::sdk_shared ALIAS ktrace_sdk_shared)

    target_include_directories(ktrace_sdk_shared
        PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
        PRIVATE
            ${PROJECT_SOURCE_DIR}/src
    )

    target_link_libraries(ktrace_sdk_shared PUBLIC
        ${_ktrace_kcli_shared_dep}
    )

    # Internal trace macros require a compile-time namespace string.
    target_compile_definitions(ktrace_sdk_shared PRIVATE KTRACE_NAMESPACE="ktrace")

    set_target_properties(ktrace_sdk_shared PROPERTIES
        OUTPUT_NAME ktrace
        EXPORT_NAME sdk_shared
        POSITION_INDEPENDENT_CODE ON
    )
    ktools_apply_runtime_rpath(ktrace_sdk_shared)
endif()

if(TARGET ktrace_sdk_shared)
    add_library(ktrace::sdk ALIAS ktrace_sdk_shared)
elseif(TARGET ktrace_sdk_static)
    add_library(ktrace::sdk ALIAS ktrace_sdk_static)
endif()
