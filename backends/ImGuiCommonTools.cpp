
#include <filesystem>
#include <algorithm>
#include <thread>
#include <fstream>
#include <cstdint>

#include "ImGuiCommonTools.h"
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

    namespace
    {
        constexpr uint32_t kShaderCacheMagic   = 0x42534D49; // 'IMSB'
        constexpr uint32_t kShaderCacheVersion = 1;
        constexpr uint32_t kShaderCacheMaxText = 4096;
        constexpr uint32_t kShaderCacheMaxBlob = 16 * 1024 * 1024;

        bool shaderKeyComplete(const ShaderBinaryKey &key)
        {
            return !key.compileTime.empty() && !key.systemVersion.empty() && !key.gpuName.empty() && !key.driverVersion.empty();
        }

        bool readPod(std::istream &in, void *data, size_t size)
        {
            in.read(reinterpret_cast<char *>(data), static_cast<std::streamsize>(size));
            return static_cast<bool>(in);
        }

        bool readU32(std::istream &in, uint32_t &value)
        {
            return readPod(in, &value, sizeof(value));
        }

        bool readText(std::istream &in, std::string &text)
        {
            uint32_t length = 0;
            if (!readU32(in, length) || length > kShaderCacheMaxText)
                return false;
            text.resize(length);
            return length == 0 || readPod(in, text.data(), length);
        }

        bool writePod(std::ostream &out, const void *data, size_t size)
        {
            out.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(size));
            return static_cast<bool>(out);
        }

        bool writeU32(std::ostream &out, uint32_t value)
        {
            return writePod(out, &value, sizeof(value));
        }

        bool writeText(std::ostream &out, const std::string &text)
        {
            if (text.size() > kShaderCacheMaxText)
                return false;
            return writeU32(out, static_cast<uint32_t>(text.size())) && (text.empty() || writePod(out, text.data(), text.size()));
        }
    } // namespace

    std::string getExecutableCompileTime()
    {
        std::error_code ec;
        const auto      writeTime = fs::last_write_time(fs::u8path(getApplicationPath()), ec);
        if (ec)
            return {};
        return std::to_string(static_cast<long long>(writeTime.time_since_epoch().count()));
    }

    bool loadCachedShaderBinary(const std::string &path, const ShaderBinaryKey &key, std::vector<uint8_t> &binary,
                                uint32_t &binaryFormat)
    {
        binary.clear();
        binaryFormat = 0;
        if (!shaderKeyComplete(key))
            return false;

        std::ifstream in(fs::u8path(path), std::ios::binary);
        if (!in)
            return false;

        uint32_t        magic   = 0;
        uint32_t        version = 0;
        uint32_t        format  = 0;
        ShaderBinaryKey stored;
        uint32_t        blobSize = 0;
        if (!readU32(in, magic) || magic != kShaderCacheMagic || !readU32(in, version) || version != kShaderCacheVersion
            || !readU32(in, format) || !readText(in, stored.compileTime) || !readText(in, stored.systemVersion)
            || !readText(in, stored.gpuName) || !readText(in, stored.driverVersion) || !readU32(in, blobSize) || blobSize == 0
            || blobSize > kShaderCacheMaxBlob)
            return false;
        if (stored.compileTime != key.compileTime || stored.systemVersion != key.systemVersion || stored.gpuName != key.gpuName
            || stored.driverVersion != key.driverVersion)
            return false;

        binary.resize(blobSize);
        if (!readPod(in, binary.data(), blobSize))
        {
            binary.clear();
            return false;
        }
        binaryFormat = format;
        return true;
    }

    bool saveCachedShaderBinary(const std::string &path, const ShaderBinaryKey &key, const void *data, size_t size,
                                uint32_t binaryFormat)
    {
        if (!shaderKeyComplete(key) || data == nullptr || size == 0 || size > kShaderCacheMaxBlob)
            return false;

        std::ofstream out(fs::u8path(path), std::ios::binary | std::ios::trunc);
        if (!out)
            return false;
        const auto blobSize = static_cast<uint32_t>(size);
        return writeU32(out, kShaderCacheMagic) && writeU32(out, kShaderCacheVersion) && writeU32(out, binaryFormat)
            && writeText(out, key.compileTime) && writeText(out, key.systemVersion) && writeText(out, key.gpuName)
            && writeText(out, key.driverVersion) && writeU32(out, blobSize) && writePod(out, data, size);
    }

} // namespace ImGui
