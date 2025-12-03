#ifndef IMGUI_COMMON_TOOLS_H
#define IMGUI_COMMON_TOOLS_H

#include <stdint.h>
#include <vector>
#include <string>
#include <memory>
#include <sstream>

#ifdef IMGUI_ENABLE_FREETYPE
    #include "ft2build.h"
    #include FT_FREETYPE_H
#endif

#include "imgui_internal.h"

#if defined(_WIN32)
    #include <Windows.h>
    #undef min
    #undef max
#endif

#ifndef dbg
    #define dbg(fmt, ...) fprintf(stderr, "[%s:%d] " fmt, __func__, __LINE__, ##__VA_ARGS__);
#endif

namespace ImGui
{

#define IMGUI_WINDOW_API_WIN32 0
#define IMGUI_WINDOW_API_GLFW  1

#define IMGUI_RENDER_API_DX11   0
#define IMGUI_RENDER_API_OPENGL 1

    template <typename _T>
    static inline auto arrayMakeSharedPtr(size_t length)
    {
        return std::shared_ptr<_T[]>(new _T[length]);
    }

    template <typename _T>
    static inline auto arrayMakeSharedPtr(size_t length, _T defaultValue)
    {
        auto ptr = std::shared_ptr<_T[]>(new _T[length]);
        for (size_t i = 0; i < length; i++)
            ptr[i] = defaultValue;

        return ptr;
    }

#if defined(_WIN32)
    std::shared_ptr<std::shared_ptr<char[]>[]> CommandLineToArgvA(int *argc);
    std::string                                HResultToStr(HRESULT hr);
#endif

    std::string utf8ToLocal(const std::string &str);
    std::string localToUtf8(const std::string &str);

    std::string  unicodeToUtf8(const std::wstring &wStr);
    std::wstring utf8ToUnicode(const std::string &str);

    std::string  unicodeToLocal(const std::wstring &wStr);
    std::wstring localToUnicode(const std::string &str);

    template <typename... Args>
    std::string combineString(Args... args)
    {
        std::stringstream ss;
        (ss << ... << args); // C++17 fold expression
        return ss.str();
    }

    std::string getLastError();

    // from errno
    std::string getSystemError();

    void startFontPixPreload();
    void waitFontPixPreload();

    // Platform Relative

#if defined(WIN32) || defined(_WIN32)
    #define fseek64 _fseeki64
    #define ftell64 _ftelli64
    using file_stat64_t = struct _stat64;
    #define stat64 _stat64

#elif defined(__linux)
    #define fseek64 fseeko64
    #define ftell64 ftello64
    using file_stat64_t = struct stat64;

#else // MacOS
    #define fseek64 fseek
    #define ftell64 ftell
    #define stat64  stat
    using file_stat64_t = struct stat;

#endif

    typedef struct
    {
        std::string filter; //*.xxx;*.xxx;...
        std::string description;
    } FilterSpec;

    const std::vector<FilterSpec> &getImageFilter();
    const std::vector<FilterSpec> &getVideoFilter();
    const std::vector<FilterSpec> &getAudioFilter();
    const std::vector<FilterSpec> &getTextFilter();

    std::string              selectDir(const std::string &initDirPath = std::string());
    std::string              selectFile(const std::vector<FilterSpec> &typeFilters = std::vector<FilterSpec>(),
                                        const std::string             &initDirPath = std::string());
    std::vector<std::string> selectMultipleFiles(const std::vector<FilterSpec> &typeFilters = std::vector<FilterSpec>(),
                                                 const std::string             &initDirPath = std::string());
    std::string              getSavePath(const std::vector<FilterSpec> &typeFilters = std::vector<FilterSpec>(),
                                         const std::string &defaultExt = std::string(), const std::string &initDirPath = std::string());
    void                     openDebugWindow();

    std::string getSystemPictureFolder();

    std::string getApplicationPath();

    bool restartApplication(const std::string &scriptPath, const std::string &programPath);

#ifdef IMGUI_ENABLE_FREETYPE

    struct FreetypeFontInfo
    {
        std::string style;
        std::string styleDisplayName;
        std::string path;
        uint32_t    index;
    };

    struct FreetypeFontFamilyInfo
    {
        std::string                   name;
        std::string                   displayName;
        std::vector<FreetypeFontInfo> fonts;
    };

    std::vector<FreetypeFontFamilyInfo> listSystemFonts();

    void sortFonts(std::vector<FreetypeFontFamilyInfo> &fontFamilies);
#endif

    // End Platform Relative
    std::string getResourcesDir();

    // Backend Relative
    void setApplicationTitle(const std::string &title);

    ImRect getDisplayWorkArea();
    ImRect getMainWindowRect();
    ImRect maximizeMainWindow();
    void   normalizeApplication(const ImRect &winRect);
    void   minimizeMainWindow();
    ImRect getWindowRect();
    ImRect getWindowWithoutDecorationRect();

    // End Backend Relative

} // namespace ImGui

#endif