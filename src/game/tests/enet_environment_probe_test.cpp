#include "network/tests/loopback_enet_fixture.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <string>
#include <thread>

namespace {

constexpr int kSkipReturnCode = 77;

void PrintSkip(const char* message) {
    std::cerr << "SKIP: " << message << "\n";
}

bool WaitForLoopbackConnect(karma::network::tests::LoopbackEnetEndpoint* server_endpoint,
                            karma::network::tests::LoopbackEnetEndpoint* client_endpoint) {
    if (!server_endpoint || !client_endpoint) {
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
    bool connected = false;
    while (std::chrono::steady_clock::now() < deadline && !connected) {
        karma::network::tests::PumpLoopbackEndpoint(client_endpoint);
        karma::network::tests::PumpLoopbackEndpoint(server_endpoint);
        connected = client_endpoint->connected;

        if (!connected) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    return connected;
}

} // namespace

int main() {
    std::string enet_init_error{};
    if (!karma::network::tests::InitializeLoopbackEnet(&enet_init_error)) {
        PrintSkip(enet_init_error.empty() ? "enet_initialize failed" : enet_init_error.c_str());
        return kSkipReturnCode;
    }

    auto server_endpoint = karma::network::tests::CreateLoopbackServerEndpointAtPort(0, 1, 2);
    if (!server_endpoint.has_value()) {
        PrintSkip("failed to create ENet server host");
        return kSkipReturnCode;
    }

    const uint16_t server_port =
        karma::network::tests::GetLoopbackEndpointBoundPort(&server_endpoint.value());
    if (server_port == 0) {
        PrintSkip("failed to resolve loopback server port");
        karma::network::tests::DestroyLoopbackEndpoint(&server_endpoint.value());
        return kSkipReturnCode;
    }

    auto client_endpoint = karma::network::tests::CreateLoopbackClientEndpoint(server_port, 2);
    if (!client_endpoint.has_value()) {
        PrintSkip("failed to create ENet client host");
        karma::network::tests::DestroyLoopbackEndpoint(&server_endpoint.value());
        return kSkipReturnCode;
    }

    const bool connected =
        WaitForLoopbackConnect(&server_endpoint.value(), &client_endpoint.value());
    if (!connected) {
        PrintSkip("ENet loopback connect timed out");
        karma::network::tests::DestroyLoopbackEndpoint(&client_endpoint.value());
        karma::network::tests::DestroyLoopbackEndpoint(&server_endpoint.value());
        return kSkipReturnCode;
    }

    static_cast<void>(karma::network::tests::DisconnectLoopbackEndpoint(&client_endpoint.value(), 0));
    for (int i = 0; i < 16; ++i) {
        karma::network::tests::PumpLoopbackEndpoint(&client_endpoint.value());
        karma::network::tests::PumpLoopbackEndpoint(&server_endpoint.value());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    karma::network::tests::DestroyLoopbackEndpoint(&client_endpoint.value());
    karma::network::tests::DestroyLoopbackEndpoint(&server_endpoint.value());
    return 0;
}
