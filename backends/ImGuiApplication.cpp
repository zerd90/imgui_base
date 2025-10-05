#include <stdint.h>
#include <filesystem>
#include <string>

#if defined(__linux) || defined(__APPLE__)
    #include <unistd.h>
#endif

#include "ImGuiApplication.h"

#include "imgui_common_tools.h"
#include "ApplicationSetting.h"

using std::string;
namespace fs = std::filesystem;

using namespace ImGui;

ImGuiApplication *gUserApp = nullptr;

ImGuiApplication::ImGuiApplication()
    : mLogger("Application Log"),
      mFontChooser("Font Chooser", std::bind(&ImGuiApplication::onFontChanged, this, std::placeholders::_1, std::placeholders::_2,
                                             std::placeholders::_3, std::placeholders::_4))
{
    mWindowRect      = {100, 100, 640, 480};
    mApplicationName = "ImGui Application";

    mExePath = getApplicationPath();

    fs::path exeDir = fs::u8path(mExePath).parent_path();
    mConfigPath     = localToUtf8((exeDir / "Setting.ini").string());

#ifdef _WIN32
    mScriptPath = localToUtf8((exeDir / "script.bat").string());
#else
    mScriptPath = localToUtf8((exeDir / "script.sh").string());
#endif
    std::error_code ec;
    if (fs::exists(fs::u8path(mScriptPath), ec))
    {
        fs::remove(fs::u8path(mScriptPath), ec);
    }

    addSetting(
        SettingValue::SettingInt, "Theme", [this](const void *val) { mAppTheme = (GUI_THEME)(*((int *)val)); },
        [this](void *val) { *((int *)val) = mAppTheme; });
    addSetting(
        SettingValue::SettingBool, "GUI VSync", [this](const void *val) { mGuiVSync = *((bool *)val); },
        [this](void *val) { *((bool *)val) = mGuiVSync; });
    addSetting(
        SettingValue::SettingBool, "Show Log Window", [this](const void *val) { mShowLogWindow = *((bool *)val); },
        [this](void *val) { *((bool *)val) = mShowLogWindow; });

    if (mAppFontPath.empty())
    {
        addSetting(
            SettingValue::SettingStr, "GUI Font Path", [this](const void *val) { mAppFontPath = (char *)val; },
            [this](void *val)
            {
                char **pStr = (char **)val;
                *pStr       = (char *)mAppFontPath.c_str();
            });
        addSetting(
            SettingValue::SettingInt, "GUI Font Index", [this](const void *val) { mAppFontIdx = *((int *)val); },
            [this](void *val) { *((int *)val) = mAppFontIdx; });
        addSetting(
            SettingValue::SettingFloat, "GUI Font Size", [this](const void *val) { mAppFontSize = *((float *)val); },
            [this](void *val) { *((float *)val) = mAppFontSize; });
    }

    mFontChooser.setHasCloseButton(false);
    mFontChooser.addWindowFlag(ImGuiWindowFlags_NoScrollbar);

    gUserApp = this;
}

void ImGuiApplication::preset()
{
    auto &io                            = ImGui::GetIO();
    io.ConfigErrorRecoveryEnableTooltip = false;
    io.ConfigErrorRecoveryEnableAssert  = false;

    presetInternal();
    addSettingArr(
        SettingValue::SettingArrInt, "WinRect", 4,
        [this](const void *val)
        {
            int *arr      = (int *)val;
            mWindowRect.x = arr[0];
            mWindowRect.y = arr[1];
            mWindowRect.w = arr[2];
            mWindowRect.h = arr[3];

            // Make sure the window is in the display area
            ImRect workArea = getDisplayWorkArea();
            if (mWindowRect.x > workArea.Max.x)
                mWindowRect.x = (int)workArea.Max.x - mWindowRect.w;
            if (mWindowRect.x < workArea.Min.x)
                mWindowRect.x = (int)workArea.Min.x;
            if (mWindowRect.y > workArea.Max.y)
                mWindowRect.y = (int)workArea.Max.y - mWindowRect.h;
            if (mWindowRect.y < workArea.Min.y)
                mWindowRect.y = (int)workArea.Min.y;
        },
        [this](void *val)
        {
            int *arr = (int *)val;
            arr[0]   = mWindowRect.x;
            arr[1]   = mWindowRect.y;
            arr[2]   = mWindowRect.w;
            arr[3]   = mWindowRect.h;
        });

    ImGuiSettingsHandler ini_handler;
    ini_handler.TypeName   = "App Window";
    ini_handler.TypeHash   = ImHashStr("App Window");
    ini_handler.ReadOpenFn = WinSettingsHandler_ReadOpen;
    ini_handler.ReadLineFn = WinSettingsHandler_ReadLine;
    ini_handler.WriteAllFn = WinSettingsHandler_WriteAll;
    ini_handler.UserData   = &mAppSettings;

    ImGui::AddSettingsHandler(&ini_handler);

    // Menu
    if (mMenuEnable)
    {
        addMenu({"Settings", "Show Log"}, [this]() { mLogger.open(); }, &mShowLogWindow);
        addMenu(
            {"GUI", "Theme", "Dark"},
            [this]()
            {
                mAppTheme = THEME_DARK;
                ImGui::StyleColorsDark();
            },
            [this]() { return mAppTheme == THEME_DARK; });

        addMenu(
            {"GUI", "Theme", "Light"},
            [this]()
            {
                mAppTheme = THEME_LIGHT;
                ImGui::StyleColorsLight();
            },
            [this]() { return mAppTheme == THEME_LIGHT; });

        addMenu(
            {"GUI", "Theme", "Classic"},
            [this]()
            {
                mAppTheme = THEME_CLASSIC;
                ImGui::StyleColorsClassic();
            },
            [this]() { return mAppTheme == THEME_CLASSIC; });

        addMenu({"GUI", "V-Sync"}, nullptr, &mGuiVSync);
        addMenu({"GUI", "Show Status"}, nullptr, &mShowUIStatus);
        if (mAppFontPath.empty())
            addMenu({"GUI", "Font"}, [&]() { mFontChooser.open(); });
    }
    else
    {
        removeWindowFlag(ImGuiWindowFlags_MenuBar);
    }
}

void ImGuiApplication::addSetting(SettingValue::SettingType type, std::string name, std::function<void(const void *)> setVal,
                                  std::function<void(void *)> getVal)
{
    mAppSettings.emplace_back(type, name, setVal, getVal);
}

void ImGuiApplication::addSettingArr(SettingValue::SettingType type, std::string name, int arrLen,
                                     std::function<void(const void *)> setVal, std::function<void(void *)> getVal)
{
    mAppSettings.emplace_back(type, name, setVal, getVal, arrLen);
}

void ImGuiApplication::showContent()
{
    if (renderUI())
        this->close();

    if (mShowUIStatus)
        ImGui::ShowMetricsWindow(&mShowUIStatus);

    if (mShowLogWindow)
    {
        mLogger.show();
        if (mLogger.justClosed())
            mShowLogWindow = false;
    }
    mFontChooser.show();
}

void ImGuiApplication::addLog(const std::string &logString)
{
    mLogger.appendString(logString);
}

void ImGuiApplication::restart()
{
    if (restartApplication(mScriptPath, mExePath))
        this->close();
}

void ImGuiApplication::windowRectChange(ImGuiAppRect rect)
{
    if (rect.x >= 0)
        mWindowRect.x = rect.x;
    if (rect.y >= 0)
        mWindowRect.y = rect.y;
    if (rect.w > 0)
        mWindowRect.w = rect.w;
    if (rect.h > 0)
        mWindowRect.h = rect.h;
}

void ImGuiApplication::getWindowSizeLimit(ImVec2 &minSize, ImVec2 &maxSize)
{
    minSize = mWindowSizeMin;
    maxSize = mWindowSizeMax;
}

#ifdef IMGUI_ENABLE_FREETYPE
bool checkFontAvailable(const std::string &fontPath, int fontIdx, bool &fontSupportFullRange, bool &fontSupportEnglish)
{
    bool       fontAvailable = false;
    FT_Library ftLibrary     = nullptr;
    FT_Face    face          = nullptr;
    FT_UInt    charIndex;

    int err = FT_Init_FreeType(&ftLibrary);
    if (err)
    {
        gUserApp->addLog(combineString("init freetype library fail: ", FT_Error_String(err), "\n"));
        goto CHECK_DONE;
    }
    err = FT_New_Face(ftLibrary, fontPath.c_str(), fontIdx, &face);
    if (err || !face)
    {
        gUserApp->addLog(combineString("init freetype face fail: ", FT_Error_String(err), "\n"));
        goto CHECK_DONE;
    }
    err = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
    if (err)
    {
        gUserApp->addLog(combineString("select freetype charmap fail: ", FT_Error_String(err), "\n"));
        goto CHECK_DONE;
    }

    // TODO: support multi language
    // check if the font support chinese, if not, use simhei as default font
    charIndex = FT_Get_Char_Index(face, u'中');
    if (charIndex == 0)
    {
        printf("font not support chinese character\n");
        fontSupportFullRange = false;
    }
    else
    {
        fontSupportFullRange = true;
    }

    charIndex = FT_Get_Char_Index(face, u'A');
    if (charIndex == 0)
    {
        printf("font not support english character\n");
        fontSupportEnglish = false;
    }
    else
    {
        printf("font support english character %d\n", charIndex);
        fontSupportEnglish = true;
    }

    fontAvailable = true;

CHECK_DONE:
    FT_Done_Face(face);
    FT_Done_FreeType(ftLibrary);

    return fontAvailable;
}
#endif

void ImGuiApplication::loadResources()
{
    setTitle(mApplicationName);

    switch (mAppTheme)
    {
        default:
        case THEME_DARK:
            ImGui::StyleColorsDark();
            break;
        case THEME_LIGHT:
            ImGui::StyleColorsLight();
            break;
        case THEME_CLASSIC:
            ImGui::StyleColorsClassic();
            break;
    }

    auto &io = ImGui::GetIO();
    if (mAppFontSize <= 0)
        mAppFontSize = 15;
    if (mAppFontPath.empty())
    {
#ifdef _WIN32
        mAppFontPath = R"(C:\WINDOWS\FONTS\SEGUIVAR.TTF)";
// TODO:
#elif defined(__linux)
        // not sure if these fonts is always available under any linux distribution...
        mAppFontPath = "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc";
#elif defined(__APPLE__)
        mAppFontPath = "/System/Library/Fonts/Supplemental/Times New Roman.ttf";
#endif
    }
    // check if font is available
    bool fontAvailable = true;
#ifdef IMGUI_ENABLE_FREETYPE
    bool fontSupportFullRange = true;
    bool fontSupportEnglish   = true;
    fontAvailable             = checkFontAvailable(mAppFontPath, mAppFontIdx, fontSupportFullRange, fontSupportEnglish);
#endif
    // TODO: support multi language
    if (fontAvailable)
    {
        ImFontConfig fontConfig;
        fontConfig.FontNo = mAppFontIdx;
        io.Fonts->AddFontFromFileTTF(mAppFontPath.c_str(), mAppFontSize, &fontConfig, io.Fonts->GetGlyphRangesChineseFull());
        addLog(combineString("load font: ", mAppFontPath, " index: ", std::to_string(mAppFontIdx), "\n"));
        mFontChooser.setCurrentFont(mAppFontPath, mAppFontIdx, mAppFontSize);

#ifdef IMGUI_ENABLE_FREETYPE
        if (!fontSupportFullRange)
        {
            addLog("font not support full range character, use default for chinese\n");
            fontConfig           = ImFontConfig();
            fontConfig.MergeMode = true;
    #ifdef _WIN32
            io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\simhei.ttf)", mAppFontSize, &fontConfig,
                                         io.Fonts->GetGlyphRangesChineseFull());
    #elif defined(__linux)
            io.Fonts->AddFontFromFileTTF(R"(/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf)", mAppFontSize,
                                         &fontConfig, io.Fonts->GetGlyphRangesChineseFull());
    #elif defined(__APPLE__)
            auto systemFonts = mFontChooser.getSystemFontFamilies();
            auto familyIter  = std::find_if(systemFonts.begin(), systemFonts.end(),
                                            [](const FreetypeFontFamilyInfo &info) { return info.name == "PingFang SC"; });

            if (familyIter != systemFonts.end())
            {
                auto fontIter = std::find_if(familyIter->fonts.begin(), familyIter->fonts.end(),
                                             [](const FreetypeFontInfo &info) { return info.style == "Regular"; });
                if (fontIter != familyIter->fonts.end())
                {
                    fontConfig.FontNo = fontIter->index;
                    io.Fonts->AddFontFromFileTTF(familyIter->fonts[0].path.c_str(), mAppFontSize, &fontConfig,
                                                 io.Fonts->GetGlyphRangesChineseFull());
                }
            }
    #endif
        }

        if (!fontSupportEnglish)
        {
            addLog("font not support english character, use default for english\n");
            fontConfig           = ImFontConfig();
            fontConfig.MergeMode = true;
    #ifdef _WIN32
            io.Fonts->AddFontFromFileTTF(R"(C:\Windows\Fonts\simhei.ttf)", mAppFontSize, &fontConfig,
                                         io.Fonts->GetGlyphRangesDefault());
    #elif defined(__linux)
            io.Fonts->AddFontFromFileTTF(R"(/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf)", mAppFontSize, &fontConfig,
                                         io.Fonts->GetGlyphRangesDefault());
    #elif defined(__APPLE__)
            io.Fonts->AddFontFromFileTTF(R"(mAppFontPath = "/System/Library/Fonts/Supplemental/Times New Roman.ttf")",
                                         mAppFontSize, &fontConfig, io.Fonts->GetGlyphRangesDefault());
    #endif
        }
#endif
    }

    if (mShowLogWindow)
        mLogger.open();
}

bool ImGuiApplication::VSyncEnabled()
{
    return mGuiVSync;
}

void ImGuiApplication::onFontChanged(const std::string &fontPath, int fontIdx, float fontSize, bool applyNow)
{
    mAppFontPath = fontPath;
    mAppFontIdx  = fontIdx;
    mAppFontSize = fontSize;

    if (applyNow)
        restart();
}
