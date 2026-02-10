#include "client/net/client_connection.hpp"
#include "server/net/enet_event_source.hpp"

#include "karma/common/config_store.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/common/world_archive.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
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
        auto source = bz3::server::net::CreateEnetServerEventSource(port);
        if (source) {
            return ServerFixture{port, std::move(source)};
        }
    }
    return std::nullopt;
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

void HashBytesFNV1a(uint64_t& hash, const std::byte* bytes, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        hash ^= static_cast<uint64_t>(std::to_integer<unsigned char>(bytes[i]));
        hash *= 1099511628211ULL;
    }
}

void HashStringFNV1a(uint64_t& hash, std::string_view value) {
    const auto* bytes = reinterpret_cast<const std::byte*>(value.data());
    HashBytesFNV1a(hash, bytes, value.size());
}

std::string Hash64ToHex(uint64_t hash) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << hash;
    return out.str();
}

std::string ComputeWorldPackageHash(const std::vector<std::byte>& bytes) {
    uint64_t hash = 14695981039346656037ULL;
    for (const auto value : bytes) {
        hash ^= static_cast<uint64_t>(std::to_integer<unsigned char>(value));
        hash *= 1099511628211ULL;
    }
    return Hash64ToHex(hash);
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
        fixture.world_package = world::BuildWorldArchive(world_dir);
    } catch (const std::exception&) {
        return std::nullopt;
    }

    fixture.world_package_size = static_cast<uint64_t>(fixture.world_package.size());
    fixture.world_package_hash = ComputeWorldPackageHash(fixture.world_package);

    std::vector<std::filesystem::path> files{};
    for (const auto& entry : std::filesystem::recursive_directory_iterator(world_dir)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    uint64_t content_hash = 14695981039346656037ULL;
    uint64_t manifest_hash = 14695981039346656037ULL;
    const std::byte separator{0};
    std::array<char, 64 * 1024> buffer{};

    for (const auto& path : files) {
        const auto rel = std::filesystem::relative(path, world_dir).generic_string();
        HashStringFNV1a(content_hash, rel);
        HashBytesFNV1a(content_hash, &separator, 1);

        std::ifstream input(path, std::ios::binary);
        if (!input) {
            return std::nullopt;
        }

        uint64_t file_hash = 14695981039346656037ULL;
        uint64_t file_size = 0;
        while (input.good()) {
            input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
            const std::streamsize read_count = input.gcount();
            if (read_count > 0) {
                const auto* bytes = reinterpret_cast<const std::byte*>(buffer.data());
                HashBytesFNV1a(content_hash, bytes, static_cast<size_t>(read_count));
                HashBytesFNV1a(file_hash, bytes, static_cast<size_t>(read_count));
                file_size += static_cast<uint64_t>(read_count);
            }
        }
        if (!input.eof()) {
            return std::nullopt;
        }
        HashBytesFNV1a(content_hash, &separator, 1);

        const std::string file_hash_hex = Hash64ToHex(file_hash);
        const std::string file_size_text = std::to_string(file_size);
        HashStringFNV1a(manifest_hash, rel);
        HashBytesFNV1a(manifest_hash, &separator, 1);
        HashStringFNV1a(manifest_hash, file_size_text);
        HashBytesFNV1a(manifest_hash, &separator, 1);
        HashStringFNV1a(manifest_hash, file_hash_hex);
        HashBytesFNV1a(manifest_hash, &separator, 1);

        fixture.world_manifest.push_back(bz3::server::net::WorldManifestEntry{
            .path = rel,
            .size = file_size,
            .hash = file_hash_hex});
    }

    fixture.world_content_hash = Hash64ToHex(content_hash);
    fixture.world_manifest_hash = Hash64ToHex(manifest_hash);
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
        PrintSkip("unable to create ENet server fixture");
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
    const TestResult result = TestFailedWorldUpdatePreservesPreviousCache();
    if (result == TestResult::Pass) {
        return 0;
    }
    if (result == TestResult::Skip) {
        return kSkipReturnCode;
    }
    return 1;
}
