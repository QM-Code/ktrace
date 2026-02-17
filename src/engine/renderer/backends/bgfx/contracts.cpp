#include "internal.hpp"

#include "../direct_sampler_observability_internal.hpp"

#include "karma/common/data_path_resolver.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace karma::renderer_backend {
namespace {

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
constexpr std::size_t kBgfxTrustChainIdentityMaxLength = 64u;
constexpr std::size_t kBgfxSignedEnvelopeSignatureMaxHexLength = 512u;

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
    if (candidate.size() < 16u ||
        candidate.size() > kBgfxSignedEnvelopeSignatureMaxHexLength ||
        (candidate.size() % 2u) != 0u) {
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

bool IsCanonicalLowerHexToken(const std::string& token, std::size_t expected_size) {
    if (token.size() != expected_size) {
        return false;
    }
    for (char ch : token) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (std::isxdigit(uch) == 0 || std::isupper(uch) != 0) {
            return false;
        }
    }
    return true;
}

bool ValidateBgfxTrustIdentityToken(const std::string& token) {
    if (token.empty() || token.size() > kBgfxTrustChainIdentityMaxLength) {
        return false;
    }
    for (char ch : token) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        const bool allowed = (std::isalnum(uch) != 0) || ch == '.' || ch == '_' || ch == '-';
        if (!allowed) {
            return false;
        }
    }
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
    int version = 0;
    std::string algorithm{};
    bool version_supported = false;
    bool algorithm_supported = false;
    bool hash_present = false;
    std::string hash{};
    bool signed_envelope_declared = false;
    bool signed_envelope_mode_supported = false;
    std::string signed_envelope_mode{"none"};
    std::string signed_envelope_signature{};
    std::string signed_envelope_trust_chain{};
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
        manifest.version = parsed_version;
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
        manifest.algorithm = algorithm;
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
        manifest.signed_envelope_mode = signed_envelope_mode;
        if (signed_envelope_mode == "none") {
            if (has_signature || has_trust_chain) {
                manifest.parse_reason = "source_missing_and_integrity_manifest_signed_envelope_metadata_unexpected";
                return manifest;
            }
        } else {
            if (!has_signature || !has_trust_chain) {
                manifest.parse_reason = "source_missing_and_integrity_manifest_signed_envelope_metadata_missing";
                return manifest;
            }
            std::string normalized_signature{};
            if (!ParseBgfxIntegrityManifestEnvelopeSignature(signature_it->second, normalized_signature)) {
                manifest.parse_reason =
                    "source_missing_and_integrity_manifest_signed_envelope_signature_invalid_value_form";
                return manifest;
            }
            if (!ValidateBgfxIntegrityManifestTrustChainToken(trust_chain_it->second)) {
                manifest.parse_reason =
                    "source_missing_and_integrity_manifest_signed_envelope_trust_chain_invalid_value_form";
                return manifest;
            }
            manifest.signed_envelope_declared = true;
            manifest.signed_envelope_mode_supported = (signed_envelope_mode == kBgfxIntegritySignedEnvelopeMode);
            manifest.signed_envelope_signature = normalized_signature;
            manifest.signed_envelope_trust_chain = TrimAscii(trust_chain_it->second);
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

std::string ComputeFnv1a64TextHash(const std::string& text) {
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (char ch : text) {
        hash ^= static_cast<unsigned char>(ch);
        hash *= UINT64_C(1099511628211);
    }
    std::ostringstream encoded;
    encoded << std::hex << std::nouppercase << std::setfill('0') << std::setw(16) << hash;
    return encoded.str();
}

std::optional<std::string> ParseBgfxTrustRootIdFromChain(const std::string& trust_chain) {
    constexpr const char* kRootPrefix = "root:";
    const std::string chain = TrimAscii(trust_chain);
    if (chain.rfind(kRootPrefix, 0u) != 0u) {
        return std::nullopt;
    }
    const std::size_t root_begin = std::strlen(kRootPrefix);
    const std::size_t slash_pos = chain.find('/', root_begin);
    const std::string root_id =
        (slash_pos == std::string::npos) ? chain.substr(root_begin) : chain.substr(root_begin, slash_pos - root_begin);
    if (!ValidateBgfxTrustIdentityToken(root_id)) {
        return std::nullopt;
    }
    return root_id;
}

std::optional<std::string> ParseBgfxSigningKeyIdFromChain(const std::string& trust_chain) {
    constexpr const char* kRootPrefix = "root:";
    constexpr const char* kSigningKeyPrefix = "/signing-key:";
    const std::string chain = TrimAscii(trust_chain);
    if (chain.rfind(kRootPrefix, 0u) != 0u) {
        return std::nullopt;
    }

    const std::size_t root_begin = std::strlen(kRootPrefix);
    const std::size_t key_pos = chain.find(kSigningKeyPrefix, root_begin);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const std::string root_id = chain.substr(root_begin, key_pos - root_begin);
    if (!ValidateBgfxTrustIdentityToken(root_id)) {
        return std::nullopt;
    }

    const std::size_t key_begin = key_pos + std::strlen(kSigningKeyPrefix);
    const std::string key_id = chain.substr(key_begin);
    if (key_id.find('/') != std::string::npos) {
        return std::nullopt;
    }
    if (!ValidateBgfxTrustIdentityToken(key_id)) {
        return std::nullopt;
    }
    return key_id;
}

struct BgfxTrustRootPolicyResult {
    bool verification_available = false;
    bool trust_root_available = false;
    std::string trust_root_secret{};
};

BgfxTrustRootPolicyResult ResolveBgfxSignedEnvelopeTrustRootPolicy(
    const std::optional<std::string>& trust_root_id) {
    static const std::array<std::pair<const char*, const char*>, 2u> kTrustedRoots{{
        {"karma-dev", "bz3_bgfx_trust_root_karma_dev"},
        {"karma-release", "bz3_bgfx_trust_root_karma_release"},
    }};

    BgfxTrustRootPolicyResult policy{};
    policy.verification_available = !kTrustedRoots.empty();
    if (!policy.verification_available || !trust_root_id.has_value()) {
        return policy;
    }

    for (const auto& [id, secret] : kTrustedRoots) {
        if (trust_root_id.value() == id) {
            policy.trust_root_available = true;
            policy.trust_root_secret = secret;
            break;
        }
    }
    return policy;
}

std::string ComposeBgfxSignedEnvelopeVerificationPayload(
    const std::string& binary_hash,
    const std::string& trust_chain,
    const std::string& trust_root_id,
    const std::string& signing_key_id,
    const std::string& signed_envelope_mode,
    int manifest_version,
    const std::string& manifest_algorithm,
    const std::string& trust_root_secret) {
    return std::string("bgfx_signed_envelope_v1")
        + "|mode=" + signed_envelope_mode
        + "|manifest.version=" + std::to_string(manifest_version)
        + "|manifest.algorithm=" + manifest_algorithm
        + "|binary.hash=" + binary_hash
        + "|trust.root=" + trust_root_id
        + "|trust.key=" + signing_key_id
        + "|trust.chain=" + trust_chain
        + "|trust.root_secret=" + trust_root_secret;
}

} // namespace

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
        bool signed_envelope_verification_available = false;
        bool signed_envelope_mode_supported = false;
        bool signed_envelope_trust_root_available = false;
        bool signed_envelope_trust_chain_valid = false;
        bool signed_envelope_signature_material_valid = false;
        bool signed_envelope_signature_verified = false;
        bool signed_envelope_verification_inputs_checked = false;
        bool signed_envelope_verification_inputs_valid = true;
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
            signed_envelope_mode_supported = parsed_manifest.signed_envelope_mode_supported;
        }

        std::optional<std::string> actual_hash{};
        if (report.binary_exists && report.binary_non_empty) {
            actual_hash = ComputeFnv1a64FileHash(report.binary_path);
        }
        binary_hash_available = actual_hash.has_value();
        hash_matches_manifest =
            manifest_hash_present && binary_hash_available && (*actual_hash == parsed_manifest.hash);

        if (signed_envelope_declared) {
            const std::optional<std::string> trust_root_id =
                ParseBgfxTrustRootIdFromChain(parsed_manifest.signed_envelope_trust_chain);
            const std::optional<std::string> signing_key_id =
                ParseBgfxSigningKeyIdFromChain(parsed_manifest.signed_envelope_trust_chain);
            signed_envelope_trust_chain_valid = trust_root_id.has_value();
            signed_envelope_verification_inputs_checked = true;

            const BgfxTrustRootPolicyResult trust_root_policy =
                ResolveBgfxSignedEnvelopeTrustRootPolicy(trust_root_id);
            signed_envelope_verification_available = trust_root_policy.verification_available;
            signed_envelope_trust_root_available = trust_root_policy.trust_root_available;

            signed_envelope_signature_material_valid =
                (parsed_manifest.signed_envelope_signature.size() == 16u);

            const bool binary_hash_canonical =
                actual_hash.has_value() &&
                IsCanonicalLowerHexToken(actual_hash.value(), 16u);
            const bool manifest_hash_canonical =
                parsed_manifest.hash_present &&
                IsCanonicalLowerHexToken(parsed_manifest.hash, 16u);
            const bool signed_envelope_mode_canonical =
                (parsed_manifest.signed_envelope_mode == kBgfxIntegritySignedEnvelopeMode);
            const bool manifest_version_canonical =
                (parsed_manifest.version == kBgfxIntegrityManifestVersion);
            const bool manifest_algorithm_canonical =
                (parsed_manifest.algorithm == kBgfxIntegrityManifestAlgorithm);
            signed_envelope_verification_inputs_valid =
                signing_key_id.has_value() &&
                binary_hash_canonical &&
                manifest_hash_canonical &&
                signed_envelope_mode_canonical &&
                manifest_version_canonical &&
                manifest_algorithm_canonical;

            if (signed_envelope_verification_available &&
                signed_envelope_mode_supported &&
                signed_envelope_trust_root_available &&
                signed_envelope_trust_chain_valid &&
                signed_envelope_verification_inputs_valid &&
                signed_envelope_signature_material_valid &&
                hash_matches_manifest &&
                actual_hash.has_value()) {
                const std::string expected_signature = ComputeFnv1a64TextHash(
                    ComposeBgfxSignedEnvelopeVerificationPayload(
                        actual_hash.value(),
                        parsed_manifest.signed_envelope_trust_chain,
                        trust_root_id.value(),
                        signing_key_id.value(),
                        parsed_manifest.signed_envelope_mode,
                        parsed_manifest.version,
                        parsed_manifest.algorithm,
                        trust_root_policy.trust_root_secret));
                signed_envelope_signature_verified =
                    (expected_signature == parsed_manifest.signed_envelope_signature);
            }
        }

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
                signed_envelope_verification_available,
                signed_envelope_mode_supported,
                signed_envelope_trust_root_available,
                signed_envelope_trust_chain_valid,
                signed_envelope_signature_material_valid,
                signed_envelope_signature_verified,
                signed_envelope_verification_inputs_checked,
                signed_envelope_verification_inputs_valid,
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

} // namespace karma::renderer_backend
