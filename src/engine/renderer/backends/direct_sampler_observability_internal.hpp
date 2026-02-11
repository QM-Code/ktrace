#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace karma::renderer_backend::detail {

struct SamplerVariableAvailability {
    bool srb_ready = false;
    bool has_s_tex = false;
    bool has_s_normal = false;
    bool has_s_occlusion = false;
};

constexpr std::size_t kDiligentMaterialVariantCount = 4u;

struct DiligentDirectSamplerContractReport {
    bool material_pipeline_contract_ready = false;
    bool line_pipeline_contract_ready = false;
    bool ready_for_direct_path = false;
    std::string reason = "uninitialized";
};

struct BgfxDirectSamplerContractReport {
    bool uniform_contract_ready = false;
    bool shader_alignment_ready = false;
    bool ready_for_direct_path = false;
    std::string reason = "uninitialized";
};

struct BgfxDirectSamplerAlignmentInput {
    bool source_exists = false;
    bool source_declares_direct_contract = false;
    bool binary_exists = false;
    bool binary_non_empty = false;
    bool binary_up_to_date = false;
    bool source_absent_integrity_ready = false;
    std::string source_absent_integrity_reason = "source_missing_and_integrity_contract_unavailable";
};

struct BgfxDirectSamplerAlignmentReport {
    bool source_present_mode = false;
    bool source_absent_compat_mode = false;
    bool ready = false;
    std::string reason = "uninitialized";
};

struct BgfxSourceAbsentIntegrityInput {
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
};

struct BgfxSourceAbsentIntegrityReport {
    bool ready = false;
    std::string reason = "uninitialized";
};

inline void AppendDirectSamplerReason(std::string& reason, const std::string& token) {
    if (token.empty()) {
        return;
    }
    if (!reason.empty()) {
        reason += ",";
    }
    reason += token;
}

inline DiligentDirectSamplerContractReport EvaluateDiligentDirectSamplerContract(
    const std::array<SamplerVariableAvailability, kDiligentMaterialVariantCount>& material_variants,
    const SamplerVariableAvailability& line_pipeline) {
    DiligentDirectSamplerContractReport report{};
    std::string reasons{};
    bool material_ready = true;

    for (std::size_t i = 0; i < material_variants.size(); ++i) {
        const SamplerVariableAvailability& variant = material_variants[i];
        const std::string prefix = "variant" + std::to_string(i);
        if (!variant.srb_ready) {
            material_ready = false;
            AppendDirectSamplerReason(reasons, prefix + "_srb_missing");
            continue;
        }
        if (!variant.has_s_tex) {
            material_ready = false;
            AppendDirectSamplerReason(reasons, prefix + "_missing_s_tex");
        }
        if (!variant.has_s_normal) {
            material_ready = false;
            AppendDirectSamplerReason(reasons, prefix + "_missing_s_normal");
        }
        if (!variant.has_s_occlusion) {
            material_ready = false;
            AppendDirectSamplerReason(reasons, prefix + "_missing_s_occlusion");
        }
    }
    report.material_pipeline_contract_ready = material_ready;

    bool line_ready = true;
    if (!line_pipeline.srb_ready) {
        line_ready = false;
        AppendDirectSamplerReason(reasons, "line_srb_missing");
    } else {
        if (!line_pipeline.has_s_tex) {
            line_ready = false;
            AppendDirectSamplerReason(reasons, "line_missing_s_tex");
        }
        if (!line_pipeline.has_s_normal) {
            line_ready = false;
            AppendDirectSamplerReason(reasons, "line_missing_s_normal");
        }
        if (!line_pipeline.has_s_occlusion) {
            line_ready = false;
            AppendDirectSamplerReason(reasons, "line_missing_s_occlusion");
        }
    }
    report.line_pipeline_contract_ready = line_ready;
    report.ready_for_direct_path = material_ready;

    if (report.ready_for_direct_path) {
        report.reason = line_ready ? "ok" : "ok_line_sampler_contract_unavailable";
    } else if (reasons.empty()) {
        report.reason = "material_sampler_contract_unavailable";
    } else {
        report.reason = reasons;
    }
    return report;
}

inline BgfxSourceAbsentIntegrityReport EvaluateBgfxSourceAbsentIntegrityPolicy(
    const BgfxSourceAbsentIntegrityInput& input) {
    BgfxSourceAbsentIntegrityReport report{};
    if (!input.manifest_exists) {
        report.reason = "source_missing_and_integrity_manifest_missing";
        return report;
    }
    if (!input.manifest_parse_ready) {
        report.reason = input.manifest_parse_reason.empty()
                            ? "source_missing_and_integrity_manifest_parse_failed"
                            : input.manifest_parse_reason;
        return report;
    }
    if (!input.manifest_version_supported) {
        report.reason = "source_missing_and_integrity_manifest_version_unsupported";
        return report;
    }
    if (!input.manifest_algorithm_supported) {
        report.reason = "source_missing_and_integrity_manifest_algorithm_unsupported";
        return report;
    }
    if (!input.manifest_hash_present) {
        report.reason = "source_missing_and_integrity_manifest_hash_missing";
        return report;
    }
    if (!input.binary_hash_available) {
        report.reason = "source_missing_and_integrity_binary_hash_unavailable";
        return report;
    }
    if (!input.hash_matches_manifest) {
        report.reason = "source_missing_and_integrity_hash_mismatch";
        return report;
    }
    if (input.signed_envelope_declared && !input.signed_envelope_verification_available) {
        report.reason = "source_missing_and_integrity_signed_envelope_verification_deferred";
        return report;
    }
    report.ready = true;
    report.reason = "ok_source_absent_binary_integrity";
    return report;
}

inline BgfxDirectSamplerAlignmentReport EvaluateBgfxDirectSamplerAlignmentPolicy(
    const BgfxDirectSamplerAlignmentInput& input) {
    BgfxDirectSamplerAlignmentReport report{};
    report.source_present_mode = input.source_exists;
    report.source_absent_compat_mode = !input.source_exists;

    if (input.source_exists) {
        if (!input.source_declares_direct_contract) {
            report.reason = "source_missing_direct_sampler_contract";
            return report;
        }
        if (!input.binary_exists) {
            report.reason = "binary_missing";
            return report;
        }
        if (!input.binary_non_empty) {
            report.reason = "binary_empty";
            return report;
        }
        if (!input.binary_up_to_date) {
            report.reason = "binary_stale_vs_source";
            return report;
        }
        report.ready = true;
        report.reason = "ok";
        return report;
    }

    if (!input.binary_exists) {
        report.reason = "source_missing_and_binary_missing";
        return report;
    }
    if (!input.binary_non_empty) {
        report.reason = "source_missing_and_binary_empty";
        return report;
    }
    if (!input.source_absent_integrity_ready) {
        report.reason = input.source_absent_integrity_reason.empty()
                            ? "source_missing_and_integrity_contract_unavailable"
                            : input.source_absent_integrity_reason;
        return report;
    }
    report.ready = true;
    report.reason = input.source_absent_integrity_reason.empty()
                        ? "ok_source_absent_binary_integrity"
                        : input.source_absent_integrity_reason;
    return report;
}

inline BgfxDirectSamplerContractReport EvaluateBgfxDirectSamplerContract(
    bool uniform_contract_ready,
    const BgfxDirectSamplerAlignmentReport& shader_alignment) {
    BgfxDirectSamplerContractReport report{};
    report.uniform_contract_ready = uniform_contract_ready;
    report.shader_alignment_ready = shader_alignment.ready;
    report.ready_for_direct_path = uniform_contract_ready && shader_alignment.ready;
    if (report.ready_for_direct_path) {
        report.reason = shader_alignment.reason.empty() ? "ok" : shader_alignment.reason;
    } else if (!uniform_contract_ready) {
        report.reason = "uniform_contract_unavailable";
    } else if (!shader_alignment.reason.empty()) {
        report.reason = shader_alignment.reason;
    } else {
        report.reason = "shader_alignment_unavailable";
    }
    return report;
}

struct DirectSamplerDrawInvariantInput {
    bool direct_path_enabled = false;
    std::size_t total_draws = 0u;
    std::size_t direct_contract_draws = 0u;
    std::size_t direct_draws = 0u;
    std::size_t fallback_draws = 0u;
    std::size_t forced_fallback_draws = 0u;
    std::size_t unexpected_direct_draws = 0u;
};

struct DirectSamplerDrawInvariantReport {
    bool ok = false;
    std::string reason = "uninitialized";
};

inline DirectSamplerDrawInvariantReport EvaluateDirectSamplerDrawInvariants(
    const DirectSamplerDrawInvariantInput& input) {
    DirectSamplerDrawInvariantReport report{};
    std::string reasons{};

    if ((input.direct_draws + input.fallback_draws) != input.total_draws) {
        AppendDirectSamplerReason(reasons, "draw_partition_mismatch");
    }
    if (input.direct_contract_draws > input.total_draws) {
        AppendDirectSamplerReason(reasons, "direct_contract_exceeds_total");
    }
    if (input.unexpected_direct_draws != 0u) {
        AppendDirectSamplerReason(reasons, "unexpected_direct_draws_nonzero");
    }

    if (input.direct_path_enabled) {
        if (input.forced_fallback_draws != 0u) {
            AppendDirectSamplerReason(reasons, "forced_fallback_while_enabled");
        }
        if (input.direct_draws != input.direct_contract_draws) {
            AppendDirectSamplerReason(reasons, "direct_contract_draw_mismatch_enabled");
        }
    } else {
        if (input.direct_draws != 0u) {
            AppendDirectSamplerReason(reasons, "direct_draws_while_disabled");
        }
        if (input.forced_fallback_draws != input.direct_contract_draws) {
            AppendDirectSamplerReason(reasons, "forced_fallback_draw_mismatch_disabled");
        }
    }

    report.ok = reasons.empty();
    report.reason = report.ok ? "ok" : reasons;
    return report;
}

} // namespace karma::renderer_backend::detail
