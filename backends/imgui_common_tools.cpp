
#include <algorithm>

#include "imgui_common_tools.h"

using std::string;
using std::stringstream;
using std::vector;
using std::wstring;

string gLastError;

string getLastError() {
    string res = gLastError;
    gLastError.clear();
    return res;
}

const std::vector<FilterSpec> &getImageFilter() {
    static vector<FilterSpec> gImageFilterSpecs = {
        {"*.jpg;*.jpeg", "JPEG Images"},
        {"*.png",       "PNG Images" },
        {"*.bmp",       "BMP Images" },
    };
    return gImageFilterSpecs;
}
const std::vector<FilterSpec> &getVideoFilter() {
    static vector<FilterSpec> gVideoFilterSpecs = {
        {"*.mp4;*.mkv;*.avi", "Video Files"     },
        {"*.mov",           "QuickTime Movies"},
        {"*.flv",           "Flash Video"     },
        {"*.webm",          "WebM Video"      },
    };
    return gVideoFilterSpecs;
}
const std::vector<FilterSpec> &getAudioFilter() {
    static vector<FilterSpec> gAudioFilterSpecs = {
        {"*.mp3",  "MP3 Audio" },
        {"*.wav",  "WAV Audio" },
        {"*.flac", "FLAC Audio"},
        {"*.aac",  "AAC Audio" },
    };
    return gAudioFilterSpecs;
}
const std::vector<FilterSpec> &getTextFilter() {
    static vector<FilterSpec> gTextFilterSpecs = {
        {"*.txt", "Text Files"},
        {"*.csv", "CSV Files" },
        {"*.log", "Log Files" },
    };
    return gTextFilterSpecs;
}

#ifdef IMGUI_ENABLE_FREETYPE

void sortFonts(std::vector<FreetypeFontFamilyInfo> &fontFamilies) {
    if (fontFamilies.empty())
        return;
    std::sort(fontFamilies.begin(), fontFamilies.end(), [](const FreetypeFontFamilyInfo &a, const FreetypeFontFamilyInfo &b) {
        bool aIsAlpha = a.displayName[0] > 0 && isalnum(a.displayName[0]);
        bool bIsAlpha = b.displayName[0] > 0 && isalnum(b.displayName[0]);
        if (aIsAlpha && bIsAlpha)
            return a.name < b.name;
        else if (aIsAlpha && !bIsAlpha)
            return true;
        else if (!aIsAlpha && bIsAlpha)
            return false;
        else
            return a.displayName < b.displayName;
    });
    for (auto &family : fontFamilies) {
        std::sort(family.fonts.begin(), family.fonts.end(), [](const FreetypeFontInfo &a, const FreetypeFontInfo &b) {
            static vector<string> commonStyles = {"Regular",   "UltraLight", "ExtraLight", "Thin",      "Light",
                                                  "SemiLight", "Bold",       "SemiBold",   "ExtraBold", "UltraBold"};

            auto aIter = std::find_if(std::begin(commonStyles), std::end(commonStyles),
                                      [&a](const string &s) { return a.style.find(s) != string::npos; });
            auto bIter = std::find_if(std::begin(commonStyles), std::end(commonStyles),
                                      [&b](const string &s) { return b.style.find(s) != string::npos; });

            if (aIter != commonStyles.end() && bIter != commonStyles.end()) {
                if (aIter == bIter) {
                    if (a.style.find("Italic") != string::npos)
                        return false;
                    else if (b.style.find("Italic") != string::npos)
                        return true;
                }

                return aIter < bIter;
            }
            return a.style < b.style;
        });
    }
}

#endif