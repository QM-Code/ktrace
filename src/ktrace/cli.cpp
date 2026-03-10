#include <ktrace.hpp>

#include "../ktrace.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

void printExamples(std::string_view root) {
    const std::string option_root = "--" + std::string(root);
    std::cout
        << "\nGeneral trace selector pattern:\n"
        << "  " << option_root << " <namespace>.<channel>[.<subchannel>[.<subchannel>]]\n\n"
        << "Trace selector examples:\n"
        << "  " << option_root << " '.abc'           Select local 'abc' in current namespace\n"
        << "  " << option_root << " '.abc.xyz'       Select local nested channel in current namespace\n"
        << "  " << option_root << " 'otherapp.channel' Select explicit namespace channel\n"
        << "  " << option_root << " '*.*'            Select all <namespace>.<channel> channels\n"
        << "  " << option_root << " '*.*.*'          Select all channels up to 2 levels\n"
        << "  " << option_root << " '*.*.*.*'        Select all channels up to 3 levels\n"
        << "  " << option_root << " 'alpha.*'        Select all top-level channels in alpha\n"
        << "  " << option_root << " 'alpha.*.*'      Select all channels in alpha (up to 2 levels)\n"
        << "  " << option_root << " 'alpha.*.*.*'    Select all channels in alpha (up to 3 levels)\n"
        << "  " << option_root << " '*.net'          Select 'net' across all namespaces\n"
        << "  " << option_root << " '*.scheduler.tick' Select 'scheduler.tick' across namespaces\n"
        << "  " << option_root << " '*.net.*'        Select subchannels under 'net' across namespaces\n"
        << "  " << option_root << " '*.{net,io}'     Select 'net' and 'io' across all namespaces\n"
        << "  " << option_root << " '{alpha,beta}.*' Select all top-level channels in alpha and beta\n"
        << "  " << option_root << " alpha.net\n"
        << "  " << option_root << " beta.scheduler.tick\n"
        << "  " << option_root << " alpha.net,beta.io\n"
        << "  " << option_root << " gamma.physics.*\n"
        << "  " << option_root << " gamma.physics.*.*\n"
        << "  " << option_root << " alpha.{net,cache}\n"
        << "  " << option_root << " beta.{io,scheduler}.packet\n"
        << "  " << option_root << " '{alpha,beta}.net'\n\n";
}

} // namespace

namespace ktrace {

kcli::InlineParser Logger::makeInlineParser(const TraceLogger& local_trace_logger,
                                            std::string_view trace_root) {
    const std::string local_namespace = local_trace_logger.getNamespace();

    const auto root_handler = [this, local_namespace](const kcli::HandlerContext&,
                                                      std::string_view value) {
        enableChannels(value, local_namespace);
    };

    const auto examples_handler = [](const kcli::HandlerContext& context) {
        printExamples(context.root);
    };

    const auto namespaces_handler = [this](const kcli::HandlerContext&) {
        std::vector<std::string> namespaces = getNamespaces();
        if (namespaces.empty()) {
            std::cout << "No trace namespaces defined.\n\n";
            return;
        }

        std::sort(namespaces.begin(), namespaces.end());
        std::cout << "\nAvailable trace namespaces:\n";
        for (const std::string& trace_namespace : namespaces) {
            if (!trace_namespace.empty()) {
                std::cout << "  " << trace_namespace << "\n";
            }
        }
        std::cout << "\n";
    };

    const auto channels_handler = [this](const kcli::HandlerContext&) {
        std::vector<std::string> namespaces = getNamespaces();
        std::sort(namespaces.begin(), namespaces.end());

        bool printed_any = false;
        for (const std::string& trace_namespace : namespaces) {
            if (trace_namespace.empty()) {
                continue;
            }
            std::vector<std::string> channels = getChannels(trace_namespace);
            std::sort(channels.begin(), channels.end());
            for (const std::string& channel : channels) {
                if (channel.empty()) {
                    continue;
                }
                if (!printed_any) {
                    std::cout << "\nAvailable trace channels:\n";
                    printed_any = true;
                }
                std::cout << "  " << trace_namespace << "." << channel << "\n";
            }
        }

        if (!printed_any) {
            std::cout << "No trace channels defined.\n\n";
            return;
        }
        std::cout << "\n";
    };

    const auto colors_handler = [](const kcli::HandlerContext&) {
        const auto& names = ktrace::detail::colorNames();
        std::cout << "\nAvailable trace colors:\n";
        for (const std::string_view color_name : names) {
            if (!color_name.empty()) {
                std::cout << "  " << color_name << "\n";
            }
        }
        std::cout << "\n";
    };

    const auto files_handler = [this](const kcli::HandlerContext&) {
        OutputOptions options = getOutputOptions();
        options.filenames = true;
        options.line_numbers = true;
        setOutputOptions(options);
    };

    const auto functions_handler = [this](const kcli::HandlerContext&) {
        OutputOptions options = getOutputOptions();
        options.filenames = true;
        options.line_numbers = true;
        options.function_names = true;
        setOutputOptions(options);
    };

    const auto timestamps_handler = [this](const kcli::HandlerContext&) {
        OutputOptions options = getOutputOptions();
        options.timestamps = true;
        setOutputOptions(options);
    };

    kcli::InlineParser parser("trace");
    if (!trace_root.empty()) {
        parser.setRoot(trace_root);
    }
    parser.setRootValueHandler(root_handler, "<channels>", "Trace selected channels.");
    parser.setHandler("-examples", examples_handler, "Show selector examples.");
    parser.setHandler("-namespaces", namespaces_handler, "Show initialized trace namespaces.");
    parser.setHandler("-channels", channels_handler, "Show initialized trace channels.");
    parser.setHandler("-colors", colors_handler, "Show available trace colors.");
    parser.setHandler("-files", files_handler, "Include source file and line in trace output.");
    parser.setHandler("-functions", functions_handler, "Include function names in trace output.");
    parser.setHandler("-timestamps", timestamps_handler, "Include timestamps in trace output.");

    return parser;
}

} // namespace ktrace
