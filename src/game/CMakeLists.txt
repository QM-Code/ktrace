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
    ${PROJECT_SOURCE_DIR}/src/game/client/community_server_list.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/net/client_connection.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/main.cpp
    ${PROJECT_SOURCE_DIR}/src/game/game.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/protocol_codec.cpp
)

add_executable(bz3-server
    ${PROJECT_SOURCE_DIR}/src/game/server/bootstrap.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/cli_options.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/community_heartbeat.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/domain/actor_system.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/domain/match_system.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/domain/session_system.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/domain/world_session.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/heartbeat_client.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/main.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/net/transport_event_source.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/net/event_source.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/runtime.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/runtime_event_rules.cpp
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
        ${PROJECT_SOURCE_DIR}/src/game/net/protocol_codec.cpp
        ${PROJECT_SOURCE_DIR}/src/game/server/net/event_source.cpp
        ${PROJECT_SOURCE_DIR}/src/game/server/net/transport_event_source.cpp
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

    add_executable(community_heartbeat_integration_test
        ${PROJECT_SOURCE_DIR}/src/game/tests/community_heartbeat_integration_test.cpp
        ${PROJECT_SOURCE_DIR}/src/game/server/cli_options.cpp
        ${PROJECT_SOURCE_DIR}/src/game/server/community_heartbeat.cpp
        ${PROJECT_SOURCE_DIR}/src/game/server/heartbeat_client.cpp
    )
    target_include_directories(community_heartbeat_integration_test PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src/game
    )
    target_link_libraries(community_heartbeat_integration_test PRIVATE karma_engine_core)
    add_test(NAME community_heartbeat_integration_test
             COMMAND community_heartbeat_integration_test)

    add_executable(transport_loopback_integration_test
        ${PROJECT_SOURCE_DIR}/src/game/tests/transport_loopback_integration_test.cpp
        ${PROJECT_SOURCE_DIR}/src/game/net/protocol_codec.cpp
        ${PROJECT_SOURCE_DIR}/src/game/server/net/transport_event_source.cpp
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
        ${PROJECT_SOURCE_DIR}/src/game/net/protocol_codec.cpp
        ${PROJECT_SOURCE_DIR}/src/game/server/net/transport_event_source.cpp
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
        ${PROJECT_SOURCE_DIR}/src/game/net/protocol_codec.cpp
        ${PROJECT_SOURCE_DIR}/src/game/server/net/transport_event_source.cpp
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
        ${PROJECT_SOURCE_DIR}/src/game/client/net/client_connection.cpp
        ${PROJECT_SOURCE_DIR}/src/game/net/protocol_codec.cpp
        ${PROJECT_SOURCE_DIR}/src/game/server/net/transport_event_source.cpp
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
endif()
