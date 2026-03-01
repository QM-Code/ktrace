set(KTRACE_LOG_SOURCES
    ${PROJECT_SOURCE_DIR}/src/colors.cpp
    ${PROJECT_SOURCE_DIR}/src/core.cpp
    ${PROJECT_SOURCE_DIR}/src/registry.cpp
    ${PROJECT_SOURCE_DIR}/src/selectors.cpp
    ${PROJECT_SOURCE_DIR}/src/state.cpp
    ${PROJECT_SOURCE_DIR}/src/trace.cpp
)

if(KTRACE_BUILD_SHARED)
    set(_ktrace_library_type SHARED)
else()
    set(_ktrace_library_type STATIC)
endif()

add_library(ktrace_sdk ${_ktrace_library_type} ${KTRACE_LOG_SOURCES})
add_library(ktrace::sdk ALIAS ktrace_sdk)

# Internal log headers use ShouldTraceChannel(...) inline helpers that require this macro.
target_compile_definitions(ktrace_sdk PRIVATE KTRACE_NAMESPACE="ktrace")

target_include_directories(ktrace_sdk
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(ktrace_sdk PUBLIC spdlog::spdlog)

set_target_properties(ktrace_sdk PROPERTIES
    OUTPUT_NAME ktrace
    EXPORT_NAME sdk
)
