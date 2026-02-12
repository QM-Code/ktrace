#include "karma/app/cli_client_backend_options.hpp"

#include "karma/renderer/backend.hpp"

#include <optional>
#include <vector>

namespace karma::app {
namespace {

std::vector<std::string> RenderBackendChoices(bool include_auto) {
    std::vector<std::string> choices{};
    const auto compiled = renderer_backend::CompiledBackends();
    if (include_auto && compiled.size() > 1) {
        choices.emplace_back("auto");
    }
    for (const auto backend : compiled) {
        choices.emplace_back(renderer_backend::BackendKindName(backend));
    }
    return choices;
}

std::vector<std::string> UiBackendChoices() {
    std::vector<std::string> choices{};
#if defined(KARMA_HAS_IMGUI)
    choices.emplace_back("imgui");
#endif
#if defined(KARMA_HAS_RMLUI)
    choices.emplace_back("rmlui");
#endif
    return choices;
}

std::vector<std::string> PlatformBackendChoices() {
    std::vector<std::string> choices{};
#if defined(KARMA_WINDOW_BACKEND_SDL3)
    choices.emplace_back("sdl3");
#endif
#if defined(KARMA_WINDOW_BACKEND_SDL2)
    choices.emplace_back("sdl2");
#endif
#if defined(KARMA_WINDOW_BACKEND_GLFW)
    choices.emplace_back("glfw");
#endif
    return choices;
}

bool ShouldExposeRenderBackendCliOption() {
    return renderer_backend::CompiledBackends().size() > 1;
}

bool ShouldExposeUiBackendCliOption() {
    return UiBackendChoices().size() > 1;
}

bool ShouldExposePlatformBackendCliOption() {
    return PlatformBackendChoices().size() > 1;
}

} // namespace

CliConsumeResult ConsumeRenderBackendCliOption(const std::string& arg,
                                               int& index,
                                               int argc,
                                               char** argv,
                                               std::string& backend_out,
                                               bool& explicit_out) {
    CliConsumeResult out{};
    if (!ShouldExposeRenderBackendCliOption()) {
        return out;
    }

    std::optional<std::string> raw_value{};
    if (arg == "--backend-render") {
        std::string error{};
        raw_value = RequireValue(arg, index, argc, argv, &error);
        out.consumed = true;
        if (!raw_value) {
            out.error = error;
            return out;
        }
    } else if (StartsWith(arg, "--backend-render=")) {
        raw_value = ValueAfterEquals(arg, "--backend-render=");
        out.consumed = true;
    } else {
        return out;
    }

    const auto choices = RenderBackendChoices(true);
    const auto parsed = ParseChoiceLower(*raw_value, choices);
    if (!parsed) {
        out.error = "Invalid value '" + *raw_value + "' for --backend-render. Expected: "
            + JoinChoices(choices) + ".";
        return out;
    }
    backend_out = *parsed;
    explicit_out = true;
    return out;
}

CliConsumeResult ConsumeUiBackendCliOption(const std::string& arg,
                                           int& index,
                                           int argc,
                                           char** argv,
                                           std::string& backend_out,
                                           bool& explicit_out) {
    CliConsumeResult out{};
    if (!ShouldExposeUiBackendCliOption()) {
        return out;
    }

    std::optional<std::string> raw_value{};
    if (arg == "--backend-ui") {
        std::string error{};
        raw_value = RequireValue(arg, index, argc, argv, &error);
        out.consumed = true;
        if (!raw_value) {
            out.error = error;
            return out;
        }
    } else if (StartsWith(arg, "--backend-ui=")) {
        raw_value = ValueAfterEquals(arg, "--backend-ui=");
        out.consumed = true;
    } else {
        return out;
    }

    const auto choices = UiBackendChoices();
    const auto parsed = ParseChoiceLower(*raw_value, choices);
    if (!parsed) {
        out.error = "Invalid value '" + *raw_value + "' for --backend-ui. Expected: "
            + JoinChoices(choices) + ".";
        return out;
    }
    backend_out = *parsed;
    explicit_out = true;
    return out;
}

CliConsumeResult ConsumePlatformBackendCliOption(const std::string& arg,
                                                 int& index,
                                                 int argc,
                                                 char** argv,
                                                 std::string& backend_out,
                                                 bool& explicit_out) {
    CliConsumeResult out{};
    if (!ShouldExposePlatformBackendCliOption()) {
        return out;
    }

    std::optional<std::string> raw_value{};
    if (arg == "--backend-platform") {
        std::string error{};
        raw_value = RequireValue(arg, index, argc, argv, &error);
        out.consumed = true;
        if (!raw_value) {
            out.error = error;
            return out;
        }
    } else if (StartsWith(arg, "--backend-platform=")) {
        raw_value = ValueAfterEquals(arg, "--backend-platform=");
        out.consumed = true;
    } else {
        return out;
    }

    const auto choices = PlatformBackendChoices();
    const auto parsed = ParseChoiceLower(*raw_value, choices);
    if (!parsed) {
        out.error = "Invalid value '" + *raw_value + "' for --backend-platform. Expected: "
            + JoinChoices(choices) + ".";
        return out;
    }
    backend_out = *parsed;
    explicit_out = true;
    return out;
}

void AppendClientBackendCliHelp(std::ostream& out) {
    if (ShouldExposeRenderBackendCliOption()) {
        const auto choices = RenderBackendChoices(true);
        out << "      --backend-render <name>     Render backend override ("
            << JoinChoices(choices) << ")\n";
    }
    if (ShouldExposeUiBackendCliOption()) {
        const auto choices = UiBackendChoices();
        out << "      --backend-ui <name>         UI backend override ("
            << JoinChoices(choices) << ")\n";
    }
    if (ShouldExposePlatformBackendCliOption()) {
        const auto choices = PlatformBackendChoices();
        out << "      --backend-platform <name>   Platform backend override ("
            << JoinChoices(choices) << ")\n";
    }
}

} // namespace karma::app
