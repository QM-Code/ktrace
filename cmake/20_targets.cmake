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

if(KTRACE_BUILD_SHARED)
    set(_ktrace_library_type SHARED)
else()
    set(_ktrace_library_type STATIC)
endif()

add_library(ktrace_sdk ${_ktrace_library_type} ${KTRACE_SOURCES})
add_library(ktrace::sdk ALIAS ktrace_sdk)

target_include_directories(ktrace_sdk
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(ktrace_sdk PUBLIC
    kcli::sdk
    spdlog::spdlog
)

# Internal trace macros require a compile-time namespace string.
target_compile_definitions(ktrace_sdk PRIVATE KTRACE_NAMESPACE="ktrace")

set_target_properties(ktrace_sdk PROPERTIES
    OUTPUT_NAME ktrace
    EXPORT_NAME sdk
    POSITION_INDEPENDENT_CODE ON
)
