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

set(BZ3_SERVER_TRANSPORT_EVENT_SOURCE_SRCS
    ${PROJECT_SOURCE_DIR}/src/game/server/net/transport_event_source.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/net/transport_event_source/common.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/net/transport_event_source/core.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/net/transport_event_source/receive.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/net/transport_event_source/join.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/net/transport_event_source/events.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/net/transport_event_source/send.cpp
)

set(BZ3_PROTOCOL_CODEC_SRCS
    ${PROJECT_SOURCE_DIR}/src/game/net/protocol_codec/wire_common.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/protocol_codec/decode_client.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/protocol_codec/decode_server.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/protocol_codec/encode_client.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/protocol_codec/encode_server_core.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/protocol_codec/encode_server_transfer.cpp
)

set(BZ3_CLIENT_GAME_SRCS
    ${PROJECT_SOURCE_DIR}/src/game/client/game/lifecycle.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/game/tank_entity.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/game/tank_collision.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/game/tank_motion.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/game/tank_camera.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/game/audio.cpp
)

set(BZ3_CLIENT_RUNTIME_SRCS
    ${PROJECT_SOURCE_DIR}/src/game/client/runtime/run.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/runtime/engine_config.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/runtime/startup_options.cpp
)

set(BZ3_SERVER_RUNTIME_SRCS
    ${PROJECT_SOURCE_DIR}/src/game/server/runtime/run.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/runtime/config.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/runtime/signal.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/runtime/join_result_bridge.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/runtime/event_loop.cpp
)

add_executable(bz3
    ${PROJECT_SOURCE_DIR}/src/game/client/domain/tank_drive_controller.cpp
    ${BZ3_CLIENT_RUNTIME_SRCS}
    ${PROJECT_SOURCE_DIR}/src/game/client/net/connection/core.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/net/connection/inbound/dispatch.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/net/connection/inbound/bootstrap.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/net/connection/inbound/world_transfer.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/net/connection/inbound/session_events.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/net/connection/inbound/transport_lifecycle.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/net/connection/outbound.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/net/world_package/bridge.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/net/world_package/apply.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/main.cpp
    ${BZ3_CLIENT_GAME_SRCS}
    ${BZ3_PROTOCOL_CODEC_SRCS}
)

add_executable(bz3-server
    ${PROJECT_SOURCE_DIR}/src/game/server/domain/actor_system.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/domain/match_system.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/domain/session_system.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/domain/shot_system.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/domain/world_session.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/main.cpp
    ${BZ3_SERVER_TRANSPORT_EVENT_SOURCE_SRCS}
    ${PROJECT_SOURCE_DIR}/src/game/server/net/event_source.cpp
    ${BZ3_SERVER_RUNTIME_SRCS}
    ${PROJECT_SOURCE_DIR}/src/game/server/runtime_event_rules.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/server_game.cpp
    ${BZ3_PROTOCOL_CODEC_SRCS}
)

set_target_properties(bz3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_target_properties(bz3-server PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

target_include_directories(bz3 PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/game
)
target_include_directories(bz3-server PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src/game
)

target_link_libraries(bz3 PRIVATE karma_engine_client)
target_link_libraries(bz3 PRIVATE bz3_proto)
target_link_libraries(bz3-server PRIVATE karma_engine_core)
target_link_libraries(bz3-server PRIVATE bz3_proto)

if(BUILD_TESTING)
    add_executable(transport_environment_probe_test
        ${PROJECT_SOURCE_DIR}/src/game/tests/transport_environment_probe_test.cpp
    )
    target_include_directories(transport_environment_probe_test PRIVATE
        ${PROJECT_SOURCE_DIR}/src/engine
    )
    if(TARGET network_test_support)
        target_link_libraries(transport_environment_probe_test PRIVATE network_test_support)
    endif()
    add_test(NAME transport_environment_probe_test
             COMMAND transport_environment_probe_test)
    set_tests_properties(transport_environment_probe_test PROPERTIES
                         RUN_SERIAL TRUE
                         SKIP_RETURN_CODE 77)

    add_executable(server_net_contract_test
        ${PROJECT_SOURCE_DIR}/src/game/tests/server_net_contract_test.cpp
        ${BZ3_PROTOCOL_CODEC_SRCS}
        ${PROJECT_SOURCE_DIR}/src/game/server/net/event_source.cpp
        ${BZ3_SERVER_TRANSPORT_EVENT_SOURCE_SRCS}
    )
    target_include_directories(server_net_contract_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/game
    )
    target_link_libraries(server_net_contract_test PRIVATE karma_engine_core)
    target_link_libraries(server_net_contract_test PRIVATE bz3_proto)

    add_test(NAME server_net_contract_test
             COMMAND server_net_contract_test)

    add_executable(server_runtime_event_rules_test
        ${PROJECT_SOURCE_DIR}/src/game/tests/server_runtime_event_rules_test.cpp
        ${PROJECT_SOURCE_DIR}/src/game/server/runtime_event_rules.cpp
    )
    target_include_directories(server_runtime_event_rules_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/game
    )
    add_test(NAME server_runtime_event_rules_test
             COMMAND server_runtime_event_rules_test)

    add_executable(client_runtime_cli_contract_test
        ${PROJECT_SOURCE_DIR}/src/game/tests/client_runtime_cli_contract_test.cpp
    )
    target_include_directories(client_runtime_cli_contract_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/game
    )
    target_link_libraries(client_runtime_cli_contract_test PRIVATE karma_engine_client)
    add_test(NAME client_runtime_cli_contract_test
             COMMAND client_runtime_cli_contract_test)

    add_executable(community_heartbeat_integration_test
        ${PROJECT_SOURCE_DIR}/src/game/tests/community_heartbeat_integration_test.cpp
    )
    target_include_directories(community_heartbeat_integration_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/game
    )
    target_link_libraries(community_heartbeat_integration_test PRIVATE karma_engine_core)
    add_test(NAME community_heartbeat_integration_test
             COMMAND community_heartbeat_integration_test)

    add_executable(server_join_runtime_contract_test
        ${PROJECT_SOURCE_DIR}/src/game/tests/server_join_runtime_contract_test.cpp
    )
    target_include_directories(server_join_runtime_contract_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/game
    )
    target_link_libraries(server_join_runtime_contract_test PRIVATE karma_engine_core)
    add_test(NAME server_join_runtime_contract_test
             COMMAND server_join_runtime_contract_test)

    add_executable(server_session_runtime_contract_test
        ${PROJECT_SOURCE_DIR}/src/game/tests/server_session_runtime_contract_test.cpp
    )
    target_include_directories(server_session_runtime_contract_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/game
    )
    target_link_libraries(server_session_runtime_contract_test PRIVATE karma_engine_core)
    add_test(NAME server_session_runtime_contract_test
             COMMAND server_session_runtime_contract_test)

    add_executable(transport_loopback_integration_test
        ${PROJECT_SOURCE_DIR}/src/game/tests/transport_loopback_integration_test.cpp
        ${BZ3_PROTOCOL_CODEC_SRCS}
        ${BZ3_SERVER_TRANSPORT_EVENT_SOURCE_SRCS}
    )
    target_include_directories(transport_loopback_integration_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/game
        ${PROJECT_SOURCE_DIR}/src/engine
    )
    target_link_libraries(transport_loopback_integration_test PRIVATE karma_engine_core)
    target_link_libraries(transport_loopback_integration_test PRIVATE bz3_proto)
    if(TARGET network_test_support)
        target_link_libraries(transport_loopback_integration_test PRIVATE network_test_support)
    endif()

    add_test(NAME transport_loopback_integration_test
             COMMAND transport_loopback_integration_test)
    set_tests_properties(transport_loopback_integration_test PROPERTIES
                         RUN_SERIAL TRUE
                         SKIP_RETURN_CODE 77)

    add_executable(transport_multiclient_loopback_test
        ${PROJECT_SOURCE_DIR}/src/game/tests/transport_multiclient_loopback_test.cpp
        ${BZ3_PROTOCOL_CODEC_SRCS}
        ${BZ3_SERVER_TRANSPORT_EVENT_SOURCE_SRCS}
    )
    target_include_directories(transport_multiclient_loopback_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/game
        ${PROJECT_SOURCE_DIR}/src/engine
    )
    target_link_libraries(transport_multiclient_loopback_test PRIVATE karma_engine_core)
    target_link_libraries(transport_multiclient_loopback_test PRIVATE bz3_proto)
    if(TARGET network_test_support)
        target_link_libraries(transport_multiclient_loopback_test PRIVATE network_test_support)
    endif()

    add_test(NAME transport_multiclient_loopback_test
             COMMAND transport_multiclient_loopback_test)
    set_tests_properties(transport_multiclient_loopback_test PROPERTIES
                         RUN_SERIAL TRUE
                         SKIP_RETURN_CODE 77)

    add_executable(transport_disconnect_lifecycle_integration_test
        ${PROJECT_SOURCE_DIR}/src/game/tests/transport_disconnect_lifecycle_integration_test.cpp
        ${BZ3_PROTOCOL_CODEC_SRCS}
        ${BZ3_SERVER_TRANSPORT_EVENT_SOURCE_SRCS}
    )
    target_include_directories(transport_disconnect_lifecycle_integration_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/game
        ${PROJECT_SOURCE_DIR}/src/engine
    )
    target_link_libraries(transport_disconnect_lifecycle_integration_test PRIVATE karma_engine_core)
    target_link_libraries(transport_disconnect_lifecycle_integration_test PRIVATE bz3_proto)
    if(TARGET network_test_support)
        target_link_libraries(transport_disconnect_lifecycle_integration_test PRIVATE network_test_support)
    endif()

    add_test(NAME transport_disconnect_lifecycle_integration_test
             COMMAND transport_disconnect_lifecycle_integration_test)
    set_tests_properties(transport_disconnect_lifecycle_integration_test PROPERTIES
                         RUN_SERIAL TRUE
                         SKIP_RETURN_CODE 77)

    add_executable(client_world_package_safety_integration_test
        ${PROJECT_SOURCE_DIR}/src/game/tests/client_world_package_safety_integration_test.cpp
        ${PROJECT_SOURCE_DIR}/src/game/client/net/connection/core.cpp
        ${PROJECT_SOURCE_DIR}/src/game/client/net/connection/inbound/dispatch.cpp
        ${PROJECT_SOURCE_DIR}/src/game/client/net/connection/inbound/bootstrap.cpp
        ${PROJECT_SOURCE_DIR}/src/game/client/net/connection/inbound/world_transfer.cpp
        ${PROJECT_SOURCE_DIR}/src/game/client/net/connection/inbound/session_events.cpp
        ${PROJECT_SOURCE_DIR}/src/game/client/net/connection/inbound/transport_lifecycle.cpp
        ${PROJECT_SOURCE_DIR}/src/game/client/net/connection/outbound.cpp
        ${PROJECT_SOURCE_DIR}/src/game/client/net/world_package/bridge.cpp
        ${PROJECT_SOURCE_DIR}/src/game/client/net/world_package/apply.cpp
        ${BZ3_PROTOCOL_CODEC_SRCS}
        ${BZ3_SERVER_TRANSPORT_EVENT_SOURCE_SRCS}
    )
    target_include_directories(client_world_package_safety_integration_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/game
        ${PROJECT_SOURCE_DIR}/src/engine
    )
    target_link_libraries(client_world_package_safety_integration_test PRIVATE karma_engine_core)
    target_link_libraries(client_world_package_safety_integration_test PRIVATE bz3_proto)
    if(TARGET network_test_support)
        target_link_libraries(client_world_package_safety_integration_test PRIVATE network_test_support)
    endif()

    add_test(NAME client_world_package_safety_integration_test
             COMMAND client_world_package_safety_integration_test)
    set_tests_properties(client_world_package_safety_integration_test PROPERTIES
                         RUN_SERIAL TRUE
                         SKIP_RETURN_CODE 77)

    add_executable(tank_drive_controller_test
        ${PROJECT_SOURCE_DIR}/src/game/tests/tank_drive_controller_test.cpp
        ${PROJECT_SOURCE_DIR}/src/game/client/domain/tank_drive_controller.cpp
    )
    target_include_directories(tank_drive_controller_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/game
    )
    target_link_libraries(tank_drive_controller_test PRIVATE karma_engine_core)
    add_test(NAME tank_drive_controller_test
             COMMAND tank_drive_controller_test)
endif()
