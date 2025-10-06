#ifndef _IMGUI_APPLICATION_H_
#define _IMGUI_APPLICATION_H_

#include <string>
#include <vector>
#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuiTools.h"
#include "ImGuiWindow.h"
#include "ApplicationSetting.h"
#define ADD_APPLICATION_LOG(fmt, ...)                                 \
    do                                                                \
    {                                                                 \
        char _logBuffer[1024] = {0};                                  \
        snprintf(_logBuffer, sizeof(_logBuffer), fmt, ##__VA_ARGS__); \
        gUserApp->addLog(_logBuffer);                                 \
    } while (0)
#define SET_APPLICATION_STATUS(fmt, ...)                              \
    do                                                                \
    {                                                                 \
        char _logBuffer[1024] = {0};                                  \
        snprintf(_logBuffer, sizeof(_logBuffer), fmt, ##__VA_ARGS__); \
        gUserApp->setStatus(_logBuffer);                              \
    } while (0)

namespace ImGui
{
    enum SettingWindowItemType
    {
        SettingWindowItemTypeBool,
        SettingWindowItemTypeInt,
        SettingWindowItemTypeFloat,
        SettingWindowItemTypeString,
        SettingWindowItemTypeCombo,
        SettingWindowItemTypePath,
        SettingWindowItemTypeButton,
    };

// Create the path when it does not exist, otherwise refuse the selection
#define SettingPathFlags_CreateWhenNotExist (0x1)
// Select directory instead of file
#define SettingPathFlags_SelectDir          (0x2)
// Select for save, otherwise select for load
#define SettingPathFlags_SelectForSave      (0x4)

    union SettingWindowItemData
    {
        struct
        {
            bool *boolData;
        } boolItem;
        struct
        {
            int *intData;
            int  minValue;
            int  maxValue;
            bool stepEnable;
        } intItem;
        struct
        {
            float *floatData;
            float  minValue;
            float  maxValue;
            bool   stepEnable;
        } floatItem;
        struct
        {
            std::string *stringData;
        } stringItem;
        struct
        {
            ComboTag *comboData;
        } comboItem;
        struct
        {
            std::string *pathData;
            uint32_t     flags;
        } pathItem;
    };
    struct SettingWindowItem
    {
        std::string                  label;
        SettingWindowItemType        type = SettingWindowItemTypeBool;
        SettingWindowItemData        data;
        std::function<void()>        onChange;
        std::shared_ptr<IImGuiInput> settingInput;
        std::vector<FilterSpec>      typeFilters; // filters for file dialog
    };
    struct SettingWindowCategory
    {
        std::string                        label;
        std::string                        categoryPath;
        std::vector<SettingWindowItem>     items;
        std::vector<SettingWindowCategory> subCategories;
    };

    class ImGuiApplication : public ImGui::ImGuiMainWindow
    {
        friend void *WinSettingsHandler_ReadOpen(ImGuiContext *, ImGuiSettingsHandler *handler, const char *name);
        friend void  WinSettingsHandler_ReadLine(ImGuiContext *, ImGuiSettingsHandler *handler, void *entry, const char *line);
        friend void  WinSettingsHandler_WriteAll(ImGuiContext *imgui_ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf);

    public:
        struct ImGuiAppRect
        {
            int x, y, w, h;
        };
        ImGuiApplication();
        virtual ~ImGuiApplication() {}
        std::string  getExePath() { return mExePath; };
        const char  *getConfigPath() { return mConfigPath.c_str(); };
        ImGuiAppRect getWindowInitialRect() { return mWindowRect; };
        void         preset();
        std::string  getAppName() { return mApplicationName; };
        void         windowRectChange(ImGuiAppRect rect);
        void         getWindowSizeLimit(ImVec2 &minSize, ImVec2 &maxSize);

        virtual void transferCmdArgs(std::vector<std::string> &args) { IM_UNUSED(args); };
        virtual void dropFile(const std::vector<std::string> &files) { IM_UNUSED(files); };

        virtual void setWindowHandle(void *handle) { IM_UNUSED(handle); };
        virtual void newFramePreAction() {}
        virtual void endFramePostAction() {}
        // anything ned to be done before program exit, such as waiting thread exit
        virtual void exit() {}

        // load resources like Fonts; this should be called after preset() and configs loading, before first NewFrame(),
        void loadResources();

        bool VSyncEnabled();
        void restart();
        void addLog(const std::string &logString);

    protected:
        // return if need to exit the application
        virtual bool renderUI() = 0;

        virtual void showContent() override final;

        virtual void presetInternal() {}

        // call addSetting in presetInternal or the constructor of derived class
        void addSetting(SettingValue::SettingType type, std::string name, std::function<void(const void *)> setVal,
                        std::function<void(void *)> getVal);

        void addSettingArr(SettingValue::SettingType type, std::string name, int arrLen, std::function<void(const void *)> setVal,
                           std::function<void(void *)> getVal);

        void addSettingWindowItemBool(const std::vector<std::string> &categoryPath, const std::string &label, bool *data,
                                      const std::function<void()> &onChange = nullptr);
        void addSettingWindowItemInt(const std::vector<std::string> &categoryPath, const std::string &label, int *data,
                                     int minValue = INT_MIN, int maxValue = INT_MAX, bool stepEnable = false,
                                     const std::function<void()> &onChange = nullptr);
        void addSettingWindowItemFloat(const std::vector<std::string> &categoryPath, const std::string &label, float *data,
                                       float minValue = FLT_MIN, float maxValue = FLT_MAX, bool stepEnable = false,
                                       const std::function<void()> &onChange = nullptr);
        void addSettingWindowItemString(const std::vector<std::string> &categoryPath, const std::string &label, std::string *data,
                                        const std::function<void()> &onChange = nullptr);
        void addSettingWindowItemCombo(const std::vector<std::string> &categoryPath, const std::string &label, ComboTag *data,
                                       const std::vector<std::pair<ComboTag, std::string>> &comboItems,
                                       const std::function<void()>                         &onChange = nullptr);
        void addSettingWindowItemPath(const std::vector<std::string> &categoryPath, const std::string &label, std::string *data,
                                      uint32_t flags = 0, const std::vector<FilterSpec> &typeFilters = std::vector<FilterSpec>(),
                                      const std::function<void()> &onChange = nullptr);
        void addSettingWindowItemButton(const std::vector<std::string> &categoryPath, const std::string &label,
                                        const std::function<void()> &onChange);

    private:
        void                   showSettingWindow();
        void                   showSettingWindowItem(SettingWindowItem &item);
        void                   showSettingWindowCategory(SettingWindowCategory *category);
        SettingWindowCategory *findCategory(std::vector<std::string> categoryPath);

    protected:
        // Set these in presetInternal
        ImGuiAppRect mWindowRect;
        std::string  mApplicationName;
        std::string  mExePath;
        std::string  mConfigPath;

        ImVec2 mWindowSizeMin = ImVec2(320, 240);
        ImVec2 mWindowSizeMax = ImVec2(0, 0); // no limit by default

    protected:
        std::vector<SettingValue> mAppSettings;
        bool                      mMenuEnable = true;

    private:
        std::string mScriptPath;

        // Settings
        // Saving
    protected:
        std::string mAppFontPath = "";
        float       mAppFontSize = 15;
        int         mAppFontIdx  = 0;

    protected:
        bool mGuiVSync = true;
        enum GUI_THEME : int
        {
            THEME_DARK,
            THEME_LIGHT,
            THEME_CLASSIC
        } mAppTheme         = THEME_LIGHT;
        bool mShowLogWindow = false;

    protected:
        // Not Saving
        bool mShowUIStatus = false;

    public:
        void onFontChanged(const std::string &fontPath, int fontIdx, float fontSize, bool applyNow);

    private:
        ImGui::LoggerWindow                mLogger;
        ImGui::FontChooseWindow            mFontChooser;
        IImGuiWindow                       mSettingsWindow;
        std::vector<SettingWindowCategory> mSettingCategories;
        SettingWindowCategory             *mCurrentSettingCategory = nullptr;

        SettingWindowItem *mCreateFilePathItem = nullptr;
        std::string        mCreateFilePath;
        ConfirmDialog      mCreateFileConfirmDialog;
    };

} // namespace ImGui

extern ImGui::ImGuiApplication *gUserApp;

#endif