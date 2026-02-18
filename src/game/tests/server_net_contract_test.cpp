#include "net/protocol.hpp"
#include "net/protocol_codec.hpp"
#include "server/net/transport_event_source.hpp"
#include "server/net/event_source.hpp"

#include "karma/cli/server/app_options.hpp"
#include "karma/common/content/archive.hpp"
#include "karma/common/content/manifest.hpp"
#include "karma/common/content/sync_facade.hpp"
#include "karma/network/server/auth/preauth.hpp"
#include "karma/network/transport/server.hpp"
#include "karma/common/config/store.hpp"
#include "karma/common/serialization/json.hpp"
#include "karma/common/logging/logging.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

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

std::filesystem::path MakeTestConfigPath(const char* suffix) {
    const auto nonce = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path() /
           ("bz3-server-net-contract-" + std::string(suffix) + "-" + std::to_string(nonce) + ".json");
}

std::filesystem::path MakeTempWorldDirPath(const char* suffix) {
    const auto nonce = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path() /
           ("bz3-server-net-content-sync-" + std::string(suffix) + "-" + std::to_string(nonce));
}

bool WriteTextFile(const std::filesystem::path& path, std::string_view content) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return false;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return out.good();
}

bool WriteDeterministicBinaryFile(const std::filesystem::path& path, size_t size, uint8_t seed) {
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
        return false;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
        return false;
    }

    std::vector<char> bytes(size, '\0');
    for (size_t i = 0; i < size; ++i) {
        bytes[i] = static_cast<char>((static_cast<uint32_t>(seed) + static_cast<uint32_t>(i * 31U)) & 0xFFU);
    }
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    return out.good();
}

bool InitializeConfigForEvents(const karma::common::serialization::Value& startup_events, const char* suffix) {
    karma::common::config::ConfigStore::Initialize({}, MakeTestConfigPath(suffix));
    return karma::common::config::ConfigStore::Set("server.startupEvents", startup_events);
}

std::vector<bz3::server::net::ServerInputEvent> CollectScheduledEvents(
    bz3::server::net::ServerEventSource& source,
    size_t expected_count,
    std::chrono::milliseconds timeout) {
    std::vector<bz3::server::net::ServerInputEvent> out{};
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (out.size() < expected_count && std::chrono::steady_clock::now() < deadline) {
        auto batch = source.poll();
        out.insert(out.end(), batch.begin(), batch.end());
        if (out.size() >= expected_count) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return out;
}

class FakeServerTransport final : public karma::network::ServerTransport {
 public:
    explicit FakeServerTransport(std::string backend_name)
        : backend_name_(std::move(backend_name)) {}

    bool isReady() const override { return true; }

    const char* backendName() const override { return backend_name_.c_str(); }

    size_t poll(const karma::network::ServerTransportPollOptions&,
                std::vector<karma::network::ServerTransportEvent>* out_events) override {
        if (out_events) {
            out_events->clear();
        }
        return 0;
    }

    bool sendReliable(karma::network::PeerToken, const std::vector<std::byte>&) override {
        return true;
    }

    void disconnect(karma::network::PeerToken, uint32_t) override {}

 private:
    std::string backend_name_{};
};

struct CapturedTransportConfig {
    karma::network::ServerTransportBackend backend = karma::network::ServerTransportBackend::Auto;
    std::string backend_name{};
    uint16_t listen_port = 0;
    size_t max_clients = 0;
    size_t channel_count = 0;
};

bool InitializeConfigForServerTransportBackend(std::string_view backend_value, const char* suffix) {
    karma::common::config::ConfigStore::Initialize({}, MakeTestConfigPath(suffix));
    return karma::common::config::ConfigStore::Set("network.ServerTransportBackend",
                                           std::string(backend_value));
}

bool TestJoinRequestRoundTrip() {
    std::vector<bz3::net::WorldManifestEntry> cached_manifest{};
    cached_manifest.push_back(bz3::net::WorldManifestEntry{"config.json", 128, "aaa111"});
    cached_manifest.push_back(bz3::net::WorldManifestEntry{"world.glb", 4096, "bbb222"});
    const auto payload = bz3::net::EncodeClientJoinRequest(
        "mike",
        bz3::net::kProtocolVersion,
        "pkg-hash-123",
        "world-abc",
        "rev-7",
        "content-hash-xyz",
        "manifest-hash-42",
        17,
        cached_manifest,
        "secret-token");
    if (payload.empty()) {
        return Fail("EncodeClientJoinRequest returned empty payload");
    }

    const auto decoded = bz3::net::DecodeClientMessage(payload.data(), payload.size());
    if (!decoded.has_value()) {
        return Fail("DecodeClientMessage failed for join request payload");
    }

    return Expect(decoded->type == bz3::net::ClientMessageType::JoinRequest, "join request type mismatch")
           && Expect(decoded->player_name == "mike", "join request player_name mismatch")
           && Expect(decoded->protocol_version == bz3::net::kProtocolVersion,
                     "join request protocol_version mismatch")
           && Expect(decoded->cached_world_hash == "pkg-hash-123", "join request cached_world_hash mismatch")
           && Expect(decoded->cached_world_id == "world-abc", "join request cached_world_id mismatch")
           && Expect(decoded->cached_world_revision == "rev-7", "join request cached_world_revision mismatch")
           && Expect(decoded->cached_world_content_hash == "content-hash-xyz",
                     "join request cached_world_content_hash mismatch")
           && Expect(decoded->cached_world_manifest_hash == "manifest-hash-42",
                     "join request cached_world_manifest_hash mismatch")
           && Expect(decoded->cached_world_manifest_file_count == 17,
                     "join request cached_world_manifest_file_count mismatch")
           && Expect(decoded->auth_payload == "secret-token", "join request auth_payload mismatch")
           && Expect(decoded->cached_world_manifest.size() == cached_manifest.size(),
                     "join request cached_world_manifest entry count mismatch")
           && Expect(decoded->cached_world_manifest[0].path == "config.json",
                     "join request cached_world_manifest[0].path mismatch")
           && Expect(decoded->cached_world_manifest[1].path == "world.glb",
                     "join request cached_world_manifest[1].path mismatch");
}

bool TestServerInitRoundTrip() {
    std::vector<bz3::net::WorldManifestEntry> manifest{};
    manifest.push_back(bz3::net::WorldManifestEntry{"config.json", 128, "aaa111"});
    manifest.push_back(bz3::net::WorldManifestEntry{"world.glb", 4096, "bbb222"});
    const std::vector<std::byte> world_data{
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04}};

    const auto payload = bz3::net::EncodeServerInit(7,
                                                    "bz3-server",
                                                    "common",
                                                    bz3::net::kProtocolVersion,
                                                    "pkg-hash-1",
                                                    12345,
                                                    "world-id-1",
                                                    "rev-5",
                                                    "content-hash-5",
                                                    "manifest-hash-9",
                                                    static_cast<uint32_t>(manifest.size()),
                                                    manifest,
                                                    world_data);
    if (payload.empty()) {
        return Fail("EncodeServerInit returned empty payload");
    }

    const auto decoded = bz3::net::DecodeServerMessage(payload.data(), payload.size());
    if (!decoded.has_value()) {
        return Fail("DecodeServerMessage failed for init payload");
    }

    return Expect(decoded->type == bz3::net::ServerMessageType::Init, "init message type mismatch")
           && Expect(decoded->client_id == 7, "init client_id mismatch")
           && Expect(decoded->server_name == "bz3-server", "init server_name mismatch")
           && Expect(decoded->world_name == "common", "init world_name mismatch")
           && Expect(decoded->protocol_version == bz3::net::kProtocolVersion, "init protocol_version mismatch")
           && Expect(decoded->world_hash == "pkg-hash-1", "init world_hash mismatch")
           && Expect(decoded->world_size == 12345, "init world_size mismatch")
           && Expect(decoded->world_id == "world-id-1", "init world_id mismatch")
           && Expect(decoded->world_revision == "rev-5", "init world_revision mismatch")
           && Expect(decoded->world_content_hash == "content-hash-5", "init world_content_hash mismatch")
           && Expect(decoded->world_manifest_hash == "manifest-hash-9", "init world_manifest_hash mismatch")
           && Expect(decoded->world_manifest_file_count == manifest.size(),
                     "init world_manifest_file_count mismatch")
           && Expect(decoded->world_manifest.size() == manifest.size(), "init manifest entry count mismatch")
           && Expect(decoded->world_manifest[0].path == "config.json", "init manifest[0].path mismatch")
           && Expect(decoded->world_manifest[1].path == "world.glb", "init manifest[1].path mismatch")
           && Expect(decoded->world_data.size() == world_data.size(), "init world_data size mismatch");
}

bool TestWorldTransferBeginRoundTrip() {
    const auto payload = bz3::net::EncodeServerWorldTransferBegin("xfer-1",
                                                                  "world-id-1",
                                                                  "rev-9",
                                                                  4096,
                                                                  1024,
                                                                  "pkg-hash-1",
                                                                  "content-hash-1",
                                                                  true,
                                                                  "world-id-1",
                                                                  "rev-8",
                                                                  "pkg-hash-0",
                                                                  "content-hash-0");
    if (payload.empty()) {
        return Fail("EncodeServerWorldTransferBegin returned empty payload");
    }

    const auto decoded = bz3::net::DecodeServerMessage(payload.data(), payload.size());
    if (!decoded.has_value()) {
        return Fail("DecodeServerMessage failed for world_transfer_begin payload");
    }

    return Expect(decoded->type == bz3::net::ServerMessageType::WorldTransferBegin,
                  "world_transfer_begin type mismatch")
           && Expect(decoded->transfer_id == "xfer-1", "world_transfer_begin transfer_id mismatch")
           && Expect(decoded->transfer_world_id == "world-id-1", "world_transfer_begin world_id mismatch")
           && Expect(decoded->transfer_world_revision == "rev-9",
                     "world_transfer_begin world_revision mismatch")
           && Expect(decoded->transfer_total_bytes == 4096, "world_transfer_begin total_bytes mismatch")
           && Expect(decoded->transfer_chunk_size == 1024, "world_transfer_begin chunk_size mismatch")
           && Expect(decoded->transfer_world_hash == "pkg-hash-1",
                     "world_transfer_begin world_hash mismatch")
           && Expect(decoded->transfer_world_content_hash == "content-hash-1",
                     "world_transfer_begin world_content_hash mismatch")
           && Expect(decoded->transfer_is_delta, "world_transfer_begin expected delta mode")
           && Expect(decoded->transfer_delta_base_world_id == "world-id-1",
                     "world_transfer_begin delta_base_world_id mismatch")
           && Expect(decoded->transfer_delta_base_world_revision == "rev-8",
                     "world_transfer_begin delta_base_world_revision mismatch")
           && Expect(decoded->transfer_delta_base_world_hash == "pkg-hash-0",
                     "world_transfer_begin delta_base_world_hash mismatch")
           && Expect(decoded->transfer_delta_base_world_content_hash == "content-hash-0",
                     "world_transfer_begin delta_base_world_content_hash mismatch");
}

bool TestDeltaSelectionFallsBackToFullWhenSavingsBelowMinimum() {
    const std::filesystem::path temp_root = MakeTempWorldDirPath("delta-min-savings");
    const std::filesystem::path base_dir = temp_root / "base";
    const std::filesystem::path incoming_dir = temp_root / "incoming";
    auto cleanup = [&]() {
        std::error_code cleanup_ec;
        std::filesystem::remove_all(temp_root, cleanup_ec);
    };
    auto fail = [&](const std::string& message) {
        cleanup();
        return Fail(message);
    };

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
    ec.clear();
    std::filesystem::create_directories(base_dir, ec);
    std::filesystem::create_directories(incoming_dir, ec);
    if (ec) {
        return fail("failed to create temp world directories for delta fallback test");
    }

    if (!WriteTextFile(base_dir / "config.json", "{\n  \"worldName\": \"delta-world\",\n  \"rev\": 1\n}\n")) {
        return fail("failed to write base config.json");
    }
    if (!WriteDeterministicBinaryFile(base_dir / "shared.bin", 24 * 1024, 0x11)) {
        return fail("failed to write base shared.bin");
    }
    if (!WriteDeterministicBinaryFile(base_dir / "payload.bin", 256 * 1024, 0x44)) {
        return fail("failed to write base payload.bin");
    }

    std::filesystem::copy_file(base_dir / "shared.bin",
                               incoming_dir / "shared.bin",
                               std::filesystem::copy_options::overwrite_existing,
                               ec);
    if (ec) {
        return fail("failed to copy shared.bin into incoming world");
    }
    if (!WriteTextFile(incoming_dir / "config.json", "{\n  \"worldName\": \"delta-world\",\n  \"rev\": 2\n}\n")) {
        return fail("failed to write incoming config.json");
    }
    if (!WriteDeterministicBinaryFile(incoming_dir / "payload.bin", 256 * 1024, 0x99)) {
        return fail("failed to write incoming payload.bin");
    }

    const auto base_summary = karma::common::content::ComputeDirectoryManifestSummary(base_dir);
    const auto incoming_summary = karma::common::content::ComputeDirectoryManifestSummary(incoming_dir);
    if (!base_summary.has_value() || !incoming_summary.has_value()) {
        return fail("failed to compute manifest summaries");
    }

    const auto incoming_world_package = karma::common::content::BuildWorldArchive(incoming_dir);
    if (incoming_world_package.empty()) {
        return fail("failed to build incoming world archive");
    }

    karma::common::logging::EnableTraceChannels("net.server");
    const karma::common::content::ServerContentSyncRequest request{
        .world_dir = incoming_dir,
        .world_name = "delta-world",
        .world_id = "delta-world-id",
        .world_revision = "rev-2",
        .world_package_hash = "pkg-rev-2",
        .world_content_hash = incoming_summary->content_hash,
        .world_manifest_hash = incoming_summary->manifest_hash,
        .world_manifest_file_count = static_cast<uint32_t>(incoming_summary->entries.size()),
        .world_manifest = incoming_summary->entries,
        .world_package = incoming_world_package,
        .cached_state =
            karma::common::content::ServerCachedContentState{
                .world_hash = "pkg-rev-1",
                .world_id = "delta-world-id",
                .world_revision = "rev-1",
                .world_content_hash = base_summary->content_hash,
                .world_manifest_hash = base_summary->manifest_hash,
                .world_manifest_file_count = static_cast<uint32_t>(base_summary->entries.size()),
                .world_manifest = base_summary->entries}};
    const auto plan = karma::common::content::BuildDefaultServerContentSyncPlan(request, "server-net-contract");

    const bool ok =
        Expect(plan.send_world_package, "expected send_world_package for delta fallback test") &&
        Expect(!plan.cache_hit, "expected cache miss for delta fallback test") &&
        Expect(plan.transfer_mode == "chunked_full", "expected full transfer mode when delta savings are too small") &&
        Expect(!plan.transfer_is_delta, "expected full transfer (not delta) when savings are below minimum") &&
        Expect(plan.transfer_selection_reason == "delta_savings_below_minimum",
               "unexpected transfer selection reason for small-savings delta fallback") &&
        Expect(plan.transfer_full_bytes > 0, "expected non-zero full_bytes for delta fallback test") &&
        Expect(plan.transfer_delta_bytes > 0, "expected available delta candidate bytes for fallback test") &&
        Expect(plan.transfer_delta_bytes < plan.transfer_full_bytes,
               "expected available delta candidate to be smaller than full payload") &&
        Expect(plan.transfer_saved_bytes > 0, "expected positive saved_bytes for fallback test") &&
        Expect(plan.transfer_saved_bytes < karma::common::content::kMinDeltaSelectionSavingsBytes,
               "expected saved_bytes below minimum threshold for fallback test") &&
        Expect(plan.transfer_bytes == plan.transfer_full_bytes,
               "expected transfer_bytes to remain full payload size after fallback");
    cleanup();
    return ok;
}

bool TestCreateShotRoundTrip() {
    const auto payload = bz3::net::EncodeServerCreateShot(
        42,
        9001,
        bz3::net::Vec3{1.5f, 2.5f, 3.5f},
        bz3::net::Vec3{4.0f, 5.0f, 6.0f});
    if (payload.empty()) {
        return Fail("EncodeServerCreateShot returned empty payload");
    }

    const auto decoded = bz3::net::DecodeServerMessage(payload.data(), payload.size());
    if (!decoded.has_value()) {
        return Fail("DecodeServerMessage failed for create_shot payload");
    }

    return Expect(decoded->type == bz3::net::ServerMessageType::CreateShot, "create_shot message type mismatch")
           && Expect(decoded->event_client_id == 42, "create_shot source_client_id mismatch")
           && Expect(decoded->event_shot_id == 9001, "create_shot global_shot_id mismatch")
           && Expect(decoded->event_shot_is_global, "create_shot expected global id flag");
}

bool TestRemoveShotRoundTrip() {
    const auto payload = bz3::net::EncodeServerRemoveShot(9001, true);
    if (payload.empty()) {
        return Fail("EncodeServerRemoveShot returned empty payload");
    }

    const auto decoded = bz3::net::DecodeServerMessage(payload.data(), payload.size());
    if (!decoded.has_value()) {
        return Fail("DecodeServerMessage failed for remove_shot payload");
    }

    return Expect(decoded->type == bz3::net::ServerMessageType::RemoveShot, "remove_shot message type mismatch")
           && Expect(decoded->event_shot_id == 9001, "remove_shot shot_id mismatch")
           && Expect(decoded->event_shot_is_global, "remove_shot global flag mismatch");
}

bool TestScriptedSourceParsesSpawnAndShot() {
    const auto events = karma::common::serialization::Array(
        {karma::common::serialization::Value{{"type", "join"}, {"atSeconds", 0.000}, {"clientId", 2}, {"playerName", "mike"}},
         karma::common::serialization::Value{{"type", "request_spawn"}, {"atSeconds", 0.005}, {"clientId", 2}},
         karma::common::serialization::Value{{"type", "create_shot"},
                            {"atSeconds", 0.010},
                            {"clientId", 2},
                            {"localShotId", 77},
                            {"posX", 1.0},
                            {"posY", 2.0},
                            {"posZ", 3.0},
                            {"velX", 4.0},
                            {"velY", 5.0},
                            {"velZ", 6.0}},
         karma::common::serialization::Value{{"type", "leave"}, {"atSeconds", 0.015}, {"clientId", 2}}});

    if (!InitializeConfigForEvents(events, "spawn-shot")) {
        return Fail("failed to initialize config for scripted source spawn/shot test");
    }

    karma::cli::server::AppOptions options{};
    const auto source = bz3::server::net::CreateServerEventSource(options, 0);
    if (!source) {
        return Fail("CreateServerEventSource returned null for scripted source test");
    }

    auto received = CollectScheduledEvents(*source, 4, std::chrono::milliseconds(500));
    if (received.size() != 4) {
        return Fail("scripted source did not emit expected 4 events");
    }

    return Expect(received[0].type == bz3::server::net::ServerInputEvent::Type::ClientJoin,
                  "scripted event[0] expected ClientJoin")
           && Expect(received[0].join.client_id == 2, "scripted join client_id mismatch")
           && Expect(received[0].join.player_name == "mike", "scripted join player_name mismatch")
           && Expect(received[1].type == bz3::server::net::ServerInputEvent::Type::ClientRequestSpawn,
                     "scripted event[1] expected ClientRequestSpawn")
           && Expect(received[1].request_spawn.client_id == 2, "scripted request_spawn client_id mismatch")
           && Expect(received[2].type == bz3::server::net::ServerInputEvent::Type::ClientCreateShot,
                     "scripted event[2] expected ClientCreateShot")
           && Expect(received[2].create_shot.client_id == 2, "scripted create_shot client_id mismatch")
           && Expect(received[2].create_shot.local_shot_id == 77, "scripted create_shot local_shot_id mismatch")
           && Expect(received[2].create_shot.vel_z == 6.0f, "scripted create_shot vel_z mismatch")
           && Expect(received[3].type == bz3::server::net::ServerInputEvent::Type::ClientLeave,
                     "scripted event[3] expected ClientLeave");
}

bool TestScriptedSourceSortsOutOfOrderAndClampsNegativeTimes() {
    const auto events = karma::common::serialization::Array(
        {karma::common::serialization::Value{{"type", "leave"}, {"atSeconds", 0.030}, {"clientId", 4}},
         karma::common::serialization::Value{{"type", "create_shot"},
                            {"atSeconds", 0.010},
                            {"clientId", 4},
                            {"localShotId", 99},
                            {"posX", 1.0},
                            {"posY", 2.0},
                            {"posZ", 3.0},
                            {"velX", 4.0},
                            {"velY", 5.0},
                            {"velZ", 6.0}},
         karma::common::serialization::Value{{"type", "join"}, {"atSeconds", 0.020}, {"clientId", 4}, {"playerName", "delta"}},
         karma::common::serialization::Value{{"type", "request_spawn"}, {"atSeconds", -5.0}, {"clientId", 4}}});

    if (!InitializeConfigForEvents(events, "ordered-clamped")) {
        return Fail("failed to initialize config for scripted source ordering test");
    }

    karma::cli::server::AppOptions options{};
    const auto source = bz3::server::net::CreateServerEventSource(options, 0);
    if (!source) {
        return Fail("CreateServerEventSource returned null for ordering scripted source test");
    }

    auto received = CollectScheduledEvents(*source, 4, std::chrono::milliseconds(500));
    if (received.size() != 4) {
        return Fail("ordering scripted source did not emit expected 4 events");
    }

    return Expect(received[0].type == bz3::server::net::ServerInputEvent::Type::ClientRequestSpawn,
                  "ordered scripted event[0] expected request_spawn from clamped negative time")
           && Expect(received[1].type == bz3::server::net::ServerInputEvent::Type::ClientCreateShot,
                     "ordered scripted event[1] expected create_shot at 0.010s")
           && Expect(received[2].type == bz3::server::net::ServerInputEvent::Type::ClientJoin,
                     "ordered scripted event[2] expected join at 0.020s")
           && Expect(received[3].type == bz3::server::net::ServerInputEvent::Type::ClientLeave,
                     "ordered scripted event[3] expected leave at 0.030s")
           && Expect(received[0].request_spawn.client_id == 4,
                     "ordered scripted request_spawn client_id mismatch")
           && Expect(received[1].create_shot.client_id == 4, "ordered scripted create_shot client_id mismatch")
           && Expect(received[2].join.client_id == 4 && received[2].join.player_name == "delta",
                     "ordered scripted join payload mismatch")
           && Expect(received[3].leave.client_id == 4, "ordered scripted leave client_id mismatch");
}

bool TestScriptedSourceSkipsInvalidShotData() {
    const auto events = karma::common::serialization::Array(
        {karma::common::serialization::Value{{"type", "join"}, {"atSeconds", 0.0}, {"clientId", 9}, {"playerName", "valid"}},
         karma::common::serialization::Value{{"type", "request_spawn"}, {"atSeconds", 0.0}},
         karma::common::serialization::Value{{"type", "create_shot"},
                            {"atSeconds", 0.0},
                            {"clientId", 9},
                            {"localShotId", 1},
                            {"posX", 0.0},
                            {"posY", 0.0},
                            {"posZ", 0.0},
                            {"velX", 1.0},
                            {"velY", 1.0},
                            {"velZ", "bad"}},
         karma::common::serialization::Value{{"type", "unknown"}, {"atSeconds", 0.0}, {"clientId", 9}}});

    if (!InitializeConfigForEvents(events, "invalid-shot")) {
        return Fail("failed to initialize config for scripted source invalid-shot test");
    }

    karma::cli::server::AppOptions options{};
    const auto source = bz3::server::net::CreateServerEventSource(options, 0);
    if (!source) {
        return Fail("CreateServerEventSource returned null for invalid-shot scripted source test");
    }

    auto received = CollectScheduledEvents(*source, 1, std::chrono::milliseconds(200));
    if (received.size() != 1) {
        return Fail("invalid-shot scripted source expected exactly one valid event");
    }
    return Expect(received[0].type == bz3::server::net::ServerInputEvent::Type::ClientJoin,
                  "invalid-shot scripted source expected ClientJoin as sole valid event");
}

bool TestScriptedSourceJoinAuthPayload() {
    const auto events = karma::common::serialization::Array(
        {karma::common::serialization::Value{{"type", "join"},
                            {"atSeconds", 0.0},
                            {"clientId", 3},
                            {"playerName", "alice"},
                            {"authPayload", "auth-v1"}}});
    if (!InitializeConfigForEvents(events, "join-auth")) {
        return Fail("failed to initialize config for scripted source join-auth test");
    }

    karma::cli::server::AppOptions options{};
    const auto source = bz3::server::net::CreateServerEventSource(options, 0);
    if (!source) {
        return Fail("CreateServerEventSource returned null for join-auth scripted source test");
    }

    auto received = CollectScheduledEvents(*source, 1, std::chrono::milliseconds(200));
    if (received.size() != 1) {
        return Fail("join-auth scripted source expected exactly one event");
    }
    return Expect(received[0].type == bz3::server::net::ServerInputEvent::Type::ClientJoin,
                  "join-auth scripted source expected ClientJoin event")
           && Expect(received[0].join.client_id == 3, "join-auth scripted source client_id mismatch")
           && Expect(received[0].join.player_name == "alice", "join-auth scripted source player_name mismatch")
           && Expect(received[0].join.auth_payload == "auth-v1", "join-auth scripted source auth_payload mismatch");
}

bool TestServerPreAuthAcceptReject() {
    const karma::network::ServerPreAuthConfig config{
        "expected-secret",
        "denied"};
    const auto accepted = karma::network::EvaluateServerPreAuth(
        config,
        karma::network::ServerPreAuthRequest{
            .client_id = 10,
            .player_name = "alice",
            .auth_payload = "expected-secret",
            .peer_ip = "127.0.0.1",
            .peer_port = 11899});
    const auto rejected = karma::network::EvaluateServerPreAuth(
        config,
        karma::network::ServerPreAuthRequest{
            .client_id = 11,
            .player_name = "bob",
            .auth_payload = "wrong-secret",
            .peer_ip = "127.0.0.1",
            .peer_port = 11899});
    const auto structured_accepted = karma::network::EvaluateServerPreAuth(
        config,
        karma::network::ServerPreAuthRequest{
            .client_id = 13,
            .player_name = "dave",
            .auth_payload = R"({"username":"dave","password":"expected-secret"})",
            .peer_ip = "127.0.0.1",
            .peer_port = 11899});
    const auto structured_rejected = karma::network::EvaluateServerPreAuth(
        config,
        karma::network::ServerPreAuthRequest{
            .client_id = 14,
            .player_name = "erin",
            .auth_payload = R"({"username":"erin","password":"wrong-secret"})",
            .peer_ip = "127.0.0.1",
            .peer_port = 11899});
    const auto disabled = karma::network::EvaluateServerPreAuth(
        karma::network::ServerPreAuthConfig{},
        karma::network::ServerPreAuthRequest{
            .client_id = 12,
            .player_name = "carol",
            .auth_payload = "",
            .peer_ip = "127.0.0.1",
            .peer_port = 11899});

    return Expect(accepted.accepted, "server pre-auth should accept matching auth payload")
           && Expect(!rejected.accepted, "server pre-auth should reject mismatched auth payload")
           && Expect(structured_accepted.accepted,
                     "server pre-auth should accept structured payload matching password")
           && Expect(!structured_rejected.accepted,
                     "server pre-auth should reject structured payload mismatched password")
           && Expect(rejected.reject_reason == "denied", "server pre-auth reject reason mismatch")
           && Expect(disabled.accepted, "server pre-auth should accept when disabled");
}

bool TestServerTransportBackendCustomIdPassThrough() {
    std::vector<CapturedTransportConfig> captures{};
    const std::string factory_backend_name = "contract-fake-custom";
    const std::string configured_backend_name = "CustomServerBackendContract";

    if (!karma::network::RegisterServerTransportFactory(
            configured_backend_name,
            [&captures, &factory_backend_name](const karma::network::ServerTransportConfig& config) {
                captures.push_back(CapturedTransportConfig{
                    .backend = config.backend,
                    .backend_name = config.backend_name,
                    .listen_port = config.listen_port,
                    .max_clients = config.max_clients,
                    .channel_count = config.channel_count,
                });
                return std::make_unique<FakeServerTransport>(factory_backend_name);
            })) {
        return Fail("failed to register fake custom transport factory");
    }

    if (!InitializeConfigForServerTransportBackend(configured_backend_name,
                                                   "backend-custom-pass-through")) {
        return Fail("failed to initialize config for custom backend pass-through test");
    }

    auto source = bz3::server::net::CreateServerTransportEventSource(0);
    if (!source) {
        return Fail("CreateServerTransportEventSource returned null for custom backend");
    }

    return Expect(captures.size() == 1, "custom backend test expected one factory capture") &&
           Expect(captures[0].backend_name == configured_backend_name,
                  "custom backend should pass configured backend_name through unchanged") &&
           Expect(captures[0].backend == karma::network::ServerTransportBackend::Auto,
                  "custom backend should keep enum value at default auto");
}

bool TestServerTransportBackendUnregisteredFailsCreation() {
    if (!InitializeConfigForServerTransportBackend("unregistered-server-backend-contract",
                                                   "backend-unregistered")) {
        return Fail("failed to initialize config for unregistered backend test");
    }

    auto source = bz3::server::net::CreateServerTransportEventSource(0);
    return Expect(!source, "unregistered backend should fail CreateServerTransportEventSource");
}

} // namespace

int main() {
    if (!TestJoinRequestRoundTrip()) {
        return 1;
    }
    if (!TestServerInitRoundTrip()) {
        return 1;
    }
    if (!TestCreateShotRoundTrip()) {
        return 1;
    }
    if (!TestRemoveShotRoundTrip()) {
        return 1;
    }
    if (!TestWorldTransferBeginRoundTrip()) {
        return 1;
    }
    if (!TestDeltaSelectionFallsBackToFullWhenSavingsBelowMinimum()) {
        return 1;
    }
    if (!TestScriptedSourceParsesSpawnAndShot()) {
        return 1;
    }
    if (!TestScriptedSourceSortsOutOfOrderAndClampsNegativeTimes()) {
        return 1;
    }
    if (!TestScriptedSourceSkipsInvalidShotData()) {
        return 1;
    }
    if (!TestScriptedSourceJoinAuthPayload()) {
        return 1;
    }
    if (!TestServerPreAuthAcceptReject()) {
        return 1;
    }
    if (!TestServerTransportBackendCustomIdPassThrough()) {
        return 1;
    }
    if (!TestServerTransportBackendUnregisteredFailsCreation()) {
        return 1;
    }
    return 0;
}
