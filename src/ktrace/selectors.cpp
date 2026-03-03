#include "../ktrace.hpp"

#include <unordered_set>

namespace {

using ktrace::detail::isSelectorIdentifier;
using ktrace::detail::trimWhitespace;

bool splitByTopLevelCommas(std::string_view value,
                           std::vector<std::string>& parts,
                           std::string& error) {
    std::size_t start = 0;
    int brace_depth = 0;

    for (std::size_t i = 0; i < value.size(); ++i) {
        const char c = value[i];
        if (c == '{') {
            ++brace_depth;
            continue;
        }
        if (c == '}') {
            if (brace_depth == 0) {
                error = "unmatched '}'";
                return false;
            }
            --brace_depth;
            continue;
        }
        if (c == ',' && brace_depth == 0) {
            parts.push_back(trimWhitespace(std::string(value.substr(start, i - start))));
            start = i + 1;
        }
    }

    if (brace_depth != 0) {
        error = "unmatched '{'";
        return false;
    }

    parts.push_back(trimWhitespace(std::string(value.substr(start))));
    return true;
}

bool splitBraceAlternatives(std::string_view value,
                            std::vector<std::string>& alternatives,
                            std::string& error) {
    if (value.empty()) {
        error = "empty brace group";
        return false;
    }

    if (!splitByTopLevelCommas(value, alternatives, error)) {
        return false;
    }
    for (const std::string& alternative : alternatives) {
        if (alternative.empty()) {
            error = "empty brace alternative";
            return false;
        }
    }
    return true;
}

bool expandBraceExpression(const std::string& value,
                           std::vector<std::string>& expanded,
                           std::string& error) {
    const std::size_t open = value.find('{');
    if (open == std::string::npos) {
        expanded.push_back(value);
        return true;
    }

    int depth = 0;
    std::size_t close = std::string::npos;
    for (std::size_t i = open; i < value.size(); ++i) {
        if (value[i] == '{') {
            ++depth;
        } else if (value[i] == '}') {
            --depth;
            if (depth == 0) {
                close = i;
                break;
            }
        }
    }
    if (close == std::string::npos) {
        error = "unmatched '{'";
        return false;
    }

    const std::string prefix = value.substr(0, open);
    const std::string suffix = value.substr(close + 1);
    const std::string_view inside(value.data() + open + 1, close - open - 1);

    std::vector<std::string> alternatives;
    if (!splitBraceAlternatives(inside, alternatives, error)) {
        return false;
    }

    for (const std::string& alternative : alternatives) {
        if (!expandBraceExpression(prefix + alternative + suffix, expanded, error)) {
            return false;
        }
    }
    return true;
}

} // namespace

namespace ktrace::detail {

bool parseSelectorChannelPattern(const std::string_view expression,
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
        if (token != "*" && !isSelectorIdentifier(token)) {
            error = "invalid channel token '" + std::string(token) + "'";
            return false;
        }
        selector.channel_tokens[depth++] = std::string(token);
        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }

    selector.channel_depth = depth;
    selector.include_top_level =
        depth == 2 && selector.channel_tokens[0] == "*" && selector.channel_tokens[1] == "*";
    return true;
}

bool parseSelectorExpression(const std::string_view raw_token,
                             const std::string_view local_namespace,
                             Selector& selector,
                             std::string& error) {
    const std::size_t dot = raw_token.find('.');
    if (dot == std::string_view::npos) {
        error = "did you mean '*.*'?";
        return false;
    }

    const std::string_view ns = raw_token.substr(0, dot);
    const std::string_view channel_pattern = raw_token.substr(dot + 1);
    if (ns == "*") {
        selector.any_namespace = true;
    } else if (ns.empty()) {
        const std::string namespace_name = trimWhitespace(std::string(local_namespace));
        if (!isSelectorIdentifier(namespace_name)) {
            error = "missing namespace";
            return false;
        }
        selector.any_namespace = false;
        selector.trace_namespace = namespace_name;
    } else if (isSelectorIdentifier(ns)) {
        selector.any_namespace = false;
        selector.trace_namespace = std::string(ns);
    } else {
        error = "invalid namespace '" + std::string(ns) + "'";
        return false;
    }

    if (!parseSelectorChannelPattern(channel_pattern, selector, error)) {
        return false;
    }
    return true;
}

std::vector<Selector> parseSelectorList(const std::string& list,
                                        const std::string_view local_namespace,
                                        std::vector<std::string>& invalid_tokens) {
    ensureInternalTraceChannelsRegistered();
    std::vector<Selector> selectors;
    std::unordered_set<std::string> invalid_seen;

    std::vector<std::string> selector_tokens;
    std::string split_error;
    if (!splitByTopLevelCommas(list, selector_tokens, split_error)) {
        invalid_tokens.push_back(split_error);
        KTRACE("selector", "parsing selectors failed (enable selector.parse for details)");
        KTRACE("selector.parse", "failed to parse selector list '{}' ({})", list, split_error);
        return selectors;
    }

    for (const std::string& token : selector_tokens) {
        const std::string name = trimWhitespace(token);
        if (name.empty()) {
            if (invalid_seen.insert("<empty>").second) {
                invalid_tokens.emplace_back("<empty>");
            }
            continue;
        }

        std::vector<std::string> expanded_tokens;
        std::string expand_error;
        if (!expandBraceExpression(name, expanded_tokens, expand_error)) {
            const std::string reason = name + " (" + expand_error + ")";
            if (invalid_seen.insert(reason).second) {
                invalid_tokens.push_back(reason);
            }
            continue;
        }

        for (const std::string& expanded_token : expanded_tokens) {
            Selector selector{};
            std::string parse_error;
            if (!parseSelectorExpression(expanded_token, local_namespace, selector, parse_error)) {
                const std::string reason = parse_error.empty()
                                               ? expanded_token
                                               : (expanded_token + " (" + parse_error + ")");
                if (invalid_seen.insert(reason).second) {
                    invalid_tokens.push_back(reason);
                }
                continue;
            }
            selectors.push_back(std::move(selector));
        }
    }

    KTRACE("selector",
           "parsed selectors (enable selector.parse for details): {} selector(s), {} invalid token(s)",
           selectors.size(),
           invalid_tokens.size());
    KTRACE("selector.parse",
           "parsed selector list '{}' -> {} selector(s), {} invalid token(s)",
           list,
           selectors.size(),
           invalid_tokens.size());
    return selectors;
}

bool matchesSelector(const Selector& selector,
                     const std::string_view trace_namespace,
                     const std::string_view category) {
    if (!selector.any_namespace && trace_namespace != selector.trace_namespace) {
        return false;
    }

    std::array<std::string_view, 3> channel_parts{};
    const int channel_depth = splitChannelPath(category, channel_parts);
    if (channel_depth <= 0) {
        return false;
    }

    if (selector.channel_depth == 1) {
        return channel_depth == 1 && matchesSelectorSegment(selector.channel_tokens[0], channel_parts[0]);
    }

    if (selector.channel_depth == 2) {
        if (channel_depth == 1 && selector.include_top_level) {
            return true;
        }
        if (channel_depth != 2) {
            return false;
        }
        return matchesSelectorSegment(selector.channel_tokens[0], channel_parts[0]) &&
               matchesSelectorSegment(selector.channel_tokens[1], channel_parts[1]);
    }

    if (selector.channel_depth == 3) {
        const bool wildcard_all = selector.channel_tokens[0] == "*" &&
                                 selector.channel_tokens[1] == "*" &&
                                 selector.channel_tokens[2] == "*";
        if (wildcard_all) {
            return channel_depth >= 1 && channel_depth <= 3;
        }
        if (channel_depth != 3) {
            return false;
        }
        return matchesSelectorSegment(selector.channel_tokens[0], channel_parts[0]) &&
               matchesSelectorSegment(selector.channel_tokens[1], channel_parts[1]) &&
               matchesSelectorSegment(selector.channel_tokens[2], channel_parts[2]);
    }

    return false;
}

} // namespace ktrace::detail
