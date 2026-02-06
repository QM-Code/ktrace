set(BZ3_PROTO_FILES
    ${PROJECT_SOURCE_DIR}/src/game/protos/messages.proto
)
protobuf_generate_cpp(BZ3_PROTO_SRCS BZ3_PROTO_HDRS ${BZ3_PROTO_FILES})
add_library(bz3_proto STATIC
    ${BZ3_PROTO_SRCS}
    ${BZ3_PROTO_HDRS}
)
target_link_libraries(bz3_proto PUBLIC protobuf::libprotobuf)
target_include_directories(bz3_proto PUBLIC
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_BINARY_DIR}
)

add_executable(bz3
    ${PROJECT_SOURCE_DIR}/src/game/client/bootstrap.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/cli_options.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/net/client_connection.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/main.cpp
    ${PROJECT_SOURCE_DIR}/src/game/game.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/protocol_codec.cpp
)

add_executable(bz3-server
    ${PROJECT_SOURCE_DIR}/src/game/server/bootstrap.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/cli_options.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/domain/actor_system.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/domain/match_system.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/domain/session_system.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/domain/world_session.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/main.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/net/enet_event_source.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/net/event_source.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/runtime.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/server_game.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/protocol_codec.cpp
)

set_target_properties(bz3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_target_properties(bz3-server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_include_directories(bz3 PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/engine
    ${PROJECT_SOURCE_DIR}/src/game
)
target_include_directories(bz3-server PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/engine
    ${PROJECT_SOURCE_DIR}/src/game
)

target_link_libraries(bz3 PRIVATE karma_engine_client)
target_link_libraries(bz3 PRIVATE bz3_proto)
target_link_libraries(bz3-server PRIVATE karma_engine_core)
target_link_libraries(bz3-server PRIVATE bz3_proto)

if(TARGET enet_static)
    target_link_libraries(bz3 PRIVATE enet_static)
    target_link_libraries(bz3-server PRIVATE enet_static)
elseif(TARGET enet)
    target_link_libraries(bz3 PRIVATE enet)
    target_link_libraries(bz3-server PRIVATE enet)
endif()
