#ifndef _IMGUI_APPLICATION_H_
#define _IMGUI_APPLICATION_H_

#include <string>
#include <vector>
#include "imgui.h"
#include "imgui_internal.h"

#define Assert(expr)                                                            \
    do                                                                          \
    {                                                                           \
        if (!(expr))                                                            \
        {                                                                       \
            printf("Asset From (%s %d) fail(%s)\n", __func__, __LINE__, #expr); \
            abort();                                                            \
        }                                                                       \
    } while (0)

struct SettingValue
{
    enum SettingType
    {
        // single value
        SettingSingleValue = 0x00010000,
        SettingInt         = 0x00010001,
        SettingFloat       = 0x00010002,
        SettingDouble      = 0x00010003,
        SettingStdString   = 0x00010004,
        SettingBool        = 0x00010005,

        // constant length
        SettingArray     = 0x00020000,
        SettingArrInt    = 0x00020001,
        SettingArrFloat  = 0x00020002,
        SettingArrDouble = 0x00020003,

        // variable length
        SettingVector       = 0x00030000,
        SettingVecStdString = 0x00030001,
    };
    SettingValue(SettingType type, std::string name, void *valAddr) : mType(type), mName(name), mVal(valAddr) { checkVariable(); }
    SettingValue(SettingType type, std::string name, void *valAddr, double minVal, double maxVal, double defVal, int arrLen = 0)
        : mType(type), mName(name), mVal(valAddr), mArrLen(arrLen), mBorderVAl({minVal, maxVal, defVal})
    {
        checkVariable();
    }
    SettingValue(SettingType type, std::string name, void *valAddr, int arrLen)
        : mType(type), mName(name), mVal(valAddr), mArrLen(arrLen)
    {
        checkVariable();
    }

private:
    void checkVariable()
    {

        Assert(0 != (mType & 0x0000ffff));
        Assert(nullptr != mVal);
        Assert(SettingArray != (mType & 0xffff0000) || mArrLen > 0);
    }

public:
    SettingType mType;
    std::string mName;
    void       *mVal    = nullptr;
    int         mArrLen = 0;

    struct BorderValue
    {
        double minVal;
        double maxVal;
        double defVal;
    } mBorderVAl = {0};
};

class ImGuiApplication
{
public:
    struct ImGuiAppRect
    {
        int x, y, w, h;
    };
    ImGuiApplication();
    virtual ~ImGuiApplication() {}
    ImGuiAppRect         getWindowInitialRect() { return mWindowRect; };
    void         preset();
    std::string getAppName() { return mApplicationName; };
    void         windowRectChange(ImGuiAppRect rect)
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

    // return if need to exit the application
    virtual bool renderUI() { return false; };

    virtual void transferCmdArgs(std::vector<std::string> &args) {};
    virtual void dropFile(std::vector<std::string> &files) {};

    virtual void setRenderContext(void *data) {};
    virtual void setWindowHandle(void *handle) {};
    // load resources like Fonts; this should be called after preset() and configs loading, before first NewFrame(),
    virtual void loadResources() {}
    virtual void newFramePreAction() {}
    virtual void endFramePostAction() {}
    // anything ned to be done before program exit, such as waiting thread exit
    virtual void exit() {}

    static void *WinSettingsHandler_ReadOpen(ImGuiContext *, ImGuiSettingsHandler *handler, const char *name);
    static void  WinSettingsHandler_ReadLine(ImGuiContext *, ImGuiSettingsHandler *handler, void *entry, const char *line);
    static void  WinSettingsHandler_WriteAll(ImGuiContext *imgui_ctx, ImGuiSettingsHandler *handler, ImGuiTextBuffer *buf);

protected:
    virtual void presetInternal() {}

protected:
    // Set these in presetInternal
    ImGuiAppRect         mWindowRect;
    std::string mApplicationName;

protected:
    std::vector<SettingValue> mAppSettings;
};

void setApp(ImGuiApplication *app);

#endif