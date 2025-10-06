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
using std::vector;
namespace fs = std::filesystem;

#define SETTING_WINDOW_WIDTH  720
#define SETTING_WINDOW_HEIGHT 540

using namespace ImGui;

ImGuiApplication *gUserApp = nullptr;

ImGuiApplication::ImGuiApplication()
    : mLogger("Application Log"),
      mFontChooser("Font Chooser", std::bind(&ImGuiApplication::onFontChanged, this, std::placeholders::_1, std::placeholders::_2,
                                             std::placeholders::_3, std::placeholders::_4)),
      mSettingsWindow("Settings"), mCreateFileConfirmDialog("Create File", "Are you sure to create the file?")
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

    mSettingsWindow.addWindowFlag(ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
    mSettingsWindow.setSize(ImVec2(SETTING_WINDOW_WIDTH, SETTING_WINDOW_HEIGHT));
    mSettingsWindow.setContent([this]() { showSettingWindow(); });

    mCreateFileConfirmDialog.addButton(
        "Yes",
        [this]()
        {
            mCreateFileConfirmDialog.close();

            if (!mCreateFilePathItem || mCreateFilePath.empty()
                || !(mCreateFilePathItem->data.pathItem.flags & SettingPathFlags_CreateWhenNotExist))
                return;

            bool createDir = false;
            if (mCreateFilePathItem->data.pathItem.flags & SettingPathFlags_SelectDir
                || mCreateFilePathItem->data.pathItem.flags & SettingPathFlags_SelectForSave)
                createDir = true;

            string createPath = mCreateFilePath;
            if (mCreateFilePathItem->data.pathItem.flags & SettingPathFlags_SelectForSave)
                createPath = fs::u8path(mCreateFilePath).parent_path().u8string();

            std::error_code ec;
            vector<string>  parentPaths;
            string          filePath = createPath;
            while (filePath != fs::u8path(filePath).parent_path().u8string())
            {
                filePath = fs::u8path(filePath).parent_path().u8string();
                parentPaths.push_back(filePath);
            }
            for (auto it = parentPaths.rbegin(); it != parentPaths.rend(); it++)
            {
                if (fs::exists(fs::u8path(*it), ec))
                    continue;
                fs::create_directories(fs::u8path(*it), ec);
            }
            if (createDir)
            {
                fs::create_directories(fs::u8path(createPath), ec);
            }
            else
            {
                FILE *fp = fopen(utf8ToLocal(createPath).c_str(), "w");
                if (fp)
                    fclose(fp);
            }

            *(mCreateFilePathItem->data.pathItem.pathData) = mCreateFilePath;
            if (mCreateFilePathItem->onChange)
                mCreateFilePathItem->onChange();
            auto pathInput = std::dynamic_pointer_cast<ImGuiInputString>(mCreateFilePathItem->settingInput);
            pathInput->setValue(mCreateFilePath);

            mCreateFilePath.clear();
            mCreateFilePathItem = nullptr;
        });
    mCreateFileConfirmDialog.addButton("No",
                                       [this]()
                                       {
                                           mCreateFilePath.clear();
                                           mCreateFilePathItem = nullptr;
                                           mCreateFileConfirmDialog.close();
                                       });

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
        addMenu({"Menu", "Settings"}, [this]() { mSettingsWindow.open(); });
    }
    else
    {
        removeWindowFlag(ImGuiWindowFlags_MenuBar);
    }

    vector<string> settingPath;
    settingPath.push_back("GUI");
    vector<std::pair<ComboTag, string>> themeItems = {
        {THEME_DARK,    "Dark"   },
        {THEME_LIGHT,   "Light"  },
        {THEME_CLASSIC, "Classic"}
    };

    addSettingWindowItemCombo(settingPath, "Theme", (ComboTag *)&mAppTheme, themeItems,
                              [this]()
                              {
                                  switch (mAppTheme)
                                  {
                                      case THEME_DARK:
                                          ImGui::StyleColorsDark();
                                          break;
                                      case THEME_LIGHT:
                                          ImGui::StyleColorsLight();
                                          break;
                                      case THEME_CLASSIC:
                                          ImGui::StyleColorsClassic();
                                          break;
                                      default:
                                          break;
                                  }
                              });

    addSettingWindowItemBool(settingPath, "V-Sync", &mGuiVSync);
    addSettingWindowItemBool(settingPath, "Show UI Status", &mShowUIStatus);
    if (mAppFontPath.empty())
    {
        addSettingWindowItemButton(settingPath, "Font", [this]() { mFontChooser.open(); });
    }
    addSettingWindowItemBool({"Debug"}, "Show Application Log", &mShowLogWindow,
                             [this]()
                             {
                                 if (mShowLogWindow)
                                     mLogger.open();
                                 else
                                     mLogger.close();
                             });
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

    mLogger.show();
    if (mLogger.justClosed())
        mShowLogWindow = false;

    mSettingsWindow.show();

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

SettingWindowCategory *ImGuiApplication::findCategory(vector<std::string> categoryPath)
{
    auto                  *subCategorys = &mSettingCategories;
    SettingWindowCategory *category     = nullptr;
    for (int i = 0; i < categoryPath.size(); i++)
    {
        std::string categoryLabel = categoryPath[i];
        auto        categoryIter =
            std::find_if(subCategorys->begin(), subCategorys->end(),
                         [&categoryLabel](const SettingWindowCategory &cat) { return cat.label == categoryLabel; });
        if (categoryIter == subCategorys->end())
        {
            SettingWindowCategory subCategory;
            subCategory.label = categoryLabel;
            for (int j = 0; j < i; j++)
            {
                subCategory.categoryPath += categoryPath[j] + "/";
            }
            subCategory.categoryPath += categoryLabel;
            subCategorys->push_back(subCategory);
            category     = &subCategorys->back();
            subCategorys = &category->subCategories;
        }
        else
        {
            category     = &(*categoryIter);
            subCategorys = &category->subCategories;
        }
    }
    return category;
}

void ImGuiApplication::addSettingWindowItemBool(const vector<std::string> &categoryPath, const std::string &label, bool *data,
                                                const std::function<void()> &onChange)
{
    auto *category = findCategory(categoryPath);
    if (category == nullptr)
        return;

    SettingWindowItem item;
    item.label    = label;
    item.type     = SettingWindowItemTypeBool;
    item.onChange = onChange;

    item.data.boolItem.boolData = data;

    category->items.push_back(item);
}

void ImGuiApplication::addSettingWindowItemInt(const vector<std::string> &categoryPath, const std::string &label, int *data,
                                               int minValue, int maxValue, bool stepEnable, const std::function<void()> &onChange)
{
    auto *category = findCategory(categoryPath);
    if (category == nullptr)
        return;

    SettingWindowItem item;
    item.type     = SettingWindowItemTypeInt;
    item.label    = label;
    item.onChange = onChange;

    item.data.intItem.minValue   = minValue;
    item.data.intItem.maxValue   = maxValue;
    item.data.intItem.stepEnable = stepEnable;
    item.data.intItem.intData    = data;

    auto intInput = std::make_shared<ImGuiInputInt>(label, *data, true);
    intInput->setSyncValue(data);
    intInput->setMinValue(minValue);
    intInput->setMaxValue(maxValue);
    if (stepEnable)
        intInput->setStep(1, 100);

    item.settingInput = intInput;

    category->items.push_back(item);
}

void ImGuiApplication::addSettingWindowItemFloat(const vector<std::string> &categoryPath, const std::string &label, float *data,
                                                 float minValue, float maxValue, bool stepEnable,
                                                 const std::function<void()> &onChange)
{
    auto *category = findCategory(categoryPath);
    if (category == nullptr)
        return;

    SettingWindowItem item;
    item.type     = SettingWindowItemTypeFloat;
    item.label    = label;
    item.onChange = onChange;

    item.data.floatItem.minValue   = minValue;
    item.data.floatItem.maxValue   = maxValue;
    item.data.floatItem.stepEnable = stepEnable;
    item.data.floatItem.floatData  = data;

    auto floatInput = std::make_shared<ImGuiInputFloat>(label, *data, true);
    floatInput->setSyncValue(data);
    floatInput->setMinValue(minValue);
    floatInput->setMaxValue(maxValue);
    if (stepEnable)
        floatInput->setStep(0.1f, 100.0f);

    item.settingInput = floatInput;

    category->items.push_back(item);
}

void ImGuiApplication::addSettingWindowItemString(const vector<std::string> &categoryPath, const std::string &label,
                                                  std::string *data, const std::function<void()> &onChange)
{
    auto *category = findCategory(categoryPath);
    if (category == nullptr)
        return;

    SettingWindowItem item;
    item.type     = SettingWindowItemTypeString;
    item.label    = label;
    item.onChange = onChange;

    item.data.stringItem.stringData = data;

    auto stringInput  = std::make_shared<ImGuiInputString>(label, *data, true);
    item.settingInput = stringInput;

    category->items.push_back(item);
}

void ImGuiApplication::addSettingWindowItemCombo(const vector<std::string> &categoryPath, const std::string &label,
                                                 ComboTag *data, const vector<std::pair<ComboTag, std::string>> &comboItems,
                                                 const std::function<void()> &onChange)
{
    auto *category = findCategory(categoryPath);
    if (category == nullptr)
        return;

    SettingWindowItem item;
    item.type     = SettingWindowItemTypeCombo;
    item.label    = label;
    item.onChange = onChange;

    item.data.comboItem.comboData = data;
    auto comboInput               = std::make_shared<ImGuiInputCombo>(label, true);
    for (auto &comboItem : comboItems)
        comboInput->addSelectableItem(comboItem.first, comboItem.second);
    comboInput->setSelected(*data);
    item.settingInput = comboInput;

    category->items.push_back(item);
}
void ImGuiApplication::addSettingWindowItemPath(const vector<std::string> &categoryPath, const std::string &label,
                                                std::string *data, uint32_t flags, const std::vector<FilterSpec> &typeFilters,
                                                const std::function<void()> &onChange)
{
    auto *category = findCategory(categoryPath);
    if (category == nullptr)
        return;

    SettingWindowItem item;
    item.type     = SettingWindowItemTypePath;
    item.label    = label;
    item.onChange = onChange;

    item.data.pathItem.pathData = data;
    item.data.pathItem.flags    = flags;

    auto pathInput    = std::make_shared<ImGuiInputString>(label, *data, true);
    item.settingInput = pathInput;
    item.typeFilters  = typeFilters;

    category->items.push_back(item);
}

void ImGuiApplication::addSettingWindowItemButton(const vector<std::string> &categoryPath, const std::string &label,
                                                  const std::function<void()> &onChange)
{
    auto *category = findCategory(categoryPath);
    if (category == nullptr)
        return;

    SettingWindowItem item;
    item.type     = SettingWindowItemTypeButton;
    item.label    = label;
    item.onChange = onChange;

    category->items.push_back(item);
}

#define TREE_NODE_FLAGS \
    (ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth)
#define TREE_LEAF_FLAGS (TREE_NODE_FLAGS | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen)

void ImGuiApplication::showSettingWindowCategory(SettingWindowCategory *category)
{
    int nodeFlags = category->subCategories.empty() ? TREE_LEAF_FLAGS : TREE_NODE_FLAGS;

    if (mCurrentSettingCategory == category)
        nodeFlags |= ImGuiTreeNodeFlags_Selected;

    ImGui::TreeNodeEx(category->label.c_str(), nodeFlags);
    if (!category->subCategories.empty())
    {
        for (auto &subCategory : category->subCategories)
            showSettingWindowCategory(&subCategory);
        ImGui::TreePop();
    }

    if (IsItemClicked())
    {
        mCurrentSettingCategory = category;
    }
}

void ImGuiApplication::showSettingWindowItem(SettingWindowItem &item)
{
    switch (item.type)
    {
        default:
            break;
        case SettingWindowItemTypeBool:
        {
            ImGui::Checkbox(item.label.c_str(), item.data.boolItem.boolData);
            if (IsItemDeactivated())
            {
                if (item.onChange)
                    item.onChange();
            }
            break;
        }
        case SettingWindowItemTypeInt:
        case SettingWindowItemTypeFloat:
        case SettingWindowItemTypeString:
        {
            item.settingInput->show();
            if (item.settingInput->isDeactivated())
            {
                if (item.onChange)
                    item.onChange();
            }
            break;
        }
        case SettingWindowItemTypeCombo:
        {
            auto comboInput = std::dynamic_pointer_cast<ImGuiInputCombo>(item.settingInput);
            comboInput->show();
            if (comboInput->selectChanged())
            {
                *(item.data.comboItem.comboData) = comboInput->getSelected();
                if (item.onChange)
                    item.onChange();
            }
            break;
        }
        case SettingWindowItemTypePath:
        {
            auto pathInput = std::dynamic_pointer_cast<ImGuiInputString>(item.settingInput);
            pathInput->show();
            string tmpResult;
            string curPath       = *(item.data.pathItem.pathData);
            bool   isPathChanged = false;
            if (pathInput->isDeactivated())
            {
                tmpResult = pathInput->getValue();
                if (!tmpResult.empty() && tmpResult != curPath)
                    isPathChanged = true;
            }
            SameLine();
            string btnLabel = "Browse##";
            btnLabel += item.label;

            if (ImGui::Button(btnLabel.c_str()))
            {
                string dlgResult;
                string initPath;
                if (!curPath.empty())
                {
                    fs::path curPathPath = fs::u8path(curPath);
                    if (fs::exists(curPathPath))
                        initPath = curPathPath.parent_path().u8string();
                }
                else
                {
                    initPath = fs::u8path(mExePath).parent_path().u8string();
                }
                if (item.data.pathItem.flags & SettingPathFlags_SelectDir)
                {
                    dlgResult = selectDir(initPath);
                }
                else if (item.data.pathItem.flags & SettingPathFlags_SelectForSave)
                {
                    dlgResult = getSavePath(item.typeFilters, "", initPath);
                }
                else
                {
                    dlgResult = selectFile(item.typeFilters, initPath);
                }

                if (!dlgResult.empty())
                {
                    tmpResult     = dlgResult;
                    isPathChanged = true;
                }
            }

            if (isPathChanged)
            {
                std::error_code ec;

                // Check if the path is valid
                string checkPath = tmpResult;
                if (item.data.pathItem.flags & SettingPathFlags_SelectForSave)
                    checkPath = fs::u8path(tmpResult).parent_path().u8string();


                bool isPathValid = fs::exists(fs::u8path(checkPath), ec);
                if (!isPathValid)
                {
                    if (item.data.pathItem.flags & SettingPathFlags_CreateWhenNotExist)
                    {
                        mCreateFileConfirmDialog.setMessage(combineString("Path ", tmpResult, " does not exist, Create it?"));
                        mCreateFileConfirmDialog.open();
                        mCreateFilePath     = tmpResult;
                        mCreateFilePathItem = &item;
                    }
                    else
                    {
                        mCreateFileConfirmDialog.setMessage(combineString("Path ", tmpResult, " does not exist!"));
                        mCreateFileConfirmDialog.open();
                        pathInput->setValue(curPath);
                    }
                }
                else
                {
                    *item.data.pathItem.pathData = tmpResult;
                    if (item.onChange)
                        item.onChange();
                    pathInput->setValue(tmpResult);
                }
            }

            // TODO
            break;
        }
        case SettingWindowItemTypeButton:
        {
            if (ImGui::Button(item.label.c_str()))
            {
                if (item.onChange)
                    item.onChange();
            }
            break;
        }
    }
}

void ImGuiApplication::showSettingWindow()
{
    if (!mCurrentSettingCategory)
    {
        mCurrentSettingCategory = &mSettingCategories[0];
    }

    ImGui::BeginChild("Setting Category", ImVec2(SETTING_WINDOW_WIDTH / 4.f, 0), ImGuiChildFlags_Borders);

    for (auto &category : mSettingCategories)
        showSettingWindowCategory(&category);

    ImGui::EndChild();
    SameLine();
    ImGui::BeginChild("Setting Options", ImVec2(0, 0), ImGuiChildFlags_Borders);
    Text(">> %s", mCurrentSettingCategory->label.c_str());
    Separator();
    for (auto &item : mCurrentSettingCategory->items)
        showSettingWindowItem(item);

    ImGui::EndChild();
    mCreateFileConfirmDialog.show();
    if (mCreateFileConfirmDialog.justClosed())
    {
        if (mCreateFilePathItem)
        {
            auto pathInput = std::dynamic_pointer_cast<ImGuiInputString>(mCreateFilePathItem->settingInput);
            pathInput->setValue(*(mCreateFilePathItem->data.pathItem.pathData));
        }
        mCreateFilePath.clear();
        mCreateFilePathItem = nullptr;
    }
}