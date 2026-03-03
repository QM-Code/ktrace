#include "../ktrace.hpp"

#include <array>
#include <cstdio>
#include <string_view>

namespace {

constexpr std::array<std::string_view, 256> kColorNames = {
    "Black",
    "Red",
    "Green",
    "Yellow",
    "Blue",
    "Magenta",
    "Cyan",
    "White",
    "BrightBlack",
    "BrightRed",
    "BrightGreen",
    "BrightYellow",
    "BrightBlue",
    "BrightMagenta",
    "BrightCyan",
    "BrightWhite",
    "Grey0",
    "NavyBlue",
    "DarkBlue",
    "Blue3",
    "Blue3B",
    "Blue1",
    "DarkGreen",
    "DeepSkyBlue4",
    "DeepSkyBlue4B",
    "DeepSkyBlue4C",
    "DodgerBlue3",
    "DodgerBlue2",
    "Green4",
    "SpringGreen4",
    "Turquoise4",
    "DeepSkyBlue3",
    "DeepSkyBlue3B",
    "DodgerBlue1",
    "Green3",
    "SpringGreen3",
    "DarkCyan",
    "LightSeaGreen",
    "DeepSkyBlue2",
    "DeepSkyBlue1",
    "Green3B",
    "SpringGreen3B",
    "SpringGreen2",
    "Cyan3",
    "DarkTurquoise",
    "Turquoise2",
    "Green1",
    "SpringGreen2B",
    "SpringGreen1",
    "MediumSpringGreen",
    "Cyan2",
    "Cyan1",
    "DarkRed",
    "DeepPink4",
    "Purple4",
    "Purple4B",
    "Purple3",
    "BlueViolet",
    "Orange4",
    "Grey37",
    "MediumPurple4",
    "SlateBlue3",
    "SlateBlue3B",
    "RoyalBlue1",
    "Chartreuse4",
    "DarkSeaGreen4",
    "PaleTurquoise4",
    "SteelBlue",
    "SteelBlue3",
    "CornflowerBlue",
    "Chartreuse3",
    "DarkSeaGreen4B",
    "CadetBlue",
    "CadetBlueB",
    "SkyBlue3",
    "SteelBlue1",
    "Chartreuse3B",
    "PaleGreen3",
    "SeaGreen3",
    "Aquamarine3",
    "MediumTurquoise",
    "SteelBlue1B",
    "Chartreuse2",
    "SeaGreen2",
    "SeaGreen1",
    "SeaGreen1B",
    "Aquamarine1",
    "DarkSlateGray2",
    "DarkRedB",
    "DeepPink4B",
    "DarkMagenta",
    "DarkMagentaB",
    "DarkViolet",
    "Purple",
    "Orange4B",
    "LightPink4",
    "Plum4",
    "MediumPurple3",
    "MediumPurple3B",
    "SlateBlue1",
    "Yellow4",
    "Wheat4",
    "Grey53",
    "LightSlateGrey",
    "MediumPurple",
    "LightSlateBlue",
    "Yellow4B",
    "DarkOliveGreen3",
    "DarkSeaGreen",
    "LightSkyBlue3",
    "LightSkyBlue3B",
    "SkyBlue2",
    "Chartreuse2B",
    "DarkOliveGreen3B",
    "PaleGreen3B",
    "DarkSeaGreen3",
    "DarkSlateGray3",
    "SkyBlue1",
    "Chartreuse1",
    "LightGreen",
    "LightGreenB",
    "PaleGreen1",
    "Aquamarine1B",
    "DarkSlateGray1",
    "Red3",
    "DeepPink4C",
    "MediumVioletRed",
    "Magenta3",
    "DarkVioletB",
    "PurpleB",
    "DarkOrange3",
    "IndianRed",
    "HotPink3",
    "MediumOrchid3",
    "MediumOrchid",
    "MediumPurple2",
    "DarkGoldenrod",
    "LightSalmon3",
    "RosyBrown",
    "Grey63",
    "MediumPurple2B",
    "MediumPurple1",
    "Gold3",
    "DarkKhaki",
    "NavajoWhite3",
    "Grey69",
    "LightSteelBlue3",
    "LightSteelBlue",
    "Yellow3",
    "DarkOliveGreen3C",
    "DarkSeaGreen3B",
    "DarkSeaGreen2",
    "LightCyan3",
    "LightSkyBlue1",
    "GreenYellow",
    "DarkOliveGreen2",
    "PaleGreen1B",
    "DarkSeaGreen2B",
    "DarkSeaGreen1",
    "PaleTurquoise1",
    "Red3B",
    "DeepPink3",
    "DeepPink3B",
    "Magenta3B",
    "Magenta3C",
    "Magenta2",
    "DarkOrange3B",
    "IndianRedB",
    "HotPink3B",
    "HotPink2",
    "Orchid",
    "MediumOrchid1",
    "Orange3",
    "LightSalmon3B",
    "LightPink3",
    "Pink3",
    "Plum3",
    "Violet",
    "Gold3B",
    "LightGoldenrod3",
    "Tan",
    "MistyRose3",
    "Thistle3",
    "Plum2",
    "Yellow3B",
    "Khaki3",
    "LightGoldenrod2",
    "LightYellow3",
    "Grey84",
    "LightSteelBlue1",
    "Yellow2",
    "DarkOliveGreen1",
    "DarkOliveGreen1B",
    "DarkSeaGreen1B",
    "Honeydew2",
    "LightCyan1",
    "Red1",
    "DeepPink2",
    "DeepPink1",
    "DeepPink1B",
    "Magenta2B",
    "Magenta1",
    "OrangeRed1",
    "IndianRed1",
    "IndianRed1B",
    "HotPink",
    "HotPinkB",
    "MediumOrchid1B",
    "DarkOrange",
    "Salmon1",
    "LightCoral",
    "PaleVioletRed1",
    "Orchid2",
    "Orchid1",
    "Orange1",
    "SandyBrown",
    "LightSalmon1",
    "LightPink1",
    "Pink1",
    "Plum1",
    "Gold1",
    "LightGoldenrod2B",
    "LightGoldenrod2C",
    "NavajoWhite1",
    "MistyRose1",
    "Thistle1",
    "Yellow1",
    "LightGoldenrod1",
    "Khaki1",
    "Wheat1",
    "Cornsilk1",
    "Grey100",
    "Grey3",
    "Grey7",
    "Grey11",
    "Grey15",
    "Grey19",
    "Grey23",
    "Grey27",
    "Grey30",
    "Grey35",
    "Grey39",
    "Grey42",
    "Grey46",
    "Grey50",
    "Grey54",
    "Grey58",
    "Grey62",
    "Grey66",
    "Grey70",
    "Grey74",
    "Grey78",
    "Grey82",
    "Grey85",
    "Grey89",
    "Grey93",
};

} // namespace

namespace ktrace::detail {

const std::array<std::string_view, 256>& colorNames() {
    return kColorNames;
}

std::optional<ColorId> resolveChannelColor(std::string_view trace_namespace,
                                           std::string_view category) {
    auto& state = getTraceState();
    std::lock_guard<std::mutex> lock(state.registry_mutex);
    const auto ns_it = state.channel_colors_by_namespace.find(std::string(trace_namespace));
    if (ns_it == state.channel_colors_by_namespace.end()) {
        return std::nullopt;
    }

    std::string key(category);
    while (!key.empty()) {
        const auto it = ns_it->second.find(key);
        if (it != ns_it->second.end()) {
            return it->second;
        }
        const std::size_t dot = key.rfind('.');
        if (dot == std::string::npos) {
            break;
        }
        key.resize(dot);
    }
    return std::nullopt;
}

const char* ansiColorCode(ColorId color) {
    if (color == kDefaultColor) {
        return "";
    }
    if (color <= 7u) {
        static thread_local char sgr[16];
        std::snprintf(sgr, sizeof(sgr), "\x1b[%um", 30u + static_cast<unsigned>(color));
        return sgr;
    }
    if (color <= 15u) {
        static thread_local char sgr[16];
        std::snprintf(sgr, sizeof(sgr), "\x1b[%um", 90u + (static_cast<unsigned>(color) - 8u));
        return sgr;
    }
    if (color <= 255u) {
        static thread_local char sgr[32];
        std::snprintf(sgr, sizeof(sgr), "\x1b[38;5;%um", static_cast<unsigned>(color));
        return sgr;
    }
    return "";
}

} // namespace ktrace::detail
