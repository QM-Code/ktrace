#include "karma/renderer/backend.hpp"

#include "../backend_factory_internal.hpp"
#include "../direct_sampler_observability_internal.hpp"
#include "../debug_line_internal.hpp"
#include "../directional_shadow_internal.hpp"
#include "../environment_lighting_internal.hpp"
#include "../material_lighting_internal.hpp"
#include "../material_semantics_internal.hpp"

#include "karma/common/logging.hpp"
#include "karma/common/data_path_resolver.hpp"
#include "karma/platform/window.hpp"

#include <spdlog/spdlog.h>

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/math.h>

#include <glm/glm.hpp>

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <array>
#include <unordered_map>
#include <vector>

namespace karma::renderer_backend {
namespace {

struct PosNormalVertex {
    float x;
    float y;
    float z;
    float nx;
    float ny;
    float nz;
    float u;
    float v;

    static bgfx::VertexLayout layout() {
        bgfx::VertexLayout layout;
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
        return layout;
    }
};

bgfx::ShaderHandle loadShader(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return BGFX_INVALID_HANDLE;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    const bgfx::Memory* mem = bgfx::alloc(static_cast<uint32_t>(size + 1));
    if (!file.read(reinterpret_cast<char*>(mem->data), size)) {
        return BGFX_INVALID_HANDLE;
    }
    mem->data[mem->size - 1] = '\0';
    return bgfx::createShader(mem);
}

std::string TrimAscii(const std::string& input) {
    std::size_t start = 0u;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
        ++start;
    }
    std::size_t end = input.size();
    while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1u])) != 0) {
        --end;
    }
    return input.substr(start, end - start);
}

constexpr int kBgfxIntegrityManifestVersion = 1;
constexpr const char* kBgfxIntegrityManifestAlgorithm = "fnv1a64";
constexpr const char* kBgfxIntegritySignedEnvelopeMode = "v1";

bool ParseBgfxIntegrityManifestVersion(const std::string& token, int& out_version) {
    if (token.empty()) {
        return false;
    }
    int version = 0;
    for (char ch : token) {
        if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
            return false;
        }
        version = (version * 10) + (ch - '0');
    }
    out_version = version;
    return true;
}

bool ParseBgfxIntegrityManifestHash(const std::string& token, std::string& out_hash) {
    std::string candidate = TrimAscii(token);
    if (candidate.rfind("0x", 0u) == 0u || candidate.rfind("0X", 0u) == 0u) {
        candidate.erase(0u, 2u);
    }
    if (candidate.size() != 16u) {
        return false;
    }
    for (char& ch : candidate) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isxdigit(uch) == 0) {
            return false;
        }
        ch = static_cast<char>(std::tolower(uch));
    }
    out_hash = candidate;
    return true;
}

bool ParseBgfxIntegrityManifestEnvelopeSignature(
    const std::string& token, std::string& out_signature) {
    std::string candidate = TrimAscii(token);
    if (candidate.rfind("0x", 0u) == 0u || candidate.rfind("0X", 0u) == 0u) {
        candidate.erase(0u, 2u);
    }
    if (candidate.size() < 16u || (candidate.size() % 2u) != 0u) {
        return false;
    }
    for (char& ch : candidate) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isxdigit(uch) == 0) {
            return false;
        }
        ch = static_cast<char>(std::tolower(uch));
    }
    out_signature = candidate;
    return true;
}

bool ValidateBgfxIntegrityManifestTrustChainToken(const std::string& token) {
    const std::string candidate = TrimAscii(token);
    if (candidate.empty()) {
        return false;
    }
    for (char ch : candidate) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        const bool allowed = (std::isalnum(uch) != 0) ||
                             ch == '.' || ch == '_' || ch == '-' || ch == ':' || ch == '/';
        if (!allowed) {
            return false;
        }
    }
    return true;
}

struct ParsedBgfxIntegrityManifest {
    bool parse_ready = false;
    std::string parse_reason = "source_missing_and_integrity_manifest_parse_failed";
    bool version_supported = false;
    bool algorithm_supported = false;
    bool hash_present = false;
    std::string hash{};
    bool signed_envelope_declared = false;
};

ParsedBgfxIntegrityManifest ParseBgfxIntegrityManifest(const std::string& manifest_path) {
    ParsedBgfxIntegrityManifest manifest{};
    std::ifstream manifest_file(manifest_path);
    if (!manifest_file) {
        return manifest;
    }

    std::unordered_map<std::string, std::string> fields{};
    const auto is_allowed_key = [](const std::string& key) -> bool {
        return key == "version" ||
               key == "algorithm" ||
               key == "hash" ||
               key == "signed_envelope" ||
               key == "signature" ||
               key == "trust_chain";
    };
    std::string line;
    while (std::getline(manifest_file, line)) {
        // Canonicalization boundary: accept LF and CRLF (normalize trailing '\r'),
        // but reject embedded carriage returns or tabs to keep manifest shape strict.
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.find('\r') != std::string::npos) {
            manifest.parse_reason = "source_missing_and_integrity_manifest_noncanonical_line_endings";
            return manifest;
        }
        const std::size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line.erase(comment_pos);
        }
        if (line.find('\t') != std::string::npos) {
            manifest.parse_reason = "source_missing_and_integrity_manifest_noncanonical_whitespace";
            return manifest;
        }
        const std::string trimmed = TrimAscii(line);
        if (trimmed.empty()) {
            continue;
        }
        const std::size_t equals_pos = trimmed.find('=');
        if (equals_pos == std::string::npos) {
            manifest.parse_reason = "source_missing_and_integrity_manifest_invalid_token_form";
            return manifest;
        }
        std::string key = TrimAscii(trimmed.substr(0u, equals_pos));
        std::string value = TrimAscii(trimmed.substr(equals_pos + 1u));
        if (key.empty() || value.empty()) {
            manifest.parse_reason = "source_missing_and_integrity_manifest_invalid_token_form";
            return manifest;
        }
        for (char ch : key) {
            if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                manifest.parse_reason = "source_missing_and_integrity_manifest_noncanonical_whitespace";
                return manifest;
            }
        }
        for (char ch : value) {
            if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
                manifest.parse_reason = "source_missing_and_integrity_manifest_noncanonical_whitespace";
                return manifest;
            }
        }
        for (char& ch : key) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (!is_allowed_key(key)) {
            manifest.parse_reason = "source_missing_and_integrity_manifest_unknown_key";
            return manifest;
        }
        if (fields.find(key) != fields.end()) {
            manifest.parse_reason = "source_missing_and_integrity_manifest_duplicate_key";
            return manifest;
        }
        fields.emplace(key, value);
    }

    if (fields.empty()) {
        manifest.parse_reason = "source_missing_and_integrity_manifest_invalid_token_form";
        return manifest;
    }

    const auto version_it = fields.find("version");
    if (version_it != fields.end()) {
        int parsed_version = 0;
        if (!ParseBgfxIntegrityManifestVersion(version_it->second, parsed_version)) {
            manifest.parse_reason = "source_missing_and_integrity_manifest_invalid_value_form";
            return manifest;
        }
        manifest.version_supported = (parsed_version == kBgfxIntegrityManifestVersion);
    }

    const auto algorithm_it = fields.find("algorithm");
    if (algorithm_it != fields.end()) {
        std::string algorithm = TrimAscii(algorithm_it->second);
        for (char& ch : algorithm) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (algorithm.empty()) {
            manifest.parse_reason = "source_missing_and_integrity_manifest_invalid_value_form";
            return manifest;
        }
        for (char ch : algorithm) {
            const unsigned char uch = static_cast<unsigned char>(ch);
            const bool allowed = (std::isalnum(uch) != 0) || ch == '-' || ch == '_';
            if (!allowed) {
                manifest.parse_reason = "source_missing_and_integrity_manifest_invalid_value_form";
                return manifest;
            }
        }
        manifest.algorithm_supported = (algorithm == kBgfxIntegrityManifestAlgorithm);
    }

    const auto hash_it = fields.find("hash");
    if (hash_it != fields.end()) {
        std::string normalized_hash{};
        if (!ParseBgfxIntegrityManifestHash(hash_it->second, normalized_hash)) {
            manifest.parse_reason = "source_missing_and_integrity_manifest_invalid_value_form";
            return manifest;
        }
        manifest.hash_present = true;
        manifest.hash = normalized_hash;
    }

    const auto signed_envelope_it = fields.find("signed_envelope");
    const auto signature_it = fields.find("signature");
    const auto trust_chain_it = fields.find("trust_chain");
    const bool has_signature = (signature_it != fields.end());
    const bool has_trust_chain = (trust_chain_it != fields.end());

    if (signed_envelope_it == fields.end()) {
        if (has_signature || has_trust_chain) {
            manifest.parse_reason = "source_missing_and_integrity_manifest_signed_envelope_metadata_without_mode";
            return manifest;
        }
    } else {
        std::string signed_envelope_mode = TrimAscii(signed_envelope_it->second);
        for (char& ch : signed_envelope_mode) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (signed_envelope_mode.empty()) {
            manifest.parse_reason = "source_missing_and_integrity_manifest_signed_envelope_invalid_value_form";
            return manifest;
        }
        if (signed_envelope_mode == "none") {
            if (has_signature || has_trust_chain) {
                manifest.parse_reason = "source_missing_and_integrity_manifest_signed_envelope_metadata_unexpected";
                return manifest;
            }
        } else if (signed_envelope_mode == kBgfxIntegritySignedEnvelopeMode) {
            if (!has_signature || !has_trust_chain) {
                manifest.parse_reason = "source_missing_and_integrity_manifest_signed_envelope_metadata_missing";
                return manifest;
            }
            std::string normalized_signature{};
            if (!ParseBgfxIntegrityManifestEnvelopeSignature(signature_it->second, normalized_signature)) {
                manifest.parse_reason = "source_missing_and_integrity_manifest_signed_envelope_invalid_value_form";
                return manifest;
            }
            if (!ValidateBgfxIntegrityManifestTrustChainToken(trust_chain_it->second)) {
                manifest.parse_reason = "source_missing_and_integrity_manifest_signed_envelope_invalid_value_form";
                return manifest;
            }
            manifest.signed_envelope_declared = true;
        } else {
            manifest.parse_reason = "source_missing_and_integrity_manifest_signed_envelope_invalid_value_form";
            return manifest;
        }
    }

    manifest.parse_ready = true;
    return manifest;
}

std::optional<std::string> ComputeFnv1a64FileHash(const std::string& path) {
    std::ifstream binary_file(path, std::ios::binary);
    if (!binary_file) {
        return std::nullopt;
    }
    std::uint64_t hash = UINT64_C(14695981039346656037);
    std::array<char, 4096> buffer{};
    while (binary_file) {
        binary_file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize bytes = binary_file.gcount();
        for (std::streamsize i = 0; i < bytes; ++i) {
            hash ^= static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);
            hash *= UINT64_C(1099511628211);
        }
    }
    if (!binary_file.eof()) {
        return std::nullopt;
    }
    std::ostringstream encoded;
    encoded << std::hex << std::nouppercase << std::setfill('0') << std::setw(16) << hash;
    return encoded.str();
}

struct BgfxDirectSamplerShaderAlignment {
    bool source_exists = false;
    bool source_declares_direct_contract = false;
    bool binary_exists = false;
    bool binary_non_empty = false;
    bool binary_up_to_date = false;
    bool source_absent_integrity_ready = false;
    bool source_present_mode = false;
    bool source_absent_compat_mode = false;
    bool aligned = false;
    std::string source_path{};
    std::string binary_path{};
    std::string integrity_manifest_path{};
    std::string source_absent_integrity_reason{"source_missing_and_integrity_contract_unavailable"};
    std::string reason{"uninitialized"};
};

BgfxDirectSamplerShaderAlignment EvaluateBgfxDirectSamplerShaderAlignment() {
    namespace fs = std::filesystem;
    BgfxDirectSamplerShaderAlignment report{};
    report.source_path = karma::data::Resolve("bgfx/shaders/mesh/fs_mesh.sc").string();
    report.binary_path = karma::data::Resolve("bgfx/shaders/bin/vk/mesh/fs_mesh.bin").string();
    report.integrity_manifest_path = report.binary_path + ".integrity";

    std::error_code ec{};
    report.source_exists = fs::exists(report.source_path, ec);
    if (!ec && report.source_exists) {
        std::ifstream source_file(report.source_path, std::ios::binary);
        std::string source_text((std::istreambuf_iterator<char>(source_file)), std::istreambuf_iterator<char>());
        const bool has_mode = source_text.find("uniform vec4 u_textureMode;") != std::string::npos;
        const bool has_normal = source_text.find("SAMPLER2D(s_normal, 1);") != std::string::npos;
        const bool has_occlusion = source_text.find("SAMPLER2D(s_occlusion, 2);") != std::string::npos;
        report.source_declares_direct_contract = has_mode && has_normal && has_occlusion;
    }

    ec.clear();
    report.binary_exists = fs::exists(report.binary_path, ec);
    if (!ec && report.binary_exists) {
        ec.clear();
        const std::uintmax_t size = fs::file_size(report.binary_path, ec);
        report.binary_non_empty = (!ec && size > 0u);
    }

    if (report.source_exists && report.binary_exists) {
        ec.clear();
        const auto source_time = fs::last_write_time(report.source_path, ec);
        const bool source_time_ok = !ec;
        ec.clear();
        const auto binary_time = fs::last_write_time(report.binary_path, ec);
        const bool binary_time_ok = !ec;
        report.binary_up_to_date = source_time_ok && binary_time_ok && (binary_time >= source_time);
    }
    detail::BgfxSourceAbsentIntegrityReport source_absent_integrity{};
    if (report.source_exists) {
        source_absent_integrity.ready = true;
        source_absent_integrity.reason = "source_present_integrity_not_required";
    } else {
        bool manifest_exists = false;
        bool manifest_parse_ready = false;
        std::string manifest_parse_reason = "source_missing_and_integrity_manifest_parse_failed";
        bool manifest_version_supported = false;
        bool manifest_algorithm_supported = false;
        bool manifest_hash_present = false;
        bool binary_hash_available = false;
        bool hash_matches_manifest = false;
        bool signed_envelope_declared = false;
        constexpr bool kSignedEnvelopeVerificationAvailable = false;
        ParsedBgfxIntegrityManifest parsed_manifest{};

        ec.clear();
        manifest_exists = fs::exists(report.integrity_manifest_path, ec) && !ec;
        if (manifest_exists) {
            parsed_manifest = ParseBgfxIntegrityManifest(report.integrity_manifest_path);
            manifest_parse_ready = parsed_manifest.parse_ready;
            manifest_parse_reason = parsed_manifest.parse_reason;
            manifest_version_supported = parsed_manifest.version_supported;
            manifest_algorithm_supported = parsed_manifest.algorithm_supported;
            manifest_hash_present = parsed_manifest.hash_present;
            signed_envelope_declared = parsed_manifest.signed_envelope_declared;
        }

        std::optional<std::string> actual_hash{};
        if (report.binary_exists && report.binary_non_empty) {
            actual_hash = ComputeFnv1a64FileHash(report.binary_path);
        }
        binary_hash_available = actual_hash.has_value();
        hash_matches_manifest =
            manifest_hash_present && binary_hash_available && (*actual_hash == parsed_manifest.hash);

        source_absent_integrity = detail::EvaluateBgfxSourceAbsentIntegrityPolicy(
            detail::BgfxSourceAbsentIntegrityInput{
                manifest_exists,
                manifest_parse_ready,
                manifest_parse_reason,
                manifest_version_supported,
                manifest_algorithm_supported,
                manifest_hash_present,
                binary_hash_available,
                hash_matches_manifest,
                signed_envelope_declared,
                kSignedEnvelopeVerificationAvailable,
            });
    }
    report.source_absent_integrity_ready = source_absent_integrity.ready;
    report.source_absent_integrity_reason = source_absent_integrity.reason;

    const detail::BgfxDirectSamplerAlignmentReport policy =
        detail::EvaluateBgfxDirectSamplerAlignmentPolicy(
            detail::BgfxDirectSamplerAlignmentInput{
                report.source_exists,
                report.source_declares_direct_contract,
                report.binary_exists,
                report.binary_non_empty,
                report.binary_up_to_date,
                report.source_absent_integrity_ready,
                report.source_absent_integrity_reason,
            });
    report.source_present_mode = policy.source_present_mode;
    report.source_absent_compat_mode = policy.source_absent_compat_mode;
    report.aligned = policy.ready;
    report.reason = policy.reason;
    return report;
}

struct Mesh {
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle ibh = BGFX_INVALID_HANDLE;
    uint32_t num_indices = 0;
    bgfx::TextureHandle tex = BGFX_INVALID_HANDLE;
    std::vector<glm::vec3> shadow_positions{};
    std::vector<uint32_t> shadow_indices{};
    glm::vec3 shadow_center{0.0f, 0.0f, 0.0f};
};

struct Material {
    detail::ResolvedMaterialSemantics semantics{};
    bgfx::TextureHandle tex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle normal_tex = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle occlusion_tex = BGFX_INVALID_HANDLE;
    detail::MaterialShaderTextureInputPath shader_input_path =
        detail::MaterialShaderTextureInputPath::Disabled;
    bool shader_uses_normal_input = false;
    bool shader_uses_occlusion_input = false;
};

class BgfxCallback final : public bgfx::CallbackI {
public:
    void fatal(const char* filePath, uint16_t line, bgfx::Fatal::Enum code, const char* str) override {
        spdlog::error("BGFX fatal: {}:{} code={} {}", filePath ? filePath : "(null)", line, static_cast<int>(code),
                      str ? str : "");
        std::abort();
    }

    void traceVargs(const char* filePath, uint16_t line, const char* format, va_list argList) override {
        if (!karma::logging::ShouldTraceChannel("render.bgfx.internal")) {
            return;
        }
        std::array<char, 2048> stack_buf{};
        va_list args_copy;
        va_copy(args_copy, argList);
        int needed = std::vsnprintf(stack_buf.data(), stack_buf.size(), format, args_copy);
        va_end(args_copy);
        if (needed < 0) {
            return;
        }
        std::string message;
        if (static_cast<size_t>(needed) < stack_buf.size()) {
            message.assign(stack_buf.data(), static_cast<size_t>(needed));
        } else {
            std::vector<char> heap_buf(static_cast<size_t>(needed) + 1);
            va_list args_copy2;
            va_copy(args_copy2, argList);
            std::vsnprintf(heap_buf.data(), heap_buf.size(), format, args_copy2);
            va_end(args_copy2);
            message.assign(heap_buf.data(), static_cast<size_t>(needed));
        }
        KARMA_TRACE("render.bgfx.internal", "{}:{}: {}", filePath ? filePath : "(null)", line, message.c_str());
    }

    void profilerBegin(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override {}
    void profilerEnd() override {}
    uint32_t cacheReadSize(uint64_t) override { return 0; }
    bool cacheRead(uint64_t, void*, uint32_t) override { return false; }
    void cacheWrite(uint64_t, const void*, uint32_t) override {}
    void screenShot(const char*, uint32_t, uint32_t, uint32_t, const void*, uint32_t, bool) override {}
    void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override {}
    void captureEnd() override {}
    void captureFrame(const void*, uint32_t) override {}
};

} // namespace

bgfx::TextureHandle createWhiteTexture() {
    const uint32_t white = 0xffffffff;
    const bgfx::Memory* mem = bgfx::copy(&white, sizeof(white));
    return bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 0, mem);
}

bgfx::TextureHandle createTextureFromData(const renderer::MeshData::TextureData& tex) {
    const std::vector<uint8_t> rgba_pixels = detail::ExpandTextureToRgba8(tex);
    if (rgba_pixels.empty() || tex.width <= 0 || tex.height <= 0) {
        return BGFX_INVALID_HANDLE;
    }
    return bgfx::createTexture2D(
        static_cast<uint16_t>(tex.width),
        static_cast<uint16_t>(tex.height),
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        0,
        bgfx::copy(rgba_pixels.data(), static_cast<uint32_t>(rgba_pixels.size())));
}

uint32_t PackColorRgba8(const glm::vec4& color) {
    const auto to_u8 = [](float value) -> uint32_t {
        const float scaled = std::clamp(value, 0.0f, 1.0f) * 255.0f;
        return static_cast<uint32_t>(scaled + 0.5f);
    };
    return (to_u8(color.r) << 24u) |
           (to_u8(color.g) << 16u) |
           (to_u8(color.b) << 8u) |
           to_u8(color.a);
}

class BgfxBackend final : public Backend {
 public:
    explicit BgfxBackend(karma::platform::Window& window) {
        karma::platform::NativeWindowHandle native = window.nativeHandle();
        bgfx::PlatformData pd{};
        if (native.is_wayland && native.wayland_surface && native.display) {
            pd.nwh = native.wayland_surface;
            pd.ndt = native.display;
            pd.type = bgfx::NativeWindowHandleType::Wayland;
            KARMA_TRACE("render.bgfx", "using Wayland surface handle");
        } else if (native.window) {
            pd.nwh = native.window;
            pd.ndt = native.display;
            pd.type = bgfx::NativeWindowHandleType::Default;
            KARMA_TRACE("render.bgfx", "using default native window handle");
        } else {
            spdlog::error("Graphics(Bgfx): failed to resolve native window handle");
        }
        KARMA_TRACE("render.bgfx",
                    "platform data type={} nwh={} ndt={}",
                    static_cast<int>(pd.type), pd.nwh, pd.ndt);
        if (!pd.ndt || !pd.nwh) {
            spdlog::error("Graphics(Bgfx): missing native display/window handle (ndt={}, nwh={})", pd.ndt, pd.nwh);
            return;
        }

        bgfx::Init init{};
        init.type = bgfx::RendererType::Vulkan;
        init.vendorId = BGFX_PCI_ID_NONE;
        init.platformData = pd;
        init.callback = &callback_;
        window.getFramebufferSize(width_, height_);
        init.resolution.width = static_cast<uint32_t>(width_);
        init.resolution.height = static_cast<uint32_t>(height_);
        init.resolution.reset = BGFX_RESET_VSYNC | BGFX_RESET_SRGB_BACKBUFFER;
        KARMA_TRACE("render.bgfx",
                    "init renderer=Vulkan size={}x{} reset=0x{:08x}",
                    init.resolution.width, init.resolution.height, init.resolution.reset);
        if (!bgfx::init(init)) {
            spdlog::error("BGFX init failed");
            return;
        }
        KARMA_TRACE("render.bgfx", "init success renderer={}",
                    bgfx::getRendererName(bgfx::getRendererType()));
        initialized_ = true;
        bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);

        const auto vs_path = karma::data::Resolve("bgfx/shaders/bin/vk/mesh/vs_mesh.bin").string();
        const auto fs_path = karma::data::Resolve("bgfx/shaders/bin/vk/mesh/fs_mesh.bin").string();
        auto vsh = loadShader(vs_path);
        auto fsh = loadShader(fs_path);
        if (!bgfx::isValid(vsh) || !bgfx::isValid(fsh)) {
            spdlog::error("Failed to load BGFX shaders: {} {}", vs_path, fs_path);
        } else {
            program_ = bgfx::createProgram(vsh, fsh, true);
        }
        KARMA_TRACE("render.bgfx", "program valid={}", bgfx::isValid(program_) ? 1 : 0);
        layout_ = PosNormalVertex::layout();
        u_color_ = bgfx::createUniform("u_color", bgfx::UniformType::Vec4);
        u_light_dir_ = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
        u_light_color_ = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
        u_ambient_color_ = bgfx::createUniform("u_ambientColor", bgfx::UniformType::Vec4);
        u_unlit_ = bgfx::createUniform("u_unlit", bgfx::UniformType::Vec4);
        u_texture_mode_ = bgfx::createUniform("u_textureMode", bgfx::UniformType::Vec4);
        s_tex_ = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);
        s_normal_ = bgfx::createUniform("s_normal", bgfx::UniformType::Sampler);
        s_occlusion_ = bgfx::createUniform("s_occlusion", bgfx::UniformType::Sampler);
        white_tex_ = createWhiteTexture();
        const bool uniform_contract_ready =
            bgfx::isValid(program_) &&
            bgfx::isValid(s_tex_) &&
            bgfx::isValid(s_normal_) &&
            bgfx::isValid(s_occlusion_) &&
            bgfx::isValid(u_texture_mode_);
        direct_sampler_shader_alignment_ = EvaluateBgfxDirectSamplerShaderAlignment();
        direct_sampler_contract_report_ = detail::EvaluateBgfxDirectSamplerContract(
            uniform_contract_ready,
            detail::BgfxDirectSamplerAlignmentReport{
                direct_sampler_shader_alignment_.source_present_mode,
                direct_sampler_shader_alignment_.source_absent_compat_mode,
                direct_sampler_shader_alignment_.aligned,
                direct_sampler_shader_alignment_.reason,
            });
        supports_direct_multi_sampler_inputs_ = direct_sampler_contract_report_.ready_for_direct_path;
        direct_sampler_disable_reason_ = direct_sampler_contract_report_.reason;
        KARMA_TRACE("render.bgfx",
                    "direct sampler readiness uniforms={} aligned={} sourceModePresent={} sourceModeCompat={} sourceAbsentIntegrity={} enabled={} reason={} integrityReason={} source='{}' binary='{}' manifest='{}'",
                    direct_sampler_contract_report_.uniform_contract_ready ? 1 : 0,
                    direct_sampler_contract_report_.shader_alignment_ready ? 1 : 0,
                    direct_sampler_shader_alignment_.source_present_mode ? 1 : 0,
                    direct_sampler_shader_alignment_.source_absent_compat_mode ? 1 : 0,
                    direct_sampler_shader_alignment_.source_absent_integrity_ready ? 1 : 0,
                    supports_direct_multi_sampler_inputs_ ? 1 : 0,
                    direct_sampler_disable_reason_,
                    direct_sampler_shader_alignment_.source_absent_integrity_reason,
                    direct_sampler_shader_alignment_.source_path,
                    direct_sampler_shader_alignment_.binary_path,
                    direct_sampler_shader_alignment_.integrity_manifest_path);
        if (!supports_direct_multi_sampler_inputs_) {
            spdlog::warn("Graphics(Bgfx): direct sampler path disabled (reason={}, integrityReason={}, source='{}', binary='{}', manifest='{}')",
                         direct_sampler_disable_reason_,
                         direct_sampler_shader_alignment_.source_absent_integrity_reason,
                         direct_sampler_shader_alignment_.source_path,
                         direct_sampler_shader_alignment_.binary_path,
                         direct_sampler_shader_alignment_.integrity_manifest_path);
        }
    }

    ~BgfxBackend() override {
        if (!initialized_) {
            return;
        }
        for (auto& [id, mesh] : meshes_) {
            if (bgfx::isValid(mesh.vbh)) bgfx::destroy(mesh.vbh);
            if (bgfx::isValid(mesh.ibh)) bgfx::destroy(mesh.ibh);
        }
        for (auto& [id, material] : materials_) {
            if (bgfx::isValid(material.tex)) bgfx::destroy(material.tex);
            if (bgfx::isValid(material.normal_tex)) bgfx::destroy(material.normal_tex);
            if (bgfx::isValid(material.occlusion_tex)) bgfx::destroy(material.occlusion_tex);
        }
        if (bgfx::isValid(program_)) bgfx::destroy(program_);
        if (bgfx::isValid(u_color_)) bgfx::destroy(u_color_);
        if (bgfx::isValid(u_light_dir_)) bgfx::destroy(u_light_dir_);
        if (bgfx::isValid(u_light_color_)) bgfx::destroy(u_light_color_);
        if (bgfx::isValid(u_ambient_color_)) bgfx::destroy(u_ambient_color_);
        if (bgfx::isValid(u_unlit_)) bgfx::destroy(u_unlit_);
        if (bgfx::isValid(u_texture_mode_)) bgfx::destroy(u_texture_mode_);
        if (bgfx::isValid(s_tex_)) bgfx::destroy(s_tex_);
        if (bgfx::isValid(s_normal_)) bgfx::destroy(s_normal_);
        if (bgfx::isValid(s_occlusion_)) bgfx::destroy(s_occlusion_);
        if (bgfx::isValid(white_tex_)) bgfx::destroy(white_tex_);
        bgfx::shutdown();
    }

    void beginFrame(int width, int height, float dt) override {
        (void)dt;
        if (!initialized_) {
            return;
        }
        draw_items_.clear();
        debug_lines_.clear();
        width_ = width;
        height_ = height;
        bgfx::reset(static_cast<uint32_t>(width), static_cast<uint32_t>(height), BGFX_RESET_VSYNC | BGFX_RESET_SRGB_BACKBUFFER);
        const detail::ResolvedEnvironmentLightingSemantics environment_semantics =
            detail::ResolveEnvironmentLightingSemantics(environment_);
        const glm::vec4 clear_color = detail::ComputeEnvironmentClearColor(environment_semantics);
        bgfx::setViewClear(0,
                           BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                           PackColorRgba8(clear_color),
                           1.0f,
                           0);
        bgfx::setViewRect(0, 0, 0, static_cast<uint16_t>(width), static_cast<uint16_t>(height));
        bgfx::touch(0);
    }

    void endFrame() override {
        if (!initialized_) {
            return;
        }
        bgfx::frame();
    }

    renderer::MeshId createMesh(const renderer::MeshData& mesh) override {
        if (!initialized_) {
            return renderer::kInvalidMesh;
        }
        KARMA_TRACE("render.bgfx", "createMesh vertices={} indices={}",
                    mesh.positions.size(), mesh.indices.size());
        std::vector<PosNormalVertex> vertices;
        vertices.reserve(mesh.positions.size());
        for (size_t i = 0; i < mesh.positions.size(); ++i) {
            const glm::vec3& p = mesh.positions[i];
            const glm::vec3 n = i < mesh.normals.size() ? mesh.normals[i] : glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::vec2 uv = i < mesh.uvs.size() ? mesh.uvs[i] : glm::vec2(0.0f, 0.0f);
            vertices.push_back({p.x, p.y, p.z, n.x, n.y, n.z, uv.x, uv.y});
        }
        const bgfx::Memory* vmem = bgfx::copy(vertices.data(), sizeof(PosNormalVertex) * vertices.size());
        const bgfx::Memory* imem = bgfx::copy(mesh.indices.data(), sizeof(uint32_t) * mesh.indices.size());

        Mesh out;
        out.vbh = bgfx::createVertexBuffer(vmem, layout_);
        out.ibh = bgfx::createIndexBuffer(imem, BGFX_BUFFER_INDEX32);
        out.num_indices = static_cast<uint32_t>(mesh.indices.size());
        out.shadow_positions = mesh.positions;
        out.shadow_indices = mesh.indices;
        if (!out.shadow_positions.empty()) {
            glm::vec3 min_pos = out.shadow_positions.front();
            glm::vec3 max_pos = out.shadow_positions.front();
            for (const glm::vec3& p : out.shadow_positions) {
                min_pos = glm::min(min_pos, p);
                max_pos = glm::max(max_pos, p);
            }
            out.shadow_center = 0.5f * (min_pos + max_pos);
        }
        if (mesh.albedo && !mesh.albedo->pixels.empty()) {
            out.tex = createTextureFromData(*mesh.albedo);
            KARMA_TRACE("render.bgfx", "createMesh texture={} {}x{}",
                        bgfx::isValid(out.tex) ? 1 : 0,
                        mesh.albedo->width, mesh.albedo->height);
        }

        renderer::MeshId id = next_mesh_id_++;
        meshes_[id] = out;
        return id;
    }

    void destroyMesh(renderer::MeshId mesh) override {
        auto it = meshes_.find(mesh);
        if (it == meshes_.end()) return;
        if (bgfx::isValid(it->second.vbh)) bgfx::destroy(it->second.vbh);
        if (bgfx::isValid(it->second.ibh)) bgfx::destroy(it->second.ibh);
        if (bgfx::isValid(it->second.tex)) bgfx::destroy(it->second.tex);
        meshes_.erase(it);
    }

    renderer::MaterialId createMaterial(const renderer::MaterialDesc& material) override {
        if (!initialized_) {
            return renderer::kInvalidMaterial;
        }
        const detail::MaterialTextureSetLifecycleIngestion texture_ingestion =
            detail::IngestMaterialTextureSetForLifecycle(material);
        const detail::MaterialShaderInputContract shader_input_contract =
            detail::ResolveMaterialShaderInputContract(
                material, texture_ingestion, supports_direct_multi_sampler_inputs_);
        const detail::ResolvedMaterialSemantics semantics =
            detail::ResolveMaterialSemantics(material);
        if (!detail::ValidateResolvedMaterialSemantics(semantics)) {
            spdlog::error("Graphics(Bgfx): material semantics validation failed");
            return renderer::kInvalidMaterial;
        }
        Material out;
        out.semantics = semantics;
        out.shader_input_path = shader_input_contract.path;
        out.shader_uses_normal_input = shader_input_contract.used_normal_lifecycle_texture;
        out.shader_uses_occlusion_input = shader_input_contract.used_occlusion_lifecycle_texture;
        if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler &&
            shader_input_contract.direct_albedo_texture) {
            out.tex = createTextureFromData(*shader_input_contract.direct_albedo_texture);
        } else if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::CompositeFallback &&
                   shader_input_contract.fallback_composite_texture) {
            out.tex = createTextureFromData(*shader_input_contract.fallback_composite_texture);
        } else if (material.albedo && !material.albedo->pixels.empty()) {
            out.tex = createTextureFromData(*material.albedo);
            KARMA_TRACE("render.bgfx", "createMaterial texture={} {}x{}",
                        bgfx::isValid(out.tex) ? 1 : 0,
                        material.albedo->width,
                        material.albedo->height);
        }
        if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler &&
            shader_input_contract.direct_normal_texture) {
            out.normal_tex = createTextureFromData(*shader_input_contract.direct_normal_texture);
        } else if (texture_ingestion.normal.texture) {
            out.normal_tex = createTextureFromData(*texture_ingestion.normal.texture);
        }
        if (shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler &&
            shader_input_contract.direct_occlusion_texture) {
            out.occlusion_tex = createTextureFromData(*shader_input_contract.direct_occlusion_texture);
        } else if (texture_ingestion.occlusion.texture) {
            out.occlusion_tex = createTextureFromData(*texture_ingestion.occlusion.texture);
        }
        KARMA_TRACE("render.bgfx",
                    "createMaterial semantics metallic={:.3f} roughness={:.3f} normalVar={:.3f} occlusion={:.3f} occlusionEdge={:.3f} emissive=({:.3f},{:.3f},{:.3f}) alphaMode={} alpha={} cutoff={:.3f} draw={} blend={} doubleSided={} mrTex={} emissiveTex={} normalTex={} occlusionTex={} normalLifecycleTex={} occlusionLifecycleTex={} normalBounded={} occlusionBounded={} shaderPath={} shaderDirect={} shaderFallbackComposite={} shaderConsumesNormal={} shaderConsumesOcclusion={} shaderUsesAlbedo={} shaderTextureBounded={}",
                    out.semantics.metallic, out.semantics.roughness, out.semantics.normal_variation, out.semantics.occlusion,
                    out.semantics.occlusion_edge,
                    out.semantics.emissive.r, out.semantics.emissive.g, out.semantics.emissive.b,
                    static_cast<int>(out.semantics.alpha_mode), out.semantics.base_color.a, out.semantics.alpha_cutoff,
                    out.semantics.draw ? 1 : 0, out.semantics.alpha_blend ? 1 : 0, out.semantics.double_sided ? 1 : 0,
                    out.semantics.used_metallic_roughness_texture ? 1 : 0,
                    out.semantics.used_emissive_texture ? 1 : 0,
                    out.semantics.used_normal_texture ? 1 : 0,
                    out.semantics.used_occlusion_texture ? 1 : 0,
                    bgfx::isValid(out.normal_tex) ? 1 : 0,
                    bgfx::isValid(out.occlusion_tex) ? 1 : 0,
                    texture_ingestion.normal.bounded ? 1 : 0,
                    texture_ingestion.occlusion.bounded ? 1 : 0,
                    static_cast<int>(shader_input_contract.path),
                    shader_input_contract.path == detail::MaterialShaderTextureInputPath::DirectMultiSampler ? 1 : 0,
                    (shader_input_contract.path == detail::MaterialShaderTextureInputPath::CompositeFallback &&
                     shader_input_contract.fallback_composite_texture && bgfx::isValid(out.tex))
                        ? 1
                        : 0,
                    shader_input_contract.used_normal_lifecycle_texture ? 1 : 0,
                    shader_input_contract.used_occlusion_lifecycle_texture ? 1 : 0,
                    shader_input_contract.used_albedo_texture ? 1 : 0,
                    shader_input_contract.bounded ? 1 : 0);
        renderer::MaterialId id = next_material_id_++;
        materials_[id] = out;
        return id;
    }

    void destroyMaterial(renderer::MaterialId material) override {
        auto it = materials_.find(material);
        if (it == materials_.end()) return;
        if (bgfx::isValid(it->second.tex)) bgfx::destroy(it->second.tex);
        if (bgfx::isValid(it->second.normal_tex)) bgfx::destroy(it->second.normal_tex);
        if (bgfx::isValid(it->second.occlusion_tex)) bgfx::destroy(it->second.occlusion_tex);
        materials_.erase(it);
    }

    void submit(const renderer::DrawItem& item) override {
        if (!initialized_) {
            return;
        }
        draw_items_.push_back(item);
    }

    void submitDebugLine(const renderer::DebugLineItem& line) override {
        if (!initialized_) {
            return;
        }
        const detail::ResolvedDebugLineSemantics semantics = detail::ResolveDebugLineSemantics(line);
        if (!semantics.draw || !detail::ValidateResolvedDebugLineSemantics(semantics)) {
            return;
        }
        debug_lines_.push_back(semantics);
    }

    void renderFrame() override {
        renderLayer(0);
    }

    void renderLayer(renderer::LayerId layer) override {
        if (!initialized_ || !bgfx::isValid(program_)) {
            return;
        }

        float view[16];
        float proj[16];
        bx::mtxLookAt(view, bx::Vec3(camera_.position.x, camera_.position.y, camera_.position.z),
                      bx::Vec3(camera_.target.x, camera_.target.y, camera_.target.z));
        // BGFX uses left-handed view/projection by default; mirror X to match the
        // engine's right-handed camera conventions, then flip culling below.
        float mirror[16];
        float view_mirror[16];
        bx::mtxScale(mirror, -1.0f, 1.0f, 1.0f);
        bx::mtxMul(view_mirror, view, mirror);
        bx::mtxProj(proj, camera_.fov_y_degrees, float(width_) / float(height_),
                    camera_.near_clip, camera_.far_clip, bgfx::getCaps()->homogeneousDepth);
        bgfx::setViewTransform(0, view_mirror, proj);

        struct RenderableDraw {
            const renderer::DrawItem* item = nullptr;
            Mesh* mesh = nullptr;
            Material* material = nullptr;
        };
        std::vector<RenderableDraw> renderables{};
        renderables.reserve(draw_items_.size());
        std::vector<detail::DirectionalShadowCaster> shadow_casters{};
        shadow_casters.reserve(draw_items_.size());
        for (const auto& item : draw_items_) {
            if (item.layer != layer) {
                continue;
            }
            auto mesh_it = meshes_.find(item.mesh);
            if (mesh_it == meshes_.end()) {
                continue;
            }
            auto mat_it = materials_.find(item.material);
            if (mat_it == materials_.end()) {
                continue;
            }
            if (!mat_it->second.semantics.draw) {
                continue;
            }
            renderables.push_back({&item, &mesh_it->second, &mat_it->second});
            if (!mesh_it->second.shadow_positions.empty() &&
                !mesh_it->second.shadow_indices.empty()) {
                detail::DirectionalShadowCaster caster{};
                caster.transform = item.transform;
                caster.positions = &mesh_it->second.shadow_positions;
                caster.indices = &mesh_it->second.shadow_indices;
                caster.sample_center = mesh_it->second.shadow_center;
                caster.casts_shadow = !mat_it->second.semantics.alpha_blend;
                shadow_casters.push_back(caster);
            }
        }

        const detail::ResolvedDirectionalShadowSemantics shadow_semantics =
            detail::ResolveDirectionalShadowSemantics(light_);
        const detail::ResolvedEnvironmentLightingSemantics environment_semantics =
            detail::ResolveEnvironmentLightingSemantics(environment_);
        const detail::DirectionalShadowMap shadow_map = detail::BuildDirectionalShadowMap(
            shadow_semantics,
            light_.direction,
            shadow_casters);
        std::size_t direct_sampler_draws = 0u;
        std::size_t fallback_sampler_draws = 0u;
        std::size_t direct_contract_draws = 0u;
        std::size_t forced_fallback_draws = 0u;
        std::size_t unexpected_direct_draws = 0u;

        for (const RenderableDraw& renderable : renderables) {
            const auto& item = *renderable.item;
            const auto& semantics = renderable.material->semantics;

            bgfx::setVertexBuffer(0, renderable.mesh->vbh);
            bgfx::setIndexBuffer(renderable.mesh->ibh);
            bgfx::setTransform(&item.transform[0][0]);

            float shadow_factor = 1.0f;
            if (!semantics.alpha_blend && shadow_map.ready) {
                const glm::vec3 sample_world = detail::TransformPoint(item.transform, renderable.mesh->shadow_center);
                const float visibility = detail::SampleDirectionalShadowVisibility(shadow_map, sample_world);
                shadow_factor = detail::ComputeDirectionalShadowFactor(shadow_map, visibility);
            }
            const detail::ResolvedMaterialLighting lighting =
                detail::ResolveMaterialLighting(semantics, light_, environment_semantics, shadow_factor);
            if (!detail::ValidateResolvedMaterialLighting(lighting)) {
                continue;
            }

            const float light_dir[4] = {light_.direction.x, light_.direction.y, light_.direction.z, 0.0f};
            const float material_color[4] = {
                lighting.color.r,
                lighting.color.g,
                lighting.color.b,
                lighting.color.a};
            const float light_color[4] = {
                lighting.light_color.r,
                lighting.light_color.g,
                lighting.light_color.b,
                lighting.light_color.a};
            const float ambient_color[4] = {
                lighting.ambient_color.r,
                lighting.ambient_color.g,
                lighting.ambient_color.b,
                lighting.ambient_color.a};
            const float unlit[4] = {light_.unlit, 0.0f, 0.0f, 0.0f};
            bgfx::setUniform(u_color_, material_color);
            bgfx::setUniform(u_light_dir_, light_dir);
            bgfx::setUniform(u_light_color_, light_color);
            bgfx::setUniform(u_ambient_color_, ambient_color);
            bgfx::setUniform(u_unlit_, unlit);

            const bool use_direct_sampler_path =
                supports_direct_multi_sampler_inputs_ &&
                renderable.material->shader_input_path == detail::MaterialShaderTextureInputPath::DirectMultiSampler;
            const bool contract_requests_direct =
                renderable.material->shader_input_path == detail::MaterialShaderTextureInputPath::DirectMultiSampler;
            if (contract_requests_direct) {
                ++direct_contract_draws;
            }
            if (use_direct_sampler_path) {
                ++direct_sampler_draws;
            } else {
                ++fallback_sampler_draws;
            }
            if (use_direct_sampler_path && !contract_requests_direct) {
                ++unexpected_direct_draws;
            }
            if (!use_direct_sampler_path && contract_requests_direct) {
                ++forced_fallback_draws;
            }
            bgfx::TextureHandle direct_albedo_texture = white_tex_;
            if (bgfx::isValid(renderable.material->tex)) {
                direct_albedo_texture = renderable.material->tex;
            }
            bgfx::TextureHandle fallback_base_texture = white_tex_;
            if (bgfx::isValid(renderable.material->tex)) {
                fallback_base_texture = renderable.material->tex;
            } else if (bgfx::isValid(renderable.mesh->tex)) {
                fallback_base_texture = renderable.mesh->tex;
            }
            const bgfx::TextureHandle base_texture = use_direct_sampler_path ? direct_albedo_texture : fallback_base_texture;
            if (bgfx::isValid(base_texture)) {
                bgfx::setTexture(0, s_tex_, base_texture);
            }
            if (bgfx::isValid(s_normal_)) {
                const bgfx::TextureHandle normal_texture =
                    (use_direct_sampler_path && bgfx::isValid(renderable.material->normal_tex))
                        ? renderable.material->normal_tex
                        : white_tex_;
                if (bgfx::isValid(normal_texture)) {
                    bgfx::setTexture(1, s_normal_, normal_texture);
                }
            }
            if (bgfx::isValid(s_occlusion_)) {
                const bgfx::TextureHandle occlusion_texture =
                    (use_direct_sampler_path && bgfx::isValid(renderable.material->occlusion_tex))
                        ? renderable.material->occlusion_tex
                        : white_tex_;
                if (bgfx::isValid(occlusion_texture)) {
                    bgfx::setTexture(2, s_occlusion_, occlusion_texture);
                }
            }
            if (bgfx::isValid(u_texture_mode_)) {
                const float texture_mode[4] = {
                    (use_direct_sampler_path && renderable.material->shader_uses_normal_input) ? 1.0f : 0.0f,
                    (use_direct_sampler_path && renderable.material->shader_uses_occlusion_input) ? 1.0f : 0.0f,
                    0.0f,
                    0.0f,
                };
                bgfx::setUniform(u_texture_mode_, texture_mode);
            }
            uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA;
            if (!semantics.double_sided) {
                state |= BGFX_STATE_CULL_CW;
            }
            if (semantics.alpha_blend) {
                state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
            }
            bgfx::setState(state);
            bgfx::submit(0, program_);
        }
        KARMA_TRACE_CHANGED(
            "render.bgfx",
            std::to_string(layer) + ":" +
                std::to_string(renderables.size()) + ":" +
                std::to_string(direct_sampler_draws) + ":" +
                std::to_string(fallback_sampler_draws) + ":" +
                std::to_string(supports_direct_multi_sampler_inputs_ ? 1 : 0) + ":" +
                direct_sampler_disable_reason_,
            "direct sampler frame layer={} draws={} direct={} fallback={} enabled={} reason={}",
            layer,
            renderables.size(),
            direct_sampler_draws,
            fallback_sampler_draws,
            supports_direct_multi_sampler_inputs_ ? 1 : 0,
            direct_sampler_disable_reason_);
        const detail::DirectSamplerDrawInvariantReport draw_invariants =
            detail::EvaluateDirectSamplerDrawInvariants(
                detail::DirectSamplerDrawInvariantInput{
                    supports_direct_multi_sampler_inputs_,
                    renderables.size(),
                    direct_contract_draws,
                    direct_sampler_draws,
                    fallback_sampler_draws,
                    forced_fallback_draws,
                    unexpected_direct_draws,
                });
        if (!draw_invariants.ok) {
            spdlog::error(
                "Graphics(Bgfx): direct sampler assertion failed (enabled={}, directContract={}, directDraws={}, fallbackDraws={}, forcedFallback={}, unexpectedDirect={}, reason={}, invariant={})",
                supports_direct_multi_sampler_inputs_ ? 1 : 0,
                direct_contract_draws,
                direct_sampler_draws,
                fallback_sampler_draws,
                forced_fallback_draws,
                unexpected_direct_draws,
                direct_sampler_disable_reason_,
                draw_invariants.reason);
        }
        KARMA_TRACE_CHANGED(
            "render.bgfx",
            std::to_string(layer) + ":" +
                std::to_string(renderables.size()) + ":" +
                std::to_string(direct_contract_draws) + ":" +
                std::to_string(direct_sampler_draws) + ":" +
                std::to_string(fallback_sampler_draws) + ":" +
                std::to_string(forced_fallback_draws) + ":" +
                std::to_string(unexpected_direct_draws) + ":" +
                std::to_string(draw_invariants.ok ? 1 : 0) + ":" +
                draw_invariants.reason,
            "direct sampler assertions layer={} draws={} contractDirect={} actualDirect={} fallback={} forcedFallback={} unexpectedDirect={} ok={} enabled={} reason={} invariant={}",
            layer,
            renderables.size(),
            direct_contract_draws,
            direct_sampler_draws,
            fallback_sampler_draws,
            forced_fallback_draws,
            unexpected_direct_draws,
            draw_invariants.ok ? 1 : 0,
            supports_direct_multi_sampler_inputs_ ? 1 : 0,
            direct_sampler_disable_reason_,
            draw_invariants.reason);

        for (const auto& line : debug_lines_) {
            if (line.layer != layer) {
                continue;
            }

            if (bgfx::getAvailTransientVertexBuffer(2, layout_) < 2) {
                continue;
            }
            bgfx::TransientVertexBuffer tvb{};
            bgfx::allocTransientVertexBuffer(&tvb, 2, layout_);

            PosNormalVertex vertices[2]{
                {line.start.x, line.start.y, line.start.z, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
                {line.end.x, line.end.y, line.end.z, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
            };
            std::memcpy(tvb.data, vertices, sizeof(vertices));
            bgfx::setVertexBuffer(0, &tvb, 0, 2);

            const float line_color[4] = {line.color.r, line.color.g, line.color.b, line.color.a};
            const float zeros[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            const float unlit[4] = {1.0f, 0.0f, 0.0f, 0.0f};
            bgfx::setUniform(u_color_, line_color);
            bgfx::setUniform(u_light_dir_, zeros);
            bgfx::setUniform(u_light_color_, zeros);
            bgfx::setUniform(u_ambient_color_, zeros);
            bgfx::setUniform(u_unlit_, unlit);
            if (bgfx::isValid(u_texture_mode_)) {
                const float texture_mode[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                bgfx::setUniform(u_texture_mode_, texture_mode);
            }
            if (bgfx::isValid(white_tex_)) {
                bgfx::setTexture(0, s_tex_, white_tex_);
                if (bgfx::isValid(s_normal_)) {
                    bgfx::setTexture(1, s_normal_, white_tex_);
                }
                if (bgfx::isValid(s_occlusion_)) {
                    bgfx::setTexture(2, s_occlusion_, white_tex_);
                }
            }

            float identity[16];
            bx::mtxIdentity(identity);
            bgfx::setTransform(identity);

            uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LESS |
                             BGFX_STATE_PT_LINES | BGFX_STATE_MSAA;
            if (line.color.a < 0.999f) {
                state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_INV_SRC_ALPHA);
            }
            bgfx::setState(state);
            bgfx::submit(0, program_);
        }

    }

    void setCamera(const renderer::CameraData& camera) override {
        camera_ = camera;
    }

    void setDirectionalLight(const renderer::DirectionalLightData& light) override {
        light_ = light;
    }

    void setEnvironmentLighting(const renderer::EnvironmentLightingData& environment) override {
        environment_ = environment;
    }

    bool isValid() const override {
        return initialized_;
    }

 private:
    BgfxCallback callback_{};
    bgfx::ProgramHandle program_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_color_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_light_dir_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_light_color_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_ambient_color_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_unlit_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle u_texture_mode_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_tex_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_normal_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle s_occlusion_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle white_tex_ = BGFX_INVALID_HANDLE;
    bgfx::VertexLayout layout_{};

    std::unordered_map<renderer::MeshId, Mesh> meshes_;
    std::unordered_map<renderer::MaterialId, Material> materials_;
    std::vector<renderer::DrawItem> draw_items_;
    std::vector<detail::ResolvedDebugLineSemantics> debug_lines_;

    renderer::CameraData camera_{};
    renderer::DirectionalLightData light_{};
    renderer::EnvironmentLightingData environment_{};
    int width_ = 1280;
    int height_ = 720;
    renderer::MeshId next_mesh_id_ = 1;
    renderer::MaterialId next_material_id_ = 1;
    BgfxDirectSamplerShaderAlignment direct_sampler_shader_alignment_{};
    detail::BgfxDirectSamplerContractReport direct_sampler_contract_report_{};
    std::string direct_sampler_disable_reason_ = "not_initialized";
    bool supports_direct_multi_sampler_inputs_ = false;
    bool initialized_ = false;
};

std::unique_ptr<Backend> CreateBgfxBackend(karma::platform::Window& window) {
    return std::make_unique<BgfxBackend>(window);
}

} // namespace karma::renderer_backend
