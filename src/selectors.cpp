#include "private.hpp"

#include <sstream>
#include <unordered_set>

namespace ktrace::detail {

bool ParseChannelPattern(const std::string_view expression,
                         Selector& selector,
                         std::string& error) {
    if (expression.empty()) {
        error = "missing channel expression";
        return false;
    }

    size_t start = 0;
    int depth = 0;
    while (start <= expression.size()) {
        if (depth >= 3) {
            error = "channel depth exceeds 3";
            return false;
        }
        const std::size_t dot = expression.find('.', start);
        const std::string_view token =
            (dot == std::string_view::npos)
                ? expression.substr(start)
                : expression.substr(start, dot - start);
        if (token.empty()) {
            error = "empty channel token";
            return false;
        }
        if (token != "*" && !IsIdentifierToken(token)) {
            error = "invalid channel token '" + std::string(token) + "'";
            return false;
        }
        selector.channelTokens[depth++] = std::string(token);
        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }

    selector.channelDepth = depth;
    selector.includeTopLevel =
        depth == 2 && selector.channelTokens[0] == "*" && selector.channelTokens[1] == "*";
    return true;
}

bool ParseSelectorToken(const std::string_view rawToken,
                        Selector& selector,
                        std::string& error) {
    if (rawToken.rfind(kLocalSelectorPrefix, 0) == 0) {
        selector.anyNamespace = true;
        const std::string_view localPattern = rawToken.substr(kLocalSelectorPrefix.size());
        if (!ParseChannelPattern(localPattern, selector, error)) {
            error = "local selector " + error;
            return false;
        }
        return true;
    }

    const std::size_t dot = rawToken.find('.');
    if (dot == std::string_view::npos) {
        error = "expected <namespace>.<channel>";
        return false;
    }

    const std::string_view ns = rawToken.substr(0, dot);
    const std::string_view channelPattern = rawToken.substr(dot + 1);
    if (ns.empty()) {
        error = "missing namespace";
        return false;
    }
    if (ns == "*") {
        selector.anyNamespace = true;
    } else if (IsIdentifierToken(ns)) {
        selector.anyNamespace = false;
        selector.traceNamespace = std::string(ns);
    } else {
        error = "invalid namespace '" + std::string(ns) + "'";
        return false;
    }

    if (!ParseChannelPattern(channelPattern, selector, error)) {
        return false;
    }
    return true;
}

std::vector<Selector> ParseAndValidateSelectors(const std::string& list,
                                                std::vector<std::string>& invalidTokens) {
    std::vector<Selector> selectors;
    std::unordered_set<std::string> invalidSeen;

    std::stringstream ss(list);
    std::string token;
    while (std::getline(ss, token, ',')) {
        const std::string name = TrimCopy(token);
        if (name.empty()) {
            if (invalidSeen.insert("<empty>").second) {
                invalidTokens.emplace_back("<empty>");
            }
            continue;
        }

        Selector selector{};
        std::string error;
        if (!ParseSelectorToken(name, selector, error)) {
            const std::string reason = error.empty() ? name : (name + " (" + error + ")");
            if (invalidSeen.insert(reason).second) {
                invalidTokens.push_back(reason);
            }
            continue;
        }
        selectors.push_back(std::move(selector));
    }

    return selectors;
}

bool SelectorMatches(const Selector& selector,
                     const std::string_view trace_namespace,
                     const std::string_view category) {
    if (!selector.anyNamespace && trace_namespace != selector.traceNamespace) {
        return false;
    }

    std::array<std::string_view, 3> channelParts{};
    const int channelDepth = SplitCategory(category, channelParts);
    if (channelDepth <= 0) {
        return false;
    }

    if (selector.channelDepth == 1) {
        return channelDepth == 1 && SegmentMatches(selector.channelTokens[0], channelParts[0]);
    }

    if (selector.channelDepth == 2) {
        if (channelDepth == 1 && selector.includeTopLevel) {
            return true;
        }
        if (channelDepth != 2) {
            return false;
        }
        return SegmentMatches(selector.channelTokens[0], channelParts[0]) &&
               SegmentMatches(selector.channelTokens[1], channelParts[1]);
    }

    if (selector.channelDepth == 3) {
        const bool wildcardAll = selector.channelTokens[0] == "*" &&
                                 selector.channelTokens[1] == "*" &&
                                 selector.channelTokens[2] == "*";
        if (wildcardAll) {
            return channelDepth >= 1 && channelDepth <= 3;
        }
        if (channelDepth != 3) {
            return false;
        }
        return SegmentMatches(selector.channelTokens[0], channelParts[0]) &&
               SegmentMatches(selector.channelTokens[1], channelParts[1]) &&
               SegmentMatches(selector.channelTokens[2], channelParts[2]);
    }

    return false;
}

std::string SelectorUsage() {
    std::ostringstream out;
    out << "Trace selector format:\n";
    out << "  --trace <namespace>.<channel>[.<sub>[.<sub>]]\n";
    out << "  --trace-local <channel>[.<sub>[.<sub>]]\n";
    out << "  --trace-channels            List known channels\n";
    out << "  --trace-channels <ns>       List known channels under a namespace\n";
    out << "Wildcard examples:\n";
    out << "  --trace '<namespace>.*'\n";
    out << "  --trace '<namespace>.*.*'\n";
    out << "  --trace '<namespace>.*.<channel>'\n";
    out << "  --trace '<namespace>.*.*.*'\n";
    out << "  --trace '*.*'\n";
    return out.str();
}

} // namespace ktrace::detail
