
#include <filesystem>
#include <algorithm>
#include <thread>

#include "imgui_common_tools.h"

using std::string;
using std::stringstream;
using std::thread;
using std::vector;
using std::wstring;
namespace fs = std::filesystem;

namespace ImGui
{

    string gLastError;

    string getLastError()
    {
        string res = gLastError;
        gLastError.clear();
        return res;
    }

    const std::vector<FilterSpec> &getImageFilter()
    {
        static vector<FilterSpec> gImageFilterSpecs = {
            {"*.jpg;*.jpeg", "JPEG Images"},
            {"*.png",        "PNG Images" },
            {"*.bmp",        "BMP Images" },
        };
        return gImageFilterSpecs;
    }
    const std::vector<FilterSpec> &getVideoFilter()
    {
        static vector<FilterSpec> gVideoFilterSpecs = {
            {"*.mp4;*.mkv;*.avi", "Video Files"     },
            {"*.mov",             "QuickTime Movies"},
            {"*.flv",             "Flash Video"     },
            {"*.webm",            "WebM Video"      },
        };
        return gVideoFilterSpecs;
    }
    const std::vector<FilterSpec> &getAudioFilter()
    {
        static vector<FilterSpec> gAudioFilterSpecs = {
            {"*.mp3",  "MP3 Audio" },
            {"*.wav",  "WAV Audio" },
            {"*.flac", "FLAC Audio"},
            {"*.aac",  "AAC Audio" },
        };
        return gAudioFilterSpecs;
    }
    const std::vector<FilterSpec> &getTextFilter()
    {
        static vector<FilterSpec> gTextFilterSpecs = {
            {"*.txt", "Text Files"},
            {"*.csv", "CSV Files" },
            {"*.log", "Log Files" },
        };
        return gTextFilterSpecs;
    }

    std::string getResourcesDir()
    {
        string          exePath      = getApplicationPath();
        auto            exeDir       = fs::u8path(exePath).parent_path();
        auto            resourcesDir = exeDir / "resources";
        std::error_code ec;
        if (!fs::exists(resourcesDir, ec))
            fs::create_directory(resourcesDir, ec);
        return resourcesDir.u8string();
    }

    static thread *gThreadFontPixPreload = nullptr;

    void startFontPixPreload()
    {
        if (gThreadFontPixPreload)
            return;
        gThreadFontPixPreload = new thread(
            []
            {
                auto          &io = ImGui::GetIO();
                unsigned char *pixels;
                int            width, height;
                io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
            });
    }

    void waitFontPixPreload()
    {
        if (!gThreadFontPixPreload)
            return;
        gThreadFontPixPreload->join();
    }

#ifdef IMGUI_ENABLE_FREETYPE

    void sortFonts(std::vector<FreetypeFontFamilyInfo> &fontFamilies)
    {
        if (fontFamilies.empty())
            return;
        std::sort(fontFamilies.begin(), fontFamilies.end(),
                  [](const FreetypeFontFamilyInfo &a, const FreetypeFontFamilyInfo &b)
                  {
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
        for (auto &family : fontFamilies)
        {
            std::sort(family.fonts.begin(), family.fonts.end(),
                      [](const FreetypeFontInfo &a, const FreetypeFontInfo &b)
                      {
                          static vector<string> commonStyles = {"Regular", "UltraLight", "Thin",      "ExtraLight",
                                                                "Light",   "Medium",     "SemiLight", "SemiBold",
                                                                "Bold",    "ExtraBold",  "UltraBold"};

                          auto aIter = std::find_if(std::begin(commonStyles), std::end(commonStyles),
                                                    [&a](const string &s) { return a.style.find(s) != string::npos; });
                          auto bIter = std::find_if(std::begin(commonStyles), std::end(commonStyles),
                                                    [&b](const string &s) { return b.style.find(s) != string::npos; });

                          if (aIter == commonStyles.end() && a.style == "Italic")
                              aIter = commonStyles.begin();
                          if (bIter == commonStyles.end() && b.style == "Italic")
                              bIter = commonStyles.begin();

                          if (aIter != commonStyles.end() && bIter != commonStyles.end())
                          {
                              if (aIter == bIter)
                              {
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

} // namespace ImGui
