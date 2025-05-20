
#include "imgui_common_tools.h"

using std::string;
using std::stringstream;
using std::wstring;

string gLastError;

string getLastError()
{
    string res = gLastError;
    gLastError.clear();
    return res;
}

#ifdef IMGUI_ENABLE_FREETYPE

void sortFonts(std::vector<FreetypeFontFamilyInfo> &fontFamilies)
{
    if (fontFamilies.empty())
        return;
    std::sort(fontFamilies.begin(), fontFamilies.end(),
              [](const FreetypeFontFamilyInfo &a, const FreetypeFontFamilyInfo &b)
              {
                  bool aIsAlpha = isalnum(a.displayName[0]);
                  bool bIsAlpha = isalnum(b.displayName[0]);
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
                      static const char *commonStyles[] = {"Regular",   "UltraLight", "ExtraLight", "Thin",
                                                           "Light",     "SemiLight",  "Bold",       "SemiBold",
                                                           "ExtraBold", "UltraBold"};

                      auto aIdx = std::find(std::begin(commonStyles), std::end(commonStyles), a.style);
                      auto bIdx = std::find(std::begin(commonStyles), std::end(commonStyles), b.style);

                      if (aIdx && bIdx)
                      {
                          if (aIdx == bIdx)
                          {
                              if (a.style.find("Italic") != string::npos && b.style.find("Italic") == string::npos)
                                  return a.style < b.style;
                              else if (a.style.find("Italic") != string::npos)
                                  return false;
                              else if (b.style.find("Italic") != string::npos)
                                  return true;
                          }

                          return aIdx < bIdx;
                      }
                      return a.style < b.style;
                  });
    }
}

#endif