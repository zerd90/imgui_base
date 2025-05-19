#ifndef _IMGUI_APPLICATION_H_
#define _IMGUI_APPLICATION_H_

#include <string>
#include <vector>
#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuiTools.h"
#include "ImGuiWindow.h"
#include "ApplicationSetting.h"

class ImGuiApplication : public ImGuiMainWindow
{
    friend void *WinSettingsHandler_ReadOpen(ImGuiContext *, ImGuiSettingsHandler *handler, const char *name);
    friend void  WinSettingsHandler_ReadLine(ImGuiContext *, ImGuiSettingsHandler *handler, void *entry,
                                             const char *line);
    friend void  WinSettingsHandler_WriteAll(ImGuiContext *imgui_ctx, ImGuiSettingsHandler *handler,
                                             ImGuiTextBuffer *buf);

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

    void addSettingArr(SettingValue::SettingType type, std::string name, int arrLen,
                       std::function<void(const void *)> setVal, std::function<void(void *)> getVal);

protected:
    // Set these in presetInternal
    ImGuiAppRect mWindowRect;
    std::string  mApplicationName;
    std::string  mExePath;
    std::string  mConfigPath;

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

private:
    bool mGuiVSync = true;
    enum GUI_THEME
    {
        THEME_DARK,
        THEME_LIGHT,
        THEME_CLASSIC
    } mAppTheme         = THEME_LIGHT;
    bool mShowLogWindow = false;

private:
    // Not Saving
    bool mShowUIStatus = false;

public:
    void onFontChanged(const std::string &fontPath, int fontIdx, float fontSize, bool applyNow);

private:
    LoggerWindow     mLogger;
    FontChooseWindow mFontChooser;
};

extern ImGuiApplication *gUserApp;

#endif