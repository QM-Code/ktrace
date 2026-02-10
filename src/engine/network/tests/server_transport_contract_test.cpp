#include "karma/network/server_transport.hpp"
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

using karma::network::ServerTransportEvent;
using karma::network::ServerTransportEventType;

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

std::vector<ServerTransportEvent> NormalizeEvents(std::vector<ServerTransportEvent> staged_events) {
    std::vector<ServerTransportEvent> out_events{};
    karma::network::detail::NormalizePumpEventsPerKey(&out_events,
                                                      &staged_events,
                                                      ServerTransportEventType::Connected,
                                                      ServerTransportEventType::Received,
                                                      ServerTransportEventType::Disconnected,
                                                      [](const ServerTransportEvent& event) {
                                                          return event.peer;
                                                      });
    return out_events;
}

std::vector<ServerTransportEventType> NormalizeTypes(std::vector<ServerTransportEvent> staged_events) {
    const auto out_events = NormalizeEvents(std::move(staged_events));
    std::vector<ServerTransportEventType> out_types{};
    out_types.reserve(out_events.size());
    for (const auto& event : out_events) {
        out_types.push_back(event.type);
    }
    return out_types;
}

ServerTransportEvent MakeEvent(ServerTransportEventType type, uint8_t payload_byte, uint32_t peer = 17) {
    ServerTransportEvent event{};
    event.type = type;
    event.peer = peer;
    event.peer_ip = "127.0.0.1";
    event.peer_port = static_cast<uint16_t>(30000 + peer);
    if (type == ServerTransportEventType::Received) {
        event.payload.push_back(static_cast<std::byte>(payload_byte));
    }
    return event;
}

bool TestConnectedReceivedDisconnectedOrdering() {
    std::vector<ServerTransportEvent> staged_events{};
    staged_events.push_back(MakeEvent(ServerTransportEventType::Disconnected, 0));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Received, 0x10));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Connected, 0));

    const auto normalized = NormalizeTypes(std::move(staged_events));
    return Expect(normalized.size() == 3, "expected three normalized server events") &&
           Expect(normalized[0] == ServerTransportEventType::Connected,
                  "server event[0] should be Connected") &&
           Expect(normalized[1] == ServerTransportEventType::Received,
                  "server event[1] should be Received") &&
           Expect(normalized[2] == ServerTransportEventType::Disconnected,
                  "server event[2] should be Disconnected");
}

bool TestDisconnectIsTerminalWithinPumpCycle() {
    std::vector<ServerTransportEvent> staged_events{};
    staged_events.push_back(MakeEvent(ServerTransportEventType::Received, 0x01));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Disconnected, 0));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Received, 0x02));

    const auto normalized = NormalizeTypes(std::move(staged_events));
    return Expect(normalized.size() == 3, "expected three normalized server events") &&
           Expect(normalized[0] == ServerTransportEventType::Received,
                  "server event[0] should be Received") &&
           Expect(normalized[1] == ServerTransportEventType::Received,
                  "server event[1] should be Received") &&
           Expect(normalized[2] == ServerTransportEventType::Disconnected,
                  "server disconnect must be terminal in cycle ordering");
}

bool TestPerPeerOrderingAndDisconnectEdges() {
    std::vector<ServerTransportEvent> staged_events{};

    // Peer 2 appears first; peer ordering should follow first appearance.
    staged_events.push_back(MakeEvent(ServerTransportEventType::Received, 0x21, 2));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Disconnected, 0x00, 1));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Connected, 0x00, 2));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Received, 0x11, 1));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Disconnected, 0x00, 2));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Connected, 0x00, 1));

    const auto normalized = NormalizeEvents(std::move(staged_events));
    return Expect(normalized.size() == 6, "expected six normalized per-peer server events") &&
           Expect(normalized[0].peer == 2 &&
                      normalized[0].type == ServerTransportEventType::Connected,
                  "peer-2 event[0] should be Connected") &&
           Expect(normalized[1].peer == 2 &&
                      normalized[1].type == ServerTransportEventType::Received,
                  "peer-2 event[1] should be Received") &&
           Expect(normalized[2].peer == 2 &&
                      normalized[2].type == ServerTransportEventType::Disconnected,
                  "peer-2 event[2] should be Disconnected terminally") &&
           Expect(normalized[3].peer == 1 &&
                      normalized[3].type == ServerTransportEventType::Connected,
                  "peer-1 event[3] should be Connected") &&
           Expect(normalized[4].peer == 1 &&
                      normalized[4].type == ServerTransportEventType::Received,
                  "peer-1 event[4] should be Received") &&
           Expect(normalized[5].peer == 1 &&
                      normalized[5].type == ServerTransportEventType::Disconnected,
                  "peer-1 event[5] should be Disconnected terminally");
}

bool TestUnregisteredBackendWarnsAndFails() {
    auto sink = std::make_shared<WarningCaptureSink>();
    auto logger = std::make_shared<spdlog::logger>("server_transport_contract_logger", sink);
    logger->set_level(spdlog::level::trace);

    auto previous_logger = spdlog::default_logger();
    spdlog::set_default_logger(logger);

    karma::network::ServerTransportConfig config{};
    config.backend_name = "unregistered-server-backend-contract";
    auto transport = karma::network::CreateServerTransport(config);

    spdlog::set_default_logger(previous_logger);

    return Expect(!transport, "unregistered server backend should fail transport creation") &&
           Expect(sink->ContainsWarning("unregistered server transport backend='unregistered-server-backend-contract'"),
                  "unregistered server backend should emit warning-level observability");
}

} // namespace

int main() {
    if (!TestConnectedReceivedDisconnectedOrdering()) {
        return 1;
    }
    if (!TestDisconnectIsTerminalWithinPumpCycle()) {
        return 1;
    }
    if (!TestPerPeerOrderingAndDisconnectEdges()) {
        return 1;
    }
    if (!TestUnregisteredBackendWarnsAndFails()) {
        return 1;
    }
    return 0;
}
