
#include <filesystem>
#include <algorithm>
#include <thread>

#include "imgui_common_tools.h"
#include "ImGuiBaseTypes.h"

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
        FT_Library ftLibrary = nullptr;
        FT_Error   err       = FT_Init_FreeType(&ftLibrary);
        if (err)
        {
            printf("init freetype library fail: %s\n", FT_Error_String(err));
            return;
        }

        ImGuiResourceGuard guard([&]() { FT_Done_FreeType(ftLibrary); });

        for (auto &family : fontFamilies)
        {
            for (auto &font : family.fonts)
            {
                FT_Face face;
                err = FT_New_Face(ftLibrary, font.path.c_str(), font.index, &face);
                if (err)
                {
                    printf("load font %s fail: %s\n", font.path.c_str(), FT_Error_String(err));
                    continue;
                }
                auto os2    = (TT_OS2 *)FT_Get_Sfnt_Table(face, ft_sfnt_os2);
                int  weight = 400;
                int  width  = 5;
                int  italic = 0;
                if (os2)
                {
                    weight = (int)os2->usWeightClass;
                    width  = (int)os2->usWidthClass;
                    if (os2->fsSelection & (0x1 << 9))
                        italic = 1; // oblique
                    else if (os2->fsSelection & 0x1)
                        italic = 2; // italic
                    else
                        italic = 0;
                }
                font.weight = weight;
                font.width  = width;
                font.italic = italic;
                FT_Done_Face(face);
            }
        }

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
                          bool res = false;
                          if (a.width != b.width)
                              res = a.width < b.width;
                          else if (a.weight != b.weight)
                              res = a.weight < b.weight;
                          else if (a.italic != b.italic)
                              res = a.italic < b.italic;
                          else
                              res = a.style < b.style;
                          return res;
                      });
        }
    }

#endif

} // namespace ImGui
