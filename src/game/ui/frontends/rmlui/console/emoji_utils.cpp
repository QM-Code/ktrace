#include "ui/frontends/rmlui/console/emoji_utils.hpp"

#include <filesystem>
#include <sstream>
#include <vector>

#include "karma/common/data/path_resolver.hpp"

namespace ui {
namespace {
std::string escapeRmlText(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '&': out.append("&amp;"); break;
            case '<': out.append("&lt;"); break;
            case '>': out.append("&gt;"); break;
            case '"': out.append("&quot;"); break;
            case '\'': out.append("&#39;"); break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

bool decodeNextUtf8(std::string_view utf8, std::size_t &offset, uint32_t &codepoint) {
    if (offset >= utf8.size()) {
        return false;
    }
    unsigned char c = static_cast<unsigned char>(utf8[offset]);
    if ((c & 0x80) == 0) {
        codepoint = c;
        offset += 1;
        return true;
    }
    int length = 0;
    if ((c & 0xE0) == 0xC0) {
        length = 2;
        codepoint = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        length = 3;
        codepoint = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        length = 4;
        codepoint = c & 0x07;
    } else {
        offset += 1;
        return false;
    }
    if (offset + length > utf8.size()) {
        offset += 1;
        return false;
    }
    for (int i = 1; i < length; ++i) {
        unsigned char cc = static_cast<unsigned char>(utf8[offset + i]);
        if ((cc & 0xC0) != 0x80) {
            offset += 1;
            return false;
        }
        codepoint = (codepoint << 6) | (cc & 0x3F);
    }
    offset += static_cast<std::size_t>(length);
    return true;
}

bool isEmojiCandidate(uint32_t cp) {
    if ((cp >= 0x1F000 && cp <= 0x1FAFF) ||
        (cp >= 0x2600 && cp <= 0x26FF) ||
        (cp >= 0x2700 && cp <= 0x27BF) ||
        (cp >= 0x2300 && cp <= 0x23FF) ||
        (cp >= 0x2B00 && cp <= 0x2BFF)) {
        return true;
    }
    switch (cp) {
        case 0x00A9:
        case 0x00AE:
        case 0x203C:
        case 0x2049:
        case 0x2122:
        case 0x2139:
        case 0x3030:
        case 0x200D:
        case 0xFE0E:
        case 0xFE0F:
            return true;
        default:
            return false;
    }
}

bool isRegionalIndicator(uint32_t cp) {
    return cp >= 0x1F1E6 && cp <= 0x1F1FF;
}

bool isEmojiBase(uint32_t cp) {
    return isEmojiCandidate(cp) && cp != 0x200D && cp != 0xFE0E && cp != 0xFE0F;
}

std::string codepointToHex(uint32_t cp) {
    std::ostringstream out;
    out << std::hex << std::nouppercase << cp;
    return out.str();
}

std::string buildTwemojiFilename(const std::vector<uint32_t> &sequence) {
    std::ostringstream out;
    for (std::size_t i = 0; i < sequence.size(); ++i) {
        if (i != 0) {
            out << '-';
        }
        out << codepointToHex(sequence[i]);
    }
    return out.str();
}
} // namespace

std::string renderTextWithTwemoji(std::string_view text) {
    std::string out;
    std::size_t offset = 0;
    while (offset < text.size()) {
        const std::size_t start = offset;
        uint32_t cp = 0;
        if (!decodeNextUtf8(text, offset, cp)) {
            out.append(escapeRmlText(text.substr(start, 1)));
            continue;
        }

        if (!isEmojiCandidate(cp) && !isRegionalIndicator(cp)) {
            out.append(escapeRmlText(text.substr(start, offset - start)));
            continue;
        }

        std::vector<uint32_t> sequence;
        sequence.push_back(cp);
        bool hasEmoji = isEmojiBase(cp) || isRegionalIndicator(cp);
        std::size_t seqOffset = offset;

        if (isRegionalIndicator(cp)) {
            uint32_t nextCp = 0;
            std::size_t lookahead = seqOffset;
            if (decodeNextUtf8(text, lookahead, nextCp) && isRegionalIndicator(nextCp)) {
                sequence.push_back(nextCp);
                seqOffset = lookahead;
                hasEmoji = true;
            }
        } else {
            bool expectEmojiAfterJoiner = false;
            while (seqOffset < text.size()) {
                uint32_t nextCp = 0;
                std::size_t lookahead = seqOffset;
                if (!decodeNextUtf8(text, lookahead, nextCp)) {
                    break;
                }
                if (nextCp == 0xFE0E || nextCp == 0xFE0F) {
                    if (nextCp != 0xFE0E) {
                        sequence.push_back(nextCp);
                    }
                    seqOffset = lookahead;
                    continue;
                }
                if (nextCp == 0x200D) {
                    sequence.push_back(nextCp);
                    expectEmojiAfterJoiner = true;
                    seqOffset = lookahead;
                    continue;
                }
                if (expectEmojiAfterJoiner && (isEmojiCandidate(nextCp) || isRegionalIndicator(nextCp))) {
                    sequence.push_back(nextCp);
                    if (isEmojiBase(nextCp) || isRegionalIndicator(nextCp)) {
                        hasEmoji = true;
                    }
                    expectEmojiAfterJoiner = false;
                    seqOffset = lookahead;
                    continue;
                }
                break;
            }
        }

        if (!hasEmoji) {
            out.append(escapeRmlText(text.substr(start, offset - start)));
            continue;
        }

        const std::string fileName = buildTwemojiFilename(sequence);
        const std::filesystem::path imagePath = karma::common::data::Resolve("client/ui/emoji/twemoji/" + fileName + ".png");
        if (std::filesystem::exists(imagePath)) {
            const std::string srcPath = "emoji/twemoji/" + fileName + ".png";
            out.append("<img src=\"");
            out.append(srcPath);
            out.append("\" class=\"emoji\" />");
        } else {
            out.append(escapeRmlText(text.substr(start, seqOffset - start)));
        }

        offset = seqOffset;
    }
    return out;
}

} // namespace ui
