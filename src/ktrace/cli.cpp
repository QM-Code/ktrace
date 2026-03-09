#include <ktrace.hpp>

#include "../ktrace.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

namespace {

void _examples(const kcli::HandlerContext& context) {
    const std::string root = "--" + std::string(context.root);
    std::cout
        << "\nGeneral trace selector pattern:\n"
        << "  " << root << " <namespace>.<channel>[.<subchannel>[.<subchannel>]]\n\n"
        << "Trace selector examples:\n"
        << "  " << root << " '.abc'           Select local 'abc' in current namespace\n"
        << "  " << root << " '.abc.xyz'       Select local nested channel in current namespace\n"
        << "  " << root << " 'otherapp.channel' Select explicit namespace channel\n"
        << "  " << root << " '*.*'            Select all <namespace>.<channel> channels\n"
        << "  " << root << " '*.*.*'          Select all channels up to 2 levels\n"
        << "  " << root << " '*.*.*.*'        Select all channels up to 3 levels\n"
        << "  " << root << " 'alpha.*'        Select all top-level channels in alpha\n"
        << "  " << root << " 'alpha.*.*'      Select all channels in alpha (up to 2 levels)\n"
        << "  " << root << " 'alpha.*.*.*'    Select all channels in alpha (up to 3 levels)\n"
        << "  " << root << " '*.net'          Select 'net' across all namespaces\n"
        << "  " << root << " '*.scheduler.tick' Select 'scheduler.tick' across all namespaces\n"
        << "  " << root << " '*.net.*'        Select subchannels under 'net' across namespaces\n"
        << "  " << root << " '*.{net,io}'     Select 'net' and 'io' across all namespaces\n"
        << "  " << root << " '{alpha,beta}.*' Select all top-level channels in alpha and beta\n"
        << "  " << root << " alpha.net\n"
        << "  " << root << " beta.scheduler.tick\n"
        << "  " << root << " alpha.net,beta.io\n"
        << "  " << root << " gamma.physics.*\n"
        << "  " << root << " gamma.physics.*.*\n"
        << "  " << root << " alpha.{net,cache}\n"
        << "  " << root << " beta.{io,scheduler}.packet\n"
        << "  " << root << " '{alpha,beta}.net'\n\n";
}

void _namespaces(const kcli::HandlerContext&) {
    std::vector<std::string> namespaces = ktrace::GetNamespaces();
    if (namespaces.empty()) {
        std::cout << "No trace namespaces defined.\n\n";
        return;
    }

    std::sort(namespaces.begin(), namespaces.end());
    std::cout << "\nAvailable trace namespaces:\n";
    for (const std::string& trace_namespace : namespaces) {
        if (trace_namespace.empty()) {
            continue;
        }
        std::cout << "  " << trace_namespace << "\n";
    }
    std::cout << "\n";
}

void _channels(const kcli::HandlerContext&) {
    std::vector<std::string> namespaces = ktrace::GetNamespaces();
    std::sort(namespaces.begin(), namespaces.end());

    bool printed_any = false;
    for (const std::string& trace_namespace : namespaces) {
        if (trace_namespace.empty()) {
            continue;
        }
        std::vector<std::string> channels = ktrace::GetChannels(trace_namespace);
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
}

void _colors(const kcli::HandlerContext&) {
    const auto& names = ktrace::detail::colorNames();
    std::cout << "\nAvailable trace colors:\n";
    for (const std::string_view color_name : names) {
        if (color_name.empty()) {
            continue;
        }
        std::cout << "  " << color_name << "\n";
    }
    std::cout << "\n";
}

void _files(const kcli::HandlerContext&) {
    ktrace::OutputOptions options = ktrace::detail::getRequestedOutputOptions();
    options.filenames = true;
    options.line_numbers = true;
    ktrace::SetOutputOptions(options);
}

void _functions(const kcli::HandlerContext&) {
    ktrace::OutputOptions options = ktrace::detail::getRequestedOutputOptions();
    options.filenames = true;
    options.line_numbers = true;
    options.function_names = true;
    ktrace::SetOutputOptions(options);
}

void _timestamps(const kcli::HandlerContext&) {
    ktrace::OutputOptions options = ktrace::detail::getRequestedOutputOptions();
    options.timestamps = true;
    ktrace::SetOutputOptions(options);
}

} // namespace

namespace ktrace {

kcli::InlineParser GetInlineParser(std::string_view trace_root, std::string_view local_namespace) {
    ktrace::detail::ensureInternalTraceChannelsRegistered();
    const std::string trace_namespace = ktrace::detail::trimWhitespace(std::string(local_namespace));
    const auto _ROOT = [trace_namespace](const kcli::HandlerContext&,std::string_view value) {
        ktrace::EnableChannels(value, trace_namespace);
    };

    kcli::InlineParser parser("trace");
    if (!trace_root.empty()) { parser.setRoot(trace_root); }
    parser.setRootValueHandler(_ROOT,"<channels>","Trace selected channels.");
    parser.setHandler("-examples", _examples, "Show selector examples.");
    parser.setHandler("-namespaces", _namespaces, "Show initialized trace namespaces.");
    parser.setHandler("-channels", _channels, "Show initialized trace channels.");
    parser.setHandler("-colors", _colors, "Show available trace colors.");
    parser.setHandler("-files", _files, "Include source file and line in trace output.");
    parser.setHandler("-functions", _functions, "Include function names in trace output.");
    parser.setHandler("-timestamps", _timestamps, "Include timestamps in trace output.");

    return parser;
}

} // namespace ktrace
