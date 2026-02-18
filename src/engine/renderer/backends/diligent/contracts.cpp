#include "internal.hpp"

#include "karma/common/logging/logging.hpp"

#include <spdlog/spdlog.h>

namespace karma::renderer_backend {

DiligentDirectSamplerContractState EvaluateDiligentDirectSamplerContractState(
    const std::array<detail::SamplerVariableAvailability, detail::kDiligentMaterialVariantCount>&
        material_sampler_availability,
    const detail::SamplerVariableAvailability& line_sampler_availability) {
    DiligentDirectSamplerContractState state{};
    state.contract_report = detail::EvaluateDiligentDirectSamplerContract(
        material_sampler_availability,
        line_sampler_availability);
    state.supports_direct_multi_sampler_inputs = state.contract_report.ready_for_direct_path;
    state.disable_reason = state.supports_direct_multi_sampler_inputs
        ? std::string("ok")
        : state.contract_report.reason;
    KARMA_TRACE("render.diligent",
                "direct sampler readiness enabled={} materialContract={} lineContract={} reason={}",
                state.supports_direct_multi_sampler_inputs ? 1 : 0,
                state.contract_report.material_pipeline_contract_ready ? 1 : 0,
                state.contract_report.line_pipeline_contract_ready ? 1 : 0,
                state.disable_reason);
    if (!state.supports_direct_multi_sampler_inputs) {
        spdlog::warn("Diligent: direct sampler path disabled (reason={})",
                     state.disable_reason);
    } else if (!state.contract_report.line_pipeline_contract_ready) {
        KARMA_TRACE("render.diligent",
                    "line sampler contract is partial; direct material path remains enabled");
    }
    return state;
}

} // namespace karma::renderer_backend
