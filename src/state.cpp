#include "private.hpp"

#include <algorithm>
#include <cctype>

namespace ktrace::detail {

State& GetState() {
    static State state;
    return state;
}

std::string TrimCopy(const std::string& value) {
    size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }
    if (start == value.size()) {
        return {};
    }
    size_t end = value.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(value[end])) != 0) {
        --end;
    }
    return value.substr(start, end - start + 1);
}

bool IsIdentifierChar(const char c) {
    const auto uc = static_cast<unsigned char>(c);
    return std::isalnum(uc) != 0 || c == '_' || c == '-';
}

bool IsIdentifierToken(const std::string_view token) {
    if (token.empty()) {
        return false;
    }
    for (const char c : token) {
        if (!IsIdentifierChar(c)) {
            return false;
        }
    }
    return true;
}

bool IsValidRegisteredChannel(const std::string_view channel) {
    if (channel.empty()) {
        return false;
    }
    size_t start = 0;
    int depth = 0;
    while (start <= channel.size()) {
        if (depth >= 3) {
            return false;
        }
        const std::size_t dot = channel.find('.', start);
        const std::string_view token =
            (dot == std::string_view::npos)
                ? channel.substr(start)
                : channel.substr(start, dot - start);
        if (token.empty() || !IsIdentifierToken(token)) {
            return false;
        }
        ++depth;
        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }
    return true;
}

int SplitCategory(std::string_view category, std::array<std::string_view, 3>& out) {
    if (category.empty()) {
        return 0;
    }
    size_t start = 0;
    int depth = 0;
    while (start <= category.size()) {
        if (depth >= 3) {
            return -1;
        }
        const std::size_t dot = category.find('.', start);
        const std::string_view token =
            (dot == std::string_view::npos)
                ? category.substr(start)
                : category.substr(start, dot - start);
        if (token.empty()) {
            return -1;
        }
        out[depth++] = token;
        if (dot == std::string_view::npos) {
            break;
        }
        start = dot + 1;
    }
    return depth;
}

bool SegmentMatches(const std::string& pattern, const std::string_view value) {
    return pattern == "*" || pattern == value;
}

} // namespace ktrace::detail
