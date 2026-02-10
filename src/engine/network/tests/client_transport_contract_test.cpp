#include "karma/network/client_transport.hpp"
#include "network/transport_pump_normalizer.hpp"

#include <spdlog/logger.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace {

using karma::network::ClientTransportEvent;
using karma::network::ClientTransportEventType;

class WarningCaptureSink final : public spdlog::sinks::base_sink<std::mutex> {
 public:
    bool ContainsWarning(std::string_view snippet) {
        std::lock_guard<std::mutex> lock(this->mutex_);
        for (const auto& warning : warnings_) {
            if (warning.find(snippet) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

 protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        if (msg.level != spdlog::level::warn) {
            return;
        }
        warnings_.emplace_back(msg.payload.data(), msg.payload.size());
    }

    void flush_() override {}

 private:
    std::vector<std::string> warnings_{};
};

bool Fail(const std::string& message) {
    std::cerr << message << "\n";
    return false;
}

bool Expect(bool condition, const std::string& message) {
    if (!condition) {
        return Fail(message);
    }
    return true;
}

std::vector<ClientTransportEventType> Normalize(std::vector<ClientTransportEvent> staged_events) {
    std::vector<ClientTransportEvent> out_events{};
    karma::network::detail::NormalizePumpEvents(&out_events,
                                                &staged_events,
                                                ClientTransportEventType::Connected,
                                                ClientTransportEventType::Received,
                                                ClientTransportEventType::Disconnected);

    std::vector<ClientTransportEventType> out_types{};
    out_types.reserve(out_events.size());
    for (const auto& event : out_events) {
        out_types.push_back(event.type);
    }
    return out_types;
}

bool TestConnectedReceivedDisconnectedOrdering() {
    std::vector<ClientTransportEvent> staged_events{};

    ClientTransportEvent disconnected{};
    disconnected.type = ClientTransportEventType::Disconnected;
    staged_events.push_back(std::move(disconnected));

    ClientTransportEvent received{};
    received.type = ClientTransportEventType::Received;
    received.payload.push_back(std::byte{0x7f});
    staged_events.push_back(std::move(received));

    ClientTransportEvent connected{};
    connected.type = ClientTransportEventType::Connected;
    staged_events.push_back(std::move(connected));

    const auto normalized = Normalize(std::move(staged_events));
    return Expect(normalized.size() == 3, "expected three normalized client events") &&
           Expect(normalized[0] == ClientTransportEventType::Connected,
                  "client event[0] should be Connected") &&
           Expect(normalized[1] == ClientTransportEventType::Received,
                  "client event[1] should be Received") &&
           Expect(normalized[2] == ClientTransportEventType::Disconnected,
                  "client event[2] should be Disconnected");
}

bool TestDisconnectIsTerminalWithinPumpCycle() {
    std::vector<ClientTransportEvent> staged_events{};

    ClientTransportEvent first_received{};
    first_received.type = ClientTransportEventType::Received;
    first_received.payload.push_back(std::byte{0x01});
    staged_events.push_back(std::move(first_received));

    ClientTransportEvent disconnected{};
    disconnected.type = ClientTransportEventType::Disconnected;
    staged_events.push_back(std::move(disconnected));

    ClientTransportEvent second_received{};
    second_received.type = ClientTransportEventType::Received;
    second_received.payload.push_back(std::byte{0x02});
    staged_events.push_back(std::move(second_received));

    const auto normalized = Normalize(std::move(staged_events));
    return Expect(normalized.size() == 3, "expected three normalized client events") &&
           Expect(normalized[0] == ClientTransportEventType::Received,
                  "client event[0] should be Received") &&
           Expect(normalized[1] == ClientTransportEventType::Received,
                  "client event[1] should be Received") &&
           Expect(normalized[2] == ClientTransportEventType::Disconnected,
                  "client disconnect must be terminal in cycle ordering");
}

bool TestReconnectConnectedPrecedesPayloadInSameCycle() {
    std::vector<ClientTransportEvent> staged_events{};

    // Reconnect can append Connected after receive handling in the same poll cycle.
    ClientTransportEvent received{};
    received.type = ClientTransportEventType::Received;
    received.payload.push_back(std::byte{0x33});
    staged_events.push_back(std::move(received));

    ClientTransportEvent reconnected{};
    reconnected.type = ClientTransportEventType::Connected;
    staged_events.push_back(std::move(reconnected));

    const auto normalized = Normalize(std::move(staged_events));
    return Expect(normalized.size() == 2, "expected two normalized client events") &&
           Expect(normalized[0] == ClientTransportEventType::Connected,
                  "reconnect Connected should be emitted before payload events") &&
           Expect(normalized[1] == ClientTransportEventType::Received,
                  "payload event should follow reconnect Connected");
}

bool TestSinglePeerLifecycleEdgesRemainNormalized() {
    std::vector<ClientTransportEvent> staged_events{};

    ClientTransportEvent disconnected_first{};
    disconnected_first.type = ClientTransportEventType::Disconnected;
    staged_events.push_back(std::move(disconnected_first));

    ClientTransportEvent connected_first{};
    connected_first.type = ClientTransportEventType::Connected;
    staged_events.push_back(std::move(connected_first));

    ClientTransportEvent received{};
    received.type = ClientTransportEventType::Received;
    received.payload.push_back(std::byte{0x55});
    staged_events.push_back(std::move(received));

    ClientTransportEvent connected_second{};
    connected_second.type = ClientTransportEventType::Connected;
    staged_events.push_back(std::move(connected_second));

    ClientTransportEvent disconnected_second{};
    disconnected_second.type = ClientTransportEventType::Disconnected;
    staged_events.push_back(std::move(disconnected_second));

    const auto normalized = Normalize(std::move(staged_events));
    return Expect(normalized.size() == 5, "expected five normalized client lifecycle-edge events") &&
           Expect(normalized[0] == ClientTransportEventType::Connected,
                  "client lifecycle-edge event[0] should be Connected") &&
           Expect(normalized[1] == ClientTransportEventType::Connected,
                  "client lifecycle-edge event[1] should be Connected") &&
           Expect(normalized[2] == ClientTransportEventType::Received,
                  "client lifecycle-edge event[2] should be Received") &&
           Expect(normalized[3] == ClientTransportEventType::Disconnected,
                  "client lifecycle-edge event[3] should be Disconnected") &&
           Expect(normalized[4] == ClientTransportEventType::Disconnected,
                  "client lifecycle-edge event[4] should be Disconnected");
}

bool TestUnregisteredBackendWarnsAndFails() {
    auto sink = std::make_shared<WarningCaptureSink>();
    auto logger = std::make_shared<spdlog::logger>("client_transport_contract_logger", sink);
    logger->set_level(spdlog::level::trace);

    auto previous_logger = spdlog::default_logger();
    spdlog::set_default_logger(logger);

    karma::network::ClientTransportConfig config{};
    config.backend_name = "unregistered-client-backend-contract";
    auto transport = karma::network::CreateClientTransport(config);

    spdlog::set_default_logger(previous_logger);

    return Expect(!transport, "unregistered client backend should fail transport creation") &&
           Expect(sink->ContainsWarning("unregistered client transport backend='unregistered-client-backend-contract'"),
                  "unregistered client backend should emit warning-level observability");
}

} // namespace

int main() {
    if (!TestConnectedReceivedDisconnectedOrdering()) {
        return 1;
    }
    if (!TestDisconnectIsTerminalWithinPumpCycle()) {
        return 1;
    }
    if (!TestReconnectConnectedPrecedesPayloadInSameCycle()) {
        return 1;
    }
    if (!TestSinglePeerLifecycleEdgesRemainNormalized()) {
        return 1;
    }
    if (!TestUnregisteredBackendWarnsAndFails()) {
        return 1;
    }
    return 0;
}
