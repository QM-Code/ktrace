#include "karma/cli/client/backend_options.hpp"

#include "karma/renderer/backend.hpp"
#include "karma/ui/backend.hpp"
#include "karma/window/backend.hpp"

#include <optional>
#include <vector>

namespace karma::cli::client {
namespace {

std::vector<std::string> RenderBackendChoices(bool include_auto) {
    std::vector<std::string> choices{};
    const auto compiled = renderer::backend::CompiledBackends();
    if (include_auto && compiled.size() > 1) {
        choices.emplace_back("auto");
    }
    for (const auto backend : compiled) {
        choices.emplace_back(renderer::backend::BackendKindName(backend));
    }
    return choices;
}

std::vector<std::string> UiBackendChoices() {
    std::vector<std::string> choices{};
    const auto compiled = ui::backend::CompiledBackends();
    for (const auto backend : compiled) {
        if (backend == ui::backend::BackendKind::ImGui || backend == ui::backend::BackendKind::RmlUi) {
            choices.emplace_back(ui::backend::BackendKindName(backend));
        }
    }
    return choices;
}

std::vector<std::string> WindowBackendChoices() {
    std::vector<std::string> choices{};
    const auto compiled = window::backend::CompiledBackends();
    for (const auto backend : compiled) {
        choices.emplace_back(window::backend::BackendKindName(backend));
    }
    return choices;
}

bool ShouldExposeRenderBackendOption() {
    return renderer::backend::CompiledBackends().size() > 1;
}

bool ShouldExposeUiBackendOption() {
    return UiBackendChoices().size() > 1;
}

bool ShouldExposeWindowBackendOption() {
    return WindowBackendChoices().size() > 1;
}

} // namespace

shared::ConsumeResult ConsumeRenderBackendOption(const std::string& arg,
                                                 int& index,
                                                 int argc,
                                                 char** argv,
                                                 std::string& backend_out,
                                                 bool& explicit_out) {
    shared::ConsumeResult out{};
    if (!ShouldExposeRenderBackendOption()) {
        return out;
    }

    std::optional<std::string> raw_value{};
    if (arg == "--backend-render") {
        std::string error{};
        raw_value = shared::RequireValue(arg, index, argc, argv, &error);
        out.consumed = true;
        if (!raw_value) {
            out.error = error;
            return out;
        }
    } else if (shared::StartsWith(arg, "--backend-render=")) {
        raw_value = shared::ValueAfterEquals(arg, "--backend-render=");
        out.consumed = true;
    } else {
        return out;
    }

    const auto choices = RenderBackendChoices(true);
    const auto parsed = shared::ParseChoiceLower(*raw_value, choices);
    if (!parsed) {
        out.error = "Invalid value '" + *raw_value + "' for --backend-render. Expected: "
            + shared::JoinChoices(choices) + ".";
        return out;
    }
    backend_out = *parsed;
    explicit_out = true;
    return out;
}

shared::ConsumeResult ConsumeUiBackendOption(const std::string& arg,
                                             int& index,
                                             int argc,
                                             char** argv,
                                             std::string& backend_out,
                                             bool& explicit_out) {
    shared::ConsumeResult out{};
    if (!ShouldExposeUiBackendOption()) {
        return out;
    }

    std::optional<std::string> raw_value{};
    if (arg == "--backend-ui") {
        std::string error{};
        raw_value = shared::RequireValue(arg, index, argc, argv, &error);
        out.consumed = true;
        if (!raw_value) {
            out.error = error;
            return out;
        }
    } else if (shared::StartsWith(arg, "--backend-ui=")) {
        raw_value = shared::ValueAfterEquals(arg, "--backend-ui=");
        out.consumed = true;
    } else {
        return out;
    }

    const auto choices = UiBackendChoices();
    const auto parsed = shared::ParseChoiceLower(*raw_value, choices);
    if (!parsed) {
        out.error = "Invalid value '" + *raw_value + "' for --backend-ui. Expected: "
            + shared::JoinChoices(choices) + ".";
        return out;
    }
    backend_out = *parsed;
    explicit_out = true;
    return out;
}

shared::ConsumeResult ConsumeWindowBackendOption(const std::string& arg,
                                                 int& index,
                                                 int argc,
                                                 char** argv,
                                                 std::string& backend_out,
                                                 bool& explicit_out) {
    shared::ConsumeResult out{};
    if (!ShouldExposeWindowBackendOption()) {
        return out;
    }

    std::optional<std::string> raw_value{};
    if (arg == "--backend-window") {
        std::string error{};
        raw_value = shared::RequireValue(arg, index, argc, argv, &error);
        out.consumed = true;
        if (!raw_value) {
            out.error = error;
            return out;
        }
    } else if (shared::StartsWith(arg, "--backend-window=")) {
        raw_value = shared::ValueAfterEquals(arg, "--backend-window=");
        out.consumed = true;
    } else {
        return out;
    }

    const auto choices = WindowBackendChoices();
    const auto parsed = shared::ParseChoiceLower(*raw_value, choices);
    if (!parsed) {
        out.error = "Invalid value '" + *raw_value + "' for --backend-window. Expected: "
            + shared::JoinChoices(choices) + ".";
        return out;
    }
    backend_out = *parsed;
    explicit_out = true;
    return out;
}

void AppendBackendHelp(std::ostream& out) {
    if (ShouldExposeRenderBackendOption()) {
        const auto choices = RenderBackendChoices(true);
        out << "      --backend-render <name>     Render backend override ("
            << shared::JoinChoices(choices) << ")\n";
    }
    if (ShouldExposeUiBackendOption()) {
        const auto choices = UiBackendChoices();
        out << "      --backend-ui <name>         UI backend override ("
            << shared::JoinChoices(choices) << ")\n";
    }
    if (ShouldExposeWindowBackendOption()) {
        const auto choices = WindowBackendChoices();
        out << "      --backend-window <name>     Window backend override ("
            << shared::JoinChoices(choices) << ")\n";
    }
}

} // namespace karma::cli::client
