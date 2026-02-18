#include "ui/frontends/rmlui/translate.hpp"

#include <RmlUi/Core/Variant.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace ui::rmlui {
namespace {

std::string escapeRmlText(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '&':
                out.append("&amp;");
                break;
            case '<':
                out.append("&lt;");
                break;
            case '>':
                out.append("&gt;");
                break;
            case '\"':
                out.append("&quot;");
                break;
            case '\'':
                out.append("&apos;");
                break;
            default:
                out.push_back(ch);
                break;
        }
    }
    return out;
}

std::string trim(std::string_view text) {
    std::size_t start = 0;
    while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start]))) {
        ++start;
    }
    std::size_t end = text.size();
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(start, end - start));
}

std::vector<std::string> split(std::string_view text, char delim) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t pos = text.find(delim, start);
        if (pos == std::string_view::npos) {
            parts.emplace_back(text.substr(start));
            break;
        }
        parts.emplace_back(text.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

void applyElementTranslation(Rml::Element *element, const karma::common::i18n::I18n &i18n) {
    if (!element) {
        return;
    }
    if (auto *attr = element->GetAttribute("data-i18n")) {
        if (attr->GetType() == Rml::Variant::STRING) {
            const std::string key = trim(attr->Get<std::string>());
            if (!key.empty()) {
                const std::string &translated = i18n.get(key);
                element->SetInnerRML(escapeRmlText(translated));
            }
        }
    }

    if (auto *attr = element->GetAttribute("data-i18n-attr")) {
        if (attr->GetType() == Rml::Variant::STRING) {
            const std::string spec = attr->Get<std::string>();
            for (const auto &segment : split(spec, ';')) {
                const auto parts = split(segment, ':');
                if (parts.size() != 2) {
                    continue;
                }
                const std::string name = trim(parts[0]);
                const std::string key = trim(parts[1]);
                if (name.empty() || key.empty()) {
                    continue;
                }
                const std::string &translated = i18n.get(key);
                element->SetAttribute(name, translated);
            }
        }
    }
}

} // namespace

void ApplyTranslations(Rml::Element *root, const karma::common::i18n::I18n &i18n) {
    if (!root) {
        return;
    }
    std::vector<Rml::Element *> stack;
    stack.push_back(root);
    while (!stack.empty()) {
        Rml::Element *element = stack.back();
        stack.pop_back();
        applyElementTranslation(element, i18n);
        const int childCount = element->GetNumChildren();
        for (int i = 0; i < childCount; ++i) {
            if (auto *child = element->GetChild(i)) {
                stack.push_back(child);
            }
        }
    }
}

} // namespace ui::rmlui
