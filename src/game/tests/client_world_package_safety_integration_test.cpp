#include "client/net/connection.hpp"
#include "net/protocol_codec.hpp"
#include "net/protocol.hpp"
#include "server/net/transport_event_source.hpp"

#include "karma/common/config_store.hpp"
#include "karma/common/content/archive.hpp"
#include "karma/common/content/manifest.hpp"
#include "karma/common/content/primitives.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "network/tests/loopback_transport_fixture.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

enum class TestResult {
    Pass,
    Skip,
    Fail
};

constexpr int kSkipReturnCode = 77;

struct ServerFixture {
    uint16_t port = 0;
    std::unique_ptr<bz3::server::net::ServerEventSource> source{};
};

class ScopedRawTransport {
 public:
    ScopedRawTransport() {
        initialized_ = karma::network::tests::InitializeLoopbackTransport();
    }

    bool initialized() const { return initialized_; }

 private:
    bool initialized_ = false;
};

struct RawServerFixture {
    uint16_t port = 0;
    karma::network::tests::LoopbackTransportEndpoint endpoint{};

    RawServerFixture() = default;
    RawServerFixture(uint16_t in_port, karma::network::tests::LoopbackTransportEndpoint&& in_endpoint)
        : port(in_port),
          endpoint(std::move(in_endpoint)) {}
    ~RawServerFixture() {
        karma::network::tests::DestroyLoopbackTransportEndpoint(&endpoint);
    }

    RawServerFixture(const RawServerFixture&) = delete;
    RawServerFixture& operator=(const RawServerFixture&) = delete;

    RawServerFixture(RawServerFixture&& other) noexcept
        : port(other.port),
          endpoint(std::move(other.endpoint)) {
        other.port = 0;
        other.endpoint = {};
    }

    RawServerFixture& operator=(RawServerFixture&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        karma::network::tests::DestroyLoopbackTransportEndpoint(&endpoint);
        port = other.port;
        endpoint = std::move(other.endpoint);
        other.port = 0;
        other.endpoint = {};
        return *this;
    }
};

struct WorldFixture {
    std::filesystem::path dir{};
    std::string world_name{};
    std::string world_id{};
    std::string world_revision{};
    std::string world_package_hash{};
    std::string world_content_hash{};
    std::string world_manifest_hash{};
    uint32_t world_manifest_file_count = 0;
    uint64_t world_package_size = 0;
    std::vector<std::byte> world_package{};
    std::vector<bz3::server::net::WorldManifestEntry> world_manifest{};
};

struct CachedIdentity {
    std::string world_hash{};
    std::string world_content_hash{};
    std::string world_id{};
    std::string world_revision{};
};

TestResult FailTest(const std::string& message) {
    std::cerr << "FAIL: " << message << "\n";
    return TestResult::Fail;
}

void PrintSkip(const std::string& message) {
    std::cerr << "SKIP: " << message << "\n";
}

std::filesystem::path MakeTempPath(const char* suffix) {
    const auto nonce = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path() /
           ("bz3-client-world-safety-" + std::string(suffix) + "-" + std::to_string(nonce));
}

std::filesystem::path MakeConfigPath(const char* suffix) {
    return MakeTempPath((std::string(suffix) + ".json").c_str());
}

std::optional<ServerFixture> CreateServerFixture() {
    constexpr uint16_t kFirstPort = 32200;
    constexpr uint16_t kLastPort = 32248;
    for (uint16_t port = kFirstPort; port < kLastPort; ++port) {
        auto source = bz3::server::net::CreateServerTransportEventSource(port);
        if (source) {
            return ServerFixture{port, std::move(source)};
        }
    }
    return std::nullopt;
}

std::optional<RawServerFixture> CreateRawServerFixture() {
    constexpr uint16_t kFirstPort = 32300;
    constexpr uint16_t kLastPort = 32348;
    for (uint16_t port = kFirstPort; port < kLastPort; ++port) {
        auto endpoint = karma::network::tests::CreateLoopbackServerTransportEndpointAtPort(port, 1, 2);
        if (endpoint.has_value()) {
            return RawServerFixture{port, std::move(*endpoint)};
        }
    }
    return std::nullopt;
}

bool SendRawServerPayload(karma::network::tests::LoopbackTransportEndpoint* server_endpoint,
                          const std::vector<std::byte>& payload) {
    if (!server_endpoint || payload.empty()) {
        return false;
    }
    return karma::network::tests::SendLoopbackTransportPayload(server_endpoint, payload);
}

void PumpRawServerEvents(RawServerFixture* server,
                         std::optional<bz3::net::ClientMessage>* join_request,
                         bool* disconnected) {
    if (!server) {
        return;
    }

    std::vector<std::vector<std::byte>> payloads{};
    karma::network::tests::PumpLoopbackTransportEndpointCapturePayloads(&server->endpoint, &payloads);
    for (const auto& payload : payloads) {
        const auto decoded = bz3::net::DecodeClientMessage(payload.data(), payload.size());
        if (decoded.has_value() && decoded->type == bz3::net::ClientMessageType::JoinRequest &&
            join_request) {
            *join_request = std::move(*decoded);
        }
    }
    if (disconnected) {
        *disconnected = *disconnected || server->endpoint.disconnected;
    }
}

template <typename StepFn, typename DoneFn>
bool WaitUntil(std::chrono::milliseconds timeout, StepFn&& step, DoneFn&& done) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        step();
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    step();
    return done();
}

std::string ComputeWorldPackageHash(const std::vector<std::byte>& bytes) {
    return karma::content::ComputeWorldPackageHash(bytes);
}

std::vector<bz3::server::net::WorldManifestEntry> ToServerManifestEntries(
    const std::vector<karma::content::ManifestEntry>& entries) {
    std::vector<bz3::server::net::WorldManifestEntry> converted{};
    converted.reserve(entries.size());
    for (const auto& entry : entries) {
        converted.push_back(bz3::server::net::WorldManifestEntry{
            .path = entry.path,
            .size = entry.size,
            .hash = entry.hash});
    }
    return converted;
}

bool WriteTextFile(const std::filesystem::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        return false;
    }
    output << content;
    return static_cast<bool>(output);
}

bool WriteDeterministicBinaryFile(const std::filesystem::path& path, size_t size, uint32_t seed) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    uint32_t state = seed == 0 ? 0x9e3779b9U : seed;
    std::array<char, 4096> buffer{};
    size_t produced = 0;
    while (produced < size) {
        const size_t to_fill = std::min(buffer.size(), size - produced);
        for (size_t i = 0; i < to_fill; ++i) {
            state = state * 1664525U + 1013904223U;
            buffer[i] = static_cast<char>((state >> 24) & 0xFFU);
        }
        output.write(buffer.data(), static_cast<std::streamsize>(to_fill));
        if (!output) {
            return false;
        }
        produced += to_fill;
    }
    return static_cast<bool>(output);
}

bool StartClientWithServerPump(bz3::client::net::ClientConnection* client,
                               bz3::server::net::ServerEventSource* source,
                               std::vector<bz3::server::net::ServerInputEvent>* buffered_events,
                               std::chrono::milliseconds timeout) {
    if (!client || !source || !buffered_events) {
        return false;
    }

    std::optional<bool> started{};
    std::thread starter([&]() { started = client->start(); });

    const bool done = WaitUntil(timeout, [&]() {
        auto events = source->poll();
        buffered_events->insert(buffered_events->end(), events.begin(), events.end());
    }, [&]() { return started.has_value(); });

    starter.join();
    auto trailing = source->poll();
    buffered_events->insert(buffered_events->end(), trailing.begin(), trailing.end());
    return done && started.value_or(false);
}

std::vector<bz3::server::net::ServerInputEvent> DrainServerEvents(
    bz3::server::net::ServerEventSource* source,
    std::vector<bz3::server::net::ServerInputEvent>* buffered_events) {
    std::vector<bz3::server::net::ServerInputEvent> out{};
    if (!source || !buffered_events) {
        return out;
    }
    out = std::move(*buffered_events);
    buffered_events->clear();
    auto polled = source->poll();
    out.insert(out.end(), polled.begin(), polled.end());
    return out;
}

std::optional<WorldFixture> BuildWorldFixture(const std::filesystem::path& world_dir,
                                              const std::string& world_name,
                                              const std::string& world_id) {
    if (!std::filesystem::is_directory(world_dir)) {
        return std::nullopt;
    }

    WorldFixture fixture{};
    fixture.dir = world_dir;
    fixture.world_name = world_name;
    fixture.world_id = world_id;

    try {
        fixture.world_package = karma::content::BuildWorldArchive(world_dir);
    } catch (const std::exception&) {
        return std::nullopt;
    }

    fixture.world_package_size = static_cast<uint64_t>(fixture.world_package.size());
    fixture.world_package_hash = ComputeWorldPackageHash(fixture.world_package);
    const auto summary = karma::content::ComputeDirectoryManifestSummary(world_dir);
    if (!summary.has_value()) {
        return std::nullopt;
    }

    fixture.world_content_hash = summary->content_hash;
    fixture.world_manifest_hash = summary->manifest_hash;
    fixture.world_manifest = ToServerManifestEntries(summary->entries);
    fixture.world_manifest_file_count = static_cast<uint32_t>(fixture.world_manifest.size());
    fixture.world_revision = fixture.world_content_hash;
    return fixture;
}

std::optional<CachedIdentity> ReadCachedIdentity(const std::filesystem::path& identity_path) {
    if (!std::filesystem::exists(identity_path) || !std::filesystem::is_regular_file(identity_path)) {
        return std::nullopt;
    }

    std::ifstream input(identity_path);
    if (!input) {
        return std::nullopt;
    }

    CachedIdentity out{};
    std::string line{};
    while (std::getline(input, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);
        if (key == "hash") {
            out.world_hash = value;
        } else if (key == "content_hash") {
            out.world_content_hash = value;
        } else if (key == "id") {
            out.world_id = value;
        } else if (key == "revision") {
            out.world_revision = value;
        }
    }
    if (out.world_id.empty() || out.world_revision.empty()) {
        return std::nullopt;
    }
    return out;
}

std::filesystem::path PackageRootForIdentity(const std::filesystem::path& server_cache_dir,
                                             std::string_view world_id,
                                             std::string_view world_revision,
                                             std::string_view package_key) {
    return server_cache_dir / "world-packages" / "by-world" / std::string(world_id) /
           std::string(world_revision) / std::string(package_key);
}

std::vector<std::byte> BuildChunk(const std::vector<std::byte>& world_package,
                                  uint32_t chunk_index,
                                  uint32_t chunk_size) {
    const size_t offset = static_cast<size_t>(chunk_index) * static_cast<size_t>(chunk_size);
    if (offset >= world_package.size()) {
        return {};
    }
    const size_t remaining = world_package.size() - offset;
    const size_t this_chunk_size = std::min<size_t>(remaining, chunk_size);
    std::vector<std::byte> chunk{};
    chunk.insert(chunk.end(),
                 world_package.begin() + static_cast<std::ptrdiff_t>(offset),
                 world_package.begin() + static_cast<std::ptrdiff_t>(offset + this_chunk_size));
    return chunk;
}

class ScopedEnvVar {
 public:
    ScopedEnvVar(std::string key, std::string value) : key_(std::move(key)) {
        const char* existing = std::getenv(key_.c_str());
        if (existing) {
            had_previous_ = true;
            previous_ = existing;
        }
#if defined(_WIN32)
        _putenv_s(key_.c_str(), value.c_str());
#else
        setenv(key_.c_str(), value.c_str(), 1);
#endif
    }

    ~ScopedEnvVar() {
#if defined(_WIN32)
        if (had_previous_) {
            _putenv_s(key_.c_str(), previous_.c_str());
        } else {
            _putenv_s(key_.c_str(), "");
        }
#else
        if (had_previous_) {
            setenv(key_.c_str(), previous_.c_str(), 1);
        } else {
            unsetenv(key_.c_str());
        }
#endif
    }

 private:
    std::string key_{};
    bool had_previous_ = false;
    std::string previous_{};
};

TestResult TestInterruptedChunkTransferResumeRecovery() {
    const std::filesystem::path temp_root = MakeTempPath("resume-root");
    const std::filesystem::path xdg_root = temp_root / "xdg";
    const std::filesystem::path world_dir = temp_root / "world";
    const std::filesystem::path config_path = MakeConfigPath("resume-config");

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
    ec.clear();
    std::filesystem::create_directories(xdg_root, ec);
    std::filesystem::create_directories(world_dir, ec);
    if (ec) {
        return FailTest("failed to create temp directories for interrupted-transfer test");
    }

    if (!WriteTextFile(world_dir / "config.json",
                       "{\n  \"worldName\": \"resume-world\",\n  \"resumeVersion\": 1\n}\n")) {
        return FailTest("failed to write interrupted-transfer world config.json");
    }
    if (!WriteDeterministicBinaryFile(world_dir / "world.glb", 192 * 1024, 0xabcddcbaU)) {
        return FailTest("failed to write interrupted-transfer world.glb");
    }

    const auto world_opt = BuildWorldFixture(world_dir, "resume-world", "resume-world");
    if (!world_opt.has_value()) {
        return FailTest("failed to build interrupted-transfer world fixture");
    }
    const WorldFixture world = *world_opt;

    ScopedEnvVar scoped_xdg("XDG_CONFIG_HOME", xdg_root.string());
    karma::data::SetDataPathSpec(karma::data::DataPathSpec{
        .appName = "bz3",
        .dataDirEnvVar = "BZ3_DATA_DIR",
        .requiredDataMarker = {},
        .fallbackAssetLayers = {}});
    karma::config::ConfigStore::Initialize({}, config_path);
    (void)karma::config::ConfigStore::Set("config.SaveIntervalSeconds", 5.0);
    (void)karma::config::ConfigStore::Set("config.MergeIntervalSeconds", 5.0);

    ScopedRawTransport raw_transport{};
    if (!raw_transport.initialized()) {
        PrintSkip("transport init failed for interrupted-transfer test");
        return TestResult::Skip;
    }

    auto raw_server_opt = CreateRawServerFixture();
    if (!raw_server_opt.has_value()) {
        PrintSkip("unable to create raw transport server fixture");
        return TestResult::Skip;
    }
    RawServerFixture raw_server = std::move(*raw_server_opt);

    bz3::client::net::ClientConnection client("127.0.0.1", raw_server.port, "resume-client");

    std::optional<bool> started{};
    std::optional<bz3::net::ClientMessage> join_request{};
    bool disconnected = false;
    std::thread starter([&]() { started = client.start(); });
    const bool start_done = WaitUntil(std::chrono::milliseconds(2500), [&]() {
        PumpRawServerEvents(&raw_server, &join_request, &disconnected);
    }, [&]() { return started.has_value(); });
    starter.join();
    if (!start_done || !started.value_or(false)) {
        return FailTest("client failed to start against raw transport server");
    }
    if (!karma::network::tests::LoopbackTransportEndpointHasPeer(&raw_server.endpoint) || disconnected) {
        client.shutdown();
        return FailTest("raw transport server did not observe connected peer");
    }
    if (!WaitUntil(std::chrono::milliseconds(2500), [&]() {
            PumpRawServerEvents(&raw_server, &join_request, &disconnected);
            client.poll();
        }, [&]() { return join_request.has_value(); })) {
        client.shutdown();
        return FailTest("raw transport server did not receive join request");
    }
    if (disconnected || !karma::network::tests::LoopbackTransportEndpointHasPeer(&raw_server.endpoint)) {
        client.shutdown();
        return FailTest("client disconnected before transfer test could begin");
    }

    std::vector<bz3::net::WorldManifestEntry> wire_manifest{};
    wire_manifest.reserve(world.world_manifest.size());
    for (const auto& entry : world.world_manifest) {
        wire_manifest.push_back(bz3::net::WorldManifestEntry{
            .path = entry.path,
            .size = entry.size,
            .hash = entry.hash});
    }

    if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerJoinResponse(true, ""))) {
        client.shutdown();
        return FailTest("failed to send raw join response");
    }
    if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerInit(2,
                                                         "raw-server",
                                                         world.world_name,
                                                         bz3::net::kProtocolVersion,
                                                         world.world_package_hash,
                                                         world.world_package_size,
                                                         world.world_id,
                                                         world.world_revision,
                                                         world.world_content_hash,
                                                         world.world_manifest_hash,
                                                         world.world_manifest_file_count,
                                                         wire_manifest,
                                                         {}))) {
        client.shutdown();
        return FailTest("failed to send raw init payload");
    }

    constexpr uint32_t kChunkSize = 16 * 1024;
    const uint32_t total_chunk_count =
        static_cast<uint32_t>((world.world_package.size() + kChunkSize - 1) / kChunkSize);
    if (total_chunk_count < 2) {
        client.shutdown();
        return FailTest("world package too small for interrupted-transfer test");
    }
    const std::string transfer_id = "resume-transfer-1";
    if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerWorldTransferBegin(transfer_id,
                                                                       world.world_id,
                                                                       world.world_revision,
                                                                       world.world_package.size(),
                                                                       kChunkSize,
                                                                       world.world_package_hash,
                                                                       world.world_content_hash))) {
        client.shutdown();
        return FailTest("failed to send raw transfer begin");
    }
    if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerWorldTransferChunk(
                                  transfer_id,
                                  0,
                                  BuildChunk(world.world_package, 0, kChunkSize)))) {
        client.shutdown();
        return FailTest("failed to send raw transfer chunk 0");
    }

    // Simulate interrupted transfer recovery by restarting begin and resuming at chunk 1.
    if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerWorldTransferBegin(transfer_id,
                                                                       world.world_id,
                                                                       world.world_revision,
                                                                       world.world_package.size(),
                                                                       kChunkSize,
                                                                       world.world_package_hash,
                                                                       world.world_content_hash))) {
        client.shutdown();
        return FailTest("failed to send raw retry begin");
    }
    for (uint32_t chunk_index = 1; chunk_index < total_chunk_count; ++chunk_index) {
        if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerWorldTransferChunk(
                                      transfer_id,
                                      chunk_index,
                                      BuildChunk(world.world_package, chunk_index, kChunkSize)))) {
            client.shutdown();
            return FailTest("failed to send resumed transfer chunk");
        }
    }
    if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerWorldTransferEnd(transfer_id,
                                                                     total_chunk_count,
                                                                     world.world_package.size(),
                                                                     world.world_package_hash,
                                                                     world.world_content_hash))) {
        client.shutdown();
        return FailTest("failed to send raw transfer end");
    }
    if (!SendRawServerPayload(
            &raw_server.endpoint,
            bz3::net::EncodeServerSessionSnapshot(
                std::vector<bz3::net::SessionSnapshotEntry>{{2, "resume-client"}}))) {
        client.shutdown();
        return FailTest("failed to send raw session snapshot");
    }

    const std::filesystem::path server_cache_dir =
        karma::data::EnsureUserWorldDirectoryForServer("127.0.0.1", raw_server.port);
    const std::filesystem::path identity_path = server_cache_dir / "active_world_identity.txt";
    const bool applied = WaitUntil(std::chrono::milliseconds(3000), [&]() {
        PumpRawServerEvents(&raw_server, &join_request, &disconnected);
        client.poll();
    }, [&]() {
        const auto identity = ReadCachedIdentity(identity_path);
        return identity.has_value() &&
               identity->world_id == world.world_id &&
               identity->world_revision == world.world_revision &&
               identity->world_content_hash == world.world_content_hash;
    });

    if (!applied || disconnected || client.shouldExit()) {
        client.shutdown();
        return FailTest("client failed to recover interrupted transfer and apply world package");
    }

    const std::filesystem::path package_root =
        PackageRootForIdentity(server_cache_dir,
                               world.world_id,
                               world.world_revision,
                               world.world_content_hash);
    if (!std::filesystem::exists(package_root) || !std::filesystem::is_directory(package_root)) {
        client.shutdown();
        return FailTest("interrupted-transfer package root missing after recovery");
    }

    client.shutdown();
    return TestResult::Pass;
}

TestResult TestInitProtocolMismatchIsRejected() {
    const std::filesystem::path temp_root = MakeTempPath("protocol-mismatch-root");
    const std::filesystem::path xdg_root = temp_root / "xdg";
    const std::filesystem::path config_path = MakeConfigPath("protocol-mismatch-config");

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
    ec.clear();
    std::filesystem::create_directories(xdg_root, ec);
    if (ec) {
        return FailTest("failed to create temp directories for protocol-mismatch test");
    }

    ScopedEnvVar scoped_xdg("XDG_CONFIG_HOME", xdg_root.string());
    karma::data::SetDataPathSpec(karma::data::DataPathSpec{
        .appName = "bz3",
        .dataDirEnvVar = "BZ3_DATA_DIR",
        .requiredDataMarker = {},
        .fallbackAssetLayers = {}});
    karma::config::ConfigStore::Initialize({}, config_path);
    (void)karma::config::ConfigStore::Set("config.SaveIntervalSeconds", 5.0);
    (void)karma::config::ConfigStore::Set("config.MergeIntervalSeconds", 5.0);

    ScopedRawTransport raw_transport{};
    if (!raw_transport.initialized()) {
        PrintSkip("transport init failed for protocol-mismatch test");
        return TestResult::Skip;
    }

    auto raw_server_opt = CreateRawServerFixture();
    if (!raw_server_opt.has_value()) {
        PrintSkip("unable to create raw transport server fixture");
        return TestResult::Skip;
    }
    RawServerFixture raw_server = std::move(*raw_server_opt);

    bz3::client::net::ClientConnection client("127.0.0.1", raw_server.port, "protocol-client");

    std::optional<bool> started{};
    std::optional<bz3::net::ClientMessage> join_request{};
    bool disconnected = false;
    std::thread starter([&]() { started = client.start(); });
    const bool start_done = WaitUntil(std::chrono::milliseconds(2500), [&]() {
        PumpRawServerEvents(&raw_server, &join_request, &disconnected);
    }, [&]() { return started.has_value(); });
    starter.join();
    if (!start_done || !started.value_or(false)) {
        return FailTest("client failed to start against raw transport server");
    }
    if (!karma::network::tests::LoopbackTransportEndpointHasPeer(&raw_server.endpoint) || disconnected) {
        client.shutdown();
        return FailTest("raw transport server did not observe connected peer");
    }
    if (!WaitUntil(std::chrono::milliseconds(2500), [&]() {
            PumpRawServerEvents(&raw_server, &join_request, &disconnected);
            client.poll();
        }, [&]() { return join_request.has_value(); })) {
        client.shutdown();
        return FailTest("raw transport server did not receive join request");
    }
    if (disconnected || !karma::network::tests::LoopbackTransportEndpointHasPeer(&raw_server.endpoint)) {
        client.shutdown();
        return FailTest("client disconnected before protocol-mismatch test could begin");
    }

    if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerJoinResponse(true, ""))) {
        client.shutdown();
        return FailTest("failed to send raw join response");
    }
    if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerInit(
                                                      2,
                                                      "raw-server",
                                                      "protocol-mismatch-world",
                                                      bz3::net::kProtocolVersion + 1,
                                                      {},
                                                      0,
                                                      "protocol-mismatch-world",
                                                      "rev-1",
                                                      {},
                                                      {},
                                                      0,
                                                      {},
                                                      {}))) {
        client.shutdown();
        return FailTest("failed to send protocol-mismatch init payload");
    }

    const bool rejected = WaitUntil(std::chrono::milliseconds(3000), [&]() {
        PumpRawServerEvents(&raw_server, &join_request, &disconnected);
        client.poll();
    }, [&]() { return disconnected || client.shouldExit(); });

    if (!rejected) {
        client.shutdown();
        return FailTest("client did not reject init protocol mismatch");
    }

    client.shutdown();
    return TestResult::Pass;
}

TestResult TestInitWorldMetadataMissingIdentityIsRejected() {
    const std::filesystem::path temp_root = MakeTempPath("metadata-identity-root");
    const std::filesystem::path xdg_root = temp_root / "xdg";
    const std::filesystem::path config_path = MakeConfigPath("metadata-identity-config");

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
    ec.clear();
    std::filesystem::create_directories(xdg_root, ec);
    if (ec) {
        return FailTest("failed to create temp directories for metadata-identity test");
    }

    ScopedEnvVar scoped_xdg("XDG_CONFIG_HOME", xdg_root.string());
    karma::data::SetDataPathSpec(karma::data::DataPathSpec{
        .appName = "bz3",
        .dataDirEnvVar = "BZ3_DATA_DIR",
        .requiredDataMarker = {},
        .fallbackAssetLayers = {}});
    karma::config::ConfigStore::Initialize({}, config_path);
    (void)karma::config::ConfigStore::Set("config.SaveIntervalSeconds", 5.0);
    (void)karma::config::ConfigStore::Set("config.MergeIntervalSeconds", 5.0);

    ScopedRawTransport raw_transport{};
    if (!raw_transport.initialized()) {
        PrintSkip("transport init failed for metadata-identity test");
        return TestResult::Skip;
    }

    auto raw_server_opt = CreateRawServerFixture();
    if (!raw_server_opt.has_value()) {
        PrintSkip("unable to create raw transport server fixture");
        return TestResult::Skip;
    }
    RawServerFixture raw_server = std::move(*raw_server_opt);

    bz3::client::net::ClientConnection client("127.0.0.1", raw_server.port, "metadata-client");

    std::optional<bool> started{};
    std::optional<bz3::net::ClientMessage> join_request{};
    bool disconnected = false;
    std::thread starter([&]() { started = client.start(); });
    const bool start_done = WaitUntil(std::chrono::milliseconds(2500), [&]() {
        PumpRawServerEvents(&raw_server, &join_request, &disconnected);
    }, [&]() { return started.has_value(); });
    starter.join();
    if (!start_done || !started.value_or(false)) {
        return FailTest("client failed to start against raw transport server");
    }
    if (!karma::network::tests::LoopbackTransportEndpointHasPeer(&raw_server.endpoint) || disconnected) {
        client.shutdown();
        return FailTest("raw transport server did not observe connected peer");
    }
    if (!WaitUntil(std::chrono::milliseconds(2500), [&]() {
            PumpRawServerEvents(&raw_server, &join_request, &disconnected);
            client.poll();
        }, [&]() { return join_request.has_value(); })) {
        client.shutdown();
        return FailTest("raw transport server did not receive join request");
    }
    if (disconnected || !karma::network::tests::LoopbackTransportEndpointHasPeer(&raw_server.endpoint)) {
        client.shutdown();
        return FailTest("client disconnected before metadata-identity test could begin");
    }

    if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerJoinResponse(true, ""))) {
        client.shutdown();
        return FailTest("failed to send raw join response");
    }
    if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerInit(
                                                      2,
                                                      "raw-server",
                                                      "metadata-missing-id-world",
                                                      bz3::net::kProtocolVersion,
                                                      "0123456789abcdef",
                                                      256,
                                                      {},
                                                      {},
                                                      "fedcba9876543210",
                                                      {},
                                                      0,
                                                      {},
                                                      {}))) {
        client.shutdown();
        return FailTest("failed to send metadata-missing-identity init payload");
    }

    const bool rejected = WaitUntil(std::chrono::milliseconds(3000), [&]() {
        PumpRawServerEvents(&raw_server, &join_request, &disconnected);
        client.poll();
    }, [&]() { return disconnected || client.shouldExit(); });

    if (!rejected) {
        client.shutdown();
        return FailTest("client did not reject world metadata init missing id/revision");
    }

    client.shutdown();
    return TestResult::Pass;
}

TestResult TestChunkTransferEndHashMismatchIsRejected() {
    const std::filesystem::path temp_root = MakeTempPath("integrity-root");
    const std::filesystem::path xdg_root = temp_root / "xdg";
    const std::filesystem::path world_dir = temp_root / "world";
    const std::filesystem::path config_path = MakeConfigPath("integrity-config");

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
    ec.clear();
    std::filesystem::create_directories(xdg_root, ec);
    std::filesystem::create_directories(world_dir, ec);
    if (ec) {
        return FailTest("failed to create temp directories for transfer-integrity test");
    }

    if (!WriteTextFile(world_dir / "config.json",
                       "{\n  \"worldName\": \"integrity-world\",\n  \"integrityVersion\": 1\n}\n")) {
        return FailTest("failed to write transfer-integrity world config.json");
    }
    if (!WriteDeterministicBinaryFile(world_dir / "world.glb", 192 * 1024, 0x10293847U)) {
        return FailTest("failed to write transfer-integrity world.glb");
    }

    const std::string unique_world_id =
        "integrity-world-" +
        std::to_string(static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto world_opt = BuildWorldFixture(world_dir, "integrity-world", unique_world_id);
    if (!world_opt.has_value()) {
        return FailTest("failed to build transfer-integrity world fixture");
    }
    const WorldFixture world = *world_opt;

    ScopedEnvVar scoped_xdg("XDG_CONFIG_HOME", xdg_root.string());
    karma::data::SetDataPathSpec(karma::data::DataPathSpec{
        .appName = "bz3",
        .dataDirEnvVar = "BZ3_DATA_DIR",
        .requiredDataMarker = {},
        .fallbackAssetLayers = {}});
    karma::config::ConfigStore::Initialize({}, config_path);
    (void)karma::config::ConfigStore::Set("config.SaveIntervalSeconds", 5.0);
    (void)karma::config::ConfigStore::Set("config.MergeIntervalSeconds", 5.0);

    ScopedRawTransport raw_transport{};
    if (!raw_transport.initialized()) {
        PrintSkip("transport init failed for transfer-integrity test");
        return TestResult::Skip;
    }

    auto raw_server_opt = CreateRawServerFixture();
    if (!raw_server_opt.has_value()) {
        PrintSkip("unable to create raw transport server fixture");
        return TestResult::Skip;
    }
    RawServerFixture raw_server = std::move(*raw_server_opt);

    bz3::client::net::ClientConnection client("127.0.0.1", raw_server.port, "integrity-client");

    std::optional<bool> started{};
    std::optional<bz3::net::ClientMessage> join_request{};
    bool disconnected = false;
    std::thread starter([&]() { started = client.start(); });
    const bool start_done = WaitUntil(std::chrono::milliseconds(2500), [&]() {
        PumpRawServerEvents(&raw_server, &join_request, &disconnected);
    }, [&]() { return started.has_value(); });
    starter.join();
    if (!start_done || !started.value_or(false)) {
        return FailTest("client failed to start against raw transport server");
    }
    if (!karma::network::tests::LoopbackTransportEndpointHasPeer(&raw_server.endpoint) || disconnected) {
        client.shutdown();
        return FailTest("raw transport server did not observe connected peer");
    }
    if (!WaitUntil(std::chrono::milliseconds(2500), [&]() {
            PumpRawServerEvents(&raw_server, &join_request, &disconnected);
            client.poll();
        }, [&]() { return join_request.has_value(); })) {
        client.shutdown();
        return FailTest("raw transport server did not receive join request");
    }
    if (disconnected || !karma::network::tests::LoopbackTransportEndpointHasPeer(&raw_server.endpoint)) {
        client.shutdown();
        return FailTest("client disconnected before integrity transfer test could begin");
    }

    std::vector<bz3::net::WorldManifestEntry> wire_manifest{};
    wire_manifest.reserve(world.world_manifest.size());
    for (const auto& entry : world.world_manifest) {
        wire_manifest.push_back(bz3::net::WorldManifestEntry{
            .path = entry.path,
            .size = entry.size,
            .hash = entry.hash});
    }

    if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerJoinResponse(true, ""))) {
        client.shutdown();
        return FailTest("failed to send raw join response");
    }
    if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerInit(2,
                                                         "raw-server",
                                                         world.world_name,
                                                         bz3::net::kProtocolVersion,
                                                         world.world_package_hash,
                                                         world.world_package_size,
                                                         world.world_id,
                                                         world.world_revision,
                                                         world.world_content_hash,
                                                         world.world_manifest_hash,
                                                         world.world_manifest_file_count,
                                                         wire_manifest,
                                                         {}))) {
        client.shutdown();
        return FailTest("failed to send raw init payload");
    }

    constexpr uint32_t kChunkSize = 16 * 1024;
    const uint32_t total_chunk_count =
        static_cast<uint32_t>((world.world_package.size() + kChunkSize - 1) / kChunkSize);
    const std::string transfer_id = "integrity-transfer-1";
    if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerWorldTransferBegin(transfer_id,
                                                                       world.world_id,
                                                                       world.world_revision,
                                                                       world.world_package.size(),
                                                                       kChunkSize,
                                                                       world.world_package_hash,
                                                                       world.world_content_hash))) {
        client.shutdown();
        return FailTest("failed to send raw transfer begin");
    }
    for (uint32_t chunk_index = 0; chunk_index < total_chunk_count; ++chunk_index) {
        if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerWorldTransferChunk(
                                      transfer_id,
                                      chunk_index,
                                      BuildChunk(world.world_package, chunk_index, kChunkSize)))) {
            client.shutdown();
            return FailTest("failed to send transfer chunk for integrity mismatch test");
        }
    }

    std::string tampered_transfer_hash = world.world_package_hash;
    if (!tampered_transfer_hash.empty()) {
        tampered_transfer_hash[0] = tampered_transfer_hash[0] == '0' ? '1' : '0';
    } else {
        tampered_transfer_hash = "1";
    }
    if (!SendRawServerPayload(&raw_server.endpoint, bz3::net::EncodeServerWorldTransferEnd(transfer_id,
                                                                     total_chunk_count,
                                                                     world.world_package.size(),
                                                                     tampered_transfer_hash,
                                                                     world.world_content_hash))) {
        client.shutdown();
        return FailTest("failed to send tampered transfer end");
    }
    if (!SendRawServerPayload(
            &raw_server.endpoint,
            bz3::net::EncodeServerSessionSnapshot(
                std::vector<bz3::net::SessionSnapshotEntry>{{2, "integrity-client"}}))) {
        client.shutdown();
        return FailTest("failed to send raw session snapshot");
    }

    const bool rejected = WaitUntil(std::chrono::milliseconds(3000), [&]() {
        PumpRawServerEvents(&raw_server, &join_request, &disconnected);
        client.poll();
    }, [&]() { return disconnected || client.shouldExit(); });

    if (!rejected) {
        client.shutdown();
        return FailTest("client did not reject chunk transfer with tampered end hash");
    }

    const std::filesystem::path server_cache_dir =
        karma::data::EnsureUserWorldDirectoryForServer("127.0.0.1", raw_server.port);
    const std::filesystem::path identity_path = server_cache_dir / "active_world_identity.txt";
    const auto identity_after_reject = ReadCachedIdentity(identity_path);
    if (identity_after_reject.has_value() &&
        identity_after_reject->world_id == world.world_id &&
        identity_after_reject->world_revision == world.world_revision &&
        identity_after_reject->world_content_hash == world.world_content_hash) {
        client.shutdown();
        return FailTest("client promoted tampered-transfer identity after end-hash mismatch");
    }
    const std::filesystem::path package_root =
        PackageRootForIdentity(server_cache_dir,
                               world.world_id,
                               world.world_revision,
                               world.world_content_hash);
    if (std::filesystem::exists(package_root)) {
        client.shutdown();
        return FailTest("client promoted world package after tampered chunk-transfer end hash");
    }

    client.shutdown();
    return TestResult::Pass;
}

TestResult TestFailedWorldUpdatePreservesPreviousCache() {
    const std::filesystem::path temp_root = MakeTempPath("root");
    const std::filesystem::path xdg_root = temp_root / "xdg";
    const std::filesystem::path world_a_dir = temp_root / "world-a";
    const std::filesystem::path world_b_dir = temp_root / "world-b";
    const std::filesystem::path config_path = MakeConfigPath("config");

    std::error_code ec;
    std::filesystem::remove_all(temp_root, ec);
    ec.clear();
    std::filesystem::create_directories(xdg_root, ec);
    std::filesystem::create_directories(world_a_dir, ec);
    std::filesystem::create_directories(world_b_dir, ec);
    if (ec) {
        return FailTest("failed to create temp directories");
    }

    if (!WriteTextFile(world_a_dir / "config.json",
                       "{\n  \"worldName\": \"cache-world\",\n  \"deltaVersion\": 1\n}\n")) {
        return FailTest("failed to write world A config.json");
    }
    if (!WriteDeterministicBinaryFile(world_a_dir / "world.glb", 256 * 1024, 0x12345678U)) {
        return FailTest("failed to write world A world.glb");
    }

    if (!WriteTextFile(world_b_dir / "config.json",
                       "{\n  \"worldName\": \"cache-world\",\n  \"deltaVersion\": 2\n}\n")) {
        return FailTest("failed to write world B config.json");
    }
    // Keep the large asset unchanged so delta selection has substantial reuse.
    std::filesystem::copy_file(world_a_dir / "world.glb",
                               world_b_dir / "world.glb",
                               std::filesystem::copy_options::overwrite_existing,
                               ec);
    if (ec) {
        return FailTest("failed to copy world.glb into world B");
    }

    const auto world_a_opt = BuildWorldFixture(world_a_dir, "cache-world", "cache-world");
    const auto world_b_opt = BuildWorldFixture(world_b_dir, "cache-world", "cache-world");
    if (!world_a_opt.has_value() || !world_b_opt.has_value()) {
        return FailTest("failed to build world package fixtures");
    }
    const WorldFixture world_a = *world_a_opt;
    const WorldFixture world_b = *world_b_opt;

    ScopedEnvVar scoped_xdg("XDG_CONFIG_HOME", xdg_root.string());
    karma::data::SetDataPathSpec(karma::data::DataPathSpec{
        .appName = "bz3",
        .dataDirEnvVar = "BZ3_DATA_DIR",
        .requiredDataMarker = {},
        .fallbackAssetLayers = {}});
    karma::config::ConfigStore::Initialize({}, config_path);
    (void)karma::config::ConfigStore::Set("config.SaveIntervalSeconds", 5.0);
    (void)karma::config::ConfigStore::Set("config.MergeIntervalSeconds", 5.0);

    auto server_opt = CreateServerFixture();
    if (!server_opt.has_value()) {
        PrintSkip("unable to create transport server fixture");
        return TestResult::Skip;
    }
    ServerFixture server = std::move(*server_opt);
    std::vector<bz3::server::net::ServerInputEvent> pending_events{};

    const std::filesystem::path server_cache_dir =
        karma::data::EnsureUserWorldDirectoryForServer("127.0.0.1", server.port);
    const std::filesystem::path identity_path = server_cache_dir / "active_world_identity.txt";

    bool seeded_join_handled = false;
    uint32_t seeded_client_id = 0;
    bz3::client::net::ClientConnection seed_client("127.0.0.1", server.port, "seed-client");
    if (!StartClientWithServerPump(&seed_client,
                                   server.source.get(),
                                   &pending_events,
                                   std::chrono::milliseconds(2500))) {
        return FailTest("seed client failed to start");
    }

    const auto pump_seed = [&]() {
        const auto events = DrainServerEvents(server.source.get(), &pending_events);
        for (const auto& event : events) {
            if (event.type != bz3::server::net::ServerInputEvent::Type::ClientJoin) {
                continue;
            }
            if (event.join.player_name != "seed-client" || seeded_join_handled) {
                continue;
            }
            seeded_join_handled = true;
            seeded_client_id = event.join.client_id;
            const std::vector<bz3::server::net::SessionSnapshotEntry> sessions{
                bz3::server::net::SessionSnapshotEntry{1, "seed-client"}};
            server.source->onJoinResult(event.join.client_id,
                                        true,
                                        "",
                                        world_a.world_name,
                                        world_a.world_id,
                                        world_a.world_revision,
                                        world_a.world_package_hash,
                                        world_a.world_content_hash,
                                        world_a.world_manifest_hash,
                                        world_a.world_manifest_file_count,
                                        world_a.world_package_size,
                                        world_a.dir,
                                        sessions,
                                        world_a.world_manifest,
                                        world_a.world_package);
        }
        seed_client.poll();
    };

    if (!WaitUntil(std::chrono::milliseconds(2500), pump_seed, [&]() {
            const auto identity = ReadCachedIdentity(identity_path);
            return seeded_join_handled && seeded_client_id != 0 && identity.has_value() &&
                   identity->world_id == world_a.world_id &&
                   identity->world_revision == world_a.world_revision &&
                   identity->world_content_hash == world_a.world_content_hash;
        })) {
        seed_client.shutdown();
        return FailTest("seed phase did not persist expected cached world identity");
    }
    seed_client.shutdown();

    const auto identity_after_seed = ReadCachedIdentity(identity_path);
    if (!identity_after_seed.has_value()) {
        return FailTest("missing identity after seed phase");
    }

    const std::filesystem::path base_package_root =
        PackageRootForIdentity(server_cache_dir,
                               world_a.world_id,
                               world_a.world_revision,
                               world_a.world_content_hash);
    if (!std::filesystem::exists(base_package_root) || !std::filesystem::is_directory(base_package_root)) {
        return FailTest("expected base package root missing after seed phase");
    }

    bool fail_join_handled = false;
    bz3::client::net::ClientConnection failing_client("127.0.0.1", server.port, "failing-client");
    if (!StartClientWithServerPump(&failing_client,
                                   server.source.get(),
                                   &pending_events,
                                   std::chrono::milliseconds(2500))) {
        return FailTest("failing client failed to start");
    }

    const std::string corrupted_content_hash = "ffffffffffffffff";
    const auto pump_failure = [&]() {
        const auto events = DrainServerEvents(server.source.get(), &pending_events);
        for (const auto& event : events) {
            if (event.type != bz3::server::net::ServerInputEvent::Type::ClientJoin) {
                continue;
            }
            if (event.join.player_name != "failing-client" || fail_join_handled) {
                continue;
            }
            fail_join_handled = true;
            const std::vector<bz3::server::net::SessionSnapshotEntry> sessions{
                bz3::server::net::SessionSnapshotEntry{2, "failing-client"}};
            // Send intentionally incorrect content hash for revision B so staged apply must fail
            // verification and preserve prior active cache identity.
            server.source->onJoinResult(event.join.client_id,
                                        true,
                                        "",
                                        world_b.world_name,
                                        world_b.world_id,
                                        world_b.world_revision,
                                        world_b.world_package_hash,
                                        corrupted_content_hash,
                                        world_b.world_manifest_hash,
                                        world_b.world_manifest_file_count,
                                        world_b.world_package_size,
                                        world_b.dir,
                                        sessions,
                                        world_b.world_manifest,
                                        world_b.world_package);
        }
        failing_client.poll();
    };

    if (!WaitUntil(std::chrono::milliseconds(3000), pump_failure, [&]() {
            return fail_join_handled && failing_client.shouldExit();
        })) {
        failing_client.shutdown();
        return FailTest("failing phase did not trigger client shutdown on corrupted update");
    }
    failing_client.shutdown();

    const auto identity_after_failure = ReadCachedIdentity(identity_path);
    if (!identity_after_failure.has_value()) {
        return FailTest("cached identity unexpectedly missing after failed update");
    }

    if (identity_after_failure->world_id != identity_after_seed->world_id ||
        identity_after_failure->world_revision != identity_after_seed->world_revision ||
        identity_after_failure->world_hash != identity_after_seed->world_hash ||
        identity_after_failure->world_content_hash != identity_after_seed->world_content_hash) {
        return FailTest("cached identity changed after failed update; expected rollback-safe behavior");
    }

    if (!std::filesystem::exists(base_package_root) || !std::filesystem::is_directory(base_package_root)) {
        return FailTest("base package root missing after failed update");
    }

    const std::filesystem::path failed_target_root =
        PackageRootForIdentity(server_cache_dir,
                               world_b.world_id,
                               world_b.world_revision,
                               corrupted_content_hash);
    if (std::filesystem::exists(failed_target_root)) {
        return FailTest("failed target package root exists after failed update");
    }

    return TestResult::Pass;
}

} // namespace

int main() {
    const std::array tests{
        &TestInterruptedChunkTransferResumeRecovery,
        &TestInitProtocolMismatchIsRejected,
        &TestInitWorldMetadataMissingIdentityIsRejected,
        &TestChunkTransferEndHashMismatchIsRejected,
        &TestFailedWorldUpdatePreservesPreviousCache,
    };

    int pass_count = 0;
    int skip_count = 0;
    for (auto test_fn : tests) {
        const TestResult result = test_fn();
        if (result == TestResult::Fail) {
            return 1;
        }
        if (result == TestResult::Skip) {
            ++skip_count;
            continue;
        }
        ++pass_count;
    }

    if (pass_count == 0 && skip_count > 0) {
        return kSkipReturnCode;
    }
    return 0;
}
